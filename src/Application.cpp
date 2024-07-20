#include <getopt.h>
#include <cstdlib>
#include <climits>
#include <spdlog/spdlog.h>
#include "server.h"
#include "Application.h"


Application::Application(const int argc, char **argv) {
    scanArguments(argc, argv);
    server = std::make_unique<Server>();

    spdlog::set_level(spdlog::level::debug);
}

[[noreturn]] void Application::usage(const std::string &name) {
    spdlog::error("Usage: {} [-a address] [-p port] "
                  "[-d vendor:product[:[serial]]] [-g direction_value[:direction_value...]] "
                  "[-c frequency] [-q] [-B] [-L] [-R] [-S] [-U] [-X]", name);
    std::exit(EXIT_FAILURE);
}

int Application::convertInt(const std::string &str) {
    char *endp;
    const long v = std::strtol(str.c_str(), &endp, 0);
    if (*endp != '\0' || endp == str.c_str()) {
        spdlog::error("Bad integer argument \"{}\"", str);
        std::exit(EXIT_FAILURE);
    }
    return static_cast<int>(v);
}

unsigned int Application::parseFrequency(const std::string_view str) const {
    char *endp;
    double frequency = std::strtod(str.data(), &endp);

    if ((endp == str.data()) ||
        ((*endp != '\0') && (*endp != 'M') && (*endp != 'k')) ||
        (*(endp + 1) != '\0')) {
        spdlog::error("{}", ERROR_BAD_CLOCK_FREQUENCY);
        std::exit(EXIT_FAILURE);
    }

    if (*endp == 'M') frequency *= 1e6;
    if (*endp == 'k') frequency *= 1e3;

    return static_cast<unsigned int>(std::clamp(frequency, 1.0, static_cast<double>(INT_MAX)));
}

std::tuple<unsigned long, unsigned long, std::string> Application::parseDeviceConfig(std::string_view str) const {
    const size_t pos1 = str.find(':');
    if (pos1 == std::string::npos || pos1 == 0) {
        spdlog::error("{}", ERROR_BAD_DEVICE_CONFIG);
        std::exit(EXIT_FAILURE);
    }

    const auto vendor = std::stoul(str.substr(0, pos1).data(), nullptr, 16);
    if (vendor > 0xFFFF) {
        spdlog::error("{}", ERROR_BAD_DEVICE_CONFIG);
        std::exit(EXIT_FAILURE);
    }

    const size_t pos2 = str.find(':', pos1 + 1);
    std::string serial;
    const auto product = std::stoul(
        str.substr(pos1 + 1, pos2 == std::string::npos ? std::string::npos : pos2 - pos1 - 1).data(), nullptr, 16);
    if (product > 0xFFFF) {
        spdlog::error("{}", ERROR_BAD_DEVICE_CONFIG);
        std::exit(EXIT_FAILURE);
    }

    if (pos2 != std::string::npos) {
        serial = str.substr(pos2 + 1);
    }

    return std::make_tuple(vendor, product, serial);
}

void Application::scanArguments(const int argc, char **argv) const {
    auto config = Config::get();
    int option;
    while ((option = getopt(argc, argv, "a:b:c:d:x:u:g:hp:qBLRSUX")) != -1) {
        switch (option) {
            case 'a': {
                config->bindAddress = optarg;
            }
            break;
            case 'c': {
                config->lockedSpeed = parseFrequency(optarg);
            }
            break;
            case 'd': {
                auto [vendor, product,serial] = parseDeviceConfig(optarg);
                config->vendorId = static_cast<uint32_t>(vendor);
                config->productId = static_cast<uint32_t>(product);
                config->serialNumber = serial;
            }
            break;
            case 'g': {
                config->gpioArgument = optarg;
            }
            break;
            case 'h': {
                usage(argv[0]);
            }
            case 'p': {
                config->port = convertInt(optarg);
            }
            break;
            case 'q': {
                config->flags->quietFlag = true;
            }
            break;
            case 'U':
            case 'u': {
                config->flags->showUSB = true;
            }
            break;
            case 'x':
            case 'X': {
                config->flags->showUSB = true;
                config->flags->showXVC = true;
            }
            break;
            case 'B': {
                config->jtagIndex = 2;
            }
            break;
            case 'L': {
                config->flags->loopback = true;
            }
            break;
            case 'R': {
                config->flags->runtFlag = true;
            }
            break;
            case 'S': {
                config->flags->statisticsFlag = true;
            }
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

[[noreturn]] void Application::start() const {
    server->start();
}
