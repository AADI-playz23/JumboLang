// src/features/Network.cpp
// Multi-threaded HTTP/1.1 server for JumboLang.
// One std::thread per accepted connection — no blocking on slow responses.
// Content-Type is auto-detected from the response body instead of hardcoding JSON.
#include "../../include/features/Network.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <mutex>
#include <functional>
#include <string>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define closesocket close
#endif

// The router mutex protects shared interpreter state (variables, route state)
// while allowing multiple threads to queue up and call it safely.
static std::mutex routerMutex;

// ─────────────────────────────────────────────────────────────────────────────
// CONTENT-TYPE AUTO-DETECTION
// Avoids the hardcoded "application/json" bug — responses get the right type.
// ─────────────────────────────────────────────────────────────────────────────
static std::string detectContentType(const std::string& body) {
    if (body.empty()) return "text/plain; charset=utf-8";
    const char first = body.front();
    if (first == '{' || first == '[') return "application/json";
    // Peek for HTML doctype / opening tag
    if (body.size() > 5) {
        if (body.rfind("<!D", 0) == 0 || body.rfind("<!d", 0) == 0 ||
            body.rfind("<ht", 0) == 0 || body.rfind("<HT", 0) == 0)
            return "text/html; charset=utf-8";
    }
    return "text/plain; charset=utf-8";
}

// ─────────────────────────────────────────────────────────────────────────────
// PER-CONNECTION HANDLER (runs in a worker thread)
// ─────────────────────────────────────────────────────────────────────────────
static void handleClient(
    SOCKET client_fd,
    std::function<std::string(std::string, std::string)> router)
{
    // Receive until we have the complete HTTP request headers
    std::string rawRequest;
    rawRequest.reserve(8192);
    char buf[4096];
    while (true) {
        int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        rawRequest += buf;
        if (rawRequest.find("\r\n\r\n") != std::string::npos) break;
    }

    if (rawRequest.empty()) {
        closesocket(client_fd);
        return;
    }

    // Parse the first line: "METHOD /path HTTP/1.1"
    std::string method = "GET";
    std::string path   = "/";
    {
        std::istringstream iss(rawRequest);
        iss >> method >> path;
    }
    if (method.empty()) { closesocket(client_fd); return; }

    std::cout << "    🌐 [HTTP] " << method << " " << path << "\n";

    // ── Call the JumboLang router (serialised behind the mutex) ──────────────
    std::string body;
    {
        std::lock_guard<std::mutex> lock(routerMutex);
        body = router(path, method);
    }

    // ── Build and send the HTTP response ─────────────────────────────────────
    std::string contentType = detectContentType(body);
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: "   + contentType + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + body;

    send(client_fd, response.c_str(), static_cast<int>(response.size()), 0);
    closesocket(client_fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC API
// ─────────────────────────────────────────────────────────────────────────────
NetworkManager::NetworkManager(int p) : server_fd(INVALID_SOCKET), port(p) {}

bool NetworkManager::initializeSocket() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    return server_fd != INVALID_SOCKET;
}

bool NetworkManager::bindToHardware() {
    struct sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(port);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    return bind(server_fd,
                reinterpret_cast<struct sockaddr*>(&address),
                sizeof(address)) >= 0;
}

void NetworkManager::listenAndServe(
    std::function<std::string(std::string, std::string)> router)
{
    if (listen(server_fd, 32) < 0) return;

    std::cout << "    📡 [NETWORK] Web Server live on http://localhost:" << port << "\n";
    std::cout << "    ⚙️  [NETWORK] Mode: multi-threaded (one thread per connection)\n";
    std::cout << "    ⏳ [NETWORK] Waiting for requests...\n";

    while (true) {
        SOCKET client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == INVALID_SOCKET) continue;

        // Spawn a detached thread for each connection — fire and forget
        std::thread(handleClient, client_fd, router).detach();
    }
}

void NetworkManager::shutdown() {
    closesocket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
}