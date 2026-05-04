#pragma once

#include <windows.h>

#include <string>

class AudioSinkController
{
public:
    AudioSinkController() = default;
    ~AudioSinkController();
    AudioSinkController(AudioSinkController const&) = delete;
    AudioSinkController& operator=(AudioSinkController const&) = delete;

    void EnumerateDevicesAsync(HWND window, std::wstring preferredDeviceId);
    void ConnectAsync(HWND window, std::wstring deviceId, std::wstring deviceName);
    void Disconnect();

    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] bool connected() const noexcept;

private:
    struct Impl;
    Impl* impl_{ nullptr };

    Impl& EnsureImpl();
};
