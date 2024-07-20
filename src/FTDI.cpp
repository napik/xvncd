#include "FTDI.h"
#include <algorithm>
#include <string>
#include <thread>

FTDI::FTDI(USB *usb, Config *config) : usb(usb), config(config) {}

unsigned int FTDI::divisorForFrequency(const unsigned int targetFrequency) {
    unsigned int frequency = targetFrequency == 0 ? 1 : targetFrequency;

    unsigned int divisor = ((FTDI_CLOCK_RATE / 2) + (frequency - 1)) / frequency;
    divisor = std::min(divisor, 0x10000u);
    divisor = std::max(divisor, 1u);

    unsigned int actualFrequency = FTDI_CLOCK_RATE / (2 * divisor);
    double ratio = static_cast<double>(frequency) / actualFrequency;

    static unsigned int warnedFrequency = ~0;
    if (warnedFrequency != actualFrequency) {
        warnedFrequency = actualFrequency;
        if (ratio < 0.999 || ratio > 1.001) {
            spdlog::warn(WARN_CLOCK_REQUESTED, frequency, actualFrequency);
        }
        if (actualFrequency < 500000) {
            spdlog::warn(WARN_CLOCK_SLOW, actualFrequency);
        }
    }

    return divisor;
}

int FTDI::setClockSpeed(const unsigned int targetFrequency) const {
    unsigned int frequency = config->lockedSpeed ? config->lockedSpeed : targetFrequency;
    unsigned int count = divisorForFrequency(frequency) - 1;

    constexpr size_t CLOCK_SPEED_SIZE = 4;
    const unsigned char clockSpeed[CLOCK_SPEED_SIZE] = {
            FTDI_DISABLE_TCK_PRESCALER,
            FTDI_SET_TCK_DIVISOR,
            static_cast<unsigned char>(count),
            static_cast<unsigned char>(count >> 8)
    };

    return usb->writeData(clockSpeed, CLOCK_SPEED_SIZE);
}

int FTDI::setGpio() const {
    constexpr auto ms100 = std::chrono::duration<int, std::milli>(100);

    unsigned char gpio[] = {
            FTDI_SET_LOW_BYTE,
            0,
            0
    };

    const std::string& gpioArgument = config->gpioArgument;
    std::string::size_type start = 0, end;

    auto processGpioValue = [&](const std::string& token) -> bool {
        unsigned long value = std::stoul(token, nullptr, 16);
        if (value > 0xFF) {
            spdlog::error(ERR_GPIO_OUT_OF_RANGE, value);
            return false;
        }

        unsigned int direction = value >> 4;
        value &= 0xF;
        gpio[1] = (value << 4) | FTDI_PIN_TMS;
        gpio[2] = (direction << 4) | FTDI_PIN_TMS | FTDI_PIN_TDI | FTDI_PIN_TCK;

        if (!usb->writeData(gpio, sizeof(gpio))) {
            return false;
        }

        return true;
    };

    while ((end = gpioArgument.find(':', start)) != std::string::npos) {
        std::string token = gpioArgument.substr(start, end - start);
        if (!processGpioValue(token)) {
            return 0;
        }
        start = end + 1;
        std::this_thread::sleep_for(ms100);
    }

    if (!gpioArgument.substr(start).empty()) {
        if (!processGpioValue(gpioArgument.substr(start))) {
            return 0;
        }
    }

    return 1;
}

int FTDI::init() const {
    // Control commands initialization
    if (!usb->setControl(BREQ_RESET, WVAL_RESET_RESET) ||
        !usb->setControl(BREQ_SET_BITMODE, WVAL_SET_BITMODE_MPSSE) ||
        !usb->setControl(BREQ_SET_LATENCY, 2) ||
        !usb->setControl(BREQ_RESET, WVAL_RESET_PURGE_TX) ||
        !usb->setControl(BREQ_RESET, WVAL_RESET_PURGE_RX)) {
        return 0;
    }

    // Set clock speed and initialize the device
    if (!setClockSpeed(10000000) || !setStartup()) {
        return 0;
    }

    // Configure GPIO if specified
    if (!config->gpioArgument.empty() && !setGpio()) {
        spdlog::error(ERR_BAD_GPIO_FORMAT);
        return 0;
    }

    return 1;
}

int FTDI::setStartup() const {
    return usb->writeData(startup.data(), startup.size());
}
