#ifndef JUMBOLANG_NETWORK_H
#define JUMBOLANG_NETWORK_H

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif

#include <string>
#include <functional>

class NetworkManager {
private:
    SOCKET server_fd;
    int port;

public:
    NetworkManager(int p);
    bool initializeSocket();
    bool bindToHardware();
    
    // NEW: The Event Loop that takes a Router function from the Interpreter
    void listenAndServe(std::function<std::string(std::string)> router);
    
    void shutdown();
};

#endif