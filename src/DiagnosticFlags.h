#ifndef XVNCD_CPP_DIAGNOSTICFLAGS_H
#define XVNCD_CPP_DIAGNOSTICFLAGS_H

#include <string>
#include <cstdint>

struct DiagnosticFlags {
public:
    bool quietFlag = false;
    bool runtFlag = false;
    bool showUSB = false;
    bool showXVC = false;
    bool statisticsFlag = false;
    bool loopback = false;
};
#endif //XVNCD_CPP_DIAGNOSTICFLAGS_H
