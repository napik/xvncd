#ifndef XVNCD_CPP_APPLICATION_H
#define XVNCD_CPP_APPLICATION_H

#include <string>
#include <memory>
#include "Config.h"

class Application {
public:
    explicit Application(int argc, char **argv);

    void start();

private:
    static constexpr const char *ERROR_BAD_DEVICE_CONFIG = "Bad -d vendor:product[:[serial]]";
    static constexpr const char *ERROR_BAD_CLOCK_FREQUENCY = "Bad clock frequency argument.";

    void scanArguments(int argc, char **argv);

    void parseFrequency(const std::string &str);

    void parseDeviceConfig(const std::string &str);

    static void usage(const std::string &name);

    static int convertInt(const std::string &str);

    std::unique_ptr<Config> config;
    std::string bindAddress;
    int port;
};

#endif // XVNCD_CPP_APPLICATION_H
