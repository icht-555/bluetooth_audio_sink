#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Audio.h>
#include <winrt/base.h>

#include <chrono>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace audio = winrt::Windows::Media::Audio;
namespace devices = winrt::Windows::Devices::Enumeration;

namespace
{
std::string Narrow(winrt::hstring const& value)
{
    return winrt::to_string(value);
}

std::string ToString(audio::AudioPlaybackConnectionState state)
{
    switch (state)
    {
    case audio::AudioPlaybackConnectionState::Closed:
        return "Closed";
    case audio::AudioPlaybackConnectionState::Opened:
        return "Opened";
    default:
        return "Unknown";
    }
}

std::string ToString(audio::AudioPlaybackConnectionOpenResultStatus status)
{
    switch (status)
    {
    case audio::AudioPlaybackConnectionOpenResultStatus::Success:
        return "Success";
    case audio::AudioPlaybackConnectionOpenResultStatus::RequestTimedOut:
        return "RequestTimedOut";
    case audio::AudioPlaybackConnectionOpenResultStatus::DeniedBySystem:
        return "DeniedBySystem";
    case audio::AudioPlaybackConnectionOpenResultStatus::UnknownFailure:
        return "UnknownFailure";
    default:
        return "UnrecognizedStatus";
    }
}

std::vector<devices::DeviceInformation> EnumeratePlaybackDevices()
{
    auto const selector = audio::AudioPlaybackConnection::GetDeviceSelector();
    auto const collection = devices::DeviceInformation::FindAllAsync(selector).get();

    std::vector<devices::DeviceInformation> result;
    result.reserve(collection.Size());

    for (auto const& item : collection)
    {
        result.push_back(item);
    }

    return result;
}

std::optional<std::size_t> ReadSelection(std::size_t maxExclusive)
{
    std::cout << "Select device index: ";

    std::size_t index = 0;
    if (!(std::cin >> index))
    {
        return std::nullopt;
    }

    if (index >= maxExclusive)
    {
        return std::nullopt;
    }

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return index;
}

void WaitForExit()
{
    std::cout << "\nPress Enter to close the connection and exit..." << std::endl;
    std::string ignored;
    std::getline(std::cin, ignored);
}
} // namespace

int main()
{
    winrt::init_apartment();

    try
    {
        std::cout << "Bluetooth audio sink MVP\n";
        std::cout << "Enumerating candidate devices...\n\n";

        auto devices = EnumeratePlaybackDevices();
        if (devices.empty())
        {
            std::cout << "No compatible devices were found.\n";
            std::cout << "Make sure the phone is paired in Windows Bluetooth settings and supports remote audio playback.\n";
            return 1;
        }

        for (std::size_t i = 0; i < devices.size(); ++i)
        {
            std::cout << "[" << i << "] " << Narrow(devices[i].Name()) << '\n';
            std::cout << "    Id: " << Narrow(devices[i].Id()) << '\n';
        }

        auto const selection = ReadSelection(devices.size());
        if (!selection.has_value())
        {
            std::cerr << "Invalid selection.\n";
            return 1;
        }

        auto const& selectedDevice = devices[*selection];
        auto connection = audio::AudioPlaybackConnection::TryCreateFromId(selectedDevice.Id());
        if (!connection)
        {
            std::cerr << "Failed to create AudioPlaybackConnection for the selected device.\n";
            return 1;
        }

        auto revoker = connection.StateChanged(winrt::auto_revoke,
            [](audio::AudioPlaybackConnection const& sender, winrt::Windows::Foundation::IInspectable const&)
            {
                std::cout << "[StateChanged] " << ToString(sender.State()) << '\n';
            });

        std::cout << "\nStarting playback capability for device: " << Narrow(selectedDevice.Name()) << '\n';
        connection.StartAsync().get();

        std::cout << "Opening connection...\n";
        auto const openResult = connection.OpenAsync().get();
        std::cout << "Open result: " << ToString(openResult.Status()) << '\n';
        std::cout << "Current state: " << ToString(connection.State()) << '\n';

        if (openResult.Status() != audio::AudioPlaybackConnectionOpenResultStatus::Success)
        {
            std::cerr << "The connection attempt did not succeed.\n";
            return 1;
        }

        std::cout << "\nIf the phone is already sending media audio, it should now play through the PC output device.\n";
        WaitForExit();

        connection.Close();
        std::cout << "Connection closed.\n";
        return 0;
    }
    catch (winrt::hresult_error const& ex)
    {
        std::cerr << "WinRT error: " << Narrow(ex.message()) << " (0x" << std::hex << ex.code().value << ")\n";
        return 1;
    }
    catch (std::exception const& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
