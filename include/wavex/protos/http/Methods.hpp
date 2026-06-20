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
