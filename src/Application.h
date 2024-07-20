#pragma once

#include <string>
#include "server.h"


class Application {
public:
    explicit Application(int argc, char **argv);

    [[noreturn]] void start() const;

private:
    const std::string ERROR_BAD_DEVICE_CONFIG = "Bad -d vendor:product[:[serial]]";
    const std::string ERROR_BAD_CLOCK_FREQUENCY = "Bad clock frequency argument.";

    void scanArguments(int argc, char **argv) const;

    [[nodiscard]] unsigned int parseFrequency(std::string_view str) const;

    [[nodiscard]] std::tuple<unsigned long, unsigned long, std::string> parseDeviceConfig(std::string_view str) const;

    [[noreturn]] static void usage(const std::string &name);

    static int convertInt(const std::string &str);

    std::unique_ptr<Server> server;
};
