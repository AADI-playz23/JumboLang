// include/features/HTTP.h
// Shared HTTP client powering both {fetch} and {llm} tags.
// Compile with -DHAVE_CURL and link -lcurl to enable real network calls.
#ifndef JUMBOLANG_HTTP_H
#define JUMBOLANG_HTTP_H

#include <string>
#include <map>

// Result returned by every HttpClient::request() call
struct HttpResponse {
    int         statusCode   = 0;
    std::string body;
    std::string contentType;
    bool        success      = false;
    std::string errorMessage;
};

class HttpClient {
public:
    // Generic HTTP request — method must be "GET","POST","PUT","DELETE","PATCH"
    static HttpResponse request(
        const std::string&                        method,
        const std::string&                        url,
        const std::string&                        requestBody = "",
        const std::map<std::string, std::string>& headers     = {}
    );

    // Convenience wrappers
    static HttpResponse get(const std::string& url,
                            const std::map<std::string, std::string>& headers = {});

    static HttpResponse post(const std::string& url,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers = {});
};

#endif // JUMBOLANG_HTTP_H
