#pragma once

#include <windows.h>

#include <string>
#include <vector>

struct DeviceEntry
{
    std::wstring name;
    std::wstring id;
};

struct DeviceEnumerationResult
{
    std::vector<DeviceEntry> devices;
    std::wstring preferredDeviceId;
    std::wstring error;
};

struct ConnectionResult
{
    bool success{ false };
    std::wstring status;
    std::wstring detail;
};

constexpr UINT kMsgDevicesEnumerated = WM_APP + 1;
constexpr UINT kMsgConnectionStateChanged = WM_APP + 2;
constexpr UINT kMsgConnectionCompleted = WM_APP + 3;
constexpr UINT kMsgTrayIcon = WM_APP + 4;
