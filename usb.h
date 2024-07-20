//
// Created by Anton Rudnitskiy on 20.07.2024.
//

#include <libusb.h>

#ifndef XVNCD_CPP_USB_H
#define XVNCD_CPP_USB_H

#define XVC_BUFFER_SIZE         1024
#define USB_BUFFER_SIZE         512
#define ID_STRING_CAPACITY   100


/* FTDI commands (first byte of bulk write transfer) */
#define FTDI_MPSSE_BIT_WRITE_TMS                0x40
#define FTDI_MPSSE_BIT_READ_DATA                0x20
#define FTDI_MPSSE_BIT_WRITE_DATA               0x10
#define FTDI_MPSSE_BIT_LSB_FIRST                0x08
#define FTDI_MPSSE_BIT_READ_ON_FALLING_EDGE     0x04
#define FTDI_MPSSE_BIT_BIT_MODE                 0x02
#define FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE    0x01
#define FTDI_MPSSE_XFER_TDI_BYTES (FTDI_MPSSE_BIT_WRITE_DATA | \
                                   FTDI_MPSSE_BIT_READ_DATA  | \
                                   FTDI_MPSSE_BIT_LSB_FIRST  | \
                                   FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE)
#define FTDI_MPSSE_XFER_TDI_BITS (FTDI_MPSSE_BIT_WRITE_DATA | \
                                  FTDI_MPSSE_BIT_READ_DATA  | \
                                  FTDI_MPSSE_BIT_LSB_FIRST  | \
                                  FTDI_MPSSE_BIT_BIT_MODE   | \
                                  FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE)
#define FTDI_MPSSE_XFER_TMS_BITS (FTDI_MPSSE_BIT_WRITE_TMS  | \
                                  FTDI_MPSSE_BIT_READ_DATA  | \
                                  FTDI_MPSSE_BIT_LSB_FIRST  | \
                                  FTDI_MPSSE_BIT_BIT_MODE   | \
                                  FTDI_MPSSE_BIT_WRITE_ON_FALLING_EDGE)


class Flags {
public:
    /*
* Diagnostics
*/
    int quietFlag;
    int runtFlag;
    int showUSB;
    unsigned int lockedSpeed;
    int statisticsFlag;

    /*
 * FTDI info
 */
    int ftdiJTAGindex;
    const char *gpioArgument;

    int showXVC;
    int loopback;
};

class USB {
public:
    explicit USB(Flags *flags) {
        this->vendorId = 0x0403;
        this->productId = 0x6014;
        this->flags = *flags;
    }

    Flags flags{};

    /*
     * Statistics
     */
    int largestShiftRequest{};
    int largestWriteRequest{};
    int largestWriteSent{};
    int largestReadRequest{};
    uint64_t shiftCount{};
    uint64_t chunkCount{};
    uint64_t bitCount{};

    /*
     * Used to find a matching device
     */
    uint32_t vendorId;
    uint32_t productId;
    const char *serialNumber{};

    /*
     * Matched device
     */
    int deviceVendorId{};
    int deviceProductId{};
    char deviceVendorString[ID_STRING_CAPACITY]{};
    char deviceProductString[ID_STRING_CAPACITY]{};
    char deviceSerialString[ID_STRING_CAPACITY]{};

    /*
     * Libusb hooks
     */
    libusb_context *usb{};
    libusb_device_handle *handle{};
    int bInterfaceNumber{};
    int isConnected{};
    int termChar{};
    unsigned char bTag{};
    int bulkOutEndpointAddress{};
    int bulkOutRequestSize{};
    int bulkInEndpointAddress{};
    int bulkInRequestSize{};

    /*
     * I/O buffers
     */
    unsigned char tmsBuf[XVC_BUFFER_SIZE]{};
    unsigned char tdiBuf[XVC_BUFFER_SIZE]{};
    unsigned char tdoBuf[XVC_BUFFER_SIZE]{};
    int txCount{};
    unsigned char ioBuf[USB_BUFFER_SIZE]{};
    unsigned char rxBuf[USB_BUFFER_SIZE]{};
    unsigned char cmdBuf[USB_BUFFER_SIZE]{};

    int connectUSB();

    int writeData(int nSend);

    int readData(unsigned char *buf, int nWant);

    void deviceConfig(const char *str);

    void clockSpeed(const char *str);

    void init();

private:
    constexpr static const uint16_t validCodes[] = {0x6010, /* FT2232H */
                                                    0x6011, /* FT4232H */
                                                    0x6014  /* FT232H  */
    };

    void getDeviceString(int i, char *dest);

    int findDevice(libusb_device **list, ssize_t n);

    void getDeviceStrings(libusb_device_descriptor *desc);

    void getEndpoints(const libusb_interface_descriptor *iface_desc);

};

#endif //XVNCD_CPP_USB_H
