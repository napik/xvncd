#pragma once

#include "xvncd.h"

class Server {
public:
    explicit Server();

    ~Server();

    void start() const;

private:
    int createSocket();

    int _socket{};

    std::unique_ptr<VncProtocol> vnc;

    void internal_loop() const;

    volatile bool isContinue = true;
};
