//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#ifndef XVNCD_CPP_MISC_H
#define XVNCD_CPP_MISC_H


class Misc {
public:
    static void showBuf(const char *name, const unsigned char *buf, uint32_t numBytes);

    static void badEOF();

    static void badChar();
};


#endif //XVNCD_CPP_MISC_H
