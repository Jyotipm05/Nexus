/**
 * @file HttpRouter.hpp
 * @brief HTTP-specific router with get/post/put/del convenience methods.
 *
 * Derives from Router<HttpProto> to add HTTP method shortcuts while
 * keeping the base router protocol-agnostic.
 */

#pragma once

#include <wavex/Engine/Router.hpp>
#include <wavex/protos/http/Methods.hpp>

#undef DELETE

namespace wavex::engine {
    /**
     * @struct HttpProto
     * @brief Protocol adapter for HTTP — provides the method type.
     */
    struct HttpProto {
        using method = protos::http::method;
    };

    /**
     * @class HttpRouter
     * @brief HTTP-specific convenience wrapper around Router<HttpProto>.
     */
    class HttpRouter : public Router<HttpProto> {
    public:
        using Router::route;
        using Router::use;
        using Router::resolve;

        /// Singleton instance getter for HttpRouter.
        static HttpRouter &instance() {
            static HttpRouter s_instance;
            return s_instance;
        }

        // ---------------------------------------------------------------
        //  Simple registration (method + pattern + handler)
        // ---------------------------------------------------------------

        void get(const std::string_view pattern, Handler h) {
            route(protos::http::method::GET, pattern, std::move(h));
        }

        void post(const std::string_view pattern, Handler h) {
            route(protos::http::method::POST, pattern, std::move(h));
        }

        void put(const std::string_view pattern, Handler h) {
            route(protos::http::method::PUT, pattern, std::move(h));
        }

        void del(const std::string_view pattern, Handler h) {
            route(protos::http::method::DELETE, pattern, std::move(h));
        }

        void head(const std::string_view pattern, Handler h) {
            route(protos::http::method::HEAD, pattern, std::move(h));
        }

        void options(const std::string_view pattern, Handler h) {
            route(protos::http::method::OPTIONS, pattern, std::move(h));
        }

        void patch(const std::string_view pattern, Handler h) {
            route(protos::http::method::PATCH, pattern, std::move(h));
        }

        void query(const std::string_view pattern, Handler h) {
            route(protos::http::method::QUERY, pattern, std::move(h));
        }

        // ---------------------------------------------------------------
        //  Registration with per-route middlewares
        // ---------------------------------------------------------------

        void get(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::GET, pattern, std::move(mws), std::move(h));
        }

        void post(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::POST, pattern, std::move(mws), std::move(h));
        }

        void put(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::PUT, pattern, std::move(mws), std::move(h));
        }

        void del(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::DELETE, pattern, std::move(mws), std::move(h));
        }

        void head(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::HEAD, pattern, std::move(mws), std::move(h));
        }

        void options(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::OPTIONS, pattern, std::move(mws), std::move(h));
        }

        void patch(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::PATCH, pattern, std::move(mws), std::move(h));
        }

        void query(const std::string_view pattern, std::vector<base::MiddlewareFn> mws, Handler h) {
            route(protos::http::method::QUERY, pattern, std::move(mws), std::move(h));
        }
    };
} // namespace wavex::engine
