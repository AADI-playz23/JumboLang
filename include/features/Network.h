// include/features/Network.h
#ifndef JUMBOLANG_NETWORK_H
#define JUMBOLANG_NETWORK_H

#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #define CLOSE_SOCKET close
    typedef int SOCKET;
    #define INVALID_SOCKET -1
#endif

class NetworkManager {
private:
    SOCKET server_fd;
    int port;

public:
    NetworkManager(int p);
    bool initializeSocket();
    bool bindToHardware();
    void startListening();
    void shutdown();
};

#endif