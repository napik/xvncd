#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include "DiagnosticFlags.h"


class Config {
public:
    static std::shared_ptr<Config> get() {
        if (!mInstance) {
            mInstance = std::make_shared<Config>();
        }
        return mInstance;
    }

    std::string bindAddress = "127.0.0.1";
    int port = 2542;

    // Vendor and Product IDs
    uint32_t vendorId = 0x0403;
    uint32_t productId = 0x6014;

    // FTDI info
    unsigned int lockedSpeed = 0;
    unsigned int jtagIndex = 1;

    // Diagnostics Config
    std::unique_ptr<DiagnosticFlags> flags = std::make_unique<DiagnosticFlags>();

    // Serial Number
    std::string serialNumber;
    std::string gpioArgument;

private:
    static inline std::shared_ptr<Config> mInstance;
};
