/**
 * @file Router.hpp
 * @brief Protocol-agnostic hybrid radix-tree router with RE2 regex constraints.
 *
 * The Router<Proto> template is parameterised on a protocol type that must
 * provide a `Proto::method` enum type. This allows the same routing engine
 * to serve HTTP, MCP, GraphQL, or any future protocol.
 *
 * Static segments use compressed radix-tree edges.
 * Dynamic segments (:param, {name:regex}) and wildcards (*, *name) are checked
 * in priority order after static children fail.
 *
 * Provides a thread-safe singleton access pattern (`Router<Proto>::instance()`)
 * that maintains unique instance identity across both header inclusions and C++20
 * module imports according to the C++ standard ODR rules.
 *
 * Performance notes (resolve() hot path):
 *  - Path normalisation is allocation-free when the incoming path is already
 *    well-formed (leading '/', no trailing '/'), which is the common case for
 *    paths coming straight off an HTTP request line.
 *  - Segments are std::string_view slices into the (possibly caller-owned)
 *    path buffer instead of individually heap-allocated std::string objects.
 *  - Static-child dispatch is O(1) average via a string_view-keyed hash index
 *    rather than a linear scan; the index's views are safe because children
 *    are stored as unique_ptr<Node>, so a Node's address (and its `prefix`
 *    storage) never moves once created, even as sibling vectors reallocate.
 */

#pragma once

#ifndef ASIO_HAS_CO_AWAIT
#define ASIO_HAS_CO_AWAIT 1
#endif

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include <functional>
#include <algorithm>

#include <re2/re2.h>

#include <wavex/Base/Request.hpp>
#include <wavex/Base/Response.hpp>
#include <wavex/Base/MiddleWare.hpp>
#include <asio/awaitable.hpp>

namespace wavex::engine {
    // Handler signature — all handlers are coroutine-based
    using Handler = std::function<asio::awaitable<void>(base::Request &, base::Response &)>;

    /**
     * @struct RouteMatch
     * @brief Result of a successful route resolution.
     */
    struct RouteMatch {
        Handler handler;
        std::vector<base::MiddlewareFn> middlewares; // full chain for this route
        std::unordered_map<std::string, std::string> params;
    };

    /**
     * @struct ScopedMiddleware
     * @brief A middleware bound to a path prefix.
     */
    struct ScopedMiddleware {
        std::string prefix;
        base::MiddlewareFn fn;
    };

    /**
     * @class Router
     * @brief Protocol-agnostic radix-tree router.
     *
     * @tparam Proto Protocol type. Must have a nested `method` enum class.
     */
    template<typename Proto>
    class Router {
    public:
        using MethodType = Proto::method;

        /**
         * @brief Constructs an empty router with a single root node at "/".
         */
        Router() {
            root_ = std::make_unique<Node>();
            root_->prefix = "/";
        }

        /**
         * @brief Singleton instance for a given protocol type.
         * @return Reference to the process-wide Router<Proto> instance, guaranteed
         *         unique across translation units and C++20 module boundaries by
         *         C++ ODR inline rules.
         */
        static Router &instance() {
            static Router s_instance;
            return s_instance;
        }

        /**
         * @brief Clears all registered routes and middlewares, resetting router state.
         */
        void clear() {
            root_ = std::make_unique<Node>();
            root_->prefix = "/";
            middlewares_.clear();
            regex_cache_.clear();
        }

        // ---------------------------------------------------------------
        //  Route registration
        // ---------------------------------------------------------------

        /**
         * @brief Registers a handler for a given method + pattern.
         * @param m Method to associate the handler with.
         * @param pattern Route pattern; may contain static segments, `:param`,
         *        `{name}` / `{name:regex}` params, or `*` / `*name` wildcards.
         * @param h Coroutine handler invoked on a match.
         */
        void route(MethodType m, const std::string_view pattern, Handler h) {
            route(m, pattern, {}, std::move(h));
        }

        /**
         * @brief Registers a handler with per-route middlewares for a given method + pattern.
         * @param m Method to associate the handler with.
         * @param pattern Route pattern (see the single-middleware overload for syntax).
         * @param mws Middlewares run only for this specific route, after global/scoped ones.
         * @param h Coroutine handler invoked on a match.
         */
        void route(MethodType m, const std::string_view pattern,
                   std::vector<base::MiddlewareFn> mws, Handler h) {
            // Normalise + split without allocating for the common case of an
            // already-well-formed literal pattern (e.g. "/api/users/:id").
            // `insert_segment` only allocates an owned std::string when it
            // actually needs to create a *new* node — matching an existing
            // static child is a pure view comparison.
            std::string scratch;
            const std::string_view normalized = normalize_path_view(pattern, scratch);
            const auto segments = split_path_view(normalized);

            // Walk/create the tree
            Node *current = root_.get();
            for (const auto &seg: segments) {
                current = insert_segment(current, seg);
            }

            // Store handler and per-route middlewares at the leaf
            current->handlers[m] = std::move(h);
            if (!mws.empty()) {
                current->route_middlewares[m] = std::move(mws);
            }
        }

        // ---------------------------------------------------------------
        //  Middleware registration
        // ---------------------------------------------------------------

        /**
         * @brief Registers a global middleware, applied to all routes.
         * @param mw Middleware function to add to the global chain.
         */
        void use(base::MiddlewareFn mw) {
            middlewares_.emplace_back("", std::move(mw));
        }

        /**
         * @brief Registers a scoped middleware, applied only to routes whose
         *        normalised path starts with `prefix`.
         * @param prefix Path prefix to scope the middleware to; empty behaves
         *        like the global overload.
         * @param mw Middleware function to add to the chain.
         */
        void use(const std::string_view prefix, base::MiddlewareFn mw) {
            if (prefix.empty()) [[unlikely]] {
                middlewares_.emplace_back("", std::move(mw));
            } else [[likely]] {
                middlewares_.emplace_back(normalize_path(prefix), std::move(mw));
            }
        }

        // ---------------------------------------------------------------
        //  Route resolution — O(path_length), hot path
        // ---------------------------------------------------------------

        /**
         * @brief Resolves a method + path to a registered handler.
         * @param m Method to look up.
         * @param path Request path to resolve; need not be pre-normalised.
         * @return A RouteMatch (handler, resolved params, and the full ordered
         *         middleware chain) on success, or std::nullopt if no route matches.
         */
        [[nodiscard]] std::optional<RouteMatch> resolve(MethodType m, const std::string_view path) const {
            // `scratch` only actually allocates when `path` isn't already
            // normalised (no leading '/', or a trailing '/'); the common
            // case coming off a parsed HTTP request line needs no copy.
            std::string scratch;
            const std::string_view normalized = normalize_path_view(path, scratch);
            const auto segments = split_path_view(normalized);

            std::unordered_map<std::string, std::string> params;
            const Node *node = resolve_node(root_.get(), segments, 0, params);

            if (!node) return std::nullopt;

            const auto it = node->handlers.find(m);
            if (it == node->handlers.end()) return std::nullopt;

            const auto mw_it = node->route_middlewares.find(m);
            const size_t route_mw_count = (mw_it != node->route_middlewares.end()) ? mw_it->second.size() : 0;

            // Build the full middleware chain:
            // [global] -> [scoped by prefix] -> [per-route] -> handler
            std::vector<base::MiddlewareFn> chain;
            chain.reserve(middlewares_.size() + route_mw_count);

            for (const auto &[prefix, fn]: middlewares_) {
                if (prefix.empty() || normalized.starts_with(prefix)) {
                    chain.emplace_back(fn);
                }
            }

            if (route_mw_count) {
                chain.insert(chain.end(), mw_it->second.begin(), mw_it->second.end());
            }

            return RouteMatch{
                it->second,
                std::move(chain),
                std::move(params)
            };
        }

    protected:
        /**
         * @struct Node
         * @brief A single radix-tree node: a static edge, a param/wildcard
         *        binder, or both a route leaf (handlers) and an internal
         *        branch simultaneously.
         */
        struct Node {
            Node() = default;

            std::string prefix; // segment label for static nodes
            bool is_param = false; // dynamic/wildcard flag
            std::string pattern; // original regex pattern string
            std::shared_ptr<re2::RE2> constraint; // compiled RE2 regex constraint (shared via regex cache)
            std::string param_name; // extracted parameter name

            std::unordered_map<MethodType, Handler> handlers{};
            std::unordered_map<MethodType, std::vector<base::MiddlewareFn> > route_middlewares{};

            std::vector<std::unique_ptr<Node> > children{}; // static children (ownership)
            // O(1)-average dispatch index for static children. Keys are
            // string_views into each child's own `prefix` member; this is
            // safe because `children` stores unique_ptr<Node>, so a Node's
            // address — and therefore its `prefix` storage — is stable for
            // the Node's whole lifetime regardless of how `children` (or
            // `static_index`) reallocates.
            std::unordered_map<std::string_view, Node *> static_index{};
            std::vector<std::unique_ptr<Node> > param_children{}; // dynamic/regex children
            std::unique_ptr<Node> wildcard_child; // * or *name catch-all child
        };

        std::unique_ptr<Node> root_;
        std::vector<ScopedMiddleware> middlewares_;

        // Cache of compiled RE2 constraints keyed by pattern text. Real route
        // tables commonly reuse the same constraint (e.g. "[0-9]+" for every
        // ":id"-like param) under many different prefixes; without this,
        // each occurrence would separately pay RE2's (relatively expensive)
        // compilation cost at registration time. Not accessed on resolve()'s
        // hot path — only during route().
        std::unordered_map<std::string, std::shared_ptr<re2::RE2> > regex_cache_;

    private:
        // ---------------------------------------------------------------
        //  Path utilities — registration-time (owning; not perf-critical)
        // ---------------------------------------------------------------

        /**
         * @brief Normalises a path into an owned string: ensures a leading
         *        '/' and strips any trailing '/'.
         * @param path Path to normalise.
         * @return Owned, normalized copy of `path`.
         */
        static std::string normalize_path(const std::string_view path) {
            if (path.empty() || path == "/") return "/";
            std::string p(path);
            if (p.front() != '/') [[unlikely]] p.insert(p.begin(), '/');
            if (p.size() > 1 && p.back() == '/') [[likely]] p.pop_back();
            return p;
        }

        // ---------------------------------------------------------------
        //  Path utilities — shared by registration and resolve(): allocation-
        //  free normalisation when possible, string_view segments (no per-
        //  segment heap allocation). Used by both route() and resolve().
        // ---------------------------------------------------------------

        /**
         * @brief Normalises `path`, writing into `scratch` only if needed,
         *        and returns a view of the result.
         *
         * `scratch` is owned by the caller so the returned view is valid for
         * exactly as long as the caller's `scratch` object is alive — no
         * dangling self-reference risk from returning a view-into-local by
         * value. `scratch` is cleared unconditionally before use, so this
         * function is safe to call with a *reused* buffer (e.g. a
         * thread-local scratch pool) — it never relies on the caller having
         * passed in an empty string.
         *
         * @param path Path to normalise; need not already be well-formed.
         * @param scratch Caller-owned buffer used only if `path` isn't
         *        already normalised (no leading '/', or has a trailing '/').
         * @return A view of the normalised path — either `path` unchanged
         *         (zero-copy fast path) or `scratch`.
         */
        [[nodiscard]] static std::string_view normalize_path_view(const std::string_view path, std::string &scratch) {
            if (path.empty() || path == "/") return "/";

            const bool needs_front = path.front() != '/';
            const bool needs_trim = path.size() > 1 && path.back() == '/';

            if (!needs_front && !needs_trim) [[likely]] {
                // ReSharper disable once CppDFALocalValueEscapesFunction
                return path; // zero-copy: caller's buffer is already normalised
            }

            scratch.clear();
            scratch.reserve(path.size() + (needs_front ? 1 : 0));
            if (needs_front) scratch.push_back('/');
            scratch.append(path);
            if (needs_trim && scratch.size() > 1 && scratch.back() == '/') {
                scratch.pop_back();
            }
            return scratch;
        }

        /**
         * @brief Splits an already-normalised path into segment views.
         * @param path Normalized path (see normalise_path_view()).
         * @return Segments as views into `path`; empty for "/" or an empty path.
         */
        [[nodiscard]] static std::vector<std::string_view> split_path_view(std::string_view path) {
            std::vector<std::string_view> segments;
            if (path.empty() || path == "/") return segments;

            segments.reserve(static_cast<size_t>(std::ranges::count(path, '/')));

            size_t start = 1;
            while (start < path.size()) {
                size_t end = path.find('/', start);
                if (end == std::string_view::npos) end = path.size();
                if (end != start) {
                    segments.emplace_back(path.substr(start, end - start));
                }
                start = end + 1;
            }
            return segments;
        }

        // ---------------------------------------------------------------
        //  Tree insertion (registration-time, but startup-cost-sensitive
        //  for large route tables). `segment` is a view — no allocation
        //  happens on the (common) path where it matches an existing
        //  child; a std::string is only materialised when a genuinely new
        //  node needs to own its label/param name/pattern.
        // ---------------------------------------------------------------

        /**
         * @brief Inserts (or finds) the child of `parent` for a single path segment.
         *
         * `segment` is a view — no allocation happens on the common path
         * where it matches an existing child; a std::string is only
         * materialised when a genuinely new node needs to own its
         * label/param name/pattern.
         *
         * @param parent Node to insert under.
         * @param segment Single path segment (e.g. "users", ":id", "{id:[0-9]+}", "*rest").
         * @return The existing or newly created child node for `segment`.
         */
        Node *insert_segment(Node *parent, std::string_view segment) {
            // 1. Check if param segment: :name
            if (!segment.empty() && segment[0] == ':') {
                std::string pname(segment.substr(1)); // direct-init: string_view -> string ctor is explicit
                for (const auto &child: parent->param_children) {
                    if (child->is_param && !child->constraint && child->param_name == pname) {
                        return child.get();
                    }
                }
                auto new_node = std::make_unique<Node>();
                new_node->is_param = true;
                new_node->param_name = pname;
                Node *res = new_node.get();
                parent->param_children.emplace_back(std::move(new_node));
                return res;
            }

            // 2. Check if brace param segment: {name} or {name:pattern}
            if (segment.size() >= 2 && segment[0] == '{' && segment.back() == '}') {
                const auto inner = segment.substr(1, segment.size() - 2);
                if (const size_t colon = inner.find(':'); colon != std::string_view::npos) {
                    auto pname = std::string(inner.substr(0, colon));
                    auto pat = std::string(inner.substr(colon + 1));
                    for (const auto &child: parent->param_children) {
                        if (child->is_param && child->constraint && child->param_name == pname && child->pattern ==
                            pat) {
                            return child.get();
                        }
                    }
                    auto new_node = std::make_unique<Node>();
                    new_node->is_param = true;
                    new_node->param_name = pname;
                    new_node->pattern = pat;
                    // Reuse a compiled RE2 for this pattern text if some
                    // other route already registered it (common: the same
                    // constraint, e.g. "[0-9]+", reused under many prefixes).
                    if (auto &cached = regex_cache_[pat]; cached) {
                        new_node->constraint = cached;
                    } else {
                        cached = std::make_shared<re2::RE2>(pat);
                        new_node->constraint = cached;
                    }
                    Node *res = new_node.get();
                    parent->param_children.emplace_back(std::move(new_node));
                    return res;
                }
                // Unconstrained brace param: {name} (works identically to :name)
                auto pname = std::string(inner);
                for (const auto &child: parent->param_children) {
                    if (child->is_param && !child->constraint && child->param_name == pname) {
                        return child.get();
                    }
                }
                auto new_node = std::make_unique<Node>();
                new_node->is_param = true;
                new_node->param_name = pname;
                Node *res = new_node.get();
                parent->param_children.emplace_back(std::move(new_node));
                return res;
            }

            // 3. Check if wildcard: * or *name
            if (!segment.empty() && segment[0] == '*') {
                if (!parent->wildcard_child) {
                    parent->wildcard_child = std::make_unique<Node>();
                    parent->wildcard_child->param_name = segment.size() > 1 ? segment.substr(1) : "*";
                    parent->wildcard_child->is_param = true;
                }
                return parent->wildcard_child.get();
            }

            // 4. Static segment — O(1) average lookup via static_index.
            if (const auto found = parent->static_index.find(segment); found != parent->static_index.end()) {
                return found->second;
            }

            auto new_node = std::make_unique<Node>();
            new_node->prefix = segment;
            Node *res = new_node.get();
            // Index by a view into the child's own storage (see Node comment).
            parent->static_index.emplace(std::string_view(new_node->prefix), res);
            parent->children.emplace_back(std::move(new_node));
            return res;
        }

        // ---------------------------------------------------------------
        //  Tree resolution (hot path)
        // ---------------------------------------------------------------

        /**
         * @brief Recursively resolves `segments[depth…]` against `node`,
         *        trying static, then param/regex, then wildcard children in
         *        priority order, and backtracking on dead ends.
         * @param node Node to resolve from.
         * @param segments Full path segment list for the request.
         * @param depth Index into `segments` currently being matched.
         * @param params Accumulator for resolved param values; entries for a
         *        param/regex child are erased again on backtrack.
         * @return The matching leaf node (with a non-empty handler map), or
         *         nullptr if no match exists under `node`.
         */
        const Node *resolve_node(const Node *node,
                                 const std::vector<std::string_view> &segments,
                                 const size_t depth,
                                 std::unordered_map<std::string, std::string> &params) const {
            if (depth == segments.size()) {
                // We've consumed all segments — check if this node has handlers
                if (!node->handlers.empty()) return node;
                return nullptr;
            }

            const auto &segment = segments[depth];

            // 1. Static children first (highest priority) — O(1) average.
            if (const auto found = node->static_index.find(segment); found != node->static_index.end()) {
                if (auto result = resolve_node(found->second, segments, depth + 1, params)) return result;
            }

            // 2. Dynamic / Regex param children
            for (const auto &child: node->param_children) {
                bool match = true;
                if (child->constraint) {
                    match = re2::RE2::FullMatch(re2::StringPiece(segment.data(), segment.size()), *child->constraint);
                }
                if (match) {
                    params[child->param_name] = std::string(segment);
                    if (auto result = resolve_node(child.get(), segments, depth + 1, params)) return result;
                    params.erase(child->param_name); // backtrack
                }
            }

            // 3. Wildcard child (*) — captures all remaining segments
            if (node->wildcard_child) {
                std::string remaining;
                for (size_t i = depth; i < segments.size(); ++i) {
                    if (!remaining.empty()) remaining += '/';
                    remaining += segments[i];
                }
                params[node->wildcard_child->param_name] = std::move(remaining);
                if (!node->wildcard_child->handlers.empty()) {
                    return node->wildcard_child.get();
                }
            }

            return nullptr;
        }
    };
} // namespace wavex::engine
