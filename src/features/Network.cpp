// src/features/Network.cpp
#include "../../include/features/Network.h"
#include <iostream>

NetworkManager::NetworkManager(int p) : port(p), server_fd(INVALID_SOCKET) {}

bool NetworkManager::initializeSocket() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) return false;

    return true;
}

bool NetworkManager::bindToHardware() {
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) return false;
    return true;
}

void NetworkManager::startListening() {
    if (listen(server_fd, 3) < 0) return;
    std::cout << "✅ [SUCCESS] JumboLang listening on port " << port << ".\n";
}

void NetworkManager::shutdown() {
    if (server_fd != INVALID_SOCKET) {
        CLOSE_SOCKET(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
    }
}