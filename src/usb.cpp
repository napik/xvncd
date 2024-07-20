#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <libusb.h>
#include <spdlog/spdlog.h>
#include "usb.h"

#include <iostream>

USB::USB() : vendorId(0x0403),
             productId(0x6014),
             serialNumber(nullptr) {
    config = Config::get();

    libusb_init_context(&usb_context, nullptr, 0);

    if (const int rc = libusb_hotplug_register_callback(usb_context,
                                                        LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                                        0,
                                                        static_cast<int>(vendorId), static_cast<int>(productId),
                                                        LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, nullptr,
                                                        &callback_handle); rc != LIBUSB_SUCCESS) {
        spdlog::info("Error creating a hotplug callback\n");
        libusb_exit(usb_context);
        std::exit(EXIT_FAILURE);
    }

    usb_handle_events();

    if (const int status = libusb_init(&usb_context); status < 0) {
        spdlog::error("Failed to initialize libusb: {}", libusb_strerror(status));
        std::exit(EXIT_FAILURE);
    }
}

USB::~USB() {
    isContinue = false;
    libusb_hotplug_deregister_callback(nullptr, callback_handle);
    libusb_exit(nullptr);
}

void USB::usb_handle_events() {
    auto f = [this] {
        while (isContinue) {
            libusb_handle_events_completed(usb_context, nullptr);
            static constexpr auto ms100 = std::chrono::duration<int, std::milli>(100);
            std::this_thread::sleep_for(ms100);
        }
    };

    callback_handle_thread = std::move(std::thread{f});
}

void USB::getDeviceString(const int index, std::string &dest) const {
    auto *pData = txBuf.buffer->data();
    ssize_t length =
            libusb_get_string_descriptor_ascii(dev_handle, index, pData, static_cast<int>(txBuf.buffer->size()));
    if (length < 0) {
        dest.clear();
        return;
    }
    length = std::min(length, static_cast<ssize_t>(ID_STRING_CAPACITY - 1));
    dest.assign(reinterpret_cast<const char *>(pData), length);
}

void USB::getDeviceStrings(const libusb_device_descriptor *desc) {
    getDeviceString(desc->iManufacturer, deviceVendorString);
    getDeviceString(desc->iProduct, deviceProductString);
    getDeviceString(desc->iSerialNumber, deviceSerialString);
}

void USB::getEndpoints(const libusb_interface_descriptor *iface_desc) {
    bulkInEndpointAddress = 0;
    bulkOutEndpointAddress = 0;

    for (int e = 0; e < iface_desc->bNumEndpoints; ++e) {
        if (const auto *ep = &iface_desc->endpoint[e];
            (ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
            if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
                if (bulkInEndpointAddress != 0) {
                    spdlog::error(ERROR_TOO_MANY_INPUT_ENDPOINTS);
                    std::exit(EXIT_FAILURE);
                }
                bulkInEndpointAddress = ep->bEndpointAddress;
                bulkInRequestSize = std::min(ep->wMaxPacketSize, static_cast<uint16_t>(txBuf.buffer->size()));
            } else {
                if (bulkOutEndpointAddress != 0) {
                    spdlog::error(ERROR_TOO_MANY_OUTPUT_ENDPOINTS);
                    std::exit(EXIT_FAILURE);
                }
                bulkOutEndpointAddress = ep->bEndpointAddress;
                bulkOutRequestSize = std::min(ep->wMaxPacketSize, static_cast<uint16_t>(USB_BUFFER_SIZE));
            }
        }
    }

    if (bulkInEndpointAddress == 0 || bulkOutEndpointAddress == 0) {
        spdlog::error(bulkInEndpointAddress == 0 ? ERROR_NO_INPUT_ENDPOINT : ERROR_NO_OUTPUT_ENDPOINT);
        std::exit(EXIT_FAILURE);
    }
}

int USB::hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data) {
    static libusb_device_handle *dev_handle = nullptr;

    libusb_device_descriptor desc{};
    (void) libusb_get_device_descriptor(dev, &desc);

    switch (event) {
        case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
            spdlog::debug("Connected USB device");

            if (const int rc = libusb_open(dev, &dev_handle); LIBUSB_SUCCESS != rc) {
                spdlog::error("Could not open USB device");
            }
            break;
        case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
            spdlog::debug("Disconnected USB device");

            if (dev_handle) {
                libusb_close(dev_handle);
                dev_handle = nullptr;
            }
            break;
        default:
            spdlog::error("Unhandled event {}", static_cast<int>(event));
            break;
    }

    return 0;
}

int USB::findDevice(libusb_device **list, ssize_t count) {
    for (ssize_t i = 0; i < count; ++i) {
        auto *dev = list[i];
        libusb_device_descriptor desc{};
        bool productMatch = false;

        int status = libusb_get_device_descriptor(dev, &desc);
        if (status < 0) {
            spdlog::error(ERROR_GET_DEVICE_DESCRIPTOR, libusb_strerror(status));
            return 0;
        }

        if (desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE) {
            continue;
        }

        if (productId == desc.idProduct) {
            productMatch = true;
        } else {
            productMatch = std::ranges::any_of(validCodes, [&desc](const uint16_t code) {
                return desc.idProduct == code;
            });
        }

        if (vendorId != desc.idVendor || !productMatch) {
            continue;
        }

        libusb_config_descriptor *libusb_config = nullptr;
        if ((libusb_get_active_config_descriptor(dev, &libusb_config) < 0) &&
            (libusb_get_config_descriptor(dev, 0, &libusb_config) < 0)) {
            spdlog::error(ERROR_GET_CONFIG_DESCRIPTOR, desc.idVendor, desc.idProduct);
            std::exit(2);
        }

        if (!libusb_config) {
            std::exit(2);
        }

        if (libusb_config->bNumInterfaces >= config->jtagIndex) {
            status = libusb_open(dev, &dev_handle);
            if (status != 0) {
                spdlog::error(ERROR_LIBUSB_OPEN, libusb_strerror(status));
                std::exit(EXIT_FAILURE);
            }
            const auto *iface = &libusb_config->interface[config->jtagIndex - 1];
            const auto *iface_desc = &iface->altsetting[0];
            if (!iface_desc) {
                spdlog::error(ERROR_LIBUSB_OPEN, libusb_strerror(status));
                std::exit(EXIT_FAILURE);
            }
            bInterfaceNumber = iface_desc->bInterfaceNumber;
            deviceVendorId = desc.idVendor;
            deviceProductId = desc.idProduct;
            getDeviceStrings(&desc);

            if (serialNumber == nullptr || std::strcmp(serialNumber, deviceSerialString.data()) == 0) {
                getEndpoints(iface_desc);
                libusb_free_config_descriptor(libusb_config);
                productId = desc.idProduct;
                return 1;
            }
            libusb_close(dev_handle);
        }
        libusb_free_config_descriptor(libusb_config);
    }
    return 0;
}

int USB::write_tx_buffer() {
    txBuf.buffer->resize(txCount);

    auto nSend = txCount;
    auto *buffer = txBuf.buffer->data();

    if (config->flags->showUSB) {
        txBuf.showBuf(nSend);
    }
    largestWriteRequest = std::max(largestWriteRequest, nSend);

    while (nSend > 0) {
        int transferred;
        if (const int status = libusb_bulk_transfer(dev_handle, bulkOutEndpointAddress, buffer, nSend, &transferred,
                                                    10000);
            status < 0) {
            spdlog::error(ERROR_USB_WRITE_FAILED, nSend, libusb_strerror(status));
            return 0;
        }
        nSend -= transferred;
        buffer += transferred;
        largestWriteSent = std::max(largestWriteSent, transferred);
    }
    return 1;
}

int USB::write_data(const std::vector<unsigned char> &data) {
    *txBuf.buffer = data;
    return write_tx_buffer();
}

int USB::read_data(const int bytes_to_read) {
    largestReadRequest = std::max(largestReadRequest, bytes_to_read);

    if (bytes_to_read + STATUS_BYTE_COUNT > bulkInRequestSize) {
        spdlog::error(ERROR_USB_READ_REQUEST_LIMIT, bytes_to_read + STATUS_BYTE_COUNT, bulkInRequestSize);
        std::exit(EXIT_FAILURE);
    }

    auto base = rxBuf.buffer->data();
    auto bytesRemaining = bytes_to_read;

    while (bytesRemaining > 0) {
        const int bytesToTransfer = std::min(bytesRemaining + STATUS_BYTE_COUNT, bulkInRequestSize);

        int bytesTransferred = 0;
        if (const auto status = libusb_bulk_transfer(dev_handle, bulkInEndpointAddress, txBuf.buffer->data(),
                                                     bytesToTransfer, &bytesTransferred, 5000); status < 0) {
            spdlog::error(ERROR_USB_READ_FAILED, libusb_strerror(status));
            std::exit(EXIT_FAILURE);
        }

        if (bytesTransferred < STATUS_BYTE_COUNT) {
            if (config->flags->runtFlag) {
                spdlog::warn(WARNING_USB_READ_LESS_THAN_STATUS_COUNT);
            }
            continue;
        }

        const auto dataBytes = bytesTransferred - STATUS_BYTE_COUNT;
        std::memcpy(base, txBuf.buffer->data() + STATUS_BYTE_COUNT, dataBytes);

        base += dataBytes;
        bytesRemaining -= dataBytes;
    }

    if (config->flags->showUSB) {
        rxBuf.showBuf(bytes_to_read);
    }

    return 1;
}

int USB::connect() {
    libusb_device **list;
    const auto n = libusb_get_device_list(usb_context, &list);
    if (n < 0) {
        spdlog::error(ERROR_LIBUSB_INIT, libusb_strerror(n));
        return 0;
    }

    int status = findDevice(list, n);
    libusb_free_device_list(list, 1);

    if (status) {
        status = libusb_kernel_driver_active(dev_handle, bInterfaceNumber);
        if (status < 0) {
            spdlog::error(ERROR_LIBUSB_KERNEL_DRIVER_ACTIVE, libusb_strerror(status));
        } else if (status) {
            status = libusb_detach_kernel_driver(dev_handle, bInterfaceNumber);
            if (status < 0) {
                spdlog::error(ERROR_LIBUSB_DETACH_KERNEL_DRIVER, libusb_strerror(status));
            }
        }
        status = libusb_claim_interface(dev_handle, bInterfaceNumber);
        if (status < 0) {
            libusb_close(dev_handle);
            spdlog::error(ERROR_LIBUSB_CLAIM_INTERFACE, libusb_strerror(status));
            return 0;
        }
        if (config->flags->showUSB || !config->flags->quietFlag) {
            spdlog::info(" Vendor ({}):  {}", vendorId, deviceVendorString.data());
            spdlog::info("Product ({}): {}", productId, deviceProductString.data());
            spdlog::info("      Serial: {}", deviceSerialString.data());
        }
    } else {
        spdlog::error(ERROR_NO_USB_DEVICE);
        return 0;
    }
    return 1;
}

void USB::close() {
    if (dev_handle) {
        libusb_close(dev_handle);
        dev_handle = nullptr;
    }
}

int USB::set_control(int bRequest, int wValue) const {
    if (config->flags->showUSB) {
        spdlog::info("setControl bmRequestType:{:02X} bRequest:{:02X} wValue:{:04X}", 64, bRequest, wValue);
    }

    if (const int result = libusb_control_transfer(dev_handle, 64, bRequest, wValue, config->jtagIndex, nullptr, 0,
                                                   1000);
        result < 0) {
        spdlog::error("usb_control_transfer failed: {}", libusb_strerror(result));
        return 0;
    }
    return 1;
}

void USB::cmdByte(const int byte) {
    if (txCount >= USB_BUFFER_SIZE) {
        spdlog::error("FTDI TX OVERFLOW!");
        std::exit(EXIT_FAILURE);
    }
    txCount++;
    txBuf.buffer->at(txCount) = byte;
}

bool USB::check(const int rxIndex, const int rxBit) const {
    return rxBuf.buffer->at(rxIndex) & rxBit;
}
