#include <getopt.h>
#include <cstdlib>
#include <climits>
#include <spdlog/spdlog.h>
#include "server.h"
#include "Application.h"

Application::Application(int argc, char **argv) : config(std::make_unique<Config>()), bindAddress("127.0.0.1"),
                                                  port(2542) {
    scanArguments(argc, argv);
}

void Application::usage(const std::string &name) {
    spdlog::error("Usage: {} [-a address] [-p port] "
                  "[-d vendor:product[:[serial]]] [-g direction_value[:direction_value...]] "
                  "[-c frequency] [-q] [-B] [-L] [-R] [-S] [-U] [-X]", name);
    std::exit(EXIT_FAILURE);
}

int Application::convertInt(const std::string &str) {
    char *endp;
    long v = std::strtol(str.c_str(), &endp, 0);
    if (*endp != '\0' || endp == str.c_str()) {
        spdlog::error("Bad integer argument \"{}\"", str);
        std::exit(EXIT_FAILURE);
    }
    return static_cast<int>(v);
}

void Application::parseFrequency(const std::string &str) {
    char *endp;
    double frequency = std::strtod(str.c_str(), &endp);

    if ((endp == str.c_str()) ||
        ((*endp != '\0') && (*endp != 'M') && (*endp != 'k')) ||
        (*(endp + 1) != '\0')) {
        spdlog::error("{}", ERROR_BAD_CLOCK_FREQUENCY);
        std::exit(EXIT_FAILURE);
    }

    if (*endp == 'M') frequency *= 1e6;
    if (*endp == 'k') frequency *= 1e3;

    config->lockedSpeed = static_cast<unsigned int>(std::clamp(frequency, 1.0, static_cast<double>(INT_MAX)));
}

void Application::parseDeviceConfig(const std::string &str) {
    size_t pos1 = str.find(':');
    if (pos1 == std::string::npos) {
        spdlog::error("{}", ERROR_BAD_DEVICE_CONFIG);
        std::exit(EXIT_FAILURE);
    }

    unsigned long vendor = std::stoul(str.substr(0, pos1), nullptr, 16);

    size_t pos2 = str.find(':', pos1 + 1);
    unsigned long product;
    if (pos2 == std::string::npos) {
        product = std::stoul(str.substr(pos1 + 1), nullptr, 16);
    } else {
        product = std::stoul(str.substr(pos1 + 1, pos2 - pos1 - 1), nullptr, 16);
        config->serialNumber = str.substr(pos2 + 1);
    }

    if (vendor > 0xFFFF || product > 0xFFFF) {
        spdlog::error("{}", ERROR_BAD_DEVICE_CONFIG);
        std::exit(EXIT_FAILURE);
    }

    config->vendorId = vendor;
    config->productId = product;
}

void Application::scanArguments(int argc, char **argv) {
    int option;
    while ((option = getopt(argc, argv, "a:b:c:d:u:x:g:hp:qBLRSUX")) != -1) {
        switch (option) {
            case 'a':
                bindAddress = optarg;
                break;
            case 'c':
                parseFrequency(optarg);
                break;
            case 'd':
                parseDeviceConfig(optarg);
                break;
            case 'g':
                config->gpioArgument = optarg;
                break;
            case 'h':
                usage(argv[0]);
                break;
            case 'p':
                port = convertInt(optarg);
                break;
            case 'q':
                config->flags->quietFlag = true;
                break;
            case 'u':
            case 'U':
                config->flags->showUSB = true;
                break;
            case 'x':
            case 'X':
                config->flags->showXVC = true;
                break;
            case 'B':
                config->jtagIndex = 2;
                break;
            case 'L':
                config->flags->loopback = true;
                break;
            case 'R':
                config->flags->runtFlag = true;
                break;
            case 'S':
                config->flags->statisticsFlag = true;
                break;
            default:
                usage(argv[0]);
        }
    }

    if (optind < argc) {
        spdlog::error("Unexpected argument: {}", argv[optind]);
        usage(argv[0]);
    }
}

void Application::start() {
    Server server(bindAddress, port, config.get());
    server.start();
}
