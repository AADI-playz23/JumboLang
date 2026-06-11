// src/features/HTTP.cpp
// Shared HTTP client using libcurl.
// If compiled WITHOUT -DHAVE_CURL every call returns a clear error message
// so the rest of the interpreter still runs and the user gets a helpful hint.
#include "../../include/features/HTTP.h"
#include <iostream>
#include <algorithm>

#ifdef HAVE_CURL
#include <curl/curl.h>

// ── libcurl write-callback (appends received data to a std::string) ──────────
static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb,
                                std::string* out) {
    size_t total = size * nmemb;
    out->append(static_cast<char*>(contents), total);
    return total;
}
#endif // HAVE_CURL

// ── Core request implementation ───────────────────────────────────────────────
HttpResponse HttpClient::request(
    const std::string&                        method,
    const std::string&                        url,
    const std::string&                        requestBody,
    const std::map<std::string, std::string>& headers)
{
    HttpResponse result;

#ifdef HAVE_CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.errorMessage = "Failed to initialize libcurl handle";
        return result;
    }

    std::string responseBody;

    // ── Build curl header list ────────────────────────────────────────────────
    struct curl_slist* headerList = nullptr;
    for (const auto& [key, val] : headers) {
        std::string h = key + ": " + val;
        headerList = curl_slist_append(headerList, h.c_str());
    }

    // ── Basic options ─────────────────────────────────────────────────────────
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,1L);
    // Identify ourselves
    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "JumboLang/2.0");

    if (headerList) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    // ── Method-specific options ───────────────────────────────────────────────
    std::string upperMethod = method;
    std::transform(upperMethod.begin(), upperMethod.end(),
                   upperMethod.begin(), ::toupper);

    if (upperMethod == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)requestBody.size());
    } else if (upperMethod == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)requestBody.size());
    } else if (upperMethod == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)requestBody.size());
    } else if (upperMethod == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET is the default — no extra options needed

    // ── Perform ───────────────────────────────────────────────────────────────
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        result.statusCode = static_cast<int>(httpCode);
        result.body       = responseBody;
        result.success    = (httpCode >= 200 && httpCode < 300);

        char* ct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
        if (ct) result.contentType = ct;

        if (!result.success) {
            result.errorMessage = "HTTP " + std::to_string(httpCode);
        }
    } else {
        result.errorMessage = curl_easy_strerror(res);
        result.success      = false;
    }

    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

#else
    // ── Stub when libcurl is not compiled in ─────────────────────────────────
    (void)method; (void)url; (void)requestBody; (void)headers;
    result.success      = false;
    result.statusCode   = 0;
    result.errorMessage = "libcurl not compiled in";
    result.body =
        "[FETCH ERROR] Network calls require libcurl.\n"
        "Recompile with:  make USE_CURL=1\n"
        "Linux:           sudo apt-get install libcurl4-openssl-dev\n"
        "Windows (vcpkg): vcpkg install curl";
    std::cerr << "    ⚠️  [HTTP] " << result.body << "\n";
#endif

    return result;
}

// ── Convenience wrappers ──────────────────────────────────────────────────────
HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& headers) {
    return request("GET", url, "", headers);
}

HttpResponse HttpClient::post(const std::string& url,
                              const std::string& body,
                              const std::map<std::string, std::string>& headers) {
    return request("POST", url, body, headers);
}
