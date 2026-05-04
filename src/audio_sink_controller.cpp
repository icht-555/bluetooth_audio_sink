#include "audio_sink_controller.h"

#include "app_messages.h"

#include <windows.h>

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Audio.h>
#include <winrt/base.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace audio = winrt::Windows::Media::Audio;
namespace devices = winrt::Windows::Devices::Enumeration;

namespace
{
std::wstring ToWideString(winrt::hstring const& value)
{
    return std::wstring(value.c_str());
}

std::wstring ToWideString(audio::AudioPlaybackConnectionState state)
{
    switch (state)
    {
    case audio::AudioPlaybackConnectionState::Closed:
        return L"Closed";
    case audio::AudioPlaybackConnectionState::Opened:
        return L"Opened";
    default:
        return L"Unknown";
    }
}

std::wstring ToWideString(audio::AudioPlaybackConnectionOpenResultStatus status)
{
    switch (status)
    {
    case audio::AudioPlaybackConnectionOpenResultStatus::Success:
        return L"Success";
    case audio::AudioPlaybackConnectionOpenResultStatus::RequestTimedOut:
        return L"RequestTimedOut";
    case audio::AudioPlaybackConnectionOpenResultStatus::DeniedBySystem:
        return L"DeniedBySystem";
    case audio::AudioPlaybackConnectionOpenResultStatus::UnknownFailure:
        return L"UnknownFailure";
    default:
        return L"UnrecognizedStatus";
    }
}

std::wstring FormatWinRtError(winrt::hresult_error const& ex)
{
    std::wstringstream stream;
    stream << ex.message().c_str() << L" (0x" << std::hex << std::uppercase << ex.code().value << L")";
    return stream.str();
}

template <typename T>
void PostPayload(HWND window, UINT message, std::unique_ptr<T> payload)
{
    if (!IsWindow(window))
    {
        return;
    }

    auto* rawPayload = payload.release();
    if (!PostMessageW(window, message, 0, reinterpret_cast<LPARAM>(rawPayload)))
    {
        delete rawPayload;
    }
}
} // namespace

struct AudioSinkController::Impl
{
    audio::AudioPlaybackConnection connection{ nullptr };
    winrt::event_token stateChangedToken{};
    bool hasStateChangedToken{ false };
    bool busy{ false };
    bool connected{ false };
};

winrt::fire_and_forget AudioSinkController::EnumerateDevicesCoroutine(
    std::shared_ptr<Impl> impl,
    HWND window,
    std::wstring preferredDeviceId)
{
    try
    {
        auto const selector = audio::AudioPlaybackConnection::GetDeviceSelector();
        auto const collection = co_await devices::DeviceInformation::FindAllAsync(selector);

        auto result = std::make_unique<DeviceEnumerationResult>();
        result->preferredDeviceId = std::move(preferredDeviceId);
        result->devices.reserve(collection.Size());

        for (auto const& item : collection)
        {
            result->devices.push_back(DeviceEntry{
                ToWideString(item.Name()),
                ToWideString(item.Id())
            });
        }

        impl->busy = false;
        PostPayload(window, kMsgDevicesEnumerated, std::move(result));
    }
    catch (winrt::hresult_error const& ex)
    {
        auto result = std::make_unique<DeviceEnumerationResult>();
        result->preferredDeviceId = std::move(preferredDeviceId);
        result->error = FormatWinRtError(ex);

        impl->busy = false;
        PostPayload(window, kMsgDevicesEnumerated, std::move(result));
    }
}

winrt::fire_and_forget AudioSinkController::ConnectCoroutine(
    std::shared_ptr<Impl> impl,
    HWND window,
    std::wstring deviceId,
    std::wstring deviceName)
{
    try
    {
        if (impl->connection)
        {
            if (impl->hasStateChangedToken)
            {
                impl->connection.StateChanged(impl->stateChangedToken);
                impl->hasStateChangedToken = false;
            }

            impl->connection.Close();
            impl->connection = nullptr;
        }

        impl->connected = false;

        auto connection = audio::AudioPlaybackConnection::TryCreateFromId(winrt::hstring(deviceId));
        if (!connection)
        {
            auto result = std::make_unique<ConnectionResult>();
            result->status = L"Failed to create connection";
            result->detail = std::move(deviceName);

            impl->busy = false;
            impl->connected = false;
            PostPayload(window, kMsgConnectionCompleted, std::move(result));
            co_return;
        }

        auto const stateChangedToken = connection.StateChanged(
            [window](audio::AudioPlaybackConnection const& sender, winrt::Windows::Foundation::IInspectable const&)
            {
                auto detail = std::make_unique<std::wstring>(L"State: " + ToWideString(sender.State()));
                PostPayload(window, kMsgConnectionStateChanged, std::move(detail));
            });

        co_await connection.StartAsync();
        auto const openResult = co_await connection.OpenAsync();

        auto result = std::make_unique<ConnectionResult>();
        impl->busy = false;

        if (openResult.Status() != audio::AudioPlaybackConnectionOpenResultStatus::Success)
        {
            connection.StateChanged(stateChangedToken);
            connection.Close();

            impl->connected = false;
            result->status = L"Connection failed";
            result->detail = ToWideString(openResult.Status());
            PostPayload(window, kMsgConnectionCompleted, std::move(result));
            co_return;
        }

        impl->connection = connection;
        impl->stateChangedToken = stateChangedToken;
        impl->hasStateChangedToken = true;
        impl->connected = true;

        result->success = true;
        result->status = L"Connected";
        result->detail = L"Phone audio should now play through the PC output device.";
        PostPayload(window, kMsgConnectionCompleted, std::move(result));
    }
    catch (winrt::hresult_error const& ex)
    {
        auto result = std::make_unique<ConnectionResult>();
        result->status = L"Connection failed";
        result->detail = FormatWinRtError(ex);

        impl->busy = false;
        impl->connected = false;
        PostPayload(window, kMsgConnectionCompleted, std::move(result));
    }
}

AudioSinkController::~AudioSinkController()
{
    Disconnect();
}

std::shared_ptr<AudioSinkController::Impl> AudioSinkController::EnsureImpl()
{
    if (!impl_)
    {
        impl_ = std::make_shared<Impl>();
    }

    return impl_;
}

bool AudioSinkController::busy() const noexcept
{
    return impl_ && impl_->busy;
}

bool AudioSinkController::connected() const noexcept
{
    return impl_ && impl_->connected;
}

void AudioSinkController::Disconnect()
{
    if (!impl_)
    {
        return;
    }

    if (impl_->connection)
    {
        if (impl_->hasStateChangedToken)
        {
            impl_->connection.StateChanged(impl_->stateChangedToken);
            impl_->hasStateChangedToken = false;
        }

        impl_->connection.Close();
        impl_->connection = nullptr;
    }

    impl_->connected = false;
}

void AudioSinkController::EnumerateDevicesAsync(HWND window, std::wstring preferredDeviceId)
{
    auto impl = EnsureImpl();
    if (impl->busy)
    {
        return;
    }

    impl->busy = true;
    EnumerateDevicesCoroutine(std::move(impl), window, std::move(preferredDeviceId));
}

void AudioSinkController::ConnectAsync(HWND window, std::wstring deviceId, std::wstring deviceName)
{
    auto impl = EnsureImpl();
    if (impl->busy)
    {
        return;
    }

    impl->busy = true;
    ConnectCoroutine(std::move(impl), window, std::move(deviceId), std::move(deviceName));
}
