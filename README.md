# bluetooth_audio_sink

使用 `C++20 + CMake + C++/WinRT` 构建的 Windows 原生蓝牙音频接收端 MVP，用于把手机音频投到电脑播放。

当前版本已经从控制台原型改成了低占用的原生 Win32 GUI，不引入 Qt、WinUI 或 WebView 等额外运行时。

## 当前功能

- 枚举 Windows 识别到的远端音频播放设备
- 在窗口中选择设备并建立 `AudioPlaybackConnection`
- 显示连接状态
- 手动断开当前连接

## 为什么这样做

如果你现在的目标只是“让声音播出来”，那么最省资源的 GUI 方案就是保留现有 WinRT 音频能力，只在外层包一层 Win32 壳：

- 启动快
- 常驻内存小
- 不需要额外 UI 框架
- 和现有 `AudioPlaybackConnection` 逻辑兼容

## 已知边界

- 只支持 Windows 10 2004 / 19041 及以上
- 依赖系统蓝牙栈，不自行实现 A2DP / SBC / HFP
- 默认假设手机已经在 Windows 蓝牙设置中完成配对
- 当前不做托盘、自动重连、开机自启、设备热插拔监听

## 构建要求

- Windows 10/11
- Visual Studio 2022
- CMake 3.24+

## 构建

直接使用 Visual Studio 生成器即可：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

如果你想生成 Release：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 运行

```powershell
.\build\Debug\bluetooth_audio_sink.exe
```

如果使用的是 Release：

```powershell
.\build\Release\bluetooth_audio_sink.exe
```

## 当前 GUI 结构

现在的窗口只保留最低限度的交互：

- 设备下拉框
- `Refresh`
- `Connect`
- `Disconnect`
- 状态文本

这套结构适合当前阶段。如果后面你想继续压低存在感，下一步建议直接改成托盘应用，把主窗口隐藏到系统托盘。
