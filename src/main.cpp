#include "app_window.h"

#include <commctrl.h>

#include <winrt/base.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    winrt::init_apartment();

    INITCOMMONCONTROLSEX commonControls{
        sizeof(INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&commonControls);

    AppWindow appWindow(instance);
    if (!appWindow.Create(showCommand))
    {
        return 1;
    }

    return appWindow.Run();
}
