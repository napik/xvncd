//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#include <getopt.h>
#include <cstdlib>
#include <cstdio>
#include "server.h"

static void usage(char *name) {
    fprintf(stderr, "Usage: %s [-a address] [-p port] "
                    "[-d vendor:product[:[serial]]] [-g direction_value[:direction_value...]] "
                    "[-c frequency] [-q] [-B] [-L] [-R] [-S] [-U] [-X]\n", name);
    exit(2);
}

static int convertInt(const char *str) {
    long v;
    char *endp;
    v = strtol(str, &endp, 0);
    if ((endp == str) || (*endp != '\0')) {
        fprintf(stderr, "Bad integer argument \"%s\"\n", str);
        exit(2);
    }
    return (int) v;
}

int main(int argc, char **argv) {
    int c;
    const char *bindAddress = "127.0.0.1";
    int port = 2542;
    Flags flags = Flags();

    while ((c = getopt(argc, argv, "a:b:c:d:g:hp:qBLRSUX")) >= 0) {
        switch (c) {
            case 'a':
                bindAddress = optarg;
                break;
//            case 'c':
//                usb->clockSpeed(optarg);
//                break;
//            case 'd':
//                usb->deviceConfig(optarg);
//                break;
            case 'g':
                flags.gpioArgument = optarg;
                break;
            case 'h':
                usage(argv[0]);
                break;
            case 'p':
                port = convertInt(optarg);
                break;
            case 'q':
                flags.quietFlag = 1;
                break;
            case 'u':
                flags.showUSB = 1;
                break;
            case 'x':
                flags.showXVC = 1;
                break;
            case 'B':
                flags.ftdiJTAGindex = 2;
                break;
            case 'L':
                flags.loopback = 1;
                break;
            case 'R':
                flags.runtFlag = 1;
                break;
            case 'S':
                flags.statisticsFlag = 1;
                break;
            case 'U':
                flags.showUSB = 1;
                break;
            case 'X':
                flags.showXVC = 1;
                break;
            default:
                usage(argv[0]);
        }
    }
    if (optind != argc) {
        fprintf(stderr, "Unexpected argument.\n");
        usage(argv[0]);
    }

    Server server(bindAddress, port, &flags);
    server.start();
}
