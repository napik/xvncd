//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#include <cstdio>
#include <cstdint>
#include "misc.h"


void Misc::showBuf(const char *name, const unsigned char *buf, uint32_t numBytes) {
    int i;
    printf("%s%4d:", name, numBytes);
    if (numBytes > 40) numBytes = 40;
    for (i = 0; i < numBytes; i++) printf(" %02X", buf[i]);
    printf("\n");
}

void Misc::badEOF() {
    fprintf(stderr, "Unexpected EOF!\n");
}

void Misc::badChar() {
    fprintf(stderr, "Unexpected character!\n");
}
