#include <sys/socket.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <print>
#include "server.h"

int Server::createSocket() const {
    const char *interface = "127.0.0.1";
    int s, o;
    struct sockaddr_in myAddr{};

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        return -1;
    }
    o = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) < 0) {
        fprintf(stderr, "Setsockopt failed: %s\n", strerror(errno));
        close(s);
        return -1;
    }
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, interface, &myAddr.sin_addr) != 1) {
        fprintf(stderr, "Bad address \"%s\"\n", interface);
        close(s);
        return -1;
    }
    myAddr.sin_port = htons(port);
    if (bind(s, (struct sockaddr *) &myAddr, sizeof(myAddr)) < 0) {
        fprintf(stderr, "Bind() failed: %s\n", strerror(errno));
        close(s);
        return -1;
    }
    if (listen(s, 1) < 0) {
        fprintf(stderr, "Listen() failed: %s\n", strerror(errno));
        close(s);
        return -1;
    }
    return s;
}

void Server::internal_loop(int socket) {
    char farName[INET_ADDRSTRLEN];

    for (;;) {
        struct sockaddr_in farAddr{};
        socklen_t addrlen = sizeof(farAddr);
        FILE *fp;

        int fd = accept(socket, (struct sockaddr *) &farAddr, &addrlen);
        if (fd < 0) {
            fprintf(stderr, "Can't accept connection: %s\n", strerror(errno));
            exit(1);
        }

        if (!vnc.connect()) {
            close(fd);
            exit(1);
        }

        if (!vnc.flags.quietFlag) {
            inet_ntop(farAddr.sin_family, &(farAddr.sin_addr), farName, sizeof(farName));
            printf("Connect %s\n", farName);
        }

        fp = fdopen(fd, "r");
        if (fp == nullptr) {
            fprintf(stderr, "fdopen failed: %s\n", strerror(errno));
            close(fd);
            exit(2);
        }

        vnc.processCommands(fp, fd);
        fclose(fp);

        if (!vnc.flags.quietFlag) {
            printf("Disconnect %s\n", farName);
        }

        vnc.close();
    }
}

void Server::start() {
    int socket;
    if ((socket = createSocket()) < 0) {
        exit(1);
    }

    internal_loop(socket);
}
