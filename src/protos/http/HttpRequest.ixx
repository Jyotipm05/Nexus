/**
 * @file HttpRequest.ixx
 * @brief C++ module interface for HttpRequest in WaveX.
 */

module;

#include <wavex/protos/http/HttpRequest.hpp>

export module wavex:protos_http_request;

export namespace wavex::protos::http {
    using wavex::protos::http::HttpRequest;
    using wavex::protos::http::Http1Request;
    using wavex::protos::http::http1request;
}
