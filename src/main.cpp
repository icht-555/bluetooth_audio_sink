#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

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
constexpr wchar_t kWindowClassName[] = L"BluetoothAudioSinkWindow";
constexpr wchar_t kWindowTitle[] = L"Bluetooth Audio Sink";
constexpr UINT kMsgConnectionStateChanged = WM_APP + 1;
constexpr UINT kMsgTrayIcon = WM_APP + 2;
constexpr UINT kTrayIconId = 1;

constexpr int kIdDeviceCombo = 1001;
constexpr int kIdRefreshButton = 1002;
constexpr int kIdConnectButton = 1003;
constexpr int kIdDisconnectButton = 1004;
constexpr int kIdStatusLabel = 1005;
constexpr int kIdDetailLabel = 1006;
constexpr int kIdTrayOpen = 2001;
constexpr int kIdTrayRefresh = 2002;
constexpr int kIdTrayConnect = 2003;
constexpr int kIdTrayDisconnect = 2004;
constexpr int kIdTrayExit = 2005;

struct DeviceEntry
{
    std::wstring name;
    std::wstring id;
};

struct AppState
{
    HWND window{};
    HWND deviceCombo{};
    HWND refreshButton{};
    HWND connectButton{};
    HWND disconnectButton{};
    HWND statusLabel{};
    HWND detailLabel{};
    HFONT font{};
    HICON trayIcon{};
    bool trayIconAdded{ false };
    bool quitRequested{ false };

    std::vector<DeviceEntry> devices;
    audio::AudioPlaybackConnection connection{ nullptr };
    winrt::event_token stateChangedToken{};
    bool hasStateChangedToken{ false };
    bool busy{ false };
    bool connected{ false };
};

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

AppState* GetState(HWND window)
{
    return reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

void SetText(HWND control, std::wstring const& text)
{
    SetWindowTextW(control, text.c_str());
}

void SetStatus(AppState& state, std::wstring const& status, std::wstring const& detail)
{
    SetText(state.statusLabel, status);
    SetText(state.detailLabel, detail);
}

NOTIFYICONDATAW BuildTrayIconData(AppState const& state)
{
    NOTIFYICONDATAW trayData{};
    trayData.cbSize = sizeof(trayData);
    trayData.hWnd = state.window;
    trayData.uID = kTrayIconId;
    trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayData.uCallbackMessage = kMsgTrayIcon;
    trayData.hIcon = state.trayIcon;
    wcscpy_s(trayData.szTip, kWindowTitle);
    return trayData;
}

void AddTrayIcon(AppState& state)
{
    if (state.trayIconAdded)
    {
        return;
    }

    auto trayData = BuildTrayIconData(state);
    state.trayIconAdded = Shell_NotifyIconW(NIM_ADD, &trayData) == TRUE;
}

void RemoveTrayIcon(AppState& state)
{
    if (!state.trayIconAdded)
    {
        return;
    }

    auto trayData = BuildTrayIconData(state);
    Shell_NotifyIconW(NIM_DELETE, &trayData);
    state.trayIconAdded = false;
}

void ShowMainWindow(AppState& state)
{
    ShowWindow(state.window, SW_SHOWNORMAL);
    SetForegroundWindow(state.window);
}

void HideMainWindow(AppState& state)
{
    ShowWindow(state.window, SW_HIDE);
}

void ToggleMainWindow(AppState& state)
{
    if (IsWindowVisible(state.window))
    {
        HideMainWindow(state);
    }
    else
    {
        ShowMainWindow(state);
    }
}

void ShowTrayMenu(AppState& state)
{
    HMENU const menu = CreatePopupMenu();
    if (!menu)
    {
        return;
    }

    auto const hasSelection = SendMessageW(state.deviceCombo, CB_GETCURSEL, 0, 0) != CB_ERR;

    AppendMenuW(menu, MF_STRING, kIdTrayOpen, IsWindowVisible(state.window) ? L"Hide Window" : L"Open Window");
    AppendMenuW(menu, MF_STRING, kIdTrayRefresh, L"Refresh Devices");
    AppendMenuW(menu, MF_STRING | (hasSelection && !state.busy ? MF_ENABLED : MF_GRAYED), kIdTrayConnect, L"Connect Selected");
    AppendMenuW(menu, MF_STRING | (state.connected && !state.busy ? MF_ENABLED : MF_GRAYED), kIdTrayDisconnect, L"Disconnect");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kIdTrayExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(state.window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0, state.window, nullptr);
    DestroyMenu(menu);
}

std::wstring SelectedDeviceId(AppState& state)
{
    auto const selection = static_cast<int>(SendMessageW(state.deviceCombo, CB_GETCURSEL, 0, 0));
    if (selection == CB_ERR || selection < 0 || static_cast<std::size_t>(selection) >= state.devices.size())
    {
        return {};
    }

    return state.devices[static_cast<std::size_t>(selection)].id;
}

void UpdateControlStates(AppState& state)
{
    auto const selection = static_cast<int>(SendMessageW(state.deviceCombo, CB_GETCURSEL, 0, 0));
    auto const hasSelection = selection != CB_ERR;

    EnableWindow(state.deviceCombo, !state.busy);
    EnableWindow(state.refreshButton, !state.busy);
    EnableWindow(state.connectButton, !state.busy && hasSelection);
    EnableWindow(state.disconnectButton, !state.busy && state.connected);
}

void PopulateDeviceCombo(AppState& state, std::wstring const& preferredDeviceId)
{
    SendMessageW(state.deviceCombo, CB_RESETCONTENT, 0, 0);

    int preferredIndex = CB_ERR;
    for (std::size_t index = 0; index < state.devices.size(); ++index)
    {
        auto const comboIndex = static_cast<int>(
            SendMessageW(state.deviceCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(state.devices[index].name.c_str())));

        if (comboIndex != CB_ERR && state.devices[index].id == preferredDeviceId)
        {
            preferredIndex = comboIndex;
        }
    }

    if (!state.devices.empty())
    {
        SendMessageW(state.deviceCombo, CB_SETCURSEL, preferredIndex == CB_ERR ? 0 : preferredIndex, 0);
    }
}

void DisconnectCurrent(AppState& state, bool updateStatus)
{
    if (state.connection)
    {
        if (state.hasStateChangedToken)
        {
            state.connection.StateChanged(state.stateChangedToken);
            state.hasStateChangedToken = false;
        }

        state.connection.Close();
        state.connection = nullptr;
    }

    state.connected = false;

    if (updateStatus)
    {
        SetStatus(state, L"Disconnected", L"");
    }
}

winrt::fire_and_forget RefreshDevicesAsync(HWND window)
{
    auto* state = GetState(window);
    if (!state || state->busy)
    {
        co_return;
    }

    auto preferredDeviceId = SelectedDeviceId(*state);
    state->busy = true;
    SetStatus(*state, L"Enumerating playback devices...", L"");
    UpdateControlStates(*state);

    try
    {
        auto const selector = audio::AudioPlaybackConnection::GetDeviceSelector();
        auto const collection = co_await devices::DeviceInformation::FindAllAsync(selector);

        std::vector<DeviceEntry> refreshedDevices;
        refreshedDevices.reserve(collection.Size());

        for (auto const& item : collection)
        {
            refreshedDevices.push_back(DeviceEntry{
                ToWideString(item.Name()),
                ToWideString(item.Id())
            });
        }

        state = GetState(window);
        if (!state)
        {
            co_return;
        }

        state->devices = std::move(refreshedDevices);
        PopulateDeviceCombo(*state, preferredDeviceId);
        state->busy = false;

        if (state->devices.empty())
        {
            SetStatus(
                *state,
                L"No compatible devices found",
                L"Pair the phone in Windows Bluetooth settings first.");
        }
        else
        {
            SetStatus(
                *state,
                L"Ready",
                L"Select a device and click Connect.");
        }

        UpdateControlStates(*state);
    }
    catch (winrt::hresult_error const& ex)
    {
        state = GetState(window);
        if (!state)
        {
            co_return;
        }

        state->busy = false;
        SetStatus(*state, L"Device enumeration failed", FormatWinRtError(ex));
        UpdateControlStates(*state);
    }
}

winrt::fire_and_forget ConnectAsync(HWND window)
{
    auto* state = GetState(window);
    if (!state || state->busy)
    {
        co_return;
    }

    auto const selection = static_cast<int>(SendMessageW(state->deviceCombo, CB_GETCURSEL, 0, 0));
    if (selection == CB_ERR || selection < 0 || static_cast<std::size_t>(selection) >= state->devices.size())
    {
        SetStatus(*state, L"No device selected", L"");
        UpdateControlStates(*state);
        co_return;
    }

    auto const selectedDevice = state->devices[static_cast<std::size_t>(selection)];
    state->busy = true;
    SetStatus(*state, L"Connecting...", selectedDevice.name);
    UpdateControlStates(*state);

    try
    {
        DisconnectCurrent(*state, false);

        auto connection = audio::AudioPlaybackConnection::TryCreateFromId(winrt::hstring(selectedDevice.id));
        if (!connection)
        {
            state->busy = false;
            SetStatus(*state, L"Failed to create connection", selectedDevice.name);
            UpdateControlStates(*state);
            co_return;
        }

        auto const stateChangedToken = connection.StateChanged(
            [window](audio::AudioPlaybackConnection const& sender, winrt::Windows::Foundation::IInspectable const&)
            {
                if (!IsWindow(window))
                {
                    return;
                }

                auto* detailText = new std::wstring(L"State: " + ToWideString(sender.State()));
                PostMessageW(window, kMsgConnectionStateChanged, 0, reinterpret_cast<LPARAM>(detailText));
            });

        co_await connection.StartAsync();
        auto const openResult = co_await connection.OpenAsync();

        state = GetState(window);
        if (!state)
        {
            connection.StateChanged(stateChangedToken);
            connection.Close();
            co_return;
        }

        state->busy = false;

        if (openResult.Status() != audio::AudioPlaybackConnectionOpenResultStatus::Success)
        {
            connection.StateChanged(stateChangedToken);
            connection.Close();
            state->connected = false;
            SetStatus(*state, L"Connection failed", ToWideString(openResult.Status()));
            UpdateControlStates(*state);
            co_return;
        }

        state->connection = connection;
        state->stateChangedToken = stateChangedToken;
        state->hasStateChangedToken = true;
        state->connected = true;

        SetStatus(
            *state,
            L"Connected",
            L"Phone audio should now play through the PC output device.");
        UpdateControlStates(*state);
    }
    catch (winrt::hresult_error const& ex)
    {
        state = GetState(window);
        if (!state)
        {
            co_return;
        }

        state->busy = false;
        state->connected = false;
        SetStatus(*state, L"Connection failed", FormatWinRtError(ex));
        UpdateControlStates(*state);
    }
}

void ApplyDefaultFont(HWND window, HFONT font)
{
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void CreateChildControls(AppState& state, HINSTANCE instance)
{
    state.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    state.trayIcon = LoadIconW(nullptr, IDI_APPLICATION);

    state.deviceCombo = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        16, 16, 330, 240,
        state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDeviceCombo)),
        instance,
        nullptr);

    state.refreshButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Refresh",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        356, 16, 100, 28,
        state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRefreshButton)),
        instance,
        nullptr);

    state.connectButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Connect",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        16, 56, 100, 28,
        state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdConnectButton)),
        instance,
        nullptr);

    state.disconnectButton = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Disconnect",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        126, 56, 100, 28,
        state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDisconnectButton)),
        instance,
        nullptr);

    state.statusLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"Ready",
        WS_CHILD | WS_VISIBLE,
        16, 104, 440, 20,
        state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatusLabel)),
        instance,
        nullptr);

    state.detailLabel = CreateWindowExW(
        0,
        WC_STATICW,
        L"",
        WS_CHILD | WS_VISIBLE,
        16, 130, 440, 40,
        state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDetailLabel)),
        instance,
        nullptr);

    ApplyDefaultFont(state.deviceCombo, state.font);
    ApplyDefaultFont(state.refreshButton, state.font);
    ApplyDefaultFont(state.connectButton, state.font);
    ApplyDefaultFont(state.disconnectButton, state.font);
    ApplyDefaultFont(state.statusLabel, state.font);
    ApplyDefaultFont(state.detailLabel, state.font);
}

void RequestExit(AppState& state)
{
    state.quitRequested = true;
    DestroyWindow(state.window);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto state = std::make_unique<AppState>();
        state->window = window;
        CreateChildControls(*state, createStruct->hInstance);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state.release()));

        auto* storedState = GetState(window);
        if (storedState)
        {
            AddTrayIcon(*storedState);
            SetStatus(*storedState, L"Ready", L"Searching for playback devices...");
            RefreshDevicesAsync(window);
        }

        return 0;
    }
    case WM_COMMAND:
    {
        auto* state = GetState(window);
        if (!state)
        {
            return 0;
        }

        switch (LOWORD(wParam))
        {
        case kIdDeviceCombo:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                UpdateControlStates(*state);
            }
            return 0;
        case kIdRefreshButton:
        case kIdTrayRefresh:
            RefreshDevicesAsync(window);
            return 0;
        case kIdConnectButton:
        case kIdTrayConnect:
            ConnectAsync(window);
            return 0;
        case kIdDisconnectButton:
        case kIdTrayDisconnect:
            DisconnectCurrent(*state, true);
            UpdateControlStates(*state);
            return 0;
        case kIdTrayOpen:
            ToggleMainWindow(*state);
            return 0;
        case kIdTrayExit:
            RequestExit(*state);
            return 0;
        default:
            return 0;
        }
    }
    case WM_SIZE:
    {
        auto* state = GetState(window);
        if (state && wParam == SIZE_MINIMIZED)
        {
            HideMainWindow(*state);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
    {
        auto* state = GetState(window);
        if (state && !state->quitRequested)
        {
            HideMainWindow(*state);
            return 0;
        }
        break;
    }
    case kMsgConnectionStateChanged:
    {
        auto detailText = std::unique_ptr<std::wstring>(reinterpret_cast<std::wstring*>(lParam));
        auto* state = GetState(window);
        if (state && detailText)
        {
            SetText(state->detailLabel, *detailText);
        }
        return 0;
    }
    case kMsgTrayIcon:
    {
        auto* state = GetState(window);
        if (!state)
        {
            return 0;
        }

        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
            ToggleMainWindow(*state);
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu(*state);
            return 0;
        default:
            return 0;
        }
    }
    case WM_DESTROY:
    {
        auto* state = GetState(window);
        if (state)
        {
            RemoveTrayIcon(*state);
            DisconnectCurrent(*state, false);
        }

        PostQuitMessage(0);
        return 0;
    }
    case WM_NCDESTROY:
    {
        auto* state = GetState(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        delete state;
        return 0;
    }
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    winrt::init_apartment();

    INITCOMMONCONTROLSEX commonControls{
        sizeof(INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&commonControls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&windowClass))
    {
        return 1;
    }

    auto window = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 490, 220,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
