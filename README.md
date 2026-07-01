<img width="502" height="387" alt="스크린샷 2026-07-01 142212" src="https://github.com/user-attachments/assets/ec951c94-40c6-46b3-a916-7f37e531a537" />
# Webp_DesktopCam

Lightweight Windows region capture app written in C++/WinAPI.

## Current buildable stage

- Drag to select a screen region on a full-screen overlay.
- Shows a persistent topmost region border.
- Blue border while idle, blinking red border while recording.
- Save the selected region as PNG or JPG using Windows Imaging Component.
- Record the selected region to animated GIF without FFmpeg.
- Record the selected region to animated WEBP with bundled libwebp DLLs.
- Choose recording FPS from 8, 16, 24, 30, or 60. The default is 24 FPS.
- Choose save folders with Browse buttons in Settings.
- Optional Windows default notification sound after saves complete.
- Choose Korean, Japanese, or English in Settings.
- Re-compress the last WEBP recording from the Compress Last panel with a quality slider.
- Shows a red `MM:SS` recording timer while recording.
- Settings are saved to `Webp_DesktopCam.ini` next to the executable.
- Default save folder falls back to the Windows Desktop.

Animated WEBP is written without FFmpeg. Frames are encoded through libwebp's `WebPAnimEncoder` API. The project statically links bundled libwebp from `third_party/libwebp-1.6.0`, so the Release folder only needs the executable and INI file.

## Build

With Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable will be created under:

```text
build\Release\Webp_DesktopCam.exe
```

No WEBP DLLs are required next to the executable.

## Notes

This project intentionally avoids FFmpeg, Python, Electron, and bundled image resources.
