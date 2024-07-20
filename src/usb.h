#pragma once

#include <libusb.h>
#include <array>
#include <string>
#include <thread>
#include "Config.h"
#include "misc.h"


class USB {
public:
    explicit USB();

    ~USB();

    static constexpr int USB_BUFFER_SIZE = 512;

    int connect();

    int write_data(const std::vector<unsigned char> &data);

    int write_tx_buffer();

    int read_data(int bytes_to_read);

    [[nodiscard]] bool check(int rxIndex, int rxBit) const;

    [[nodiscard]] int set_control(int bRequest, int wValue) const;

    void usb_handle_events();

    void close();

    int largestWriteSent{};
    int largestWriteRequest{};
    int largestReadRequest{};
    int bulkOutRequestSize{};

    int txCount = 0;

    void cmdByte(int byte);

private:
    uint32_t vendorId;
    uint32_t productId;
    const char *serialNumber;

    int deviceVendorId{};
    int deviceProductId{};
    std::string deviceVendorString{};
    std::string deviceProductString{};
    std::string deviceSerialString{};

    libusb_context *usb_context{};
    int bInterfaceNumber{};
    int isConnected{};
    int termChar{};
    unsigned char bTag{};
    int bulkOutEndpointAddress{};
    int bulkInEndpointAddress{};
    int bulkInRequestSize{};

    libusb_device_handle *dev_handle{};

    static constexpr int ID_STRING_CAPACITY = 100;

    static constexpr std::string_view ERROR_GET_DEVICE_DESCRIPTOR = "libusb_get_device_descriptor failed: {}";
    static constexpr std::string_view ERROR_NO_INPUT_ENDPOINT = "No input endpoint!";
    static constexpr std::string_view ERROR_NO_OUTPUT_ENDPOINT = "No output endpoint!";
    static constexpr std::string_view ERROR_TOO_MANY_INPUT_ENDPOINTS = "Too many input endpoints!";
    static constexpr std::string_view ERROR_TOO_MANY_OUTPUT_ENDPOINTS = "Too many output endpoints!";
    static constexpr std::string_view ERROR_NO_USB_DEVICE = "Can't find USB device.";
    static constexpr std::string_view ERROR_LIBUSB_INIT = "libusb_init() failed: {}";
    static constexpr std::string_view ERROR_LIBUSB_OPEN = "libusb_open failed: {}";
    static constexpr std::string_view ERROR_LIBUSB_KERNEL_DRIVER_ACTIVE = "libusb_kernel_driver_active() failed: {}";
    static constexpr std::string_view ERROR_LIBUSB_DETACH_KERNEL_DRIVER = "libusb_detach_kernel_driver() failed: {}";
    static constexpr std::string_view ERROR_LIBUSB_CLAIM_INTERFACE = "libusb_claim_interface failed: {}";
    static constexpr std::string_view ERROR_USB_READ_FAILED = "Bulk read failed: {}";
    static constexpr std::string_view ERROR_USB_WRITE_FAILED = "Bulk write {} failed: {}";
    static constexpr std::string_view ERROR_GET_CONFIG_DESCRIPTOR = "Can't get vendor {} product {} configuration.";
    static constexpr std::string_view ERROR_USB_READ_REQUEST_LIMIT = "USB read request size %d exceeds limit {}.";
    static constexpr std::string_view WARNING_USB_READ_LESS_THAN_STATUS_COUNT = "Received less than status byte count.";

    static constexpr std::array<uint16_t, 3> validCodes = {
        0x6010, // FT2232H
        0x6011, // FT4232H
        0x6014 // FT232H
    };

    const int STATUS_BYTE_COUNT = 2;

    MyBuffer txBuf{"Tx"};

    MyBuffer rxBuf{"Rx"};

    std::shared_ptr<Config> config;

    void getDeviceString(int index, std::string &dest) const;

    int findDevice(libusb_device **list, ssize_t count);

    void getDeviceStrings(const libusb_device_descriptor *desc);

    void getEndpoints(const libusb_interface_descriptor *iface_desc);

    static int hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data);

    libusb_hotplug_callback_handle callback_handle{};
    std::thread callback_handle_thread;
    volatile bool isContinue = true;
};
