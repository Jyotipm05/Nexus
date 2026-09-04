// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/**
 * @file Server.hpp
 * @brief Templated coroutine-based TCP/TLS Server with master acceptor and dynamic slave thread pool.
 *
 * Parametric on `Codec` (default `http1codec`) and `RouterType` (default `HttpRouter`)
 * to easily support HTTP/1.x, HTTP/2, HTTP/3, and protocol upgrades.
 */

#pragma once

#ifndef ASIO_HAS_CO_AWAIT
#define ASIO_HAS_CO_AWAIT 1
#endif

#include <iostream>
#include <string>
#include <utility>
#include <memory>
#include <optional>
#include <filesystem>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/write.hpp>
#include <asio/use_awaitable.hpp>

#if WAVEX_HAS_SSL
#include <asio/ssl.hpp>
#endif

#include <wavex/Engine/HttpRouter.hpp>
#include <wavex/protos/http/http1codec.hpp>
#include <wavex/protos/http/HttpRequest.hpp>
#include <wavex/protos/http/HttpResponse.hpp>
#include <wavex/Server/ThreadPool.hpp>
#include <wavex/Server/TlsConfig.hpp>

namespace wavex::server {
    /**
     * @class Server
     * @brief Coroutine TCP/TLS Server dispatching accepted streams across a dynamic work-stealing thread pool.
     *
     * @tparam Codec Protocol codec (default `protos::http::http1codec`).
     * @tparam RouterType Router specialization (default `engine::HttpRouter`).
     */
    template<typename Codec = wavex::protos::http::http1codec, typename RouterType = wavex::engine::HttpRouter<Codec>>
    class Server {
    public:
        using codec_type = Codec;
        using RequestType = wavex::protos::http::HttpRequest<Codec>;
        using ResponseType = wavex::protos::http::HttpResponse<Codec>;

        /**
         * @brief Constructs a Server listening on the specified address and port.
         * @param router Reference to the protocol router.
         * @param address IP address or host string.
         * @param port Network port number.
         */
        Server(RouterType &router, std::string address, const unsigned short port)
            : router_(router),
              address_(std::move(address)),
              acceptor_(master_io_, asio::ip::tcp::endpoint(asio::ip::make_address(address_), port)),
              port_(port)
        {}

        ~Server() {
            stop();
        }

        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;

        /**
         * @brief Enables TLS 1.3 encryption on this server instance using a TlsConfig struct.
         * @param config TlsConfig struct containing certificate and key file paths.
         */
        void enable_tls(TlsConfig config) {
#if WAVEX_HAS_SSL
            tls_config_ = std::move(config);
            init_ssl();
            tls_enabled_ = true;
#else
            (void)config;
            throw std::runtime_error("Server::enable_tls failed: WaveX was built without OpenSSL TLS support (WAVEX_HAS_SSL=0)");
#endif
        }

        /**
         * @brief Convenience overload to enable TLS 1.3 directly with certificate and key file paths.
         * @param cert_file Path to PEM certificate chain file (defaults to ssl/test.crt).
         * @param key_file Path to PEM private key file (defaults to ssl/test.key).
         */
        void enable_tls(std::string cert_file = "ssl/test.crt", std::string key_file = "ssl/test.key") {
            TlsConfig cfg;
            cfg.cert_file = std::move(cert_file);
            cfg.key_file = std::move(key_file);
            enable_tls(std::move(cfg));
        }

        /**
         * @brief Checks if TLS encryption is enabled on this server instance.
         * @return True if TLS is enabled, false otherwise.
         */
        [[nodiscard]] bool is_tls_enabled() const { return tls_enabled_; }

        /// Start master acceptor loop and run event loop
        void run() {
            if (is_running_) [[unlikely]] {
                std::cerr << "Critical: Duplicate run() invocation detected!\n";
                return;
            }
            is_running_ = true;
            asio::co_spawn(master_io_, accept_loop(), asio::detached);
            master_io_.run();
        }

        /// Stop the server and thread pool
        void stop() {
            if (!is_running_) [[unlikely]] return;
            is_running_ = false;
            master_io_.stop();
        }

        /// Access the underlying thread pool
        ThreadPool &pool() { return pool_; }

    private:
        RouterType &router_;
        std::string address_;
        asio::io_context master_io_;
        asio::ip::tcp::acceptor acceptor_;
        ThreadPool pool_;
        unsigned short port_;
        bool is_running_{false};
        bool tls_enabled_{false};
        TlsConfig tls_config_;

#if WAVEX_HAS_SSL
        std::unique_ptr<asio::ssl::context> ssl_ctx_;

        /**
         * @brief Configures OpenSSL context for strict TLS 1.3 server operation.
         */
        void init_ssl() {
            ssl_ctx_ = std::make_unique<asio::ssl::context>(asio::ssl::context::tlsv13_server);
            ssl_ctx_->set_options(
                asio::ssl::context::default_workarounds |
                asio::ssl::context::no_sslv2 |
                asio::ssl::context::no_sslv3 |
                asio::ssl::context::no_tlsv1 |
                asio::ssl::context::no_tlsv1_1 |
                asio::ssl::context::no_tlsv1_2
            );

            if (!tls_config_.key_password.empty()) {
                ssl_ctx_->set_password_callback([pwd = tls_config_.key_password](std::size_t, asio::ssl::context::password_purpose) {
                    return pwd;
                });
            }

            std::string cert_path = tls_config_.cert_file;
            std::string key_path = tls_config_.key_file;

            if (!std::filesystem::exists(cert_path)) {
#ifdef PROJECT_DIR
                std::string alt = std::string(PROJECT_DIR) + "/" + cert_path;
                if (std::filesystem::exists(alt)) cert_path = alt;
#endif
                if (!std::filesystem::exists(cert_path) && std::filesystem::exists("../" + tls_config_.cert_file)) {
                    cert_path = "../" + tls_config_.cert_file;
                }
            }

            if (!std::filesystem::exists(key_path)) {
#ifdef PROJECT_DIR
                std::string alt = std::string(PROJECT_DIR) + "/" + key_path;
                if (std::filesystem::exists(alt)) key_path = alt;
#endif
                if (!std::filesystem::exists(key_path) && std::filesystem::exists("../" + tls_config_.key_file)) {
                    key_path = "../" + tls_config_.key_file;
                }
            }

            ssl_ctx_->use_certificate_chain_file(cert_path);
            ssl_ctx_->use_private_key_file(key_path, asio::ssl::context::pem);

            if (!tls_config_.dh_file.empty()) {
                ssl_ctx_->use_tmp_dh_file(tls_config_.dh_file);
            }
        }
#endif

        /// Master thread acceptor loop
        asio::awaitable<void> accept_loop() {
            while (is_running_) {
                try {
                    asio::ip::tcp::socket socket = co_await acceptor_.async_accept();
#if WAVEX_HAS_SSL
                    if (tls_enabled_ && ssl_ctx_) {
                        auto ssl_socket = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket>>(std::move(socket), *ssl_ctx_);
                        pool_.spawn_coroutine(handle_tls_client(std::move(ssl_socket)));
                        continue;
                    }
#endif
                    pool_.spawn_coroutine(handle_client(std::move(socket)));
                } catch (const std::exception &e) {
                    std::cerr << "[Server] Accept error: " << e.what() << "\n";
                    break;
                }
            }
        }

        /// Plain TCP client connection processing coroutine
        asio::awaitable<void> handle_client(asio::ip::tcp::socket socket) {
            try {
                char buffer[4096];
                std::size_t bytes_read = co_await socket.async_read_some(asio::buffer(buffer));
                std::string raw_data(buffer, bytes_read);

                RequestType req(raw_data);
                if (!req.parse()) {
                    ResponseType err_res;
                    err_res.status(400).send("Bad Request");
                    std::string out = err_res.serialize();
                    co_await asio::async_write(socket, asio::buffer(out));
                    co_return;
                }

                auto match = router_.resolve(req.method_type(), req.path());
                if (!match) {
                    ResponseType not_found_res;
                    not_found_res.status(404).send("Not Found");
                    std::string out = not_found_res.serialize();
                    co_await asio::async_write(socket, asio::buffer(out));
                    co_return;
                }

                ResponseType res(&socket);

                if (match->middlewares.empty()) {
                    co_await match->handler(req, res);
                } else {
                    co_await run_chain(req, res, match->middlewares, match->handler);
                }

                if (!res.is_sent()) {
                    std::string response_bytes = res.serialize();
                    co_await asio::async_write(socket, asio::buffer(response_bytes));
                }
            } catch (const std::exception &) {
                // Connection closed or socket error
            }
            co_return;
        }

#if WAVEX_HAS_SSL
        /// TLS client connection processing coroutine
        asio::awaitable<void> handle_tls_client(std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket_ptr) {
            auto& ssl_socket = *ssl_socket_ptr;
            try {
                co_await ssl_socket.async_handshake(asio::ssl::stream_base::server, asio::use_awaitable);

                char buffer[4096];
                std::size_t bytes_read = co_await ssl_socket.async_read_some(asio::buffer(buffer), asio::use_awaitable);
                std::string raw_data(buffer, bytes_read);

                RequestType req(raw_data);
                if (!req.parse()) {
                    ResponseType err_res;
                    err_res.status(400).send("Bad Request");
                    std::string out = err_res.serialize();
                    co_await asio::async_write(ssl_socket, asio::buffer(out), asio::use_awaitable);
                    co_return;
                }

                auto match = router_.resolve(req.method_type(), req.path());
                if (!match) {
                    ResponseType not_found_res;
                    not_found_res.status(404).send("Not Found");
                    std::string out = not_found_res.serialize();
                    co_await asio::async_write(ssl_socket, asio::buffer(out), asio::use_awaitable);
                    co_return;
                }

                ResponseType res;

                if (match->middlewares.empty()) {
                    co_await match->handler(req, res);
                } else {
                    co_await run_chain(req, res, match->middlewares, match->handler);
                }

                std::string response_bytes = res.serialize();
                co_await asio::async_write(ssl_socket, asio::buffer(response_bytes), asio::use_awaitable);
            } catch (const std::exception &) {
                // Connection closed or SSL handshake/read error
            }
            co_return;
        }
#endif

        /// Generic middleware chain runner helper for arbitrary CRTP Request/Response types
        template <typename ReqT, typename ResT, typename MwVec, typename H>
        static asio::awaitable<void> run_chain(
            ReqT &req,
            ResT &res,
            const MwVec &mws,
            const H &handler) {
            std::size_t idx = 0;
            while (idx < mws.size() && !res.is_sent()) {
                bool next_called = false;
                wavex::base::Next next = [&next_called]() -> asio::awaitable<void> {
                    next_called = true;
                    co_return;
                };

                co_await mws[idx](req, res, std::move(next));
                idx++;

                if (!next_called || res.is_sent()) {
                    break;
                }
            }

            if (!res.is_sent()) {
                co_await handler(req, res);
            }
            co_return;
        }
    };

    /// Concrete default HTTP/1.x server type aliases
    using Http1Server = Server<wavex::protos::http::http1codec, wavex::engine::Http1Router>;
    using http1server = Http1Server;
} // namespace wavex::server
