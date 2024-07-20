// Created by Anton Rudnitskiy on 20.07.2024.

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "usb.h"
#include "misc.h"

void USB::getDeviceString(int i, char *dest) {
    ssize_t n = libusb_get_string_descriptor_ascii(handle, i, ioBuf, sizeof(ioBuf));
    if (n < 0) {
        *dest = '\0';
        return;
    }
    if (n >= ID_STRING_CAPACITY) {
        n = ID_STRING_CAPACITY - 1;
    }
    memcpy(dest, ioBuf, n);
    dest[n] = '\0';
}

void USB::getDeviceStrings(struct libusb_device_descriptor *desc) {
    getDeviceString(desc->iManufacturer, deviceVendorString);
    getDeviceString(desc->iProduct, deviceProductString);
    getDeviceString(desc->iSerialNumber, deviceSerialString);
}

void USB::getEndpoints(const struct libusb_interface_descriptor *iface_desc) {
    bulkInEndpointAddress = 0;
    bulkOutEndpointAddress = 0;

    for (int e = 0; e < iface_desc->bNumEndpoints; e++) {
        const struct libusb_endpoint_descriptor *ep = &iface_desc->endpoint[e];
        if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
            if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
                if (bulkInEndpointAddress != 0) {
                    fprintf(stderr, "Too many input endpoints!\n");
                    exit(EXIT_FAILURE);
                }
                bulkInEndpointAddress = ep->bEndpointAddress;
                bulkInRequestSize = ep->wMaxPacketSize;
                if (bulkInRequestSize > sizeof(ioBuf)) {
                    bulkInRequestSize = sizeof(ioBuf);
                }
            } else {
                if (bulkOutEndpointAddress != 0) {
                    fprintf(stderr, "Too many output endpoints!\n");
                    exit(EXIT_FAILURE);
                }
                bulkOutEndpointAddress = ep->bEndpointAddress;
                bulkOutRequestSize = ep->wMaxPacketSize;
                if (bulkOutRequestSize > sizeof(cmdBuf)) {
                    bulkOutRequestSize = sizeof(cmdBuf);
                }
            }
        }
    }

    if (bulkInEndpointAddress == 0) {
        fprintf(stderr, "No input endpoint!\n");
        exit(EXIT_FAILURE);
    }
    if (bulkOutEndpointAddress == 0) {
        fprintf(stderr, "No output endpoint!\n");
        exit(EXIT_FAILURE);
    }
}

int USB::findDevice(libusb_device **list, ssize_t n) {
    for (ssize_t i = 0; i < n; i++) {
        libusb_device *dev = list[i];
        struct libusb_device_descriptor desc{};
        int productMatch = 0;

        int s = libusb_get_device_descriptor(dev, &desc);
        if (s != 0) {
            fprintf(stderr, "libusb_get_device_descriptor failed: %s", libusb_strerror(s));
            return 0;
        }
        if (desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE) {
            continue;
        }

        if (productId < 0) {
            for (const auto &code: validCodes) {
                if (desc.idProduct == code) {
                    productMatch = 1;
                    break;
                }
            }
        } else if (productId == desc.idProduct) {
            productMatch = 1;
        }

        if (vendorId != desc.idVendor || !productMatch) {
            continue;
        }

        struct libusb_config_descriptor *config;
        if ((libusb_get_active_config_descriptor(dev, &config) < 0) &&
            (libusb_get_config_descriptor(dev, 0, &config) < 0)) {
            fprintf(stderr, "Can't get vendor %04X product %04X configuration.\n", desc.idVendor, desc.idProduct);
            continue;
        }

        if (config == nullptr) {
            continue;
        }

        if (config->bNumInterfaces >= flags.ftdiJTAGindex) {
            s = libusb_open(dev, &handle);
            if (s == 0) {
                const struct libusb_interface *iface = &config->interface[flags.ftdiJTAGindex - 1];
                const struct libusb_interface_descriptor *iface_desc = &iface->altsetting[0];
                bInterfaceNumber = iface_desc->bInterfaceNumber;
                deviceVendorId = desc.idVendor;
                deviceProductId = desc.idProduct;
                getDeviceStrings(&desc);

                if (serialNumber == nullptr || strcmp(serialNumber, deviceSerialString) == 0) {
                    getEndpoints(iface_desc);
                    libusb_free_config_descriptor(config);
                    productId = desc.idProduct;
                    return 1;
                }
                libusb_close(handle);
            } else {
                fprintf(stderr, "libusb_open failed: %s\n", libusb_strerror(s));
                exit(EXIT_FAILURE);
            }
        }
        libusb_free_config_descriptor(config);
    }
    return 0;
}

int USB::writeData(int nSend) {
    int nSent = 0, s;
    unsigned char *buffer = ioBuf;

    if (flags.showUSB) {
        Misc::showBuf("Tx", ioBuf, nSend);
    }
    if (nSend > largestWriteRequest) {
        largestWriteRequest = nSend;
    }

    while (nSend > 0) {
        s = libusb_bulk_transfer(handle, bulkOutEndpointAddress, buffer, nSend, &nSent, 10000);
        if (s) {
            fprintf(stderr, "Bulk write (%d) failed: %s\n", nSend, libusb_strerror(s));
            return 0;
        }
        nSend -= nSent;
        buffer += nSent;  // Increment the buffer pointer
        if (nSent > largestWriteSent) {
            largestWriteSent = nSent;
        }
    }
    return 1;
}

int USB::readData(unsigned char *buf, int nWant) {
    int nWanted = nWant;
    unsigned char *base = buf;

    if (nWant > largestReadRequest) {
        largestReadRequest = nWant;
        if ((nWant + 2) > bulkInRequestSize) {
            fprintf(stderr, "usbReadData requested %d, limit is %d.\n", nWant + 2, bulkInRequestSize);
            exit(EXIT_FAILURE);
        }
    }

    while (nWant > 0) {
        int nRecv, s;
        const unsigned char *src = ioBuf;

        s = libusb_bulk_transfer(handle, bulkInEndpointAddress, ioBuf, nWant + 2, &nRecv, 5000);
        if (s) {
            fprintf(stderr, "Bulk read failed: %s\n", libusb_strerror(s));
            exit(EXIT_FAILURE);
        }
        if (nRecv <= 2) {
            if (flags.runtFlag) {
                fprintf(stderr, "wanted:%d want:%d got:%d", nWanted, nWant, nRecv);
                if (nRecv >= 1) {
                    fprintf(stderr, " [%02X", src[0]);
                    if (nRecv >= 2) {
                        fprintf(stderr, " %02X", src[1]);
                    }
                    fprintf(stderr, "]");
                }
                fprintf(stderr, "\n");
            }
            continue;
        } else {
            nRecv -= 2; // Skip FTDI status bytes
            src += 2;
        }
        if (nRecv > nWant) {
            nRecv = nWant;
        }
        memcpy(buf, src, nRecv);
        nWant -= nRecv;
        buf += nRecv;
    }

    if (flags.showUSB) {
        Misc::showBuf("Rx", base, nWanted);
    }
    return 1;
}

int USB::connectUSB() {
    libusb_device **list;
    ssize_t n = libusb_get_device_list(usb, &list);
    if (n < 0) {
        fprintf(stderr, "libusb_get_device_list failed: %s", libusb_strerror((int) n));
        return 0;
    }

    int s = findDevice(list, n);
    libusb_free_device_list(list, 1);
    if (s) {
        s = libusb_kernel_driver_active(handle, bInterfaceNumber);
        if (s < 0) {
            fprintf(stderr, "libusb_kernel_driver_active() failed: %s\n", libusb_strerror(s));
        } else if (s) {
            s = libusb_detach_kernel_driver(handle, bInterfaceNumber);
            if (s) {
                fprintf(stderr, "libusb_detach_kernel_driver() failed: %s\n", libusb_strerror(s));
            }
        }
        s = libusb_claim_interface(handle, bInterfaceNumber);
        if (s) {
            libusb_close(handle);
            fprintf(stderr, "libusb_claim_interface failed: %s\n", libusb_strerror(s));
            return 0;
        }
        if (flags.showUSB || !flags.quietFlag) {
            printf(" Vendor (%04X): \"%s\"\n", vendorId, deviceVendorString);
            printf("Product (%04X): \"%s\"\n", productId, deviceProductString);
            printf("        Serial: \"%s\"\n", deviceSerialString);
            fflush(stdout);
        }
    } else {
        fprintf(stderr, "Can't find USB device.\n");
        return 0;
    }
    return 1;
}

void USB::deviceConfig(const char *str) {
    char *endp;
    unsigned long vendor = strtoul(str, &endp, 16);
    if ((endp != str) && (*endp == ':')) {
        str = endp + 1;
        unsigned long product = strtoul(str, &endp, 16);
        if ((endp != str) && ((*endp == '\0') || (*endp == ':')) && (vendor <= 0xFFFF) && (product <= 0xFFFF)) {
            if (*endp == ':') {
                serialNumber = endp + 1;
            }
            vendorId = vendor;
            productId = product;
            return;
        }
    }
    fprintf(stderr, "Bad -d vendor:product[:[serial]]\n");
    exit(EXIT_FAILURE);
}

void USB::clockSpeed(const char *str) {
    char *endp;
    double frequency = strtod(str, &endp);
    if ((endp == str) || ((*endp != '\0') && (*endp != 'M') && (*endp != 'k')) ||
        ((*endp != '\0') && (*(endp + 1) != '\0'))) {
        fprintf(stderr, "Bad clock frequency argument.\n");
        exit(EXIT_FAILURE);
    }
    if (*endp == 'M') frequency *= 1000000;
    if (*endp == 'k') frequency *= 1000;
    if (frequency >= INT_MAX) frequency = INT_MAX;
    if (frequency <= 0) frequency = 1;
    auto uint_frequency = static_cast<unsigned int>(frequency);
    flags.lockedSpeed = uint_frequency;
//    ftdi->divisorForFrequency(uint_frequency);
}

void USB::init() {
    int s = libusb_init(&usb);
    if (s != 0) {
        fprintf(stderr, "libusb_init() failed: %s\n", libusb_strerror(s));
        exit(EXIT_FAILURE);
    }
}
