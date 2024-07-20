#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include "server.h"

int Server::createSocket() const {
    int s, o;
    struct sockaddr_in myAddr{};

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        spdlog::error("Socket creation failed: {}", strerror(errno));
        return -1;
    }

    o = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) < 0) {
        spdlog::error("Setsockopt failed: {}", strerror(errno));
        close(s);
        return -1;
    }

    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, bindAddress.c_str(), &myAddr.sin_addr) != 1) {
        spdlog::error("Bad address \"{}\"", bindAddress);
        close(s);
        return -1;
    }
    myAddr.sin_port = htons(port);

    if (bind(s, (struct sockaddr *) &myAddr, sizeof(myAddr)) < 0) {
        spdlog::error("Bind() failed: {}", strerror(errno));
        close(s);
        return -1;
    }

    if (listen(s, 1) < 0) {
        spdlog::error("Listen() failed: {}", strerror(errno));
        close(s);
        return -1;
    }

    return s;
}

void Server::internal_loop(int socket) {
    char farName[INET_ADDRSTRLEN];

    while (true) {
        struct sockaddr_in farAddr{};
        socklen_t addrlen = sizeof(farAddr);

        int fd = accept(socket, (struct sockaddr *) &farAddr, &addrlen);
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

        if (!vnc.connect()) {
            vnc.close();
            fclose(fp);
            close(fd);
            std::exit(2);
        }

        if (!vnc.flags->quietFlag) {
            inet_ntop(farAddr.sin_family, &(farAddr.sin_addr), farName, sizeof(farName));
            spdlog::info("Connect {}", farName);
        }

        vnc.processCommands(fp, fd);

        if (!vnc.flags->quietFlag) {
            spdlog::info("Disconnect {}", farName);
        }

        fclose(fp);
        close(fd);
        vnc.close();
    }
}

void Server::start() {
    int socket = createSocket();
    if (socket < 0) {
        spdlog::error("Failed to create socket, exiting.");
        std::exit(EXIT_FAILURE);
    }

    internal_loop(socket);

    close(socket);
}
