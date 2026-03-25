// include/features/Network.h
#ifndef JUMBOLANG_NETWORK_H
#define JUMBOLANG_NETWORK_H

#include <string>
#include <vector>
#include <netinet/in.h> // The Linux Header for internet protocols

class NetworkManager {
private:
    int server_fd;
    int port;
    struct sockaddr_in address;

public:
    NetworkManager(int p);
    
    // Feature: Standard HTTP/HTTPS Socket Initialization
    bool initializeSocket();
    
    // Feature: Bind the socket to the hardware
    bool bindToHardware();
    
    // Feature: Start listening for actual traffic
    void startListening();

    // Feature: Close and cleanup
    void shutdown();
};

#endif