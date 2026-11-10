
#include "CloudClient.h"

#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace
{
    void initializeCurl()
    {
        static std::once_flag flag;

        std::call_once(flag, []() {
            const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);

            if (result != CURLE_OK)
            {
                throw std::runtime_error(
                    "Failed to initialize libcurl: "
                    + std::string(curl_easy_strerror(result)));
            }
        });
    }

    struct CurlDeleter
    {
        void operator()(CURL *curl) const
        {
            if (curl)
                curl_easy_cleanup(curl);
        }
    };

    struct CurlHeadersDeleter
    {
        void operator()(curl_slist *headers) const
        {
            if (headers)
                curl_slist_free_all(headers);
        }
    };

    using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
    using CurlHeaders = std::unique_ptr<curl_slist, CurlHeadersDeleter>;

    void appendHeader(CurlHeaders &headers, const std::string &header)
    {
        curl_slist *updated = curl_slist_append(headers.get(), header.c_str());

        if (!updated)
        {
            throw std::runtime_error("Failed to allocate HTTP headers");
        }

        headers.release();
        headers.reset(updated);
    }

    void checkCurlOption(CURLcode result)
    {
        if (result != CURLE_OK)
        {
            throw std::runtime_error("Failed to configure CURL: "
                                     + std::string(curl_easy_strerror(result)));
        }
    }
} // namespace

// Constructor

CloudClient::CloudClient()
{
    initializeCurl();
}

// HTTP response callback

size_t CloudClient::writeCallback(void *contents, size_t size, size_t nmemb,
                                  void *userData)
{
    if (size != 0 && nmemb > static_cast<size_t>(-1) / size)
        return 0;

    const size_t totalSize = size * nmemb;

    auto *response = static_cast<std::string *>(userData);

    try
    {
        response->append(static_cast<const char *>(contents), totalSize);
    }
    catch (...)
    {
        // Never allow a C++ exception to cross libcurl's C callback.
        return 0;
    }

    return totalSize;
}

// HTTP POST request

HttpResponse CloudClient::postJson(const std::string &url,
                                   const std::string &apiKey,
                                   const std::string &jsonBody)
{
    CurlHandle curl(curl_easy_init());

    if (!curl)
    {
        throw std::runtime_error("Failed to create CURL session");
    }

    HttpResponse response;

    CurlHeaders headers;

    appendHeader(headers, "Content-Type: application/json");
    appendHeader(headers, "Accept: application/json");

    if (!apiKey.empty())
    {
        appendHeader(headers, "x-goog-api-key: " + apiKey);
    }

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str()));

    checkCurlOption(
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get()));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_POST, 1L));

    checkCurlOption(
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, jsonBody.c_str()));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                                     static_cast<curl_off_t>(jsonBody.size())));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION,
                                     &CloudClient::writeCallback));

    checkCurlOption(
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.body));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 60L));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L));

    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L));

    // Automatically handle compressed responses.
    checkCurlOption(curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, ""));

    // Execute request.

    const CURLcode result = curl_easy_perform(curl.get());

    if (result != CURLE_OK)
    {
        throw std::runtime_error("HTTP request failed: "
                                 + std::string(curl_easy_strerror(result)));
    }

    checkCurlOption(curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE,
                                      &response.statusCode));

    return response;
}
