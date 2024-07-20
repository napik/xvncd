#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "FTDI.h"

unsigned int FTDI::divisorForFrequency(unsigned int frequency) {
    unsigned int divisor;
    unsigned int actual;
    double r;
    static unsigned int warned = ~0;

    if (frequency <= 0) frequency = 1;
    divisor = ((FTDI_CLOCK_RATE / 2) + (frequency - 1)) / frequency;
    if (divisor >= 0x10000) {
        divisor = 0x10000;
    }
    if (divisor < 1) {
        divisor = 1;
    }
    actual = FTDI_CLOCK_RATE / (2 * divisor);
    r = (double) frequency / actual;
    if (warned != actual) {
        warned = actual;
        if ((r < 0.999) || (r > 1.001)) {
            fprintf(stderr, "Warning -- %d Hz clock requested, %d Hz actual\n", frequency, actual);
        }
        if (actual < 500000) {
            fprintf(stderr, "Warning -- %d Hz clock is a slow choice.\n", actual);
        }
    }
    return divisor;
}

int FTDI::usbControl(int bRequest, int wValue) const {
    int c;
    if (usb->flags.showUSB) {
        printf("usbControl bmRequestType:%02X bRequest:%02X wValue:%04X\n", 64, bRequest, wValue);
    }
    c = libusb_control_transfer(usb->handle, 64, bRequest, wValue, usb->flags.ftdiJTAGindex, nullptr, 0, 1000);
    if (c != 0) {
        fprintf(stderr, "usb_control_transfer failed: %s\n", libusb_strerror((libusb_error) c));
        return 0;
    }
    return 1;
}

int FTDI::ftdiSetClockSpeed(unsigned int frequency) const {
    unsigned int count;
    if (usb->flags.lockedSpeed) {
        frequency = usb->flags.lockedSpeed;
    }
    count = divisorForFrequency(frequency) - 1;
    usb->ioBuf[0] = FTDI_DISABLE_TCK_PRESCALER;
    usb->ioBuf[1] = FTDI_SET_TCK_DIVISOR;
    usb->ioBuf[2] = count;
    usb->ioBuf[3] = count >> 8;
    return usb->writeData(4);
}

int FTDI::ftdiGPIO() const {
    unsigned long value;
    unsigned int direction;
    const char *str = usb->flags.gpioArgument;
    char *endp;
    static const struct timespec ms100 = {.tv_sec = 0, .tv_nsec = 100000000};

    usb->ioBuf[0] = FTDI_SET_LOW_BYTE;
    while (*str != '\0') {
        value = strtoul(str, &endp, 16);
        if ((endp == str) || ((*endp != '\0') && (*endp != ':'))) {
            fprintf(stderr, "Invalid GPIO argument: %s\n", str);
            return 0;
        }
        str = endp + 1;
        if (value > 0xFF) {
            fprintf(stderr, "GPIO value out of range: %lx\n", value);
            return 0;
        }
        direction = value >> 4;
        value &= 0xF;
        usb->ioBuf[1] = (value << 4) | FTDI_PIN_TMS;
        usb->ioBuf[2] = (direction << 4) | FTDI_PIN_TMS | FTDI_PIN_TDI | FTDI_PIN_TCK;
        if (!usb->writeData(3)) {
            return 0;
        }
        if (*endp == '\0') {
            return 1;
        }
        nanosleep(&ms100, nullptr);
    }
    return 0;
}

int FTDI::init() const {
    memcpy(usb->ioBuf, startup, sizeof(startup));

    if (!usbControl(BREQ_RESET, WVAL_RESET_RESET) ||
        !usbControl(BREQ_SET_BITMODE, WVAL_SET_BITMODE_MPSSE) ||
        !usbControl(BREQ_SET_LATENCY, 2) ||
        !usbControl(BREQ_RESET, WVAL_RESET_PURGE_TX) ||
        !usbControl(BREQ_RESET, WVAL_RESET_PURGE_RX) ||
        !ftdiSetClockSpeed(10000000) ||
        !usb->writeData(sizeof(startup))) {
        return 0;
    }

    if (usb->flags.gpioArgument && !ftdiGPIO()) {
        fprintf(stderr, "Bad -g direction:value[:value...]\n");
        return 0;
    }
    return 1;
}

void FTDI::printStatistic() const {
    if (usb->flags.statisticsFlag) {
        printf("   Shifts: %" PRIu64 "\n", usb->shiftCount);
        printf("   Chunks: %" PRIu64 "\n", usb->chunkCount);
        printf("     Bits: %" PRIu64 "\n", usb->bitCount);
        printf(" Largest shift request: %d\n", usb->largestShiftRequest);
        printf(" Largest write request: %d\n", usb->largestWriteRequest);
        printf("Largest write transfer: %d\n", usb->largestWriteSent);
        printf("  Largest read request: %d\n", usb->largestReadRequest);
    }
}

void FTDI::close() const {
    if (usb->handle) {
        libusb_close(usb->handle);
        usb->handle = nullptr;
    }
}
