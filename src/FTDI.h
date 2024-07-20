#pragma once

#include "usb.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class FTDI {
public:
    explicit FTDI();

    [[nodiscard]] int init() const;

    [[nodiscard]] int set_gpio() const;

    [[nodiscard]] int set_clock_speed(unsigned int targetFrequency) const;

    void cmd_byte(int value) const;

    void enable_loopback() const;

    void set_tms_bits(int cmd_bit_count, int param) const;

    void set_tdi_bits(int cmd_bit_count, int param) const;

    void set_tdi_bytes(int cmdBytes) const;

    void close() const;

    std::unique_ptr<USB> usb;

private:
    static unsigned int divisorForFrequency(unsigned int frequency);

    std::shared_ptr<Config> config{};

    // Error messages
    static constexpr std::string_view WARN_CLOCK_REQUESTED = "{} Hz clock requested, {} Hz actual";
    static constexpr std::string_view WARN_CLOCK_SLOW = "{} Hz clock is a slow choice.";
    static constexpr std::string_view ERR_GPIO_OUT_OF_RANGE = "GPIO value out of range: {}";
    static constexpr std::string_view ERR_BAD_GPIO_FORMAT = "Bad -g direction:value[:value...]";

    static constexpr unsigned char FTDI_SET_LOW_BYTE = 0x80;
    static constexpr unsigned char FTDI_ENABLE_LOOPBACK = 0x84;
    static constexpr unsigned char FTDI_DISABLE_LOOPBACK = 0x85;
    static constexpr unsigned char FTDI_SET_TCK_DIVISOR = 0x86;
    static constexpr unsigned char FTDI_DISABLE_TCK_PRESCALER = 0x8A;
    static constexpr unsigned char FTDI_DISABLE_3_PHASE_CLOCK = 0x8D;
    static constexpr unsigned char FTDI_ACK_BAD_COMMAND = 0xFA;

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

    // Startup commands
    const std::vector<unsigned char> startup{
        FTDI_DISABLE_LOOPBACK,
        FTDI_DISABLE_3_PHASE_CLOCK,
        FTDI_SET_LOW_BYTE,
        FTDI_PIN_TMS,
        FTDI_PIN_TMS | FTDI_PIN_TDI | FTDI_PIN_TCK
    };

    // Define other constants
    static constexpr unsigned int FTDI_CLOCK_RATE = 60000000;
    static constexpr int BMREQTYPE_OUT = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT;
    static constexpr unsigned char BREQ_RESET = 0x00;
    static constexpr unsigned char BREQ_SET_LATENCY = 0x09;
    static constexpr unsigned char BREQ_SET_BITMODE = 0x0B;
    static constexpr unsigned int WVAL_RESET_RESET = 0x00;
    static constexpr unsigned int WVAL_RESET_PURGE_RX = 0x01;
    static constexpr unsigned int WVAL_RESET_PURGE_TX = 0x02;
    static constexpr unsigned int WVAL_SET_BITMODE_MPSSE = 0x0200 | FTDI_PIN_TCK | FTDI_PIN_TDI | FTDI_PIN_TMS;

    [[nodiscard]] int setStartup() const;

    static constexpr auto ms100 = std::chrono::duration<int, std::milli>(100);
};
