// Copyright (c) 2026 Jyotipriya Mondal
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
/**
 * @file Methods.hpp
 * @brief Strong-typed HTTP method enum for WaveX.
 */

#pragma once
#undef DELETE


namespace wavex::protos::http {
    /**
     * @enum method
     * @brief HTTP request methods per RFC 9110 + custom extensions.
     */
    enum class method {
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS,
        PATCH,
        TRACE,
        CONNECT,
        QUERY, ///< RFC 9110 draft: structured query with a body (like GET + POST)
        UNKNOWN
    };
}
