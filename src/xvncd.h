#pragma once

#include "usb.h"
#include "FTDI.h"


class VncProtocol {
public:
    explicit VncProtocol();

    ~VncProtocol();

    void processCommands();

    /*
     * The FTDI/JTAG chip can't shift data to TMS and TDI simultaneously,
     * so switch between TMS and TDI shifts commands as necessary.
     * Break into chunks small enough to fit in a single packet.
     */
    [[nodiscard]] int shiftChunks(uint32_t shiftBits);

    uint32_t shift();

    [[nodiscard]] bool isQuietMode() const;

    void close() const;

    bool connect(FILE *_fp, int _fd);

    void printStatistic() const;

private:
    DiagnosticFlags *flags;
    std::unique_ptr<FTDI> ftdi;

    MyBuffer tmsBuf{"TMS"};
    MyBuffer tdiBuf{"TDI"};
    MyBuffer tdoBuf{"TDO"};
    MyBuffer cmdBuf{"CMD"};

    const int BIT_1 = 0x01;
    const int BIT_80 = 0x80;

    uint64_t shiftCount = 0;
    uint64_t chunkCount = 0;
    uint64_t bitCount = 0;

    uint32_t largestShiftRequest = 0;

    int fetch32(uint32_t *value) const;

    int matchInput(const char *str) const;

    [[nodiscard]] int reply(const std::vector<unsigned char> &buf) const;

    [[nodiscard]] int reply32(uint32_t value) const;

    [[nodiscard]] int do_get_info() const;

    [[nodiscard]] bool do_set_tck() const;

    [[nodiscard]] bool do_shift();

    [[nodiscard]] bool do_process_s(int &c);

    void set_zero();

    static constexpr uint32_t FREQUENCY = 1000000000;

    static constexpr std::string concat_version() {
        return std::format("xvcServer_v1.0:{}", 2048);
    }

    static const std::string VERSION;

    FILE *fp{};
    int fd;
};
