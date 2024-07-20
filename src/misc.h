#pragma once

#include <vector>

class Misc {
public:
    static void badEOF();

    static void badChar(int c);
};

class MyBuffer {
public:
    explicit MyBuffer(const std::string_view _name): name(_name) {
    }

    static constexpr int XVC_BUFFER_SIZE = 1024;

    std::unique_ptr<std::vector<unsigned char> > buffer = std::make_unique<std::vector<unsigned char> >(XVC_BUFFER_SIZE);

    void showBuf(uint32_t numBytes) const;

private:
    std::string_view name;

    static constexpr uint32_t MAX_BYTES_TO_SHOW = 40;
};
