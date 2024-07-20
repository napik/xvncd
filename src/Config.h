#ifndef XVNCD_CPP_CONFIG_H
#define XVNCD_CPP_CONFIG_H

#include <cstdint>
#include <string>
#include "DiagnosticFlags.h"

struct Config {
public:
    // Vendor and Product IDs
    uint32_t vendorId = 0;
    uint32_t productId = 0;

    // FTDI info
    unsigned int lockedSpeed = 0;
    unsigned int jtagIndex = 1;

    // Diagnostics Config
    std::unique_ptr<DiagnosticFlags> flags = std::make_unique<DiagnosticFlags>();

    // Serial Number
    std::string serialNumber;
    std::string gpioArgument;
};


#endif // XVNCD_CPP_CONFIG_H
