//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#ifndef XVNCD_CPP_MISC_H
#define XVNCD_CPP_MISC_H


class Misc {
public:
    static void showBuf(const char *name, const unsigned char *buf, uint32_t numBytes);

    static void badEOF();

    static void badChar(int i);

private:
    static constexpr uint32_t MAX_BYTES_TO_SHOW = 40;
};


#endif //XVNCD_CPP_MISC_H
