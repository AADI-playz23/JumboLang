// include/features/Network.h
// Multi-threaded HTTP server for JumboLang's {https} tag.
// Each incoming connection is dispatched to its own thread so multiple clients
// can be served concurrently without blocking.
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
    int    port;

public:
    NetworkManager(int p);
    bool initializeSocket();
    bool bindToHardware();

    // Multi-threaded event loop.
    // router(path, method) is called per-request from a worker thread.
    // Returns the raw response body; Content-Type is auto-detected.
    void listenAndServe(
        std::function<std::string(std::string /*path*/, std::string /*method*/)> router
    );

    void shutdown();
};

#endif // JUMBOLANG_NETWORK_H