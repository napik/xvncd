//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#ifndef XVNCD_CPP_SERVER_H
#define XVNCD_CPP_SERVER_H

#include "xvncd.h"

class Server {
private:
    [[nodiscard]] int createSocket() const;

    const char *bindAddress;
    int port;
    VncProtocol vnc;

    [[noreturn]] void internal_loop(int i);

public:
    Server(const char *bindAddress, int port, Flags *flags) : vnc(flags) {
        this->bindAddress = bindAddress;
        this->port = port;
    }

    void start();
};


#endif //XVNCD_CPP_SERVER_H
