#ifndef XVNCD_CPP_XVNCD_H
#define XVNCD_CPP_XVNCD_H

#include "usb.h"
#include "FTDI.h"

#define FREQUENCY 1000000000

class VncProtocol {
public:
    static constexpr int XVC_BUFFER_SIZE = 1024;

    explicit VncProtocol(Config *flags);

    ~VncProtocol();

    void processCommands(FILE *fp, int fd);

    // Command to send a byte through FTDI
    void cmdByte(int byte);

    /*
     * The FTDI/JTAG chip can't shift data to TMS and TDI simultaneously,
     * so switch between TMS and TDI shifts commands as necessary.
     * Break into chunks small enough to fit in a single packet.
     */
    [[nodiscard]] int shiftChunks(uint32_t shiftBits);

    uint32_t shift(FILE *fp);

    std::unique_ptr<DiagnosticFlags> flags;

    void close();

    int connect();

    void printStatistic() const;

private:
    std::unique_ptr<USB> usb;
    std::unique_ptr<FTDI> ftdi;

    std::array<unsigned char, XVC_BUFFER_SIZE> tmsBuf{};
    std::array<unsigned char, XVC_BUFFER_SIZE> tdiBuf{};
    std::array<unsigned char, XVC_BUFFER_SIZE> tdoBuf{};
    std::array<unsigned char, XVC_BUFFER_SIZE> cmdBuf{};
    std::array<unsigned char, XVC_BUFFER_SIZE> ioBuf{};

    const int BIT_1 = 0x01;
    const int BIT_80 = 0x80;

    int txCount = 0;

    uint64_t shiftCount = 0;
    uint64_t chunkCount = 0;
    uint64_t bitCount = 0;

    uint32_t largestShiftRequest = 0;

    static int fetch32(FILE *fp, uint32_t *value);

    static int matchInput(FILE *fp, const char *str);

    static int reply(int fd, const unsigned char *buf, uint32_t len);

    static int reply32(int fd, uint32_t value);

    void set_zero();
};

#endif // XVNCD_CPP_XVNCD_H
