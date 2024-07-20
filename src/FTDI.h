#ifndef XVNCD_CPP_FTDI_H
#define XVNCD_CPP_FTDI_H

#include "usb.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class FTDI {
public:
    explicit FTDI(USB *usb, Config *pFlags);

    [[nodiscard]] int init() const;

    [[nodiscard]] int setGpio() const;

    [[nodiscard]] int setClockSpeed(unsigned int frequency) const;

    static unsigned int divisorForFrequency(unsigned int frequency);

    // Define constants for FTDI commands
    static constexpr unsigned char FTDI_MPSSE_BIT_WRITE_TMS = 0x40;
    static constexpr unsigned char FTDI_MPSSE_BIT_READ_DATA = 0x20;
    static constexpr unsigned char FTDI_MPSSE_BIT_WRITE_DATA = 0x10;
    static constexpr unsigned char FTDI_MPSSE_BIT_LSB_FIRST = 0x08;
    static constexpr unsigned char FTDI_MPSSE_BIT_READ_ON_FALLING_EDGE = 0x04;
    static constexpr unsigned char FTDI_MPSSE_BIT_BIT_MODE = 0x02;
    static constexpr unsigned char FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE = 0x01;

    static constexpr unsigned char FTDI_MPSSE_XFER_TDI_BYTES = FTDI_MPSSE_BIT_WRITE_DATA |
                                                               FTDI_MPSSE_BIT_READ_DATA |
                                                               FTDI_MPSSE_BIT_LSB_FIRST |
                                                               FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE;

    static constexpr unsigned char FTDI_MPSSE_XFER_TDI_BITS = FTDI_MPSSE_BIT_WRITE_DATA |
                                                              FTDI_MPSSE_BIT_READ_DATA |
                                                              FTDI_MPSSE_BIT_LSB_FIRST |
                                                              FTDI_MPSSE_BIT_BIT_MODE |
                                                              FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE;

    static constexpr unsigned char FTDI_MPSSE_XFER_TMS_BITS = FTDI_MPSSE_BIT_WRITE_TMS |
                                                              FTDI_MPSSE_BIT_READ_DATA |
                                                              FTDI_MPSSE_BIT_LSB_FIRST |
                                                              FTDI_MPSSE_BIT_BIT_MODE |
                                                              FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE;

    // FTDI I/O pin bits
    static constexpr unsigned char FTDI_PIN_TCK = 0x1;
    static constexpr unsigned char FTDI_PIN_TDI = 0x2;
    static constexpr unsigned char FTDI_PIN_TDO = 0x4;
    static constexpr unsigned char FTDI_PIN_TMS = 0x8;

    // Define other constants
    static constexpr unsigned int FTDI_CLOCK_RATE = 60000000;
    static constexpr int BMREQTYPE_OUT = LIBUSB_REQUEST_TYPE_VENDOR |
                                         LIBUSB_RECIPIENT_DEVICE |
                                         LIBUSB_ENDPOINT_OUT;
    static constexpr unsigned char BREQ_RESET = 0x00;
    static constexpr unsigned char BREQ_SET_LATENCY = 0x09;
    static constexpr unsigned char BREQ_SET_BITMODE = 0x0B;
    static constexpr unsigned int WVAL_RESET_RESET = 0x00;
    static constexpr unsigned int WVAL_RESET_PURGE_RX = 0x01;
    static constexpr unsigned int WVAL_RESET_PURGE_TX = 0x02;
    static constexpr unsigned int WVAL_SET_BITMODE_MPSSE = 0x0200 |
                                                           FTDI_PIN_TCK |
                                                           FTDI_PIN_TDI |
                                                           FTDI_PIN_TMS;

    static constexpr unsigned char FTDI_SET_LOW_BYTE = 0x80;
    static constexpr unsigned char FTDI_ENABLE_LOOPBACK = 0x84;
    static constexpr unsigned char FTDI_DISABLE_LOOPBACK = 0x85;
    static constexpr unsigned char FTDI_SET_TCK_DIVISOR = 0x86;
    static constexpr unsigned char FTDI_DISABLE_TCK_PRESCALER = 0x8A;
    static constexpr unsigned char FTDI_DISABLE_3_PHASE_CLOCK = 0x8D;
    static constexpr unsigned char FTDI_ACK_BAD_COMMAND = 0xFA;

private:
    std::unique_ptr<USB> usb;
    Config *config;

    // Error messages
    static constexpr const char *WARN_CLOCK_REQUESTED = "{} Hz clock requested, {} Hz actual";
    static constexpr const char *WARN_CLOCK_SLOW = "{} Hz clock is a slow choice.";
    static constexpr const char *ERR_GPIO_OUT_OF_RANGE = "GPIO value out of range: {}";
    static constexpr const char *ERR_BAD_GPIO_FORMAT = "Bad -g direction:value[:value...]";

    // Startup commands
    static constexpr std::array<unsigned char, 5> startup = {
            FTDI_DISABLE_LOOPBACK,
            FTDI_DISABLE_3_PHASE_CLOCK,
            FTDI_SET_LOW_BYTE,
            FTDI_PIN_TMS,
            FTDI_PIN_TMS | FTDI_PIN_TDI | FTDI_PIN_TCK
    };

    [[nodiscard]] int setStartup() const;
};

#endif // XVNCD_CPP_FTDI_H
