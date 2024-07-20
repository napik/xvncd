//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#ifndef XVNCD_CPP_FTDI_H
#define XVNCD_CPP_FTDI_H

#include "usb.h"

#define FTDI_CLOCK_RATE     60000000
#define BMREQTYPE_OUT (LIBUSB_REQUEST_TYPE_VENDOR | \
                       LIBUSB_RECIPIENT_DEVICE | \
                       LIBUSB_ENDPOINT_OUT)

#define BREQ_RESET          0x00
#define BREQ_SET_LATENCY    0x09
#define BREQ_SET_BITMODE    0x0B

#define WVAL_RESET_RESET        0x00
#define WVAL_RESET_PURGE_RX     0x01
#define WVAL_RESET_PURGE_TX     0x02
#define WVAL_SET_BITMODE_MPSSE (0x0200       | \
                                FTDI_PIN_TCK | \
                                FTDI_PIN_TDI | \
                                FTDI_PIN_TMS)

#define FTDI_SET_LOW_BYTE           0x80
#define FTDI_ENABLE_LOOPBACK        0x84
#define FTDI_DISABLE_LOOPBACK       0x85
#define FTDI_SET_TCK_DIVISOR        0x86
#define FTDI_DISABLE_TCK_PRESCALER  0x8A
#define FTDI_DISABLE_3_PHASE_CLOCK  0x8D
#define FTDI_ACK_BAD_COMMAND        0xFA

/* FTDI I/O pin bits */
#define FTDI_PIN_TCK    0x1
#define FTDI_PIN_TDI    0x2
#define FTDI_PIN_TDO    0x4
#define FTDI_PIN_TMS    0x8

class FTDI {
public:

    explicit FTDI(USB *usb) {
        this->usb = usb;
    }

    [[nodiscard]] int init() const;

    [[nodiscard]] int ftdiGPIO() const;

    [[nodiscard]] int ftdiSetClockSpeed(unsigned int frequency) const;

    static unsigned int divisorForFrequency(unsigned int frequency);

    [[nodiscard]] int usbControl(int bRequest, int wValue) const;

    USB *usb;

    void printStatistic() const;

    void close() const;

private:
    constexpr static const unsigned char startup[] = {
            FTDI_DISABLE_LOOPBACK,
            FTDI_DISABLE_3_PHASE_CLOCK,
            FTDI_SET_LOW_BYTE,
            FTDI_PIN_TMS,
            FTDI_PIN_TMS | FTDI_PIN_TDI | FTDI_PIN_TCK
    };
};


#endif //XVNCD_CPP_FTDI_H
