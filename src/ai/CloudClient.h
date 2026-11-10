#pragma once

#include <cstddef>
#include <string>

struct HttpResponse
{
    long statusCode = 0;
    std::string body;
};

class CloudClient
{
public:
    CloudClient();

    HttpResponse postJson(const std::string &url, const std::string &apiKey,
                          const std::string &jsonBody);

private:
    static size_t writeCallback(void *contents, size_t size, size_t nmemb,
                                void *userData);
};
