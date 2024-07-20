#include "FTDI.h"
#include <algorithm>
#include <string>
#include <thread>
#include <vector>

FTDI::FTDI() {
    config = Config::get();
    usb = std::make_unique<USB>();
}

unsigned int FTDI::divisorForFrequency(const unsigned int targetFrequency) {
    unsigned int frequency = targetFrequency == 0 ? 1 : targetFrequency;

    unsigned int divisor = ((FTDI_CLOCK_RATE / 2) + (frequency - 1)) / frequency;
    divisor = std::min(divisor, 0x10000u);
    divisor = std::max(divisor, 1u);

    unsigned int actualFrequency = FTDI_CLOCK_RATE / (2 * divisor);
    const double ratio = static_cast<double>(frequency) / actualFrequency;

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

int FTDI::set_clock_speed(const unsigned int targetFrequency) const {
    const unsigned int frequency = config->lockedSpeed ? config->lockedSpeed : targetFrequency;
    const unsigned int count = divisorForFrequency(frequency) - 1;

    const std::vector clockSpeed = {
        FTDI_DISABLE_TCK_PRESCALER,
        FTDI_SET_TCK_DIVISOR,
        static_cast<unsigned char>(count),
        static_cast<unsigned char>(count >> 8)
    };

    return usb->write_data(clockSpeed);
}

void FTDI::cmd_byte(const int value) const {
    usb->cmdByte(value);
}

void FTDI::enable_loopback() const {
    usb->cmdByte(FTDI_ENABLE_LOOPBACK);
}

void FTDI::set_tms_bits(const int cmd_bit_count, int param) const {
    usb->cmdByte(FTDI_MPSSE_XFER_TMS_BITS);
    usb->cmdByte(cmd_bit_count - 1);
    usb->cmdByte(param);
}

void FTDI::set_tdi_bytes(const int cmdBytes) const {
    usb->cmdByte(FTDI_MPSSE_XFER_TDI_BYTES);
    usb->cmdByte(cmdBytes - 1);
    usb->cmdByte((cmdBytes - 1) >> 8);
}

void FTDI::close() const {
    usb->close();
}

void FTDI::set_tdi_bits(const int cmd_bit_count, int param) const {
    usb->cmdByte(FTDI_MPSSE_XFER_TDI_BITS);
    usb->cmdByte(cmd_bit_count - 1);
    usb->cmdByte(param);
}

int FTDI::set_gpio() const {
    std::vector<unsigned char> gpio = {FTDI_SET_LOW_BYTE, 0, 0, 0};

    const std::string &gpioArgument = config->gpioArgument;
    std::string::size_type start = 0;
    std::string::size_type end;

    auto processGpioValue = [&](const std::string &token) {
        unsigned long value = std::stoul(token, nullptr, 16);
        if (value > 0xFF) {
            spdlog::error(ERR_GPIO_OUT_OF_RANGE, value);
            return false;
        }

        const unsigned int direction = value >> 4;
        value &= 0xF;
        gpio[1] = (value << 4) | FTDI_PIN_TMS;
        gpio[2] = (direction << 4) | FTDI_PIN_TMS | FTDI_PIN_TDI | FTDI_PIN_TCK;

        if (!usb->write_data(gpio)) {
            return false;
        }

        return true;
    };

    while ((end = gpioArgument.find(':', start)) != std::string::npos) {
        if (std::string token = gpioArgument.substr(start, end - start); !processGpioValue(token)) {
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
    if (!usb->connect()) {
        return 0;
    }

    // Control commands initialization
    if (!usb->set_control(BREQ_RESET, WVAL_RESET_RESET) ||
        !usb->set_control(BREQ_SET_BITMODE, WVAL_SET_BITMODE_MPSSE) ||
        !usb->set_control(BREQ_SET_LATENCY, 2) ||
        !usb->set_control(BREQ_RESET, WVAL_RESET_PURGE_TX) ||
        !usb->set_control(BREQ_RESET, WVAL_RESET_PURGE_RX)) {
        return 0;
    }

    // Set clock speed and initialize the device
    if (!set_clock_speed(10000000) || !setStartup()) {
        return 0;
    }

    // Configure GPIO if specified
    if (!config->gpioArgument.empty() && !set_gpio()) {
        spdlog::error(ERR_BAD_GPIO_FORMAT);
        return 0;
    }

    return 1;
}

int FTDI::setStartup() const {
    return usb->write_data(startup);
}
