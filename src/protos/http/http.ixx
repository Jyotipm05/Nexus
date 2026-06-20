/**
 * @file http.ixx
 * @brief Primary C++ module interface partition for HTTP protocol support in WaveX.
 */

module;

#include <wavex/protos/http/Methods.hpp>
#include <wavex/protos/http/http1codec.hpp>
#include <wavex/protos/http/HttpRequest.hpp>
#include <wavex/protos/http/HttpResponse.hpp>

export module wavex:protos_http;

export import :protos_http_methods;
export import :protos_http_codec;
export import :protos_http_request;
export import :protos_http_response;

export namespace wavex::protos::http {
    using wavex::protos::http::method;
    using wavex::protos::http::header;
    using wavex::protos::http::message_base;
    using wavex::protos::http::request;
    using wavex::protos::http::response;
    using wavex::protos::http::to_string;
    using wavex::protos::http::from_string;
    using wavex::protos::http::status_text_for;
    using wavex::protos::http::parser;
    using wavex::protos::http::encoder;
    using wavex::protos::http::HttpRequest;
    using wavex::protos::http::HttpResponse;
}
