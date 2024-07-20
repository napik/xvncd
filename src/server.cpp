#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include "server.h"

Server::Server() : vnc(std::make_unique<VncProtocol>()) {
    isContinue = true;
    if (const auto rc = createSocket(); rc < 0) {
        spdlog::error("Failed to create socket, exiting.");
        std::exit(EXIT_FAILURE);
    }
}

Server::~Server() {
    isContinue = false;
    close(_socket);
}

int Server::createSocket() {
    const auto config = Config::get();

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket < 0) {
        spdlog::error("Socket creation failed: {}", strerror(errno));
        return -1;
    }

    if (int o; setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) < 0) {
        spdlog::error("Setsockopt failed: {}", strerror(errno));
        close(_socket);
        return -1;
    }

    sockaddr_in myAddr{};
    std::memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(config->port);
    if (inet_pton(AF_INET, config->bindAddress.data(), &myAddr.sin_addr) != 1) {
        spdlog::error("Bad address \"{}\"", config->bindAddress);
        close(_socket);
        return -1;
    }

    if (bind(_socket, reinterpret_cast<sockaddr *>(&myAddr), sizeof(myAddr)) < 0) {
        spdlog::error("Bind() failed: {}", strerror(errno));
        close(_socket);
        return -1;
    }

    if (listen(_socket, 1) < 0) {
        spdlog::error("Listen() failed: {}", strerror(errno));
        close(_socket);
        return -1;
    }

    spdlog::info("Server initialized with address {} and port {}", config->bindAddress, config->port);
    return 0;
}

void Server::internal_loop() const {
    std::string farName(INET_ADDRSTRLEN, '\0');

    while (isContinue) {
        sockaddr_in farAddr{};
        socklen_t addrlen = sizeof(farAddr);

        const int fd = accept(_socket, reinterpret_cast<struct sockaddr *>(&farAddr), &addrlen);
        if (fd < 0) {
            spdlog::error("Can't accept connection: {}", strerror(errno));
            std::exit(2);
        }

        FILE *fp = fdopen(fd, "r");
        if (fp == nullptr) {
            spdlog::error("fdopen failed: {}", strerror(errno));
            close(fd);
            std::exit(2);
        }

        if (!vnc->connect(fp, fd)) {
            vnc->close();
            fclose(fp);
            close(fd);
            std::exit(2);
        }

        if (!vnc->isQuietMode()) {
            inet_ntop(farAddr.sin_family, &farAddr.sin_addr, farName.data(), sizeof(farAddr.sin_addr));
            spdlog::info("Connect {}", farName);
        }

        vnc->processCommands();

        if (!vnc->isQuietMode()) {
            spdlog::info("Disconnect {}", farName);
        }

        fclose(fp);
        close(fd);
        vnc->close();
    }
}

void Server::start() const {
    internal_loop();
}
