// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file Server.hpp
 * @brief Templated coroutine-based TCP Server with master acceptor and dynamic slave thread pool.
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
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/write.hpp>

#include <wavex/Engine/HttpRouter.hpp>
#include <wavex/protos/http/http1codec.hpp>
#include <wavex/protos/http/HttpRequest.hpp>
#include <wavex/protos/http/HttpResponse.hpp>
#include <wavex/Server/ThreadPool.hpp>

namespace wavex::server {
    /**
     * @class Server
     * @brief Coroutine TCP Server dispatching accepted streams across a dynamic work-stealing thread pool.
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

        /// Master thread acceptor loop
        asio::awaitable<void> accept_loop() {
            while (is_running_) {
                try {
                    asio::ip::tcp::socket socket = co_await acceptor_.async_accept();
                    // Dispatch accepted stream to the dynamic worker pool
                    pool_.spawn_coroutine(handle_client(std::move(socket)));
                } catch (const std::exception &e) {
                    std::cerr << "[Server] Accept error: " << e.what() << "\n";
                    break;
                }
            }
        }

        /// Client connection processing coroutine
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
                    not_found_res.status(404).send("Not Found"); // Not found Page to be added later.
                    std::string out = not_found_res.serialize();
                    co_await asio::async_write(socket, asio::buffer(out));
                    co_return;
                }

                ResponseType res(&socket);

                // Execute per-route middleware chain and handler
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
