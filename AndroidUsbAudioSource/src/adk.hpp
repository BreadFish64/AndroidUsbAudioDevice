#pragma once

#include <libusb.h>

#include <functional>
#include <span>
#include <vector>

struct AndroidOpenAccessory {
    bool SwitchModes(libusb_device_handle* handle);
    bool ConnectToAccessory();
    bool FindEndpoints();

    static inline libusb_context* context{};

    static constexpr std::uint16_t PID_ACCESSORY = 0x2D00;

    libusb_device_handle* handle{};
    libusb_device* device{};
    uint16_t vid{};

    uint8_t in_endpoint{};
    uint8_t out_endpoint{};

    const char* manufacturer{};
    const char* model_name{};
    const char* description{};
    const char* version{};
    const char* uri{};
    const char* serial_number{};

    bool disconnected = false;

    static std::vector<std::unique_ptr<AndroidOpenAccessory>> GetDevices(
        const char* manufacturer, const char* model_name, const char* description,
        const char* version, const char* uri, const char* serial_number);
    static std::unique_ptr<AndroidOpenAccessory> Connect(
        const char* manufacturer, const char* model_name, const char* description,
        const char* version, const char* uri, const char* serial_number, libusb_device* device);
    ~AndroidOpenAccessory();

    bool SendSync(std::span<const unsigned char> data, unsigned timeout = 10);

    std::span<unsigned char> RecieveSync(std::span<unsigned char> data, unsigned timeout);
};
