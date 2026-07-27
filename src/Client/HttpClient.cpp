/**
 * @file HttpClient.cpp
 * @brief Implementation of HttpClient methods for 3rd-party HTTP service calls.
 */

#include <wavex/Client/HttpClient.hpp>
#include <asio/write.hpp>
#include <asio/connect.hpp>

namespace wavex::client {

    asio::awaitable<HttpResponse> HttpClient::send(HttpRequest req) {
        HttpResponse res;

        std::string raw_target(req.target());
        url::Url parsed_url = url::Url::parse(raw_target);

        if (parsed_url.host.empty()) {
            res.status(400).send("Invalid target URL: missing host");
            co_return std::move(res);
        }

        std::string host = parsed_url.host;
        uint16_t port = parsed_url.effective_port();
        std::string port_str = std::to_string(port);

        // Path and query for the HTTP request line
        std::string path_target = parsed_url.path.empty() ? "/" : parsed_url.path;
        if (!parsed_url.query.empty()) {
            path_target += "?";
            path_target += parsed_url.query;
        }
        req.target(path_target);

        // Ensure Host header is set
        if (!req.header("Host")) {
            std::string host_header = host;
            if (port != 80 && port != 443) {
                host_header += ":" + port_str;
            }
            req.set_header("Host", host_header);
        }

        // Ensure User-Agent header is set
        if (!req.header("User-Agent")) {
            req.set_header("User-Agent", "WaveX-Client/0.1.0");
        }

        // Default Connection header
        if (!req.header("Connection")) {
            req.set_header("Connection", "close");
        }

        auto executor = co_await asio::this_coro::executor;
        asio::ip::tcp::resolver resolver(executor);
        asio::ip::tcp::socket socket(executor);

        try {
            auto endpoints = co_await resolver.async_resolve(host, port_str, asio::use_awaitable);
            co_await asio::async_connect(socket, endpoints, asio::use_awaitable);

            std::string wire = req.serialize();
            co_await asio::async_write(socket, asio::buffer(wire), asio::use_awaitable);

            std::string response_buffer;
            char buf[4096];
            asio::error_code ec;

            while (true) {
                std::size_t bytes = co_await socket.async_read_some(
                    asio::buffer(buf), asio::redirect_error(asio::use_awaitable, ec));
                if (ec || bytes == 0) break;
                response_buffer.append(buf, bytes);
            }

            if (!res.parse(response_buffer)) {
                res.status(502).send("Bad Gateway: Invalid response format from upstream server");
            }
        } catch (const std::exception &ex) {
            res.status(502).send(std::string("Bad Gateway: ") + ex.what());
        }

        asio::error_code ignore_ec;
        socket.close(ignore_ec);

        // Use std::move to invoke the move ctor (which preserves
        // string_view validity via heap-pointer transfer) instead of
        // the copy ctor (which would leave views dangling).
        co_return std::move(res);
    }

    asio::awaitable<HttpResponse> HttpClient::get(const std::string_view url) {
        return request(method::GET, url);
    }

    asio::awaitable<HttpResponse> HttpClient::post(const std::string_view url, const nlohmann::json &json_body) {
        return request(method::POST, url, json_body.dump(), {{"Content-Type", "application/json"}});
    }

    asio::awaitable<HttpResponse> HttpClient::request(
        const method m,
        const std::string_view url,
        const std::string_view body,
        const std::vector<std::pair<std::string, std::string>> &headers) {

        HttpRequest req(m, url);
        for (const auto &[k, v]: headers) {
            req.set_header(k, v);
        }
        if (!body.empty()) {
            req.set_body(body);
        }
        return send(std::move(req));
    }

} // namespace wavex::client
