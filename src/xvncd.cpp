#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <print>
#include "xvncd.h"
#include "misc.h"


VncProtocol::VncProtocol(): ftdi(std::make_unique<FTDI>()), fd(0) {
    const auto config = Config::get();
    flags = config->flags.get();
}

VncProtocol::~VncProtocol() {
    close();
}

int VncProtocol::do_get_info() const {
    if (flags->showXVC) {
        spdlog::info("getinfo: {}", VERSION);
    }
    std::vector<unsigned char> cBuf{};
    for (const char ch: VERSION) {
        cBuf.emplace_back(static_cast<unsigned char>(ch));
    }
    cBuf.emplace_back(static_cast<unsigned char>('\n'));
    return reply(cBuf);
}

int VncProtocol::shiftChunks(const uint32_t shiftBits) {
    uint32_t nBits = shiftBits;
    unsigned char iBit = 0x01;
    int iIndex = 0;
    int cmdBitCount;
    int tmsBit;
    int tdoBit = 0x01;
    int tdoIndex = 0;
    std::vector<unsigned short> rxBitCounts((USB::USB_BUFFER_SIZE / 3) + 1);

    if (flags->loopback) {
        ftdi->enable_loopback();
    }

    while (nBits) {
        int rxBytesWanted = 0;
        int rxBitCountIndex = 0;

        ftdi->usb->txCount = 0;
        chunkCount++;

        do {
            const int tdiFirstState = (tdiBuf.buffer->at(iIndex) & iBit) != 0;
            cmdBitCount = 0;
            int cmdBit = 0x01;
            int tmsBits = 0;
            do {
                tmsBit = (tmsBuf.buffer->at(iIndex) & iBit) ? cmdBit : 0;
                tmsBits |= tmsBit;
                if (iBit == 0x80) {
                    iBit = 0x01;
                    iIndex++;
                } else {
                    iBit <<= 1;
                }
                cmdBitCount++;
                cmdBit <<= 1;
            } while (cmdBitCount < 6 && cmdBitCount < nBits && (tdiBuf.buffer->at(iIndex) & iBit) != 0 == tdiFirstState)
            ;

            // Duplicate the final TMS bit
            tmsBits |= (tmsBit << 1);
            const int tmsState = tmsBit != 0;

            // Send the TMS bits and TDI value
            ftdi->set_tms_bits(cmdBitCount, (tdiFirstState << 7) | tmsBits);

            rxBitCountIndex++;
            rxBitCounts[rxBitCountIndex] = cmdBitCount;

            rxBytesWanted++;
            nBits -= cmdBitCount;

            // Stash TDI bits until bit limit reached or TMS change of state
            cmdBitCount = 0;
            int cmdIndex = 0;
            cmdBit = 0x01;
            cmdBuf.buffer->at(0) = 0;

            while (nBits > 0 &&
                   ((tmsBuf.buffer->at(iIndex) & iBit) == tmsState) &&
                   (ftdi->usb->txCount + (cmdBitCount / 8)) < (ftdi->usb->bulkOutRequestSize - 5)) {
                if (tdiBuf.buffer->at(iIndex) & iBit) {
                    cmdBuf.buffer->at(cmdIndex) |= cmdBit;
                }
                if (cmdBit == 0x80) {
                    cmdBit = 0x01;
                    cmdIndex++;
                    cmdBuf.buffer->at(cmdIndex) = 0;
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
                rxBitCountIndex++;
                rxBitCounts[rxBitCountIndex] = cmdBitCount;

                const int cmdBytes = cmdBitCount / 8;
                if (cmdBitCount >= 8) {
                    rxBytesWanted += cmdBytes;
                    cmdBitCount -= cmdBytes * 8;

                    ftdi->set_tdi_bytes(cmdBytes);

                    for (int i = 0; i < cmdBytes; i++) {
                        ftdi->cmd_byte(cmdBuf.buffer->at(i));
                    }
                }
                if (cmdBitCount) {
                    rxBytesWanted++;
                    ftdi->set_tdi_bits(cmdBitCount, cmdBuf.buffer->at(cmdBytes));
                }
            }
        } while (nBits != 0 && ftdi->usb->txCount + cmdBitCount / 8 < (ftdi->usb->bulkOutRequestSize - 6));

        // Shift data
        if (const int wr = ftdi->usb->write_tx_buffer(); !wr) {
            return 0;
        }

        if (const int rd = ftdi->usb->read_data(rxBytesWanted); !rd) {
            return 0;
        }

        // Process received data
        int rxIndex = 0;
        for (int i = 0; i <= rxBitCountIndex; i++) {
            int rxBitCount = rxBitCounts[i];
            int rxBit = (rxBitCount < 8) ? (1 << (8 - rxBitCount)) : 0x01;

            while (rxBitCount--) {
                if (tdoBit == 0x1) {
                    tdoBuf.buffer->at(tdoIndex) = 0;
                }
                if (ftdi->usb->check(rxIndex, rxBit)) {
                    tdoBuf.buffer->at(tdoIndex) |= tdoBit;
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

int VncProtocol::fetch32(uint32_t *value) const {
    uint32_t v = 0;
    for (int i = 0; i < 32; i += 8) {
        const int c = fgetc(fp);
        if (c == EOF) {
            Misc::badEOF();
            return 0;
        }
        v |= c << i;
    }
    *value = v;
    return 1;
}

uint32_t VncProtocol::shift() {
    uint32_t nBits;
    if (!fetch32(&nBits)) {
        return 0;
    }
    if (nBits > largestShiftRequest) {
        largestShiftRequest = nBits;
    }
    bitCount += nBits;
    shiftCount++;

    if (flags->showXVC) {
        spdlog::info("shift: {}", nBits);
    }

    uint32_t nBytes = (nBits + 7) / 8;
    if (nBytes > MyBuffer::XVC_BUFFER_SIZE) {
        spdlog::error("Client requested {}, max is {}", nBytes, MyBuffer::XVC_BUFFER_SIZE);
        std::exit(EXIT_FAILURE);
    }
    if (fread(tmsBuf.buffer->data(), 1, nBytes, fp) != nBytes ||
        fread(tdiBuf.buffer->data(), 1, nBytes, fp) != nBytes) {
        return 0;
    }
    if (flags->showXVC) {
        tmsBuf.showBuf(nBytes);
        tdiBuf.showBuf(nBytes);
    }
    if (!shiftChunks(nBits)) {
        return 0;
    }
    if (flags->showXVC) {
        tdoBuf.showBuf(nBytes);
    }
    if (flags->loopback && std::memcmp(tdiBuf.buffer->data(), tdoBuf.buffer->data(), nBytes) != 0) {
        spdlog::error("Loopback failed.");
    }
    return nBytes;
}

int VncProtocol::matchInput(const char *str) const {
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

int VncProtocol::reply(const std::vector<unsigned char> &buf) const {
    if (write(fd, buf.data(), buf.size()) != buf.size()) {
        spdlog::error("reply failed: {}", strerror(errno));
        return 0;
    }
    return 1;
}

int VncProtocol::reply32(const uint32_t value) const {
    std::vector<unsigned char> cbuf(4);
    std::memcpy(cbuf.data(), &value, sizeof(value));
    return reply(cbuf);
}

bool VncProtocol::do_set_tck() const {
    uint32_t num;
    if (!fetch32(&num)) return true;
    uint32_t frequency = FREQUENCY / num;
    if (flags->showXVC) {
        spdlog::info("settck: {} ({} Hz)", num, frequency);
    }
    if (!ftdi->set_clock_speed(frequency)) return true;
    if (!reply32(num)) return true;
    return false;
}

bool VncProtocol::do_shift() {
    if (const uint32_t nBytes = shift(); nBytes <= 0) {
        return true;
    } else {
        tdoBuf.buffer->resize(nBytes);
        if (!reply(*tdoBuf.buffer)) {
            return true;
        }
    }
    return false;
}

bool VncProtocol::do_process_s(int &c) {
    switch (c = std::fgetc(fp)) {
        case 'e': {
            if (!matchInput("ttck:")) return true;
            if (do_set_tck()) return true;
        }
        break;

        case 'h': {
            if (!matchInput("ift:")) return true;
            if (do_shift()) return true;
        }
        break;

        default:
            if (flags->showXVC) {
                spdlog::error("Bad second char 0x{:02x}", c);
            }
            Misc::badChar(c);
            return true;
    }
    return false;
}

void VncProtocol::processCommands() {
    int c;
    while (true) {
        switch (c = std::fgetc(fp)) {
            case 's':
                if (do_process_s(c)) return;
                break;

            case 'g':
                if (!matchInput("etinfo:")) return;
                if (do_get_info()) break;
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

void VncProtocol::close() const {
    printStatistic();
    ftdi->close();
}

bool VncProtocol::connect(FILE *_fp, const int _fd) {
    fp = _fp;
    fd = _fd;
    if (!ftdi->init()) {
        return false;
    }
    set_zero();
    return true;
}

void VncProtocol::printStatistic() const {
    if (flags->statisticsFlag) {
        spdlog::info("   Shifts: {}", shiftCount);
        spdlog::info("   Chunks: {}", chunkCount);
        spdlog::info("     Bits: {}", bitCount);
        spdlog::info(" Largest shift request: {}", largestShiftRequest);
        spdlog::info(" Largest write request: {}", ftdi->usb->largestWriteRequest);
        spdlog::info("Largest write transfer: {}", ftdi->usb->largestWriteSent);
        spdlog::info("  Largest read request: {}", ftdi->usb->largestReadRequest);
    }
}

const std::string VncProtocol::VERSION = concat_version();

bool VncProtocol::isQuietMode() const {
    return flags->quietFlag;
}
