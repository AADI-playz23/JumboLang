#include "../../include/features/Network.h"
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define closesocket close
#endif

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
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Allow immediate port reuse so you don't get "Address already in use" crashes
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    return bind(server_fd, (struct sockaddr *)&address, sizeof(address)) >= 0;
}

void NetworkManager::listenAndServe(std::function<std::string(std::string)> router) {
    if (listen(server_fd, 10) < 0) return;
    std::cout << "    📡 [NETWORK] Web Server Live on http://localhost:" << port << "\n";
    std::cout << "    ⏳ [NETWORK] Waiting for requests...\n";

    while (true) {
        SOCKET client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == INVALID_SOCKET) continue;

        char buffer[4096] = {0};
        recv(client_fd, buffer, 4096, 0);
        std::string request(buffer);

        // Parse the HTTP Method and Path (e.g., "GET /api HTTP/1.1")
        std::string path = "/";
        std::istringstream iss(request);
        std::string method;
        iss >> method >> path;

        if (method.empty()) {
            closesocket(client_fd);
            continue;
        }

        std::cout << "    🌐 [HTTP] " << method << " " << path << "\n";

        // Ask the JumboLang Interpreter what the response should be!
        std::string body = router(path);

        // Package it as a standard HTTP network response
        std::string httpResponse = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(body.length()) + "\r\n"
            "Connection: close\r\n\r\n" + body;

        send(client_fd, httpResponse.c_str(), httpResponse.length(), 0);
        closesocket(client_fd);
    }
}

void NetworkManager::shutdown() {
    closesocket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
}