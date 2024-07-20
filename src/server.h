#ifndef XVNCD_CPP_SERVER_H
#define XVNCD_CPP_SERVER_H

#include "xvncd.h"
#include <spdlog/spdlog.h>

class Server {
private:
    [[nodiscard]] int createSocket() const;

    const std::string bindAddress;
    const int port;
    VncProtocol vnc;

    inline void internal_loop(int i);

public:
    Server(const std::string &bindAddress, int port, Config *flags) : bindAddress(bindAddress), port(port), vnc(flags) {
        spdlog::info("Server initialized with address {} and port {}", bindAddress, port);
    }

    void start();
};

#endif //XVNCD_CPP_SERVER_H
