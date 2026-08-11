/**
 * @file HttpClient.hpp
 * @brief Async coroutine-based HTTP/1.1 client for sending requests to 3rd-party services.
 */

#pragma once

#ifndef ASIO_HAS_CO_AWAIT
#define ASIO_HAS_CO_AWAIT 1
#endif

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <optional>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>
#include <asio/co_spawn.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/redirect_error.hpp>
#include <nlohmann/json.hpp>

#include <wavex/Base/Url.hpp>
#include <wavex/protos/http/HttpRequest.hpp>
#include <wavex/protos/http/HttpResponse.hpp>

namespace wavex::client {
    using wavex::protos::http::method;
    using wavex::protos::http::HttpRequest;
    using wavex::protos::http::HttpResponse;

    /**
     * @class HttpClient
     * @brief Coroutine-native HTTP client for sending requests and receiving responses from 3rd-party services.
     */
    class HttpClient {
    public:
        HttpClient() = default;

        /**
         * @brief Send an HttpRequest and await the HttpResponse asynchronously.
         * @param req The HTTP request object. Target URL can be full URL ("http://api.site.com/v1/data").
         * @return Coroutine awaitable producing the populated HttpResponse.
         */
        static asio::awaitable<HttpResponse> send(HttpRequest req);

        /**
         * @brief Convenience GET request to a 3rd party URL.
         * @param url Full target URL, e.g. "http://127.0.0.1:8080/api/json"
         * @return Coroutine awaitable producing HttpResponse.
         */
        static asio::awaitable<HttpResponse> get(std::string_view url);

        /**
         * @brief Convenience POST JSON request to a 3rd party URL.
         * @param url Full target URL
         * @param json_body JSON payload
         * @return Coroutine awaitable producing HttpResponse.
         */
        static asio::awaitable<HttpResponse> post(std::string_view url, const nlohmann::json &json_body);

        /**
         * @brief Send a generic HTTP request by method and URL.
         * @param m HTTP method (GET, POST, PUT, DELETE, etc.)
         * @param url Full target URL
         * @param body Request body
         * @param headers Extra request headers as key-value pairs
         * @return Coroutine awaitable producing HttpResponse.
         */
        static asio::awaitable<HttpResponse> request(
            method m,
            std::string_view url,
            std::string_view body = "",
            const std::vector<std::pair<std::string, std::string>> &headers = {});
    };
} // namespace wavex::client
