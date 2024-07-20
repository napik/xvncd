//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <print>
#include "xvncd.h"
#include "misc.h"

void VncProtocol::cmdByte(int byte) const {
    if (ftdi.usb->txCount == USB_BUFFER_SIZE) {
        fprintf(stderr, "ftdi TX OVERFLOW!\n");
        exit(4);
    }
    ftdi.usb->ioBuf[ftdi.usb->txCount++] = byte;
}

/*
 * The ftdi/JTAG chip can't shift data to TMS and TDI simultaneously,
 * so switch between TMS and TDI shift commands as necessary.
 * Break into chunks small enough to fit in a single packet.
 */
int VncProtocol::shiftChunks(uint32_t nBits) const {
    int iBit = 0x01, iIndex = 0;
    int cmdBit, cmdIndex, cmdBitcount;
    int tmsBit, tmsBits, tmsState;
    int rxBit, rxIndex;
    int tdoBit = 0x01, tdoIndex = 0;
    unsigned short rxBitcounts[USB_BUFFER_SIZE / 3];

    if (ftdi.usb->flags.loopback) {
        cmdByte(FTDI_ENABLE_LOOPBACK);
    }
    while (nBits) {
        int rxBytesWanted = 0;
        int rxBitcountIndex = 0;
        ftdi.usb->txCount = 0;
        ftdi.usb->chunkCount++;
        do {
            /*
             * Stash TMS bits until bit limit reached or TDI would change state
             */
            int tdiFirstState = ((ftdi.usb->tdiBuf[iIndex] & iBit) != 0);
            cmdBitcount = 0;
            cmdBit = 0x01;
            tmsBits = 0;
            do {
                tmsBit = (ftdi.usb->tmsBuf[iIndex] & iBit) ? cmdBit : 0;
                tmsBits |= tmsBit;
                if (iBit == 0x80) {
                    iBit = 0x01;
                    iIndex++;
                } else {
                    iBit <<= 1;
                }
                cmdBitcount++;
                cmdBit <<= 1;
            } while ((cmdBitcount < 6) && (cmdBitcount < nBits)
                     && (((ftdi.usb->tdiBuf[iIndex] & iBit) != 0) == tdiFirstState));

            /*
             * Duplicate the final TMS bit so the TMS pin holds
             * its value for subsequent TDI shift commands.
             * This is why the bit limit above is 6 and not 7 since
             * we need space to hold the copy of the final bit.
             */
            tmsBits |= (tmsBit << 1);
            tmsState = (tmsBit != 0);

            /*
             * Send the TMS bits and TDI value.
             */
            cmdByte(FTDI_MPSSE_XFER_TMS_BITS);
            cmdByte(cmdBitcount - 1);
            cmdByte((tdiFirstState << 7) | tmsBits);
            rxBitcounts[rxBitcountIndex++] = cmdBitcount;
            rxBytesWanted++;
            nBits -= cmdBitcount;

            /*
             * Stash TDI bits until bit limit reached
             * or TMS change of state
             * or transmitter buffer capacity reached.
             */
            cmdBitcount = 0;
            cmdIndex = 0;
            cmdBit = 0x01;
            ftdi.usb->cmdBuf[0] = 0;
            while ((nBits != 0)
                   && (((ftdi.usb->tmsBuf[iIndex] & iBit) != 0) == tmsState)
                   && ((ftdi.usb->txCount + (cmdBitcount / 8)) < (ftdi.usb->bulkOutRequestSize - 5))) {
                if (ftdi.usb->tdiBuf[iIndex] & iBit) {
                    ftdi.usb->cmdBuf[cmdIndex] |= cmdBit;
                }
                if (cmdBit == 0x80) {
                    cmdBit = 0x01;
                    cmdIndex++;
                    ftdi.usb->cmdBuf[cmdIndex] = 0;
                } else {
                    cmdBit <<= 1;
                }
                if (iBit == 0x80) {
                    iBit = 0x01;
                    iIndex++;
                } else {
                    iBit <<= 1;
                }
                cmdBitcount++;
                nBits--;
            }

            /*
             * Send stashed TDI bits
             */
            if (cmdBitcount > 0) {
                int cmdBytes = cmdBitcount / 8;
                rxBitcounts[rxBitcountIndex++] = cmdBitcount;
                if (cmdBitcount >= 8) {
                    int i;
                    rxBytesWanted += cmdBytes;
                    cmdBitcount -= cmdBytes * 8;
                    i = cmdBytes - 1;
                    cmdByte(FTDI_MPSSE_XFER_TDI_BYTES);
                    cmdByte(i);
                    cmdByte(i >> 8);
                    for (i = 0; i < cmdBytes; i++) {
                        cmdByte(ftdi.usb->cmdBuf[i]);
                    }
                }
                if (cmdBitcount) {
                    rxBytesWanted++;
                    cmdByte(FTDI_MPSSE_XFER_TDI_BITS);
                    cmdByte(cmdBitcount - 1);
                    cmdByte(ftdi.usb->cmdBuf[cmdBytes]);
                }
            }
        } while ((nBits != 0)
                 && ((ftdi.usb->txCount + (cmdBitcount / 8)) < (ftdi.usb->bulkOutRequestSize - 6)));

        /*
         * Shift
         */
        if (!ftdi.usb->writeData(ftdi.usb->txCount)
            || !ftdi.usb->readData(ftdi.usb->rxBuf, rxBytesWanted)) {
            return 0;
        }

        /*
         * Process received data
         */
        rxIndex = 0;
        for (int i = 0; i < rxBitcountIndex; i++) {
            int rxBitcount = rxBitcounts[i];
            if (rxBitcount < 8) {
                rxBit = 0x1 << (8 - rxBitcount);
            } else {
                rxBit = 0x01;
            }
            while (rxBitcount--) {
                if (tdoBit == 0x1) {
                    ftdi.usb->tdoBuf[tdoIndex] = 0;
                }
                if (ftdi.usb->rxBuf[rxIndex] & rxBit) {
                    ftdi.usb->tdoBuf[tdoIndex] |= tdoBit;
                }
                if (rxBit == 0x80) {
                    if (rxBitcount < 8) {
                        rxBit = 0x1 << (8 - rxBitcount);
                    } else {
                        rxBit = 0x01;
                    }
                    rxIndex++;
                } else {
                    rxBit <<= 1;
                }
                if (tdoBit == 0x80) {
                    tdoBit = 0x01;
                    tdoIndex++;
                } else {
                    tdoBit <<= 1;
                }
            }
        }
        if (rxIndex != rxBytesWanted) {
            printf("Warning -- consumed %d but supplied %d\n", rxIndex,
                   rxBytesWanted);
        }
    }
    return 1;
}

/*
 * Fetch 32-bit values from a client
 */
int VncProtocol::fetch32(FILE *fp, uint32_t *value) {
    int i;
    uint32_t v;

    for (i = 0, v = 0; i < 32; i += 8) {
        int c = fgetc(fp);
        if (c == EOF) {
            Misc::badEOF();
            return 0;
        }
        v |= c << i;
    }
    *value = v;
    return 1;
}


/*
 * Shift a client packet set of bits
 */
uint32_t VncProtocol::shift(FILE *fp) const {
    uint32_t nBits, nBytes;

    if (!fetch32(fp, &nBits)) {
        return 0;
    }
    if (nBits > (unsigned int) ftdi.usb->largestShiftRequest) {
        ftdi.usb->largestShiftRequest = nBits;
    }
    ftdi.usb->bitCount += nBits;
    ftdi.usb->shiftCount++;
    nBytes = (nBits + 7) / 8;
    if (ftdi.usb->flags.showXVC) {
        printf("shift:%d\n", (int) nBits);
    }
    if (nBytes > XVC_BUFFER_SIZE) {
        fprintf(stderr, "Client requested %u, max is %u\n", nBytes, XVC_BUFFER_SIZE);
        exit(1);
    }
    if ((fread(ftdi.usb->tmsBuf, 1, nBytes, fp) != nBytes)
        || (fread(ftdi.usb->tdiBuf, 1, nBytes, fp) != nBytes)) {
        return 0;
    }
    if (ftdi.usb->flags.showXVC) {
        Misc::showBuf("TMS", ftdi.usb->tmsBuf, nBytes);
        Misc::showBuf("TDI", ftdi.usb->tdiBuf, nBytes);
    }
    if (!shiftChunks(nBits)) {
        return 0;
    }
    if (ftdi.usb->flags.showXVC) {
        Misc::showBuf("TDO", ftdi.usb->tdoBuf, nBytes);
    }
    if (ftdi.usb->flags.loopback) {
        if (memcmp(ftdi.usb->tdiBuf, ftdi.usb->tdoBuf, nBytes) != 0) {
            printf("Loopback failed.\n");
        }
    }
    return nBytes;
}

/*
 * Fetch a known string
 */
int VncProtocol::matchInput(FILE *fp, const char *str) {
    while (*str) {
        int c = fgetc(fp);
        if (c == EOF) {
            Misc::badEOF();
            return 0;
        }
        if (c != *str) {
            fprintf(stderr, "Expected 0x%2x, got 0x%2x\n", *str, c);
            return 0;
        }
        str++;
    }
    return 1;
}

int VncProtocol::reply(int fd, const unsigned char *buf, uint32_t len) {
    if (write(fd, buf, len) != len) {
        fprintf(stderr, "reply failed: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

/*
 * Return a 32-bit value
 */
int VncProtocol::reply32(int fd, uint32_t value) {
    int i;
    unsigned char cbuf[4];

    for (i = 0; i < 4; i++) {
        cbuf[i] = value;
        value >>= 8;
    }
    return reply(fd, cbuf, 4);
}

/*
 * Read and process commands
 */
void VncProtocol::processCommands(FILE *fp, int fd) const {
    int c;

    for (;;) {
        switch (c = fgetc(fp)) {
            case 's':
                switch (c = fgetc(fp)) {
                    case 'e': {
                        uint32_t num;
                        int frequency;
                        if (!matchInput(fp, "ttck:")) return;
                        if (!fetch32(fp, &num)) return;
                        frequency = FREQUENCY / num;
                        if (ftdi.usb->flags.showXVC) {
                            printf("settck:%d  (%d Hz)\n", (int) num, frequency);
                        }
                        if (!ftdi.ftdiSetClockSpeed(frequency)) return;
                        if (!reply32(fd, num)) return;
                    }
                        break;

                    case 'h': {
                        uint32_t nBytes;
                        if (!matchInput(fp, "ift:")) return;
                        nBytes = shift(fp);
                        if ((nBytes <= 0) || !reply(fd, ftdi.usb->tdoBuf, nBytes)) {
                            return;
                        }
                    }
                        break;

                    default:
                        if (ftdi.usb->flags.showXVC) {
                            printf("Bad second char 0x%02x\n", c);
                        }
                        Misc::badChar();
                        return;
                }
                break;

            case 'g':
                if (matchInput(fp, "etinfo:")) {
                    char cBuf[40];
                    int len;
                    if (ftdi.usb->flags.showXVC) {
                        printf("getinfo:\n");
                    }
                    len = snprintf(cBuf, sizeof(cBuf), "xvcServer_v1.0:%u\n", XVC_BUFFER_SIZE);
                    if (reply(fd, (unsigned char *) cBuf, len)) {
                        break;
                    }
                }
                return;

            case EOF:
                return;

            default:
                if (ftdi.usb->flags.showXVC) {
                    printf("Bad initial char 0x%02x\n", c);
                }
                Misc::badChar();
                return;
        }
    }
}

void VncProtocol::set_zero() const {
    ftdi.usb->shiftCount = 0;
    ftdi.usb->chunkCount = 0;
    ftdi.usb->bitCount = 0;
}

void VncProtocol::close() {
    ftdi.printStatistic();
    ftdi.close();
}

int VncProtocol::connect() {
    ftdi.usb->init();

    if (usb.handle == nullptr && !ftdi.usb->connectUSB()) {
        return EXIT_FAILURE;
    }

    if (ftdi.init()) {
        return EXIT_FAILURE;

    }

    set_zero();

    return EXIT_SUCCESS;
}
