#include "adk.hpp"

#include <fmt/ostream.h>

#include <chrono>
#include <span>
#include <thread>

using namespace std::literals;

std::vector<std::unique_ptr<AndroidOpenAccessory>> AndroidOpenAccessory::GetDevices(
    const char* manufacturer, const char* model_name, const char* description, const char* version,
    const char* uri, const char* serial_number) {
    if (!context) {
        libusb_init(&context);
        libusb_set_debug(context, 3);
    }

    std::vector<std::unique_ptr<AndroidOpenAccessory>> android_devices;
    libusb_device** devices;
    const ssize_t device_count = libusb_get_device_list(context, &devices);

    for (ssize_t i = 0; i < device_count; ++i) {
        libusb_device* device = devices[i];
        std::unique_ptr<AndroidOpenAccessory> android_device =
            Connect(manufacturer, model_name, description, version, uri, serial_number, device);
        if (android_device) { android_devices.emplace_back(std::move(android_device)); }
    }

    libusb_free_device_list(devices, false);

    return android_devices;
}

std::unique_ptr<AndroidOpenAccessory> AndroidOpenAccessory::Connect(
    const char* manufacturer, const char* model_name, const char* description, const char* version,
    const char* uri, const char* serial_number, libusb_device* device) {

    auto accessory = std::make_unique<AndroidOpenAccessory>();

    accessory->manufacturer = manufacturer;
    accessory->model_name = model_name;
    accessory->description = description;
    accessory->version = version;
    accessory->uri = uri;
    accessory->serial_number = serial_number;
    accessory->device = device;

    libusb_device_descriptor descriptor;
    if (libusb_get_device_descriptor(device, &descriptor) < 0) return {};
    accessory->vid = descriptor.idVendor;

    if (PID_ACCESSORY == descriptor.idProduct) {
        if (libusb_open(device, &accessory->handle) < 0) return {};
        if (libusb_claim_interface(accessory->handle, 0) < 0) return {};
    } else {
        libusb_device_handle* device_handle;
        if (libusb_open(device, &device_handle) < 0) return {};
        bool connected = accessory->SwitchModes(device_handle);
        libusb_close(device_handle);
        if (!connected) return {};
    }

    if (!accessory->FindEndpoints()) return {};

    return accessory;
}

bool AndroidOpenAccessory::SwitchModes(libusb_device_handle* device_handle) {
    unsigned char io_buffer[2];
    int dev_version;
    if (libusb_claim_interface(device_handle, 0) < 0) return false;
    if (libusb_control_transfer(device_handle, 0xC0, 51, 0, 0, io_buffer, 2, 0) < 0) return false;

    dev_version = io_buffer[1] << 8 | io_buffer[0];
    fmt::print("Verion Code Device: {}\n", dev_version);

    std::this_thread::sleep_for(1000us); // sometimes hangs on the next transfer :(

    if (libusb_control_transfer(device_handle, 0x40, 52, 0, 0, (unsigned char*)manufacturer,
                                strlen(manufacturer), 0) < 0)
        return false;
    if (libusb_control_transfer(device_handle, 0x40, 52, 0, 1, (unsigned char*)model_name,
                                strlen(model_name) + 1, 0) < 0)
        return false;
    if (libusb_control_transfer(device_handle, 0x40, 52, 0, 2, (unsigned char*)description,
                                strlen(description) + 1, 0) < 0)
        return false;
    if (libusb_control_transfer(device_handle, 0x40, 52, 0, 3, (unsigned char*)version,
                                strlen(version) + 1, 0) < 0)
        return false;
    if (libusb_control_transfer(device_handle, 0x40, 52, 0, 4, (unsigned char*)uri, strlen(uri) + 1,
                                0) < 0)
        return false;
    if (libusb_control_transfer(device_handle, 0x40, 52, 0, 5, (unsigned char*)serial_number,
                                strlen(serial_number) + 1, 0) < 0)
        return false;

    fmt::print("Accessory Identification sent\n");

    if (libusb_control_transfer(device_handle, 0x40, 53, 0, 0, nullptr, 0, 0) < 0) return false;

    fmt::print("Attempted to put device into accessory mode\n");

    if (libusb_release_interface(device_handle, 0) < 0) return false;

    return ConnectToAccessory();
}

bool AndroidOpenAccessory::ConnectToAccessory() {
    int tries = 5;
    for (;;) {
        tries--;
        if ((handle = libusb_open_device_with_vid_pid(NULL, vid, PID_ACCESSORY)) == NULL) {
            if (tries < 0) return {};
        } else {
            break;
        }
        fmt::print("Attempting to connect to device, {} tries left\n", tries);
        std::this_thread::sleep_for(1s);
    }
    if (handle) libusb_claim_interface(handle, 0);
    return handle;
}

bool AndroidOpenAccessory::FindEndpoints() {
    libusb_config_descriptor* con_desc;
    if (libusb_get_active_config_descriptor(device, &con_desc) < 0) return false;
    const libusb_interface* interfaceList = con_desc->interface;
    uint16_t numInterface = con_desc->bNumInterfaces;

    // accessory interface is always 0
    libusb_interface interface = con_desc->interface[0];
    const libusb_interface_descriptor* intDescList = interface.altsetting;
    int numAlt = interface.num_altsetting;
    for (int p = 0; p < numAlt; p++) {
        libusb_interface_descriptor intDesc = intDescList[p];
        uint8_t numEnd = intDesc.bNumEndpoints;
        const libusb_endpoint_descriptor* ends = intDesc.endpoint;
        for (int k = 0; k < numEnd; k++) {
            libusb_endpoint_descriptor endpoint = ends[k];
            uint8_t type = 0x03 & endpoint.bmAttributes;
            uint8_t address = endpoint.bEndpointAddress;
            fmt::print("Endpoint {:#04X}\n Attribs  {:#04X}\n", address, endpoint.bmAttributes);
            switch (type) {
            case LIBUSB_TRANSFER_TYPE_CONTROL: break;
            case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS: break;
            case LIBUSB_TRANSFER_TYPE_BULK:
                if (address & LIBUSB_ENDPOINT_IN) {
                    in_endpoint = address;
                } else {
                    out_endpoint = address;
                }
                break;
            case LIBUSB_TRANSFER_TYPE_INTERRUPT: break;
            case LIBUSB_TRANSFER_TYPE_BULK_STREAM: break;
            }
        }
    }
    return in_endpoint && out_endpoint;
}

AndroidOpenAccessory::~AndroidOpenAccessory() {
    if (handle) libusb_close(handle);
    if (device) libusb_unref_device(device);
}

// libusb_transfer* xfr = libusb_alloc_transfer(0);
// auto buf = new unsigned char[size];
// std::memcpy(buf, input, size);
// libusb_fill_bulk_transfer(
//    xfr, handle, out_endpoint, buf, size,
//    [](libusb_transfer* xfr) {
//        delete xfr->buffer;
//        libusb_free_transfer(xfr);
//    },
//    nullptr, 100);
// libusb_submit_transfer(xfr);

bool AndroidOpenAccessory::SendSync(std::span<const unsigned char> data, unsigned timeout) {
    if (disconnected) return false;
    auto ret = libusb_bulk_transfer(handle, out_endpoint, (unsigned char*)data.data(), data.size(),
                                    nullptr, timeout);
    disconnected |= ret == LIBUSB_ERROR_NO_DEVICE;
    return ret < 0;
}

std::span<unsigned char> AndroidOpenAccessory::RecieveSync(std::span<unsigned char> data,
                                                           unsigned timeout) {
    if (disconnected) return {};
    int xfr_length;
    auto ret =
        libusb_bulk_transfer(handle, in_endpoint, data.data(), data.size(), &xfr_length, timeout);
    disconnected |= ret == LIBUSB_ERROR_NO_DEVICE;
    if (ret < 0) return {};
    return data.first(xfr_length);
}