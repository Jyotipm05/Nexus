// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/**
 * @file Chainable.hpp
 * @brief C++23 Explicit Object Parameter ("Deducing This") static dispatch interface.
 *
 * Provides the wavex::Chainable base class for zero-overhead compile-time
 * polymorphism, method chaining, static pipeline composition, and semi-static handlers.
 */

#pragma once

#include <utility>
#include <string_view>
#include <concepts>
#include <tuple>
#include <functional>

#ifndef ASIO_HAS_CO_AWAIT
#define ASIO_HAS_CO_AWAIT 1
#endif
#include <asio/awaitable.hpp>

namespace wavex {

    /**
     * @class Chainable
     * @brief C++23 "Deducing This" CRTP replacement.
     *
     * Enables compile-time static dispatch without requiring the base class
     * to be a template parameterized on the derived type (e.g. `Derived : Chainable`
     * instead of `Derived : Chainable<Derived>`).
     *
     * Value category (lvalue, rvalue, const) is preserved automatically via
     * `std::forward<Self>(self)`.
     */
    class Chainable {
    protected:
        /// Non-virtual protected destructor prevents unsafe polymorphic deletion through Base*
        ~Chainable() = default;

    public:
        /**
         * @brief Statically dispatches to `self.handle_impl(args...)`.
         * @tparam Self Deduced type and value category of the calling object (`this`).
         * @tparam Args Types of arguments forwarded to the handler.
         * @param self Explicit object parameter (`this Self&&`).
         * @param args Arguments forwarded to `handle_impl`.
         * @return Result of `self.handle_impl(args...)`.
         */
        template <typename Self, typename... Args>
        decltype(auto) handle(this Self&& self, Args&&... args) {
            return std::forward<Self>(self).handle_impl(std::forward<Args>(args)...);
        }

        /**
         * @brief Statically dispatches to `self.name_impl()`.
         * @tparam Self Deduced type and value category of the calling object (`this`).
         * @param self Explicit object parameter (`this Self&&`).
         * @return Result of `self.name_impl()`.
         */
        template <typename Self>
        decltype(auto) name(this Self&& self) {
            return std::forward<Self>(self).name_impl();
        }
    };

    /**
     * @concept ChainableHandler
     * @brief Concept ensuring a type satisfies the Chainable handle requirements for given arguments.
     * @tparam T Candidate handler type.
     * @tparam Args Argument types.
     */
    template <typename T, typename... Args>
    concept ChainableHandler = requires(T& handler, Args&&... args) {
        handler.handle(std::forward<Args>(args)...);
    };

    /**
     * @class StaticChain
     * @brief Static compile-time tuple-based pipeline of Chainable handlers.
     *
     * Evaluates a sequence of handlers in order with zero dynamic memory allocation
     * and short-circuit capability.
     *
     * @tparam Handlers Sequence of Chainable handler types stored in the static pipeline tuple.
     */
    template <typename... Handlers>
    class StaticChain {
        std::tuple<Handlers...> handlers_;

        template <typename T>
        static decltype(auto) unwrap(T& t) { return t; }

        template <typename T>
        static decltype(auto) unwrap(std::reference_wrapper<T>& rw) { return rw.get(); }

        template <typename T>
        static decltype(auto) unwrap(const std::reference_wrapper<T>& rw) { return rw.get(); }

    public:
        /**
         * @brief Constructs a StaticChain with the specified handler instances.
         * @param handlers Handlers to move into the internal tuple pipeline.
         */
        constexpr explicit StaticChain(Handlers... handlers)
            : handlers_(std::move(handlers)...) {}

        /**
         * @brief Const accessor to the internal tuple of handlers.
         * @return Const reference to internal std::tuple.
         */
        [[nodiscard]] constexpr const std::tuple<Handlers...>& get_tuple() const {
            return handlers_;
        }

        /**
         * @brief Mutable accessor to the internal tuple of handlers.
         * @return Mutable reference to internal std::tuple.
         */
        [[nodiscard]] constexpr std::tuple<Handlers...>& get_tuple() {
            return handlers_;
        }

        /**
         * @brief Synchronously processes all handlers in sequence.
         * @tparam Args Context argument types.
         * @param args Context arguments forwarded to each handler.
         * @return true if all handlers succeeded (or returned true), false if short-circuited.
         */
        template <typename... Args>
        bool process_all(Args&&... args) {
            return std::apply([&](auto&... h) {
                return (unwrap(h).handle(args...) && ...);
            }, handlers_);
        }

        /**
         * @brief Coroutine asynchronous process_all for coroutine-based handlers.
         * @tparam Args Context argument types.
         * @param args Context arguments forwarded to each handler.
         * @return asio::awaitable<bool> returning true if all handlers succeeded, false if short-circuited.
         */
        template <typename... Args>
        asio::awaitable<bool> process_all_async(Args&&... args) {
            auto run_one = [&]<typename H>(H& h) -> asio::awaitable<bool> {
                using Ret = decltype(unwrap(h).handle(args...));
                if constexpr (std::same_as<Ret, bool>) {
                    co_return unwrap(h).handle(args...);
                } else if constexpr (requires { co_await unwrap(h).handle(args...); }) {
                    using AwaitRet = decltype(co_await unwrap(h).handle(args...));
                    if constexpr (std::same_as<AwaitRet, bool>) {
                        co_return co_await unwrap(h).handle(args...);
                    } else {
                        co_await unwrap(h).handle(args...);
                        co_return true;
                    }
                } else {
                    unwrap(h).handle(args...);
                    co_return true;
                }
            };

            auto tuple_runner = [&](auto&... h) -> asio::awaitable<bool> {
                bool ok = true;
                ((ok = ok && co_await run_one(h)), ...);
                co_return ok;
            };

            co_return co_await std::apply(tuple_runner, handlers_);
        }
    };

    /**
     * @brief Helper factory function to deduce StaticChain types cleanly.
     * @tparam Handlers Deduced types of handlers.
     * @param handlers Handler arguments.
     * @return StaticChain<std::decay_t<Handlers>...> instance.
     */
    template <typename... Handlers>
    constexpr auto make_chain(Handlers&&... handlers) {
        return StaticChain<std::decay_t<Handlers>...>(std::forward<Handlers>(handlers)...);
    }

    /**
     * @class ConditionalChainable
     * @brief Semi-static wrapper around a Chainable handler allowing runtime enable/disable toggling.
     * @tparam Handler Inner Chainable handler type.
     */
    template <typename Handler>
    class ConditionalChainable : public Chainable {
        Handler handler_;
        bool enabled_ = true;

    public:
        /**
         * @brief Constructs a ConditionalChainable wrapper around a handler.
         * @param h Inner handler instance.
         * @param enabled Initial enable state (defaults to true).
         */
        constexpr explicit ConditionalChainable(Handler h, bool enabled = true)
            : handler_(std::move(h)), enabled_(enabled) {}

        /**
         * @brief Enables or disables this handler dynamically.
         * @param enabled True to enable execution, false to bypass.
         */
        void set_enabled(bool enabled) { enabled_ = enabled; }

        /**
         * @brief Checks if this handler is currently enabled.
         * @return True if enabled, false otherwise.
         */
        [[nodiscard]] bool is_enabled() const { return enabled_; }

        /**
         * @brief Statically dispatches to inner handler if enabled, or bypasses if disabled.
         * @tparam Self Deduced type and value category of object (`this`).
         * @tparam Args Forwarded argument types.
         * @param self Explicit object parameter.
         * @param args Forwarded arguments.
         * @return Result of inner handler execution or bypass fallback.
         */
        template <typename Self, typename... Args>
        decltype(auto) handle_impl(this Self&& self, Args&&... args) {
            if (!self.enabled_) {
                using Ret = decltype(self.handler_.handle(std::forward<Args>(args)...));
                if constexpr (std::same_as<Ret, bool>) {
                    return true;
                } else if constexpr (requires { co_await self.handler_.handle(std::forward<Args>(args)...); }) {
                    auto bypass = []() -> asio::awaitable<bool> { co_return true; };
                    return bypass();
                } else {
                    return;
                }
            }
            return self.handler_.handle(std::forward<Args>(args)...);
        }

        /**
         * @brief Statically dispatches name query to inner handler.
         * @tparam Self Deduced type and value category of object (`this`).
         * @param self Explicit object parameter.
         * @return Result of inner handler's name().
         */
        template <typename Self>
        decltype(auto) name_impl(this Self&& self) {
            return self.handler_.name();
        }
    };

} // namespace wavex
