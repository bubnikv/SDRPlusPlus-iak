#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>

#include <module.h>

namespace net::http {

    struct CurlRequestOptions {
        int connectTimeoutMs = 15000;
        int timeoutMs = 30000;
        int maxRedirects = 5;
        size_t maxBody = 2 * 1024 * 1024;
        std::map<std::string, std::string> headers;
        // Optional cancellation callback. Polled periodically by libcurl while
        // the transfer is in progress; returning true aborts curlGet promptly
        // with std::runtime_error. Lets callers shut down without waiting for
        // timeoutMs.
        std::function<bool()> shouldCancel;
    };

    struct CurlResponse {
        long status = 0;
        std::string body;
        std::string effectiveUrl;
        // Lower-case response header names. Providers use etag and
        // last-modified to avoid downloading an unchanged database.
        std::map<std::string, std::string> headers;
    };

    // HTTPS-capable GET implemented by core through libcurl. Modules should
    // use this instead of including <curl/curl.h> or linking libcurl directly.
    SDRPP_EXPORT CurlResponse curlGet(const std::string& url, const CurlRequestOptions& options = CurlRequestOptions());

}
