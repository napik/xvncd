//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#ifndef XVNCD_CPP_XVNCD_H
#define XVNCD_CPP_XVNCD_H

#include "usb.h"
#include "FTDI.h"

#define FREQUENCY 1000000000

class VncProtocol {
public:
    explicit VncProtocol(Flags *flags) : usb(flags), ftdi(&usb) {
        usb.init();
    }

    void processCommands(FILE *fp, int fd) const;

    void cmdByte(int byte) const;

    [[nodiscard]] int shiftChunks(uint32_t nBits) const;

    uint32_t shift(FILE *fp) const;

    Flags flags{};

    void close();

    int connect();

private:
    static int fetch32(FILE *fp, uint32_t *value);

    static int matchInput(FILE *fp, const char *str);

    static int reply(int fd, const unsigned char *buf, uint32_t len);

    static int reply32(int fd, uint32_t value);

    void set_zero() const;

    FTDI ftdi;
    USB usb;

};


#endif //XVNCD_CPP_XVNCD_H
