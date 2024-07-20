#include <spdlog/spdlog.h>
#include "misc.h"


void Misc::badEOF() {
    spdlog::error("Unexpected EOF!");
}

void Misc::badChar(int c) {
    spdlog::error("Unexpected character! {}", static_cast<char>(c));
}

void MyBuffer::showBuf(uint32_t numBytes) const {
    const uint32_t limit = std::min(numBytes, MAX_BYTES_TO_SHOW);

    std::string result = std::format("{}{}:", name, numBytes);
    for (uint32_t i = 0; i < limit; ++i) {
        result += std::format(" {:02x}", static_cast<int>(buffer->at(i)));
    }
    spdlog::debug(result);
}
