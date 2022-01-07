#include "adk.hpp"

#include <pa_win_wasapi.h>
#include <portaudio.h>

#include <fmt/ostream.h>

#include <atomic>
#include <chrono>
#include <ranges>
#include <thread>

namespace ranges = std::ranges;
using namespace std::literals;

std::unique_ptr<AndroidOpenAccessory> android_device;

static struct {
    int sample_rate = 96000;
    int sample_size = 3;
    int channels = 2;
} audio_params;

int InputCallback(const void* input, void* output, unsigned long frameCount,
                  const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
                  void* userData) {
    unsigned long bytes_per_frame = audio_params.sample_size * audio_params.channels;
    unsigned long size = frameCount * bytes_per_frame;

    android_device->SendSync({reinterpret_cast<const unsigned char*>(input), size});
    return paContinue;
}

PaStream* InitPortAudio() {
    // ------------------------------------------------------------------------
    // Portaudio initialization
    // ------------------------------------------------------------------------
    Pa_Initialize();

    PaHostApiIndex api_idx = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
    if (api_idx < 0) {
        fmt::print("WASAPI not found");
        return 0;
    }
    const PaHostApiInfo* api_info = Pa_GetHostApiInfo(api_idx);

    const PaDeviceInfo* device_info = nullptr;
    int device_idx = -1;
    for (int i = 0; i < api_info->deviceCount; ++i) {
        auto global_idx = Pa_HostApiDeviceIndexToDeviceIndex(api_idx, i);
        const auto* device = Pa_GetDeviceInfo(global_idx);
        
        // First - try to get Virtual Cable, if couldn't - try to get VoiceMeeter(or something else with VB-Audio in the name)
        if (std::string{device->name}.find("VB-Audio Virtual Cable") != std::string::npos &&
            device->maxInputChannels > 0) {
            device_info = device;
            device_idx = global_idx;
            break;
        } else if (std::string{ device->name }.find("VB-Audio") != std::string::npos &&
            device->maxInputChannels > 0) {
            device_info = device;
            device_idx = global_idx;
        }
    }
    if (!device_info) {
        fmt::print("Audio cable not found");
        return 0;
    } else {
        fmt::print("Piping audio from {}\n", device_info->name);
    }

    audio_params.sample_rate = device_info->defaultSampleRate;
    audio_params.channels = device_info->maxInputChannels;

    PaWasapiStreamInfo wasapi_stream_info{
        .size = sizeof(PaWasapiStreamInfo),
        .hostApiType = paWASAPI,
        .version = 1,
        .flags = (paWinWasapiExclusive | paWinWasapiThreadPriority),
        .threadPriority = eThreadPriorityProAudio,
    };
    PaSampleFormat format;
    switch (audio_params.sample_size) {
    case 1: format = paInt8; break;
    case 2: format = paInt16; break;
    case 3: format = paInt24; break;
    case 4: format = paFloat32; break;
    default: break;
    }
    PaStreamParameters stream_parameters{
        .device = device_idx,
        .channelCount = audio_params.channels,
        .sampleFormat = format,
        .suggestedLatency = 1.0 / 144,
        .hostApiSpecificStreamInfo = &wasapi_stream_info,
    };

    PaStream* stream;

    Pa_OpenStream(&stream, &stream_parameters, nullptr, audio_params.sample_rate,
                  paFramesPerBufferUnspecified, paNoFlag, InputCallback, nullptr);

    const auto* stream_info = Pa_GetStreamInfo(stream);
    fmt::print("Sample Rate {} Hz\nLatency {} seconds\n", stream_info->sampleRate,
               stream_info->inputLatency);

    return stream;
}

int main() {
    auto stream = InitPortAudio();
    if (!stream) { return 0; }

    while (true) {
        if (android_device) {
            if (android_device->disconnected) {
                Pa_AbortStream(stream);
                android_device = {};
            } else {
                unsigned char buf[1 << 14];
                auto msg = android_device->RecieveSync(buf, 100);
                if (!msg.empty()) {
                    std::string_view str{(const char*)msg.data(), msg.size()};
                    fmt::print("{}\n", str);
                }
            }
        } else {
            auto devices = AndroidOpenAccessory::GetDevices(
                "BreadFish64", "UsbAudioSource", "Pumps audio data to an Android device over USB",
                "1.0", "https://breadfish64.github.io", "0000000SerialNo.");
            if (devices.empty()) {
                std::this_thread::sleep_for(1s);
                continue;
            }

            android_device = std::move(devices[0]);

            char sentinel[] = "BreadFish64";
            android_device->SendSync({(unsigned char*)sentinel, strlen(sentinel)});
            fmt::print("Sent sentinel\n");

            unsigned char sentinel_recieve_buffer[64];
            auto recieved_sentinel = android_device->RecieveSync(sentinel_recieve_buffer, 0);
            fmt::print(
                "Recieved {} B sentinel: {}\n", recieved_sentinel.size(),
                std::string_view{(const char*)recieved_sentinel.data(), recieved_sentinel.size()});

            android_device->SendSync({(unsigned char*)&audio_params, sizeof(audio_params)});

            std::string_view sentinel_string;
            do {
                recieved_sentinel = android_device->RecieveSync(sentinel_recieve_buffer, 0);
                sentinel_string = std::string_view{(const char*)recieved_sentinel.data(),
                                                   recieved_sentinel.size()};
                fmt::print("Recieved: {}\n", sentinel_string);
            } while (sentinel_string != "BreadFish64");

            Pa_StartStream(stream);
        }
    }
}
