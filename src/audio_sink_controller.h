#pragma once

#include <windows.h>

#include <memory>
#include <string>

#include <winrt/base.h>

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
    static winrt::fire_and_forget EnumerateDevicesCoroutine(
        std::shared_ptr<Impl> impl,
        HWND window,
        std::wstring preferredDeviceId);
    static winrt::fire_and_forget ConnectCoroutine(
        std::shared_ptr<Impl> impl,
        HWND window,
        std::wstring deviceId,
        std::wstring deviceName);

    std::shared_ptr<Impl> impl_;

    std::shared_ptr<Impl> EnsureImpl();
};
