#ifndef XVNCD_CPP_USB_H
#define XVNCD_CPP_USB_H

#include <libusb.h>
#include "Config.h"
#include <array>
#include <string>

class USB {
public:
    explicit USB(Config *flags);

    static constexpr int USB_BUFFER_SIZE = 512;
    static constexpr int ID_STRING_CAPACITY = 100;

    uint32_t vendorId;
    uint32_t productId;
    const char *serialNumber;

    int deviceVendorId{};
    int deviceProductId{};
    std::string deviceVendorString{};
    std::string deviceProductString{};
    std::string deviceSerialString{};

    libusb_context *usb{};
    int bInterfaceNumber{};
    int isConnected{};
    int termChar{};
    unsigned char bTag{};
    int bulkOutEndpointAddress{};
    int bulkOutRequestSize{};
    int bulkInEndpointAddress{};
    int bulkInRequestSize{};

    std::array<unsigned char, USB::USB_BUFFER_SIZE> rxBuf{};

    int connect();

    int writeData(const unsigned char data[], size_t nSend);

    int readData(int nWant);

    int largestWriteSent{};
    int largestWriteRequest{};
    int largestReadRequest{};

    void close();

    [[nodiscard]] int setControl(int bRequest, int wValue) const;

private:
    libusb_device_handle *handle{};

    static constexpr const char *ERROR_GET_DEVICE_DESCRIPTOR = "libusb_get_device_descriptor failed: {}";
    static constexpr const char *ERROR_NO_INPUT_ENDPOINT = "No input endpoint!";
    static constexpr const char *ERROR_NO_OUTPUT_ENDPOINT = "No output endpoint!";
    static constexpr const char *ERROR_TOO_MANY_INPUT_ENDPOINTS = "Too many input endpoints!";
    static constexpr const char *ERROR_TOO_MANY_OUTPUT_ENDPOINTS = "Too many output endpoints!";
    static constexpr const char *ERROR_NO_USB_DEVICE = "Can't find USB device.";
    static constexpr const char *ERROR_LIBUSB_INIT = "libusb_init() failed: {}";
    static constexpr const char *ERROR_LIBUSB_OPEN = "libusb_open failed: {}";
    static constexpr const char *ERROR_LIBUSB_KERNEL_DRIVER_ACTIVE = "libusb_kernel_driver_active() failed: {}";
    static constexpr const char *ERROR_LIBUSB_DETACH_KERNEL_DRIVER = "libusb_detach_kernel_driver() failed: {}";
    static constexpr const char *ERROR_LIBUSB_CLAIM_INTERFACE = "libusb_claim_interface failed: {}";
    static constexpr const char *ERROR_USB_READ_FAILED = "Bulk read failed: {}";
    static constexpr const char *ERROR_USB_WRITE_FAILED = "Bulk write {} failed: {}";
    static constexpr const char *ERROR_GET_CONFIG_DESCRIPTOR = "Can't get vendor {} product {} configuration.";
    static constexpr const char *ERROR_USB_READ_REQUEST_LIMIT = "USB read request size %d exceeds limit {}.";
    static constexpr const char *WARNING_USB_READ_LESS_THAN_STATUS_COUNT = "Received less than status byte count.";

    static constexpr std::array<uint16_t, 3> validCodes = {
            0x6010, // FT2232H
            0x6011, // FT4232H
            0x6014  // FT232H
    };

    const int STATUS_BYTE_COUNT = 2;

    std::array<unsigned char, USB_BUFFER_SIZE> ioBuf{};

    Config *config;

    void getDeviceString(int index, std::string &dest);

    int findDevice(libusb_device **list, ssize_t count);

    void getDeviceStrings(const libusb_device_descriptor *desc);

    void getEndpoints(const libusb_interface_descriptor *iface_desc);
};

#endif // XVNCD_CPP_USB_H
