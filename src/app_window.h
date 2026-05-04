#pragma once

#include "audio_sink_controller.h"
#include "app_messages.h"

#include <windows.h>

#include <string>
#include <vector>

class AppWindow
{
public:
    explicit AppWindow(HINSTANCE instance);

    AppWindow(AppWindow const&) = delete;
    AppWindow& operator=(AppWindow const&) = delete;

    bool Create(int showCommand);
    int Run();

private:
    static constexpr wchar_t kWindowClassName[] = L"BluetoothAudioSinkWindow";
    static constexpr wchar_t kWindowTitle[] = L"Bluetooth Audio Sink";
    static constexpr UINT kTrayIconId = 1;

    static constexpr int kIdDeviceCombo = 1001;
    static constexpr int kIdRefreshButton = 1002;
    static constexpr int kIdConnectButton = 1003;
    static constexpr int kIdDisconnectButton = 1004;
    static constexpr int kIdStatusLabel = 1005;
    static constexpr int kIdDetailLabel = 1006;
    static constexpr int kIdTrayOpen = 2001;
    static constexpr int kIdTrayRefresh = 2002;
    static constexpr int kIdTrayConnect = 2003;
    static constexpr int kIdTrayDisconnect = 2004;
    static constexpr int kIdTrayExit = 2005;

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static AppWindow* FromHandle(HWND window);

    bool Register();
    void CreateChildControls();
    void ApplyDefaultFont(HWND control) const;
    void SetText(HWND control, std::wstring const& text) const;
    void SetStatus(std::wstring const& status, std::wstring const& detail) const;
    void UpdateControlStates() const;
    std::wstring SelectedDeviceId() const;
    const DeviceEntry* SelectedDevice() const;
    void PopulateDeviceCombo(std::wstring const& preferredDeviceId);

    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void ShowMainWindow();
    void HideMainWindow();
    void ToggleMainWindow();
    void RequestExit();

    void BeginRefresh();
    void BeginConnect();
    void DisconnectCurrent(bool updateStatus);

    void OnDevicesEnumerated(DeviceEnumerationResult& result);
    void OnConnectionCompleted(ConnectionResult& result);
    void OnConnectionStateChanged(std::wstring const& detail);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_{};
    HWND window_{};
    HWND deviceCombo_{};
    HWND refreshButton_{};
    HWND connectButton_{};
    HWND disconnectButton_{};
    HWND statusLabel_{};
    HWND detailLabel_{};
    HFONT font_{};
    HICON trayIcon_{};
    bool trayIconAdded_{ false };
    bool quitRequested_{ false };
    std::vector<DeviceEntry> devices_;
    AudioSinkController controller_;
};
