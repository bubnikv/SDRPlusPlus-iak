#include "curl_http.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include <utils/curl_init.h>

namespace net::http {

    namespace {
        struct ResponseBuffer {
            std::string body;
            size_t maxBody = 0;
            bool overflow = false;
        };

        struct SListGuard {
            curl_slist* list = nullptr;

            SListGuard() = default;

            ~SListGuard() {
                if (list) {
                    curl_slist_free_all(list);
                }
            }

            SListGuard(const SListGuard&) = delete;
            SListGuard& operator=(const SListGuard&) = delete;
        };

        struct EasyGuard {
            CURL* handle = nullptr;

            explicit EasyGuard(CURL* handle) : handle(handle) {}

            ~EasyGuard() {
                if (handle) {
                    curl_easy_cleanup(handle);
                }
            }

            EasyGuard(const EasyGuard&) = delete;
            EasyGuard& operator=(const EasyGuard&) = delete;
        };

        size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
            size_t total = size * nmemb;
            auto* buffer = static_cast<ResponseBuffer*>(userdata);

            if (buffer->maxBody > 0 && buffer->body.size() + total > buffer->maxBody) {
                buffer->overflow = true;
                return 0;
            }

            buffer->body.append(ptr, total);
            return total;
        }

        size_t writeHeader(char* ptr, size_t size, size_t nmemb, void* userdata) {
            size_t total = size * nmemb;
            auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
            std::string line(ptr, total);
            // libcurl calls this for every response in a redirect/auth chain.
            // Retain validators from the final response only.
            if (line.rfind("HTTP/", 0) == 0) {
                headers->clear();
                return total;
            }
            size_t colon = line.find(':');
            if (colon == std::string::npos) {
                return total;
            }
            std::string name = line.substr(0, colon);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            size_t first = line.find_first_not_of(" \t", colon + 1);
            size_t last = line.find_last_not_of(" \t\r\n");
            std::string value;
            if (first != std::string::npos && last != std::string::npos && last >= first) {
                value = line.substr(first, last - first + 1);
            }
            (*headers)[std::move(name)] = std::move(value);
            return total;
        }

        int xferInfo(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
            const auto* cb = static_cast<const std::function<bool()>*>(clientp);
            return (*cb && (*cb)()) ? 1 : 0;
        }

        void setOption(CURL* handle, CURLoption option, long value, const char* name) {
            CURLcode rc = curl_easy_setopt(handle, option, value);
            if (rc != CURLE_OK) {
                throw std::runtime_error(std::string("curl_easy_setopt(") + name + ") failed: " + curl_easy_strerror(rc));
            }
        }
    }

    CurlResponse curlGet(const std::string& url, const CurlRequestOptions& options) {
        EasyGuard easy(curl::make_easy());
        if (!easy.handle) {
            throw std::runtime_error("curl_easy_init failed");
        }

        char error[CURL_ERROR_SIZE] = {};
        ResponseBuffer buffer;
        buffer.maxBody = options.maxBody;
        CurlResponse response;

        CURLcode rc = curl_easy_setopt(easy.handle, CURLOPT_ERRORBUFFER, error);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_ERRORBUFFER) failed: ") + curl_easy_strerror(rc));
        }

        rc = curl_easy_setopt(easy.handle, CURLOPT_URL, url.c_str());
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_URL) failed: ") + curl_easy_strerror(rc));
        }

        rc = curl_easy_setopt(easy.handle, CURLOPT_WRITEFUNCTION, writeBody);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed: ") + curl_easy_strerror(rc));
        }

        rc = curl_easy_setopt(easy.handle, CURLOPT_WRITEDATA, &buffer);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_WRITEDATA) failed: ") + curl_easy_strerror(rc));
        }

        rc = curl_easy_setopt(easy.handle, CURLOPT_HEADERFUNCTION, writeHeader);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_HEADERFUNCTION) failed: ") + curl_easy_strerror(rc));
        }
        rc = curl_easy_setopt(easy.handle, CURLOPT_HEADERDATA, &response.headers);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_HEADERDATA) failed: ") + curl_easy_strerror(rc));
        }

        setOption(easy.handle, CURLOPT_CONNECTTIMEOUT_MS, options.connectTimeoutMs, "CURLOPT_CONNECTTIMEOUT_MS");
        if (options.timeoutMs > 0) {
            setOption(easy.handle, CURLOPT_TIMEOUT_MS, options.timeoutMs, "CURLOPT_TIMEOUT_MS");
        }
        setOption(easy.handle, CURLOPT_MAXREDIRS, options.maxRedirects, "CURLOPT_MAXREDIRS");

        if (options.shouldCancel) {
            rc = curl_easy_setopt(easy.handle, CURLOPT_XFERINFOFUNCTION, xferInfo);
            if (rc != CURLE_OK) {
                throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_XFERINFOFUNCTION) failed: ") + curl_easy_strerror(rc));
            }
            rc = curl_easy_setopt(easy.handle, CURLOPT_XFERINFODATA, &options.shouldCancel);
            if (rc != CURLE_OK) {
                throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_XFERINFODATA) failed: ") + curl_easy_strerror(rc));
            }
            setOption(easy.handle, CURLOPT_NOPROGRESS, 0, "CURLOPT_NOPROGRESS");
        }

        SListGuard headers;
        for (const auto& [name, value] : options.headers) {
            std::string header = name + ": " + value;
            curl_slist* next = curl_slist_append(headers.list, header.c_str());
            if (!next) {
                throw std::runtime_error("curl_slist_append failed");
            }
            headers.list = next;
        }
        if (headers.list) {
            rc = curl_easy_setopt(easy.handle, CURLOPT_HTTPHEADER, headers.list);
            if (rc != CURLE_OK) {
                throw std::runtime_error(std::string("curl_easy_setopt(CURLOPT_HTTPHEADER) failed: ") + curl_easy_strerror(rc));
            }
        }

        rc = curl_easy_perform(easy.handle);
        if (rc != CURLE_OK) {
            if (buffer.overflow) {
                throw std::runtime_error("HTTP response exceeded maximum body size");
            }
            const char* detail = error[0] ? error : curl_easy_strerror(rc);
            throw std::runtime_error(std::string("curl_easy_perform failed: ") + detail);
        }

        response.body = std::move(buffer.body);

        rc = curl_easy_getinfo(easy.handle, CURLINFO_RESPONSE_CODE, &response.status);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_easy_getinfo(CURLINFO_RESPONSE_CODE) failed: ") + curl_easy_strerror(rc));
        }

        char* effectiveUrl = nullptr;
        rc = curl_easy_getinfo(easy.handle, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
        if (rc == CURLE_OK && effectiveUrl) {
            response.effectiveUrl = effectiveUrl;
        }

        return response;
    }

}
