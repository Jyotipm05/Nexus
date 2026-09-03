// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file http1codec.ixx
 * @brief C++ module interface for the HTTP/1.x codec in WaveX.
 */

module;

#include <wavex/protos/http/http1codec.hpp>

export module wavex:protos_http_codec;

export namespace wavex::protos::http {
    using wavex::protos::http::header;
    using wavex::protos::http::message_base;
    using wavex::protos::http::request;
    using wavex::protos::http::response;
    using wavex::protos::http::to_string;
    using wavex::protos::http::from_string;
    using wavex::protos::http::status_text_for;
    using wavex::protos::http::parser;
    using wavex::protos::http::encoder;
    using wavex::protos::http::http1codec;
}

