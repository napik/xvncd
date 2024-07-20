#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include "misc.h"

void Misc::showBuf(const char *name, const unsigned char *buf, uint32_t numBytes) {
    std::ostringstream oss;
    oss << name << numBytes << ":";

    uint32_t limit = std::min(numBytes, MAX_BYTES_TO_SHOW);
    for (uint32_t i = 0; i < limit; ++i) {
        oss << " " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buf[i]);
    }
    spdlog::info(oss.str());
}

void Misc::badEOF() {
    spdlog::error("Unexpected EOF!");
}

void Misc::badChar(int c) {
    spdlog::error("Unexpected character! {}", (char) c);
}
