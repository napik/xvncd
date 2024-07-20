#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <print>
#include "xvncd.h"
#include "misc.h"


VncProtocol::VncProtocol(Config *config) : usb(std::make_unique<USB>(config)),
                                           ftdi(std::make_unique<FTDI>(usb.get(), config)),
                                           flags(config->flags.get()) {}

VncProtocol::~VncProtocol() {
    close();
}

void VncProtocol::cmdByte(int byte) {
    if (txCount >= USB::USB_BUFFER_SIZE) {
        spdlog::error("FTDI TX OVERFLOW!");
        std::exit(EXIT_FAILURE);
    }
    ioBuf[txCount++] = byte;
}

int VncProtocol::shiftChunks(const uint32_t shiftBits) {
    uint32_t nBits = shiftBits;
    int iBit = 0x01, iIndex = 0;
    int cmdBit, cmdIndex, cmdBitCount;
    int tmsBit, tmsBits, tmsState;
    int rxBit, rxIndex;
    int tdoBit = 0x01, tdoIndex = 0;
    std::vector<unsigned short> rxBitCounts((USB::USB_BUFFER_SIZE / 3) + 1);

    if (flags->loopback) {
        cmdByte(FTDI::FTDI_ENABLE_LOOPBACK);
    }

    while (nBits) {
        int rxBytesWanted = 0;
        int rxBitCountIndex = 0;

        txCount = 0;
        chunkCount++;

        do {
            int tdiFirstState = (tdiBuf[iIndex] & iBit) != 0;
            cmdBitCount = 0;
            cmdBit = 0x01;
            tmsBits = 0;
            do {
                tmsBit = (tmsBuf[iIndex] & iBit) ? cmdBit : 0;
                tmsBits |= tmsBit;
                if (iBit == 0x80) {
                    iBit = 0x01;
                    iIndex++;
                } else {
                    iBit <<= 1;
                }
                cmdBitCount++;
                cmdBit <<= 1;
            } while ((cmdBitCount < 6) && (cmdBitCount < nBits) &&
                     (((tdiBuf[iIndex] & iBit) != 0) == tdiFirstState));

            // Duplicate the final TMS bit
            tmsBits |= (tmsBit << 1);
            tmsState = tmsBit != 0;

            // Send the TMS bits and TDI value
            cmdByte(FTDI::FTDI_MPSSE_XFER_TMS_BITS);
            cmdByte(cmdBitCount - 1);
            cmdByte((tdiFirstState << 7) | tmsBits);
            rxBitCounts[rxBitCountIndex++] = cmdBitCount;
            rxBytesWanted++;
            nBits -= cmdBitCount;

            // Stash TDI bits until bit limit reached or TMS change of state
            cmdBitCount = 0;
            cmdIndex = 0;
            cmdBit = 0x01;
            cmdBuf[0] = 0;

            while (nBits > 0 &&
                   ((tmsBuf[iIndex] & iBit) == tmsState) &&
                   (txCount + (cmdBitCount / 8)) < (usb->bulkOutRequestSize - 5)) {
                if (tdiBuf[iIndex] & iBit) {
                    cmdBuf[cmdIndex] |= cmdBit;
                }
                if (cmdBit == 0x80) {
                    cmdBit = 0x01;
                    cmdIndex++;
                    cmdBuf[cmdIndex] = 0;
                } else {
                    cmdBit <<= 1;
                }
                if (iBit == 0x80) {
                    iBit = 0x01;
                    iIndex++;
                } else {
                    iBit <<= 1;
                }
                cmdBitCount++;
                nBits--;
            }

            // Send stashed TDI bits
            if (cmdBitCount > 0) {
                int cmdBytes = cmdBitCount / 8;
                rxBitCounts[rxBitCountIndex++] = cmdBitCount;
                if (cmdBitCount >= 8) {
                    rxBytesWanted += cmdBytes;
                    cmdBitCount -= cmdBytes * 8;

                    cmdByte(FTDI::FTDI_MPSSE_XFER_TDI_BYTES);
                    cmdByte(cmdBytes - 1);
                    cmdByte((cmdBytes - 1) >> 8);

                    for (int i = 0; i < cmdBytes; i++) {
                        cmdByte(cmdBuf[i]);
                    }
                }
                if (cmdBitCount) {
                    rxBytesWanted++;
                    cmdByte(FTDI::FTDI_MPSSE_XFER_TDI_BITS);
                    cmdByte(cmdBitCount - 1);
                    cmdByte(cmdBuf[cmdBytes]);
                }
            }
        } while ((nBits != 0) && (txCount + (cmdBitCount / 8)) < (usb->bulkOutRequestSize - 6));

        // Shift data
        int wr = usb->writeData(ioBuf.data(), txCount);
        int rd = usb->readData(rxBytesWanted);
        if (!wr || !rd) {
            return 0;
        }

        // Process received data
        rxIndex = 0;
        for (int i = 0; i < rxBitCountIndex; i++) {
            int rxBitCount = rxBitCounts[i];
            rxBit = (rxBitCount < 8) ? (1 << (8 - rxBitCount)) : 0x01;

            while (rxBitCount--) {
                if (tdoBit == 0x1) {
                    tdoBuf[tdoIndex] = 0;
                }
                if (usb->rxBuf[rxIndex] & rxBit) {
                    tdoBuf[tdoIndex] |= tdoBit;
                }
                if (rxBit == 0x80) {
                    rxBit = (rxBitCount < 8) ? (1 << (8 - rxBitCount)) : 0x01;
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
            spdlog::warn("consumed {} but supplied {}", rxIndex, rxBytesWanted);
        }
    }
    return 1;
}

int VncProtocol::fetch32(FILE *fp, uint32_t *value) {
    uint32_t v = 0;
    for (int i = 0; i < 32; i += 8) {
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

uint32_t VncProtocol::shift(FILE *fp) {
    uint32_t nBits, nBytes;
    if (!fetch32(fp, &nBits)) {
        return 0;
    }
    if (nBits > largestShiftRequest) {
        largestShiftRequest = nBits;
    }
    bitCount += nBits;
    shiftCount++;
    nBytes = (nBits + 7) / 8;
    if (flags->showXVC) {
        spdlog::info("shift: {}", nBits);
    }
    if (nBytes > XVC_BUFFER_SIZE) {
        spdlog::error("Client requested {}, max is {}", nBytes, XVC_BUFFER_SIZE);
        std::exit(EXIT_FAILURE);
    }
    if (fread(tmsBuf.data(), 1, nBytes, fp) != nBytes ||
        fread(tdiBuf.data(), 1, nBytes, fp) != nBytes) {
        return 0;
    }
    if (flags->showXVC) {
        Misc::showBuf("TMS", tmsBuf.data(), nBytes);
        Misc::showBuf("TDI", tdiBuf.data(), nBytes);
    }
    if (!shiftChunks(nBits)) {
        return 0;
    }
    if (flags->showXVC) {
        Misc::showBuf("TDO", tdoBuf.data(), nBytes);
    }
    if (flags->loopback) {
        if (std::memcmp(tdiBuf.data(), tdoBuf.data(), nBytes) != 0) {
            spdlog::error("Loopback failed.");
        }
    }
    return nBytes;
}

int VncProtocol::matchInput(FILE *fp, const char *str) {
    while (*str) {
        int c = fgetc(fp);
        if (c == EOF) {
            Misc::badEOF();
            return 0;
        }
        if (c != *str) {
            spdlog::error("Expected 0x{:02x}, got 0x{:02x}", *str, c);
            return 0;
        }
        str++;
    }
    return 1;
}

int VncProtocol::reply(int fd, const unsigned char *buf, uint32_t len) {
    if (write(fd, buf, len) != static_cast<ssize_t>(len)) {
        spdlog::error("reply failed: {}", strerror(errno));
        return 0;
    }
    return 1;
}

int VncProtocol::reply32(int fd, uint32_t value) {
    unsigned char cbuf[4];
    std::memcpy(cbuf, &value, sizeof(value));
    return reply(fd, cbuf, sizeof(cbuf));
}

void VncProtocol::processCommands(FILE *fp, int fd) {
    int c;
    while (true) {
        switch (c = std::fgetc(fp)) {
            case 's':
                switch (c = std::fgetc(fp)) {
                    case 'e': {
                        uint32_t num;
                        int frequency;
                        if (!matchInput(fp, "ttck:")) return;
                        if (!fetch32(fp, &num)) return;
                        frequency = FREQUENCY / num;
                        if (flags->showXVC) {
                            spdlog::info("settck: {} ({} Hz)", num, frequency);
                        }
                        if (!ftdi->setClockSpeed(frequency)) return;
                        if (!reply32(fd, num)) return;
                    }
                        break;

                    case 'h': {
                        uint32_t nBytes;
                        if (!matchInput(fp, "ift:")) return;
                        nBytes = shift(fp);
                        if ((nBytes <= 0) || !reply(fd, tdoBuf.data(), nBytes)) {
                            return;
                        }
                    }
                        break;

                    default:
                        if (flags->showXVC) {
                            spdlog::error("Bad second char 0x{:02x}", c);
                        }
                        Misc::badChar(c);
                        return;
                }
                break;

            case 'g':
                if (matchInput(fp, "etinfo:")) {
                    std::string cBuf = "xvcServer_v1.0:" + std::to_string(XVC_BUFFER_SIZE);
                    if (flags->showXVC) {
                        spdlog::info("getinfo: {}", cBuf);
                    }
                    cBuf += "\n";
                    if (reply(fd, reinterpret_cast<const unsigned char *>(cBuf.c_str()), cBuf.size())) {
                        break;
                    }
                }
                return;

            case EOF:
                return;

            default:
                if (flags->showXVC) {
                    spdlog::error("Bad initial char 0x{:02x}", c);
                }
                Misc::badChar(c);
                return;
        }
    }
}

void VncProtocol::set_zero() {
    shiftCount = 0;
    chunkCount = 0;
    bitCount = 0;
}

void VncProtocol::close() {
    printStatistic();
    usb->close();
}

int VncProtocol::connect() {
    if (!usb->connect() || !ftdi->init()) {
        return 0;
    }
    set_zero();
    return 1;
}

void VncProtocol::printStatistic() const {
    if (flags->statisticsFlag) {
        spdlog::info("   Shifts: {}", shiftCount);
        spdlog::info("   Chunks: {}", chunkCount);
        spdlog::info("     Bits: {}", bitCount);
        spdlog::info(" Largest shift request: {}", largestShiftRequest);
        spdlog::info(" Largest write request: {}", usb->largestWriteRequest);
        spdlog::info("Largest write transfer: {}", usb->largestWriteSent);
        spdlog::info("  Largest read request: {}", usb->largestReadRequest);
    }
}