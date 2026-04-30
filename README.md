# bluetooth_audio_sink

使用 `C++20 + CMake + C++/WinRT` 构建的 Windows 最小 MVP，用于把手机通过蓝牙投到电脑播放。

当前版本是控制台程序，聚焦一件事：发现系统支持远端音频播放的已配对设备，并尝试建立 `AudioPlaybackConnection`。

## 能力范围

- 枚举 Windows 识别到的可用远端音频播放设备
- 选择一个设备并调用 `StartAsync()` / `OpenAsync()`
- 打印连接状态变化
- 在连接建立后保持进程存活，直到用户按回车退出

## 已知边界

- 只支持 Windows 10 2004 / 19041 及以上
- 依赖系统蓝牙栈，不自行实现 A2DP / SBC / HFP
- 默认假设手机已经在系统蓝牙设置中完成配对
- 当前不做托盘、GUI、自动重连、开机自启
- 当前不提供音频处理能力，只负责让声音播出来

## 构建要求

- Windows 10/11
- Visual Studio 2022 或安装了 MSVC 与 Windows SDK 的 Build Tools
- CMake 3.24+

## 构建

建议在 “x64 Native Tools Command Prompt for VS 2022” 或可用 MSVC 环境中执行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## 运行

```powershell
.\build\Debug\bluetooth_audio_sink.exe
```

程序启动后会：

1. 枚举可用设备
2. 输出设备列表
3. 让你输入设备序号
4. 尝试建立远端音频播放连接

如果手机端正在播放媒体音频，并且 Windows 设备支持该连接，声音会经由当前电脑默认输出设备播放。

## 下一步建议

- 改成托盘应用，避免控制台驻留
- 加设备缓存与自动重连
- 加更清晰的错误码映射
- 改用 `DeviceWatcher` 做设备热插拔监听

