// src/features/Network.cpp
#include "../../include/features/Network.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

NetworkManager::NetworkManager(int p) : port(p), server_fd(-1) {}

bool NetworkManager::initializeSocket() {
    // 1. Create the socket file descriptor
    // AF_INET = IPv4, SOCK_STREAM = TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_fd == 0) {
        std::cerr << "❌ [NETWORK ERROR] Failed to create socket.\n";
        return false;
    }

    // 2. Set socket options (Allow us to reuse the port immediately after a restart)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "❌ [NETWORK ERROR] Could not set socket options.\n";
        return false;
    }

    return true;
}

bool NetworkManager::bindToHardware() {
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces
    address.sin_port = htons(port);      // Convert port number to network byte order

    // 3. Bind the socket to the port (e.g., 443)
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "❌ [NETWORK ERROR] Bind failed on port " << port << ".\n";
        return false;
    }

    return true;
}

void NetworkManager::startListening() {
    // 4. Set the socket to "Passive" mode (Listen for incoming connections)
    if (listen(server_fd, 3) < 0) {
        std::cerr << "❌ [NETWORK ERROR] Listen failed.\n";
        return;
    }
    
    std::cout << "✅ [SUCCESS] JumboLang is now listening for traffic on port " << port << ".\n";
}

void NetworkManager::shutdown() {
    if (server_fd != -1) {
        close(server_fd);
        std::cout << "🏁 [NETWORK] Socket closed cleanly.\n";
    }
}