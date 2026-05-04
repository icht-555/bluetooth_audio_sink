#include "app_window.h"

#include "../resource.h"

#include <commctrl.h>
#include <shellapi.h>

#include <memory>

namespace
{
HICON LoadAppIcon(HINSTANCE instance)
{
    auto* icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!icon)
    {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    return icon;
}
} // namespace

AppWindow::AppWindow(HINSTANCE instance)
    : instance_(instance)
{
}

bool AppWindow::Create(int showCommand)
{
    if (!Register())
    {
        return false;
    }

    window_ = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        490,
        220,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!window_)
    {
        return false;
    }

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    return true;
}

int AppWindow::Run()
{
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

AppWindow* AppWindow::FromHandle(HWND window)
{
    return reinterpret_cast<AppWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

bool AppWindow::Register()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hIcon = LoadAppIcon(instance_);
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;
    return RegisterClassExW(&windowClass) != 0;
}

void AppWindow::ApplyDefaultFont(HWND control) const
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
}

void AppWindow::SetText(HWND control, std::wstring const& text) const
{
    SetWindowTextW(control, text.c_str());
}

void AppWindow::SetStatus(std::wstring const& status, std::wstring const& detail) const
{
    SetText(statusLabel_, status);
    SetText(detailLabel_, detail);
}

std::wstring AppWindow::SelectedDeviceId() const
{
    auto const* device = SelectedDevice();
    return device ? device->id : std::wstring{};
}

const DeviceEntry* AppWindow::SelectedDevice() const
{
    auto const selection = static_cast<int>(SendMessageW(deviceCombo_, CB_GETCURSEL, 0, 0));
    if (selection == CB_ERR || selection < 0 || static_cast<std::size_t>(selection) >= devices_.size())
    {
        return nullptr;
    }

    return &devices_[static_cast<std::size_t>(selection)];
}

void AppWindow::UpdateControlStates() const
{
    auto const hasSelection = SendMessageW(deviceCombo_, CB_GETCURSEL, 0, 0) != CB_ERR;
    auto const busy = controller_.busy();

    EnableWindow(deviceCombo_, !busy);
    EnableWindow(refreshButton_, !busy);
    EnableWindow(connectButton_, !busy && hasSelection);
    EnableWindow(disconnectButton_, !busy && controller_.connected());
}

void AppWindow::PopulateDeviceCombo(std::wstring const& preferredDeviceId)
{
    SendMessageW(deviceCombo_, CB_RESETCONTENT, 0, 0);

    int preferredIndex = CB_ERR;
    for (std::size_t index = 0; index < devices_.size(); ++index)
    {
        auto const comboIndex = static_cast<int>(
            SendMessageW(deviceCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(devices_[index].name.c_str())));

        if (comboIndex != CB_ERR && devices_[index].id == preferredDeviceId)
        {
            preferredIndex = comboIndex;
        }
    }

    if (!devices_.empty())
    {
        SendMessageW(deviceCombo_, CB_SETCURSEL, preferredIndex == CB_ERR ? 0 : preferredIndex, 0);
    }
}

NOTIFYICONDATAW BuildTrayIconData(HWND window, HICON icon, UINT callbackMessage, UINT trayIconId, wchar_t const* title)
{
    NOTIFYICONDATAW trayData{};
    trayData.cbSize = sizeof(trayData);
    trayData.hWnd = window;
    trayData.uID = trayIconId;
    trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayData.uCallbackMessage = callbackMessage;
    trayData.hIcon = icon;
    wcscpy_s(trayData.szTip, title);
    return trayData;
}

void AppWindow::AddTrayIcon()
{
    if (trayIconAdded_)
    {
        return;
    }

    auto trayData = BuildTrayIconData(window_, trayIcon_, kMsgTrayIcon, kTrayIconId, kWindowTitle);
    trayIconAdded_ = Shell_NotifyIconW(NIM_ADD, &trayData) == TRUE;
}

void AppWindow::RemoveTrayIcon()
{
    if (!trayIconAdded_)
    {
        return;
    }

    auto trayData = BuildTrayIconData(window_, trayIcon_, kMsgTrayIcon, kTrayIconId, kWindowTitle);
    Shell_NotifyIconW(NIM_DELETE, &trayData);
    trayIconAdded_ = false;
}

void AppWindow::ShowMainWindow()
{
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
}

void AppWindow::HideMainWindow()
{
    ShowWindow(window_, SW_HIDE);
}

void AppWindow::ToggleMainWindow()
{
    if (IsWindowVisible(window_))
    {
        HideMainWindow();
    }
    else
    {
        ShowMainWindow();
    }
}

void AppWindow::ShowTrayMenu()
{
    HMENU const menu = CreatePopupMenu();
    if (!menu)
    {
        return;
    }

    auto const hasSelection = SendMessageW(deviceCombo_, CB_GETCURSEL, 0, 0) != CB_ERR;
    auto const busy = controller_.busy();

    AppendMenuW(menu, MF_STRING, kIdTrayOpen, IsWindowVisible(window_) ? L"Hide Window" : L"Open Window");
    AppendMenuW(menu, MF_STRING, kIdTrayRefresh, L"Refresh Devices");
    AppendMenuW(menu, MF_STRING | (hasSelection && !busy ? MF_ENABLED : MF_GRAYED), kIdTrayConnect, L"Connect Selected");
    AppendMenuW(menu, MF_STRING | (controller_.connected() && !busy ? MF_ENABLED : MF_GRAYED), kIdTrayDisconnect, L"Disconnect");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kIdTrayExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0, window_, nullptr);
    DestroyMenu(menu);
}

void AppWindow::CreateChildControls()
{
    font_ = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    trayIcon_ = LoadAppIcon(instance_);

    deviceCombo_ = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        16,
        16,
        330,
        240,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDeviceCombo)),
        instance_,
        nullptr);

    refreshButton_ = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Refresh",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        356,
        16,
        100,
        28,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRefreshButton)),
        instance_,
        nullptr);

    connectButton_ = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Connect",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        16,
        56,
        100,
        28,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdConnectButton)),
        instance_,
        nullptr);

    disconnectButton_ = CreateWindowExW(
        0,
        WC_BUTTONW,
        L"Disconnect",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        126,
        56,
        100,
        28,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDisconnectButton)),
        instance_,
        nullptr);

    statusLabel_ = CreateWindowExW(
        0,
        WC_STATICW,
        L"Ready",
        WS_CHILD | WS_VISIBLE,
        16,
        104,
        440,
        20,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatusLabel)),
        instance_,
        nullptr);

    detailLabel_ = CreateWindowExW(
        0,
        WC_STATICW,
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        130,
        440,
        40,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDetailLabel)),
        instance_,
        nullptr);

    ApplyDefaultFont(deviceCombo_);
    ApplyDefaultFont(refreshButton_);
    ApplyDefaultFont(connectButton_);
    ApplyDefaultFont(disconnectButton_);
    ApplyDefaultFont(statusLabel_);
    ApplyDefaultFont(detailLabel_);
}

void AppWindow::RequestExit()
{
    quitRequested_ = true;
    DestroyWindow(window_);
}

void AppWindow::BeginRefresh()
{
    SetStatus(L"Enumerating playback devices...", L"");
    UpdateControlStates();
    controller_.EnumerateDevicesAsync(window_, SelectedDeviceId());
}

void AppWindow::BeginConnect()
{
    auto const* device = SelectedDevice();
    if (!device)
    {
        SetStatus(L"No device selected", L"");
        UpdateControlStates();
        return;
    }

    SetStatus(L"Connecting...", device->name);
    UpdateControlStates();
    controller_.ConnectAsync(window_, device->id, device->name);
}

void AppWindow::DisconnectCurrent(bool updateStatus)
{
    controller_.Disconnect();
    if (updateStatus)
    {
        SetStatus(L"Disconnected", L"");
    }
}

void AppWindow::OnDevicesEnumerated(DeviceEnumerationResult& result)
{
    devices_ = std::move(result.devices);
    PopulateDeviceCombo(result.preferredDeviceId);

    if (!result.error.empty())
    {
        SetStatus(L"Device enumeration failed", result.error);
    }
    else if (devices_.empty())
    {
        SetStatus(L"No compatible devices found", L"Pair the phone in Windows Bluetooth settings first.");
    }
    else
    {
        SetStatus(L"Ready", L"Select a device and click Connect.");
    }

    UpdateControlStates();
}

void AppWindow::OnConnectionCompleted(ConnectionResult& result)
{
    SetStatus(result.status, result.detail);
    UpdateControlStates();
}

void AppWindow::OnConnectionStateChanged(std::wstring const& detail)
{
    SetText(detailLabel_, detail);
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateChildControls();
        AddTrayIcon();
        SetStatus(L"Ready", L"Searching for playback devices...");
        BeginRefresh();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kIdDeviceCombo:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                UpdateControlStates();
            }
            return 0;
        case kIdRefreshButton:
        case kIdTrayRefresh:
            BeginRefresh();
            return 0;
        case kIdConnectButton:
        case kIdTrayConnect:
            BeginConnect();
            return 0;
        case kIdDisconnectButton:
        case kIdTrayDisconnect:
            DisconnectCurrent(true);
            UpdateControlStates();
            return 0;
        case kIdTrayOpen:
            ToggleMainWindow();
            return 0;
        case kIdTrayExit:
            RequestExit();
            return 0;
        default:
            return 0;
        }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            HideMainWindow();
            return 0;
        }
        break;
    case WM_CLOSE:
        if (!quitRequested_)
        {
            HideMainWindow();
            return 0;
        }
        break;
    case kMsgDevicesEnumerated:
    {
        auto result = std::unique_ptr<DeviceEnumerationResult>(reinterpret_cast<DeviceEnumerationResult*>(lParam));
        if (result)
        {
            OnDevicesEnumerated(*result);
        }
        return 0;
    }
    case kMsgConnectionStateChanged:
    {
        auto detail = std::unique_ptr<std::wstring>(reinterpret_cast<std::wstring*>(lParam));
        if (detail)
        {
            OnConnectionStateChanged(*detail);
        }
        return 0;
    }
    case kMsgConnectionCompleted:
    {
        auto result = std::unique_ptr<ConnectionResult>(reinterpret_cast<ConnectionResult*>(lParam));
        if (result)
        {
            OnConnectionCompleted(*result);
        }
        return 0;
    }
    case kMsgTrayIcon:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
            ToggleMainWindow();
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu();
            return 0;
        default:
            return 0;
        }
    case WM_DESTROY:
        RemoveTrayIcon();
        controller_.Disconnect();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

LRESULT CALLBACK AppWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* appWindow = FromHandle(window);

    if (message == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        appWindow = static_cast<AppWindow*>(createStruct->lpCreateParams);
        appWindow->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(appWindow));
    }

    if (!appWindow)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    if (message == WM_NCDESTROY)
    {
        auto result = appWindow->HandleMessage(message, wParam, lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        appWindow->window_ = nullptr;
        return result;
    }

    return appWindow->HandleMessage(message, wParam, lParam);
}
