#include <windows.h>
#include <windowsx.h>
#include <wincodec.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <webp/encode.h>
#include <webp/mux.h>

#include "../res/resource.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace {

constexpr int kHotkeyRecord = 1001;
constexpr int kHotkeyScreenshot = 1002;
constexpr int kTimerRecord = 2001;
constexpr int kTimerBlink = 2002;
#ifndef WDA_EXCLUDEFROMCAPTURE
constexpr DWORD WDA_EXCLUDEFROMCAPTURE = 0x00000011;
#endif

enum class RecordFormat { Gif, Webp };
enum class ImageFormat { Jpg, Png };
enum class QualityPreset { High, Medium, Low };
enum class Language { Korean, Japanese, English };

struct Preset {
    int gifColors;
    int jpgQuality;
    int webpQuality;
};

Preset presetFor(QualityPreset preset) {
    switch (preset) {
    case QualityPreset::High: return {256, 92, 90};
    case QualityPreset::Medium: return {128, 82, 75};
    case QualityPreset::Low: return {64, 70, 55};
    }
    return {128, 82, 75};
}

struct Hotkey {
    UINT modifiers;
    UINT vk;
};

struct AppState {
    HWND main = nullptr;
    HWND overlay = nullptr;
    HWND border = nullptr;
    HWND settings = nullptr;
    HWND compress = nullptr;
    HFONT uiFont = nullptr;
    HFONT statusFont = nullptr;
    HFONT timerFont = nullptr;
    HICON appIcon = nullptr;
    HICON appIconSmall = nullptr;
    RECT selected{};
    bool hasSelection = false;
    bool selecting = false;
    bool recording = false;
    bool blink = false;
    bool capturingRecordHotkey = false;
    bool capturingScreenshotHotkey = false;
    POINT dragStart{};
    POINT dragEnd{};
    RecordFormat recordFormat = RecordFormat::Gif;
    ImageFormat imageFormat = ImageFormat::Png;
    QualityPreset preset = QualityPreset::Medium;
    Language language = Language::Korean;
    int recordFps = 24;
    bool notifySound = true;
    Hotkey recordHotkey{MOD_CONTROL, VK_F11};
    Hotkey screenshotHotkey{MOD_CONTROL, VK_F12};
    std::wstring recordingPath;
    std::wstring screenshotPath;
    std::wstring iniPath;
    std::wstring lastRecording;
    RecordFormat lastRecordingFormat = RecordFormat::Gif;
    std::vector<std::vector<uint8_t>> lastFrames;
    int lastFrameWidth = 0;
    int lastFrameHeight = 0;
    int lastRecordFps = 24;
    std::vector<std::vector<uint8_t>> frames;
    int frameWidth = 0;
    int frameHeight = 0;
    std::chrono::steady_clock::time_point recordStarted;
};

AppState g;

enum class TextId {
    AppTitle, SelectRegion, NoRegion, RecordingFormat, Quality, High, Medium, Low,
    RecordFps, StartRecording, StopRecording, ScreenshotFormat, SaveScreenshot,
    CompressLast, Settings, Ready, SettingsTitle, RecordingFolder, ScreenshotFolder,
    Browse, NotifySound, Save, Cancel, LanguageLabel, CompressTitle, QualityLabel,
    EstimatedSize, SaveCompressed, Close, NoWebpRecording, SavingCompressed,
    CompressNeedsWebp, CompressionFailed, RegionSelected, RecordingStarted, SavingRecording
};

const wchar_t* text(TextId id) {
    switch (g.language) {
    case Language::Japanese:
        switch (id) {
        case TextId::AppTitle: return L"Webp_DesktopCam";
        case TextId::SelectRegion: return L"範囲選択";
        case TextId::NoRegion: return L"範囲未選択";
        case TextId::RecordingFormat: return L"録画形式";
        case TextId::Quality: return L"画質";
        case TextId::High: return L"高";
        case TextId::Medium: return L"中";
        case TextId::Low: return L"低";
        case TextId::RecordFps: return L"録画 FPS";
        case TextId::StartRecording: return L"録画開始";
        case TextId::StopRecording: return L"録画停止";
        case TextId::ScreenshotFormat: return L"スクリーンショット形式";
        case TextId::SaveScreenshot: return L"スクリーンショット保存";
        case TextId::CompressLast: return L"圧縮";
        case TextId::Settings: return L"設定";
        case TextId::Ready: return L"準備完了";
        case TextId::SettingsTitle: return L"設定";
        case TextId::RecordingFolder: return L"録画保存先";
        case TextId::ScreenshotFolder: return L"画像保存先";
        case TextId::Browse: return L"参照...";
        case TextId::NotifySound: return L"保存完了時に通知音を鳴らす";
        case TextId::Save: return L"保存";
        case TextId::Cancel: return L"キャンセル";
        case TextId::LanguageLabel: return L"言語";
        case TextId::CompressTitle: return L"録画圧縮";
        case TextId::QualityLabel: return L"品質";
        case TextId::EstimatedSize: return L"推定サイズ";
        case TextId::SaveCompressed: return L"圧縮保存";
        case TextId::Close: return L"閉じる";
        case TextId::NoWebpRecording: return L"WEBP録画がまだありません。";
        case TextId::SavingCompressed: return L"圧縮コピーを保存中...";
        case TextId::CompressNeedsWebp: return L"先にWEBPで録画してください。";
        case TextId::CompressionFailed: return L"圧縮に失敗しました。";
        case TextId::RegionSelected: return L"範囲選択完了: ";
        case TextId::RecordingStarted: return L"録画開始";
        case TextId::SavingRecording: return L"録画を保存中...";
        }
        break;
    case Language::English:
        switch (id) {
        case TextId::AppTitle: return L"Webp_DesktopCam";
        case TextId::SelectRegion: return L"Select Region";
        case TextId::NoRegion: return L"No region selected";
        case TextId::RecordingFormat: return L"Recording format";
        case TextId::Quality: return L"Quality";
        case TextId::High: return L"High";
        case TextId::Medium: return L"Medium";
        case TextId::Low: return L"Low";
        case TextId::RecordFps: return L"Record FPS";
        case TextId::StartRecording: return L"Start Recording";
        case TextId::StopRecording: return L"Stop Recording";
        case TextId::ScreenshotFormat: return L"Screenshot format";
        case TextId::SaveScreenshot: return L"Save Screenshot";
        case TextId::CompressLast: return L"Compress Last";
        case TextId::Settings: return L"Settings";
        case TextId::Ready: return L"Ready.";
        case TextId::SettingsTitle: return L"Settings";
        case TextId::RecordingFolder: return L"Recording folder";
        case TextId::ScreenshotFolder: return L"Screenshot folder";
        case TextId::Browse: return L"Browse...";
        case TextId::NotifySound: return L"Play default sound when save completes";
        case TextId::Save: return L"Save";
        case TextId::Cancel: return L"Cancel";
        case TextId::LanguageLabel: return L"Language";
        case TextId::CompressTitle: return L"Compress Recording";
        case TextId::QualityLabel: return L"Quality";
        case TextId::EstimatedSize: return L"Estimated size";
        case TextId::SaveCompressed: return L"Save Compressed";
        case TextId::Close: return L"Close";
        case TextId::NoWebpRecording: return L"No WEBP recording yet.";
        case TextId::SavingCompressed: return L"Saving compressed copy...";
        case TextId::CompressNeedsWebp: return L"Record a WEBP clip first, then compress it here.";
        case TextId::CompressionFailed: return L"Compression failed.";
        case TextId::RegionSelected: return L"Region selected: ";
        case TextId::RecordingStarted: return L"Recording started.";
        case TextId::SavingRecording: return L"Saving recording...";
        }
        break;
    case Language::Korean:
        break;
    }
    switch (id) {
    case TextId::AppTitle: return L"Webp_DesktopCam";
    case TextId::SelectRegion: return L"영역 선택";
    case TextId::NoRegion: return L"선택된 영역 없음";
    case TextId::RecordingFormat: return L"녹화 형식";
    case TextId::Quality: return L"화질";
    case TextId::High: return L"높음";
    case TextId::Medium: return L"중간";
    case TextId::Low: return L"낮음";
    case TextId::RecordFps: return L"녹화 FPS";
    case TextId::StartRecording: return L"녹화 시작";
    case TextId::StopRecording: return L"녹화 중지";
    case TextId::ScreenshotFormat: return L"스크린샷 형식";
    case TextId::SaveScreenshot: return L"스크린샷 저장";
    case TextId::CompressLast: return L"용량 조절";
    case TextId::Settings: return L"설정";
    case TextId::Ready: return L"준비 완료";
    case TextId::SettingsTitle: return L"설정";
    case TextId::RecordingFolder: return L"녹화 저장 위치";
    case TextId::ScreenshotFolder: return L"스크린샷 저장 위치";
    case TextId::Browse: return L"찾아보기...";
    case TextId::NotifySound: return L"저장 완료 알림음 재생";
    case TextId::Save: return L"저장";
    case TextId::Cancel: return L"취소";
    case TextId::LanguageLabel: return L"언어";
    case TextId::CompressTitle: return L"녹화본 용량 조절";
    case TextId::QualityLabel: return L"품질";
    case TextId::EstimatedSize: return L"예상 용량";
    case TextId::SaveCompressed: return L"압축 저장";
    case TextId::Close: return L"닫기";
    case TextId::NoWebpRecording: return L"아직 WEBP 녹화본이 없습니다.";
    case TextId::SavingCompressed: return L"압축본 저장 중...";
    case TextId::CompressNeedsWebp: return L"먼저 WEBP로 녹화한 뒤 압축하세요.";
    case TextId::CompressionFailed: return L"압축에 실패했습니다.";
    case TextId::RegionSelected: return L"영역 선택 완료: ";
    case TextId::RecordingStarted: return L"녹화 시작";
    case TextId::SavingRecording: return L"녹화 저장 중...";
    }
    return L"";
}

std::wstring nowStamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[64]{};
    swprintf_s(buf, L"%04u-%02u-%02u_%02u%02u%02u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring desktopPath() {
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path))) {
        out = path;
        CoTaskMemFree(path);
    }
    if (out.empty()) {
        wchar_t buf[MAX_PATH]{};
        GetCurrentDirectoryW(MAX_PATH, buf);
        out = buf;
    }
    return out;
}

std::wstring moduleDir() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return path;
}

bool ensureDirectory(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring validFolderOrDesktop(const std::wstring& path) {
    return ensureDirectory(path) ? path : desktopPath();
}

std::wstring joinPath(const std::wstring& dir, const std::wstring& file) {
    wchar_t out[MAX_PATH]{};
    wcscpy_s(out, dir.c_str());
    PathAppendW(out, file.c_str());
    return out;
}

void setStatus(const std::wstring& text) {
    SetDlgItemTextW(g.main, 3001, text.c_str());
}

const wchar_t* appInfoText() {
    return L"제작자 : 조마일즈  URL : https://github.com/chomiles  EMAIL : mileschokr@gmail.com\r\n"
           L"이 앱은 오픈소스로 제작되었습니다.";
}

std::wstring initialStatusText() {
    return std::wstring(text(TextId::Ready)) +
        L"\r\n제작자 : 조마일즈  URL : https://github.com/chomiles  EMAIL : mileschokr@gmail.com  이 앱은 오픈소스로 제작되었습니다.";
}

std::wstring vkName(UINT vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= '0' && vk <= '9') return std::wstring(1, static_cast<wchar_t>(vk));
    switch (vk) {
    case VK_SPACE: return L"SPACE";
    case VK_TAB: return L"TAB";
    case VK_ESCAPE: return L"ESC";
    case VK_RETURN: return L"ENTER";
    case VK_INSERT: return L"INSERT";
    case VK_DELETE: return L"DELETE";
    case VK_HOME: return L"HOME";
    case VK_END: return L"END";
    case VK_PRIOR: return L"PAGE UP";
    case VK_NEXT: return L"PAGE DOWN";
    case VK_LEFT: return L"LEFT";
    case VK_RIGHT: return L"RIGHT";
    case VK_UP: return L"UP";
    case VK_DOWN: return L"DOWN";
    default: break;
    }
    wchar_t name[64]{};
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16;
    if (GetKeyNameTextW(static_cast<LONG>(scan), name, 64) > 0) return name;
    return L"VK" + std::to_wstring(vk);
}

std::wstring hotkeyText(const Hotkey& hotkey) {
    std::wstring out;
    if (hotkey.modifiers & MOD_CONTROL) out += L"CTRL+";
    if (hotkey.modifiers & MOD_SHIFT) out += L"SHIFT+";
    if (hotkey.modifiers & MOD_ALT) out += L"ALT+";
    if (hotkey.modifiers & MOD_WIN) out += L"WIN+";
    out += vkName(hotkey.vk);
    return out;
}

bool sameHotkey(const Hotkey& a, const Hotkey& b) {
    return a.modifiers == b.modifiers && a.vk == b.vk;
}

void updateHotkeyLabels() {
    if (!g.main) return;
    SetDlgItemTextW(g.main, 1009, (L"현재 설정된 녹화 바로 시작 키 : " + hotkeyText(g.recordHotkey)).c_str());
    SetDlgItemTextW(g.main, 1010, (L"현재 설정된 스크린샷 저장 키 : " + hotkeyText(g.screenshotHotkey)).c_str());
}

bool registerAppHotkeys() {
    if (!g.main) return false;
    UnregisterHotKey(g.main, kHotkeyRecord);
    UnregisterHotKey(g.main, kHotkeyScreenshot);
    bool rec = RegisterHotKey(g.main, kHotkeyRecord, g.recordHotkey.modifiers, g.recordHotkey.vk) != 0;
    bool shot = RegisterHotKey(g.main, kHotkeyScreenshot, g.screenshotHotkey.modifiers, g.screenshotHotkey.vk) != 0;
    updateHotkeyLabels();
    if (!rec || !shot) {
        setStatus(L"Hotkey registration failed. Another app may already use it.");
    }
    return rec && shot;
}

BOOL CALLBACK applyFontToChild(HWND hwnd, LPARAM font) {
    SendMessageW(hwnd, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
}

void applyUiFont(HWND root) {
    if (!g.uiFont || !root) return;
    SendMessageW(root, WM_SETFONT, reinterpret_cast<WPARAM>(g.uiFont), TRUE);
    EnumChildWindows(root, applyFontToChild, reinterpret_cast<LPARAM>(g.uiFont));
}

std::wstring fileSizeText(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return L"File size: unknown";
    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    double mb = static_cast<double>(size.QuadPart) / (1024.0 * 1024.0);
    wchar_t buf[80]{};
    swprintf_s(buf, L"File size: %.2f MB", mb);
    return buf;
}

RECT normalizeRect(POINT a, POINT b) {
    RECT r{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
    return r;
}

int width(const RECT& r) { return r.right - r.left; }
int height(const RECT& r) { return r.bottom - r.top; }

RECT virtualScreenRect() {
    return {
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)
    };
}

std::wstring selectionText() {
    if (!g.hasSelection) return L"No region selected";
    wchar_t buf[80]{};
    swprintf_s(buf, L"%d x %d", width(g.selected), height(g.selected));
    return buf;
}

std::vector<uint8_t> captureBgra(const RECT& r, int& outW, int& outH) {
    outW = width(r);
    outH = height(r);
    std::vector<uint8_t> pixels(outW * outH * 4);
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = outW;
    bmi.bmiHeader.biHeight = -outH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, outW, outH, screen, r.left, r.top, SRCCOPY);
    std::memcpy(pixels.data(), bits, pixels.size());
    for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return pixels;
}

bool saveWic(const std::wstring& path, const std::vector<uint8_t>& bgra, int w, int h, REFGUID container, float quality) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IWICStream* stream = nullptr;
    IPropertyBag2* props = nullptr;
    const bool isJpeg = IsEqualGUID(container, GUID_ContainerFormatJpeg);
    WICPixelFormatGUID fmt = isJpeg ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
    std::vector<uint8_t> bgr;
    const BYTE* pixelBytes = bgra.data();
    UINT stride = w * 4;
    UINT byteCount = static_cast<UINT>(bgra.size());
    bool ok = false;

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) goto done;
    if (FAILED(factory->CreateStream(&stream))) goto done;
    if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) goto done;
    if (FAILED(factory->CreateEncoder(container, nullptr, &encoder))) goto done;
    if (FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache))) goto done;
    if (FAILED(encoder->CreateNewFrame(&frame, &props))) goto done;
    if (isJpeg && props) {
        PROPBAG2 option{};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value{};
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = quality;
        props->Write(1, &option, &value);
    }
    if (FAILED(frame->Initialize(props))) goto done;
    if (FAILED(frame->SetSize(w, h))) goto done;
    if (FAILED(frame->SetPixelFormat(&fmt))) goto done;
    if (isJpeg) {
        bgr.resize(static_cast<size_t>(w) * h * 3);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                size_t src = (static_cast<size_t>(y) * w + x) * 4;
                size_t dst = (static_cast<size_t>(y) * w + x) * 3;
                bgr[dst + 0] = bgra[src + 0];
                bgr[dst + 1] = bgra[src + 1];
                bgr[dst + 2] = bgra[src + 2];
            }
        }
        pixelBytes = bgr.data();
        stride = w * 3;
        byteCount = static_cast<UINT>(bgr.size());
    }
    if (FAILED(frame->WritePixels(h, stride, byteCount, const_cast<BYTE*>(pixelBytes)))) goto done;
    if (FAILED(frame->Commit())) goto done;
    if (FAILED(encoder->Commit())) goto done;
    ok = true;

done:
    if (props) props->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    return ok;
}

bool encodeWicToMemory(const std::vector<uint8_t>& bgra, int w, int h, REFGUID container, float quality, std::vector<uint8_t>& out) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IStream* stream = nullptr;
    IPropertyBag2* props = nullptr;
    HGLOBAL memory = nullptr;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    bool ok = false;

    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) goto done;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) goto done;
    if (FAILED(factory->CreateEncoder(container, nullptr, &encoder))) goto done;
    if (FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache))) goto done;
    if (FAILED(encoder->CreateNewFrame(&frame, &props))) goto done;
    if (props) {
        PROPBAG2 option{};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value{};
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = quality;
        props->Write(1, &option, &value);
    }
    if (FAILED(frame->Initialize(props))) goto done;
    if (FAILED(frame->SetSize(w, h))) goto done;
    if (FAILED(frame->SetPixelFormat(&fmt))) goto done;
    if (FAILED(frame->WritePixels(h, w * 4, static_cast<UINT>(bgra.size()), const_cast<BYTE*>(bgra.data())))) goto done;
    if (FAILED(frame->Commit())) goto done;
    if (FAILED(encoder->Commit())) goto done;
    if (FAILED(GetHGlobalFromStream(stream, &memory))) goto done;

    {
        SIZE_T size = GlobalSize(memory);
        void* bytes = GlobalLock(memory);
        if (!bytes || size == 0) goto done;
        out.assign(static_cast<uint8_t*>(bytes), static_cast<uint8_t*>(bytes) + size);
        GlobalUnlock(memory);
    }
    ok = true;

done:
    if (props) props->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (factory) factory->Release();
    if (stream) stream->Release();
    return ok;
}

void appendFourCc(std::vector<uint8_t>& out, const char cc[4]) {
    out.insert(out.end(), cc, cc + 4);
}

void appendLe16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void appendLe24(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
}

void appendLe32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

bool extractWebpFrameChunks(const std::vector<uint8_t>& webp, std::vector<uint8_t>& chunks) {
    if (webp.size() < 12) return false;
    if (std::memcmp(webp.data(), "RIFF", 4) != 0 || std::memcmp(webp.data() + 8, "WEBP", 4) != 0) return false;
    size_t pos = 12;
    while (pos + 8 <= webp.size()) {
        uint32_t size = static_cast<uint32_t>(webp[pos + 4]) |
            (static_cast<uint32_t>(webp[pos + 5]) << 8) |
            (static_cast<uint32_t>(webp[pos + 6]) << 16) |
            (static_cast<uint32_t>(webp[pos + 7]) << 24);
        size_t total = 8ull + size + (size & 1u);
        if (pos + total > webp.size()) return false;
        const char* tag = reinterpret_cast<const char*>(webp.data() + pos);
        if (std::memcmp(tag, "VP8 ", 4) == 0 || std::memcmp(tag, "VP8L", 4) == 0 || std::memcmp(tag, "ALPH", 4) == 0) {
            chunks.insert(chunks.end(), webp.begin() + pos, webp.begin() + pos + total);
        }
        pos += total;
    }
    return !chunks.empty();
}

bool saveAnimatedWebp(const std::wstring& path, const std::vector<std::vector<uint8_t>>& frames, int w, int h, int delayMs, int quality) {
    if (frames.empty() || w <= 0 || h <= 0) return false;

    WebPAnimEncoderOptions options{};
    if (!WebPAnimEncoderOptionsInit(&options)) return false;
    options.anim_params.bgcolor = 0xFFFFFFFF;
    options.anim_params.loop_count = 0;
    options.minimize_size = 1;

    WebPConfig config{};
    if (!WebPConfigInit(&config)) return false;
    config.quality = static_cast<float>(quality);
    config.method = 4;
    config.thread_level = 1;
    config.alpha_quality = 100;
    config.exact = 1;
    if (!WebPValidateConfig(&config)) return false;

    WebPAnimEncoder* encoder = WebPAnimEncoderNew(w, h, &options);
    if (!encoder) return false;

    bool ok = true;
    int timestamp = 0;
    for (const auto& frame : frames) {
        WebPPicture picture{};
        if (!WebPPictureInit(&picture)) {
            ok = false;
            break;
        }
        picture.width = w;
        picture.height = h;
        picture.use_argb = 1;
        if (!WebPPictureImportBGRA(&picture, frame.data(), w * 4)) {
            WebPPictureFree(&picture);
            ok = false;
            break;
        }
        if (!WebPAnimEncoderAdd(encoder, &picture, timestamp, &config)) {
            WebPPictureFree(&picture);
            ok = false;
            break;
        }
        WebPPictureFree(&picture);
        timestamp += std::max(10, delayMs);
    }

    WebPData webpData{};
    if (ok) ok = WebPAnimEncoderAdd(encoder, nullptr, timestamp, nullptr) != 0;
    if (ok) ok = WebPAnimEncoderAssemble(encoder, &webpData) != 0;

    if (ok) {
        std::ofstream out(path, std::ios::binary);
        ok = out.good();
        if (ok) {
            out.write(reinterpret_cast<const char*>(webpData.bytes), static_cast<std::streamsize>(webpData.size));
            ok = out.good();
        }
    }

    WebPDataClear(&webpData);
    WebPAnimEncoderDelete(encoder);
    return ok;
}

uint8_t quantize332(uint8_t b, uint8_t gch, uint8_t r) {
    return static_cast<uint8_t>((r & 0xE0) | ((gch & 0xE0) >> 3) | (b >> 6));
}

void writeWord(std::ofstream& f, uint16_t v) {
    f.put(static_cast<char>(v & 0xFF));
    f.put(static_cast<char>((v >> 8) & 0xFF));
}

class BitWriter {
public:
    void write(int code, int bits) {
        buffer_ |= (code << bitCount_);
        bitCount_ += bits;
        while (bitCount_ >= 8) {
            data_.push_back(static_cast<uint8_t>(buffer_ & 0xFF));
            buffer_ >>= 8;
            bitCount_ -= 8;
        }
    }

    std::vector<uint8_t> finish() {
        if (bitCount_ > 0) data_.push_back(static_cast<uint8_t>(buffer_ & 0xFF));
        return data_;
    }

private:
    int buffer_ = 0;
    int bitCount_ = 0;
    std::vector<uint8_t> data_;
};

std::vector<uint8_t> gifLzwUncompressed(const std::vector<uint8_t>& indices) {
    constexpr int clear = 256;
    constexpr int end = 257;
    constexpr int codeSize = 9;
    BitWriter writer;
    writer.write(clear, codeSize);
    int sinceClear = 0;
    for (uint8_t index : indices) {
        if (sinceClear >= 128) {
            writer.write(clear, codeSize);
            sinceClear = 0;
        }
        writer.write(index, codeSize);
        ++sinceClear;
    }
    writer.write(end, codeSize);
    return writer.finish();
}

void writeGifSubBlocks(std::ofstream& f, const std::vector<uint8_t>& data) {
    size_t pos = 0;
    while (pos < data.size()) {
        uint8_t n = static_cast<uint8_t>(std::min<size_t>(255, data.size() - pos));
        f.put(static_cast<char>(n));
        f.write(reinterpret_cast<const char*>(data.data() + pos), n);
        pos += n;
    }
    f.put('\0');
}

bool saveAnimatedGif(const std::wstring& path, const std::vector<std::vector<uint8_t>>& frames, int w, int h, int delayCs) {
    if (frames.empty() || w <= 0 || h <= 0) return false;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write("GIF89a", 6);
    writeWord(f, static_cast<uint16_t>(w));
    writeWord(f, static_cast<uint16_t>(h));
    f.put(static_cast<char>(0xF7));
    f.put('\0');
    f.put('\0');
    for (int i = 0; i < 256; ++i) {
        uint8_t r = static_cast<uint8_t>(((i >> 5) & 0x07) * 255 / 7);
        uint8_t gch = static_cast<uint8_t>(((i >> 2) & 0x07) * 255 / 7);
        uint8_t b = static_cast<uint8_t>((i & 0x03) * 255 / 3);
        f.put(static_cast<char>(r));
        f.put(static_cast<char>(gch));
        f.put(static_cast<char>(b));
    }
    f.put(0x21); f.put(0xFF); f.put(0x0B);
    f.write("NETSCAPE2.0", 11);
    f.put(0x03); f.put(0x01); writeWord(f, 0); f.put(0x00);

    std::vector<uint8_t> indices(static_cast<size_t>(w) * h);
    for (const auto& bgra : frames) {
        for (size_t p = 0, i = 0; p + 3 < bgra.size(); p += 4, ++i) {
            indices[i] = quantize332(bgra[p], bgra[p + 1], bgra[p + 2]);
        }
        f.put(0x21); f.put(0xF9); f.put(0x04); f.put(0x04);
        writeWord(f, static_cast<uint16_t>(std::max(1, delayCs)));
        f.put('\0'); f.put('\0');
        f.put(0x2C);
        writeWord(f, 0); writeWord(f, 0);
        writeWord(f, static_cast<uint16_t>(w));
        writeWord(f, static_cast<uint16_t>(h));
        f.put('\0');
        f.put(8);
        writeGifSubBlocks(f, gifLzwUncompressed(indices));
    }
    f.put(0x3B);
    return true;
}

void saveSettings() {
    WritePrivateProfileStringW(L"capture", L"record_format", g.recordFormat == RecordFormat::Gif ? L"gif" : L"webp", g.iniPath.c_str());
    WritePrivateProfileStringW(L"capture", L"image_format", g.imageFormat == ImageFormat::Png ? L"png" : L"jpg", g.iniPath.c_str());
    WritePrivateProfileStringW(L"capture", L"preset", g.preset == QualityPreset::High ? L"high" : g.preset == QualityPreset::Medium ? L"medium" : L"low", g.iniPath.c_str());
    WritePrivateProfileStringW(L"capture", L"record_fps", std::to_wstring(g.recordFps).c_str(), g.iniPath.c_str());
    WritePrivateProfileStringW(L"capture", L"notify_sound", g.notifySound ? L"1" : L"0", g.iniPath.c_str());
    WritePrivateProfileStringW(L"hotkeys", L"record_mod", std::to_wstring(g.recordHotkey.modifiers).c_str(), g.iniPath.c_str());
    WritePrivateProfileStringW(L"hotkeys", L"record_vk", std::to_wstring(g.recordHotkey.vk).c_str(), g.iniPath.c_str());
    WritePrivateProfileStringW(L"hotkeys", L"screenshot_mod", std::to_wstring(g.screenshotHotkey.modifiers).c_str(), g.iniPath.c_str());
    WritePrivateProfileStringW(L"hotkeys", L"screenshot_vk", std::to_wstring(g.screenshotHotkey.vk).c_str(), g.iniPath.c_str());
    WritePrivateProfileStringW(L"ui", L"language",
        g.language == Language::Korean ? L"ko" : g.language == Language::Japanese ? L"ja" : L"en",
        g.iniPath.c_str());
    WritePrivateProfileStringW(L"paths", L"recording", g.recordingPath.c_str(), g.iniPath.c_str());
    WritePrivateProfileStringW(L"paths", L"screenshot", g.screenshotPath.c_str(), g.iniPath.c_str());
}

std::wstring readIni(const wchar_t* section, const wchar_t* key, const wchar_t* fallback) {
    wchar_t buf[MAX_PATH]{};
    GetPrivateProfileStringW(section, key, fallback, buf, MAX_PATH, g.iniPath.c_str());
    return buf;
}

void loadSettings() {
    g.iniPath = joinPath(moduleDir(), L"Webp_DesktopCam.ini");
    std::wstring desktop = desktopPath();
    g.recordingPath = validFolderOrDesktop(readIni(L"paths", L"recording", desktop.c_str()));
    g.screenshotPath = validFolderOrDesktop(readIni(L"paths", L"screenshot", desktop.c_str()));
    std::wstring rf = readIni(L"capture", L"record_format", L"gif");
    std::wstring im = readIni(L"capture", L"image_format", L"png");
    std::wstring pr = readIni(L"capture", L"preset", L"medium");
    std::wstring fps = readIni(L"capture", L"record_fps", L"24");
    std::wstring sound = readIni(L"capture", L"notify_sound", L"1");
    std::wstring lang = readIni(L"ui", L"language", L"ko");
    std::wstring recMod = readIni(L"hotkeys", L"record_mod", std::to_wstring(MOD_CONTROL).c_str());
    std::wstring recVk = readIni(L"hotkeys", L"record_vk", std::to_wstring(VK_F11).c_str());
    std::wstring shotMod = readIni(L"hotkeys", L"screenshot_mod", std::to_wstring(MOD_CONTROL).c_str());
    std::wstring shotVk = readIni(L"hotkeys", L"screenshot_vk", std::to_wstring(VK_F12).c_str());
    g.recordFormat = rf == L"webp" ? RecordFormat::Webp : RecordFormat::Gif;
    g.imageFormat = im == L"jpg" ? ImageFormat::Jpg : ImageFormat::Png;
    g.preset = pr == L"high" ? QualityPreset::High : pr == L"low" ? QualityPreset::Low : QualityPreset::Medium;
    g.language = lang == L"ja" ? Language::Japanese : lang == L"en" ? Language::English : Language::Korean;
    int parsedFps = _wtoi(fps.c_str());
    if (parsedFps == 8 || parsedFps == 16 || parsedFps == 24 || parsedFps == 30 || parsedFps == 60) {
        g.recordFps = parsedFps;
    }
    g.notifySound = sound != L"0";
    UINT loadedRecVk = static_cast<UINT>(_wtoi(recVk.c_str()));
    UINT loadedShotVk = static_cast<UINT>(_wtoi(shotVk.c_str()));
    g.recordHotkey = {static_cast<UINT>(_wtoi(recMod.c_str())), loadedRecVk ? loadedRecVk : VK_F11};
    g.screenshotHotkey = {static_cast<UINT>(_wtoi(shotMod.c_str())), loadedShotVk ? loadedShotVk : VK_F12};
}

void updateRecordingTimer() {
    HWND timer = GetDlgItem(g.main, 1008);
    if (!timer) return;
    if (!g.recording) {
        SetWindowTextW(timer, L"");
        return;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - g.recordStarted).count();
    wchar_t buf[32]{};
    swprintf_s(buf, L"%02lld:%02lld", elapsed / 60, elapsed % 60);
    SetWindowTextW(timer, buf);
}

void applyLanguageToMain() {
    if (!g.main) return;
    SetWindowTextW(g.main, text(TextId::AppTitle));
    SetDlgItemTextW(g.main, 1001, text(TextId::SelectRegion));
    SetDlgItemTextW(g.main, 2001, text(TextId::RecordingFormat));
    SetDlgItemTextW(g.main, 2002, text(TextId::Quality));
    SetDlgItemTextW(g.main, 1301, text(TextId::High));
    SetDlgItemTextW(g.main, 1302, text(TextId::Medium));
    SetDlgItemTextW(g.main, 1303, text(TextId::Low));
    SetDlgItemTextW(g.main, 2003, text(TextId::RecordFps));
    SetDlgItemTextW(g.main, 2004, text(TextId::ScreenshotFormat));
    SetDlgItemTextW(g.main, 1005, text(TextId::SaveScreenshot));
    SetDlgItemTextW(g.main, 1006, text(TextId::CompressLast));
    SetDlgItemTextW(g.main, 1007, text(TextId::Settings));
}

void updateControls() {
    applyLanguageToMain();
    CheckRadioButton(g.main, 1101, 1102, g.recordFormat == RecordFormat::Gif ? 1101 : 1102);
    CheckRadioButton(g.main, 1201, 1202, g.imageFormat == ImageFormat::Jpg ? 1201 : 1202);
    CheckRadioButton(g.main, 1301, 1303, g.preset == QualityPreset::High ? 1301 : g.preset == QualityPreset::Medium ? 1302 : 1303);
    HWND fps = GetDlgItem(g.main, 1401);
    if (fps) {
        int index = 2;
        if (g.recordFps == 8) index = 0;
        else if (g.recordFps == 16) index = 1;
        else if (g.recordFps == 24) index = 2;
        else if (g.recordFps == 30) index = 3;
        else if (g.recordFps == 60) index = 4;
        SendMessageW(fps, CB_SETCURSEL, index, 0);
    }
    SetDlgItemTextW(g.main, 1002, g.hasSelection ? selectionText().c_str() : text(TextId::NoRegion));
    EnableWindow(GetDlgItem(g.main, 1004), g.hasSelection);
    EnableWindow(GetDlgItem(g.main, 1005), g.hasSelection);
    SetDlgItemTextW(g.main, 1004, g.recording ? text(TextId::StopRecording) : text(TextId::StartRecording));
    updateRecordingTimer();
    updateHotkeyLabels();
}

void moveBorder() {
    if (!g.hasSelection) return;
    if (!g.border) {
        g.border = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            L"Webp_DesktopCamBorder", L"", WS_POPUP, g.selected.left, g.selected.top, width(g.selected), height(g.selected),
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        SetLayeredWindowAttributes(g.border, RGB(0, 0, 0), 255, LWA_COLORKEY);
        SetWindowDisplayAffinity(g.border, WDA_EXCLUDEFROMCAPTURE);
    }
    SetWindowPos(g.border, HWND_TOPMOST, g.selected.left, g.selected.top, width(g.selected), height(g.selected),
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g.border, nullptr, TRUE);
}

void createOverlay() {
    RECT vs = virtualScreenRect();
    g.overlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"Webp_DesktopCamOverlay", L"", WS_POPUP, vs.left, vs.top, width(vs), height(vs),
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetLayeredWindowAttributes(g.overlay, 0, 170, LWA_ALPHA);
    ShowWindow(g.overlay, SW_SHOW);
    SetCapture(g.overlay);
    SetFocus(g.overlay);
    g.selecting = false;
}

void takeScreenshot() {
    if (!g.hasSelection || width(g.selected) <= 0 || height(g.selected) <= 0) {
        setStatus(L"Select a valid region first.");
        return;
    }
    int w = 0, h = 0;
    auto pixels = captureBgra(g.selected, w, h);
    std::wstring ext = g.imageFormat == ImageFormat::Png ? L".png" : L".jpg";
    std::wstring file = L"Screenshot_" + nowStamp() + ext;
    std::wstring path = joinPath(validFolderOrDesktop(g.screenshotPath), file);
    bool ok = saveWic(path, pixels, w, h, g.imageFormat == ImageFormat::Png ? GUID_ContainerFormatPng : GUID_ContainerFormatJpeg, 0.88f);
    if (ok) {
        if (g.notifySound) MessageBeep(MB_OK);
        setStatus(L"Saved: " + path + L"\r\n" + fileSizeText(path));
    }
    else setStatus(L"Error: failed to save screenshot.");
}

void stopRecording();

void startRecording() {
    if (!g.hasSelection || width(g.selected) <= 0 || height(g.selected) <= 0) {
        setStatus(L"Select a valid region first.");
        return;
    }
    g.frames.clear();
    g.recording = true;
    g.blink = true;
    g.recordStarted = std::chrono::steady_clock::now();
    SetTimer(g.main, kTimerRecord, std::max(1, 1000 / g.recordFps), nullptr);
    SetTimer(g.main, kTimerBlink, 1000, nullptr);
    setStatus(text(TextId::RecordingStarted));
    updateControls();
    InvalidateRect(g.border, nullptr, TRUE);
}

void stopRecording() {
    if (!g.recording) return;
    KillTimer(g.main, kTimerRecord);
    KillTimer(g.main, kTimerBlink);
    g.recording = false;
    g.blink = false;
    setStatus(text(TextId::SavingRecording));

    Preset p = presetFor(g.preset);
    int delayCs = std::max(1, 100 / g.recordFps);
    int delayMs = std::max(10, 1000 / g.recordFps);
    std::wstring ext = g.recordFormat == RecordFormat::Webp ? L".webp" : L".gif";
    std::wstring path = joinPath(validFolderOrDesktop(g.recordingPath), L"Recording_" + nowStamp() + ext);
    bool ok = false;
    if (g.recordFormat == RecordFormat::Webp) {
        ok = saveAnimatedWebp(path, g.frames, g.frameWidth, g.frameHeight, delayMs, p.webpQuality);
    } else {
        ok = saveAnimatedGif(path, g.frames, g.frameWidth, g.frameHeight, delayCs);
    }
    if (ok) {
        g.lastRecording = path;
        g.lastRecordingFormat = g.recordFormat;
        g.lastFrames = g.frames;
        g.lastFrameWidth = g.frameWidth;
        g.lastFrameHeight = g.frameHeight;
        g.lastRecordFps = g.recordFps;
        if (g.notifySound) MessageBeep(MB_OK);
        setStatus(L"Saved: " + path + L"\r\n" + fileSizeText(path));
    } else {
        setStatus(g.recordFormat == RecordFormat::Webp
            ? L"Error: failed to save WEBP recording."
            : L"Error: failed to save recording.");
    }
    g.frames.clear();
    updateControls();
    InvalidateRect(g.border, nullptr, TRUE);
}

void recordFrame() {
    int w = 0, h = 0;
    auto pixels = captureBgra(g.selected, w, h);
    if (g.frames.empty()) {
        g.frameWidth = w;
        g.frameHeight = h;
    }
    if (w == g.frameWidth && h == g.frameHeight) g.frames.push_back(std::move(pixels));
}

std::wstring browseForFolder(HWND owner, const std::wstring& current) {
    IFileDialog* dialog = nullptr;
    IShellItem* folder = nullptr;
    IShellItem* result = nullptr;
    PWSTR selected = nullptr;
    std::wstring out;

    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) goto done;
    dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Choose save folder");
    if (SUCCEEDED(SHCreateItemFromParsingName(validFolderOrDesktop(current).c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
        dialog->SetDefaultFolder(folder);
        dialog->SetFolder(folder);
    }
    if (FAILED(dialog->Show(owner))) goto done;
    if (FAILED(dialog->GetResult(&result))) goto done;
    if (FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &selected))) goto done;
    out = selected;

done:
    if (selected) CoTaskMemFree(selected);
    if (result) result->Release();
    if (folder) folder->Release();
    if (dialog) dialog->Release();
    return out;
}

POINT childWindowPositionNear(HWND parent, int childW, int childH) {
    RECT parentRect{};
    GetWindowRect(parent, &parentRect);

    HMONITOR monitor = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(MONITORINFO)};
    GetMonitorInfoW(monitor, &info);
    RECT work = info.rcWork;

    POINT pos{parentRect.right + 12, parentRect.top};
    if (pos.x + childW > work.right) {
        pos.x = parentRect.left;
        pos.y = parentRect.bottom + 12;
    }
    if (pos.x + childW > work.right) pos.x = work.right - childW;
    if (pos.y + childH > work.bottom) pos.y = work.bottom - childH;
    if (pos.x < work.left) pos.x = work.left;
    if (pos.y < work.top) pos.y = work.top;
    return pos;
}

uint64_t fileSizeBytes(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return size.QuadPart;
}

std::wstring compressedRecordingPath(int quality) {
    wchar_t path[MAX_PATH]{};
    wcscpy_s(path, g.lastRecording.c_str());
    PathRemoveExtensionW(path);
    wchar_t suffix[80]{};
    swprintf_s(suffix, L"_compressed_q%d.webp", quality);
    return std::wstring(path) + suffix;
}

void updateCompressionEstimate(HWND dlg) {
    int quality = static_cast<int>(SendDlgItemMessageW(dlg, 5002, TBM_GETPOS, 0, 0));
    if (quality <= 0) quality = 75;
    uint64_t original = fileSizeBytes(g.lastRecording);
    double ratio = std::pow(std::max(1, quality) / 100.0, 1.35);
    double estimate = original > 0 ? (static_cast<double>(original) * ratio / (1024.0 * 1024.0)) : 0.0;
    wchar_t estimateText[160]{};
    swprintf_s(estimateText, L"%s: %d    %s: %.2f MB", text(TextId::QualityLabel), quality, text(TextId::EstimatedSize), estimate);
    SetDlgItemTextW(dlg, 5003, estimateText);
}

void saveCompressedRecording(HWND dlg) {
    if (g.lastRecording.empty() || g.lastRecordingFormat != RecordFormat::Webp || g.lastFrames.empty()) {
        setStatus(L"Compress needs a WEBP recording first.");
        SetDlgItemTextW(dlg, 5004, text(TextId::CompressNeedsWebp));
        return;
    }

    int quality = static_cast<int>(SendDlgItemMessageW(dlg, 5002, TBM_GETPOS, 0, 0));
    quality = std::max(1, std::min(100, quality));
    std::wstring path = compressedRecordingPath(quality);
    int delayMs = std::max(10, 1000 / std::max(1, g.lastRecordFps));

    SetDlgItemTextW(dlg, 5004, text(TextId::SavingCompressed));
    bool ok = saveAnimatedWebp(path, g.lastFrames, g.lastFrameWidth, g.lastFrameHeight, delayMs, quality);

    if (!ok) {
        SetDlgItemTextW(dlg, 5004, text(TextId::CompressionFailed));
        setStatus(L"Error: failed to save compressed WEBP.");
        return;
    }

    std::wstring result = L"Saved: " + path + L"\r\n" + fileSizeText(path);
    SetDlgItemTextW(dlg, 5004, result.c_str());
    setStatus(result);
    if (g.notifySound) MessageBeep(MB_OK);
}

LRESULT CALLBACK CompressWndProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        SetWindowTextW(dlg, text(TextId::CompressTitle));
        CreateWindowW(L"STATIC", text(TextId::QualityLabel), WS_CHILD | WS_VISIBLE, 18, 28, 110, 22, dlg, nullptr, nullptr, nullptr);
        HWND slider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            130, 22, 260, 32, dlg, reinterpret_cast<HMENU>(5002), nullptr, nullptr);
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
        SendMessageW(slider, TBM_SETPOS, TRUE, 75);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 130, 60, 310, 22, dlg, reinterpret_cast<HMENU>(5003), nullptr, nullptr);
        CreateWindowW(L"STATIC", g.lastRecording.empty() ? text(TextId::NoWebpRecording) : g.lastRecording.c_str(),
            WS_CHILD | WS_VISIBLE, 18, 96, 450, 44, dlg, reinterpret_cast<HMENU>(5004), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::SaveCompressed), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            255, 155, 130, 30, dlg, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::Close), WS_CHILD | WS_VISIBLE, 400, 155, 80, 30, dlg, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
        applyUiFont(dlg);
        updateCompressionEstimate(dlg);
        return 0;
    }
    case WM_HSCROLL:
        updateCompressionEstimate(dlg);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            saveCompressedRecording(dlg);
            return 0;
        }
        if (LOWORD(wp) == IDCANCEL) {
            DestroyWindow(dlg);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(dlg);
        return 0;
    case WM_DESTROY:
        g.compress = nullptr;
        return 0;
    }
    return DefWindowProcW(dlg, msg, wp, lp);
}

void showCompressionPanel(HWND parent) {
    if (g.compress) {
        SetForegroundWindow(g.compress);
        return;
    }
    constexpr int dialogW = 520;
    constexpr int dialogH = 230;
    POINT pos = childWindowPositionNear(parent, dialogW, dialogH);
    g.compress = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"Webp_DesktopCamCompress", L"Compress Recording",
        WS_CAPTION | WS_SYSMENU | WS_POPUP, pos.x, pos.y, dialogW, dialogH,
        parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(g.compress, SW_SHOW);
}

LRESULT CALLBACK SettingsWndProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN: {
        if (!g.capturingRecordHotkey && !g.capturingScreenshotHotkey) break;
        UINT vk = static_cast<UINT>(wp);
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) return 0;
        UINT modifiers = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
        if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
        if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
        if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) modifiers |= MOD_WIN;
        Hotkey next{modifiers, vk};
        if (g.capturingRecordHotkey) {
            if (sameHotkey(next, g.screenshotHotkey)) {
                SetDlgItemTextW(dlg, 4011, L"Already used by screenshot.");
            } else {
                g.recordHotkey = next;
                SetDlgItemTextW(dlg, 4011, hotkeyText(g.recordHotkey).c_str());
            }
            g.capturingRecordHotkey = false;
        } else {
            if (sameHotkey(next, g.recordHotkey)) {
                SetDlgItemTextW(dlg, 4013, L"Already used by recording.");
            } else {
                g.screenshotHotkey = next;
                SetDlgItemTextW(dlg, 4013, hotkeyText(g.screenshotHotkey).c_str());
            }
            g.capturingScreenshotHotkey = false;
        }
        ReleaseCapture();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            wchar_t rec[MAX_PATH]{}, shot[MAX_PATH]{};
            GetDlgItemTextW(dlg, 4001, rec, MAX_PATH);
            GetDlgItemTextW(dlg, 4002, shot, MAX_PATH);
            g.recordingPath = validFolderOrDesktop(rec);
            g.screenshotPath = validFolderOrDesktop(shot);
            g.notifySound = SendDlgItemMessageW(dlg, 4005, BM_GETCHECK, 0, 0) == BST_CHECKED;
            int langSel = static_cast<int>(SendDlgItemMessageW(dlg, 4006, CB_GETCURSEL, 0, 0));
            g.language = langSel == 1 ? Language::Japanese : langSel == 2 ? Language::English : Language::Korean;
            saveSettings();
            registerAppHotkeys();
            updateControls();
            setStatus(L"Settings saved.");
            DestroyWindow(dlg);
            return 0;
        }
        if (LOWORD(wp) == IDCANCEL) {
            DestroyWindow(dlg);
            return 0;
        }
        if (LOWORD(wp) == 4003) {
            std::wstring chosen = browseForFolder(dlg, g.recordingPath);
            if (!chosen.empty()) SetDlgItemTextW(dlg, 4001, chosen.c_str());
            return 0;
        }
        if (LOWORD(wp) == 4004) {
            std::wstring chosen = browseForFolder(dlg, g.screenshotPath);
            if (!chosen.empty()) SetDlgItemTextW(dlg, 4002, chosen.c_str());
            return 0;
        }
        if (LOWORD(wp) == 4010) {
            g.capturingRecordHotkey = true;
            g.capturingScreenshotHotkey = false;
            SetDlgItemTextW(dlg, 4011, L"Press keys...");
            SetFocus(dlg);
            SetCapture(dlg);
            return 0;
        }
        if (LOWORD(wp) == 4012) {
            g.capturingScreenshotHotkey = true;
            g.capturingRecordHotkey = false;
            SetDlgItemTextW(dlg, 4013, L"Press keys...");
            SetFocus(dlg);
            SetCapture(dlg);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(dlg);
        return 0;
    case WM_DESTROY:
        g.capturingRecordHotkey = false;
        g.capturingScreenshotHotkey = false;
        ReleaseCapture();
        EnableWindow(g.main, TRUE);
        SetForegroundWindow(g.main);
        g.settings = nullptr;
        return 0;
    }
    return DefWindowProcW(dlg, msg, wp, lp);
}

void forwardHotkeyCaptureMessage(MSG* msg) {
    if (!g.settings || (!g.capturingRecordHotkey && !g.capturingScreenshotHotkey)) return;
    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        SendMessageW(g.settings, WM_KEYDOWN, msg->wParam, msg->lParam);
        msg->message = WM_NULL;
    }
}

void showSimpleSettings(HWND parent) {
    if (g.settings) {
        SetForegroundWindow(g.settings);
        return;
    }
    constexpr int dialogW = 620;
    constexpr int dialogH = 355;
    POINT pos = childWindowPositionNear(parent, dialogW, dialogH);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"Webp_DesktopCamSettings", L"Settings",
        WS_CAPTION | WS_SYSMENU | WS_POPUP, pos.x, pos.y, dialogW, dialogH,
        parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    g.settings = dlg;
    SetWindowTextW(dlg, text(TextId::SettingsTitle));
    CreateWindowW(L"STATIC", text(TextId::RecordingFolder), WS_CHILD | WS_VISIBLE, 18, 22, 120, 22, dlg, nullptr, nullptr, nullptr);
    CreateWindowW(L"EDIT", g.recordingPath.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 20, 360, 24, dlg, reinterpret_cast<HMENU>(4001), nullptr, nullptr);
    CreateWindowW(L"BUTTON", text(TextId::Browse), WS_CHILD | WS_VISIBLE, 515, 19, 80, 26, dlg, reinterpret_cast<HMENU>(4003), nullptr, nullptr);
    CreateWindowW(L"STATIC", text(TextId::ScreenshotFolder), WS_CHILD | WS_VISIBLE, 18, 58, 120, 22, dlg, nullptr, nullptr, nullptr);
    CreateWindowW(L"EDIT", g.screenshotPath.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 56, 360, 24, dlg, reinterpret_cast<HMENU>(4002), nullptr, nullptr);
    CreateWindowW(L"BUTTON", text(TextId::Browse), WS_CHILD | WS_VISIBLE, 515, 55, 80, 26, dlg, reinterpret_cast<HMENU>(4004), nullptr, nullptr);
    CreateWindowW(L"STATIC", text(TextId::LanguageLabel), WS_CHILD | WS_VISIBLE, 18, 96, 120, 22, dlg, nullptr, nullptr, nullptr);
    HWND lang = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        145, 92, 160, 100, dlg, reinterpret_cast<HMENU>(4006), nullptr, nullptr);
    SendMessageW(lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"한국어"));
    SendMessageW(lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"日本語"));
    SendMessageW(lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(lang, CB_SETCURSEL, g.language == Language::Japanese ? 1 : g.language == Language::English ? 2 : 0, 0);
    HWND sound = CreateWindowW(L"BUTTON", text(TextId::NotifySound), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        145, 128, 310, 24, dlg, reinterpret_cast<HMENU>(4005), nullptr, nullptr);
    SendMessageW(sound, BM_SETCHECK, g.notifySound ? BST_CHECKED : BST_UNCHECKED, 0);
    CreateWindowW(L"STATIC", L"Record hotkey", WS_CHILD | WS_VISIBLE, 18, 168, 120, 22, dlg, nullptr, nullptr, nullptr);
    CreateWindowW(L"STATIC", hotkeyText(g.recordHotkey).c_str(), WS_CHILD | WS_VISIBLE | SS_SUNKEN, 145, 166, 180, 24, dlg, reinterpret_cast<HMENU>(4011), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"Set", WS_CHILD | WS_VISIBLE, 340, 164, 70, 28, dlg, reinterpret_cast<HMENU>(4010), nullptr, nullptr);
    CreateWindowW(L"STATIC", L"Screenshot hotkey", WS_CHILD | WS_VISIBLE, 18, 204, 120, 22, dlg, nullptr, nullptr, nullptr);
    CreateWindowW(L"STATIC", hotkeyText(g.screenshotHotkey).c_str(), WS_CHILD | WS_VISIBLE | SS_SUNKEN, 145, 202, 180, 24, dlg, reinterpret_cast<HMENU>(4013), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"Set", WS_CHILD | WS_VISIBLE, 340, 200, 70, 28, dlg, reinterpret_cast<HMENU>(4012), nullptr, nullptr);
    CreateWindowW(L"BUTTON", text(TextId::Save), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 410, 260, 80, 28, dlg, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", text(TextId::Cancel), WS_CHILD | WS_VISIBLE, 505, 260, 80, 28, dlg, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
    applyUiFont(dlg);
    EnableWindow(parent, FALSE);
    ShowWindow(dlg, SW_SHOW);
}

LRESULT CALLBACK BorderProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(dc, &rc, black);
        DeleteObject(black);
        COLORREF color = RGB(0, 120, 255);
        if (g.recording) color = g.blink ? RGB(255, 0, 0) : RGB(128, 0, 0);
        HPEN pen = CreatePen(PS_SOLID, 3, color);
        HGDIOBJ old = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, 1, 1, rc.right - 1, rc.bottom - 1);
        SelectObject(dc, oldBrush);
        SelectObject(dc, old);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            ReleaseCapture();
            DestroyWindow(hwnd);
            g.overlay = nullptr;
            setStatus(L"Region selection canceled.");
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        g.selecting = true;
        g.dragStart = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        g.dragEnd = g.dragStart;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_MOUSEMOVE:
        if (g.selecting) {
            g.dragEnd = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g.selecting) {
            RECT vs = virtualScreenRect();
            POINT a{g.dragStart.x + vs.left, g.dragStart.y + vs.top};
            POINT b{GET_X_LPARAM(lp) + vs.left, GET_Y_LPARAM(lp) + vs.top};
            RECT r = normalizeRect(a, b);
            ReleaseCapture();
            DestroyWindow(hwnd);
            g.overlay = nullptr;
            g.selecting = false;
            if (width(r) <= 0 || height(r) <= 0) {
                setStatus(L"Invalid region.");
                return 0;
            }
            g.selected = r;
            g.hasSelection = true;
            moveBorder();
            updateControls();
            setStatus(std::wstring(text(TextId::RegionSelected)) + selectionText());
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH dim = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(dc, &client, dim);
        DeleteObject(dim);
        if (g.selecting) {
            RECT r = normalizeRect(g.dragStart, g.dragEnd);
            HBRUSH clear = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(dc, &r, clear);
            DeleteObject(clear);
            HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 120, 255));
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
            std::wstring label = std::to_wstring(width(r)) + L" x " + std::to_wstring(height(r));
            SetTextColor(dc, RGB(255, 255, 255));
            SetBkMode(dc, TRANSPARENT);
            int labelY = static_cast<int>(std::max<LONG>(0, r.top - 24));
            TextOutW(dc, r.left + 8, labelY, label.c_str(), static_cast<int>(label.size()));
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (g.overlay == hwnd) g.overlay = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g.main = hwnd;
        CreateWindowW(L"BUTTON", text(TextId::SelectRegion), WS_CHILD | WS_VISIBLE, 16, 16, 120, 30, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
        CreateWindowW(L"STATIC", text(TextId::NoRegion), WS_CHILD | WS_VISIBLE, 150, 22, 220, 22, hwnd, reinterpret_cast<HMENU>(1002), nullptr, nullptr);
        CreateWindowW(L"STATIC", text(TextId::RecordingFormat), WS_CHILD | WS_VISIBLE, 16, 62, 120, 22, hwnd, reinterpret_cast<HMENU>(2001), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"GIF", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 150, 60, 70, 24, hwnd, reinterpret_cast<HMENU>(1101), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"WEBP", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 220, 60, 80, 24, hwnd, reinterpret_cast<HMENU>(1102), nullptr, nullptr);
        CreateWindowW(L"STATIC", text(TextId::Quality), WS_CHILD | WS_VISIBLE, 16, 94, 120, 22, hwnd, reinterpret_cast<HMENU>(2002), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::High), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 150, 92, 70, 24, hwnd, reinterpret_cast<HMENU>(1301), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::Medium), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 220, 92, 90, 24, hwnd, reinterpret_cast<HMENU>(1302), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::Low), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 310, 92, 70, 24, hwnd, reinterpret_cast<HMENU>(1303), nullptr, nullptr);
        CreateWindowW(L"STATIC", text(TextId::RecordFps), WS_CHILD | WS_VISIBLE, 16, 128, 120, 22, hwnd, reinterpret_cast<HMENU>(2003), nullptr, nullptr);
        HWND fps = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            150, 124, 90, 150, hwnd, reinterpret_cast<HMENU>(1401), nullptr, nullptr);
        SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"8"));
        SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"16"));
        SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"24"));
        SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"30"));
        SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"60"));
        CreateWindowW(L"BUTTON", text(TextId::StartRecording), WS_CHILD | WS_VISIBLE, 16, 164, 140, 30, hwnd, reinterpret_cast<HMENU>(1004), nullptr, nullptr);
        HWND timer = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, 16, 198, 140, 26, hwnd, reinterpret_cast<HMENU>(1008), nullptr, nullptr);
        g.timerFont = CreateFontW(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessageW(timer, WM_SETFONT, reinterpret_cast<WPARAM>(g.timerFont), TRUE);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 170, 166, 330, 22, hwnd, reinterpret_cast<HMENU>(1009), nullptr, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 170, 194, 330, 22, hwnd, reinterpret_cast<HMENU>(1010), nullptr, nullptr);
        CreateWindowW(L"STATIC", text(TextId::ScreenshotFormat), WS_CHILD | WS_VISIBLE, 16, 236, 120, 22, hwnd, reinterpret_cast<HMENU>(2004), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"JPG", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 150, 234, 70, 24, hwnd, reinterpret_cast<HMENU>(1201), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"PNG", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 220, 234, 80, 24, hwnd, reinterpret_cast<HMENU>(1202), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::SaveScreenshot), WS_CHILD | WS_VISIBLE, 16, 272, 140, 30, hwnd, reinterpret_cast<HMENU>(1005), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::CompressLast), WS_CHILD | WS_VISIBLE, 170, 272, 120, 30, hwnd, reinterpret_cast<HMENU>(1006), nullptr, nullptr);
        CreateWindowW(L"BUTTON", text(TextId::Settings), WS_CHILD | WS_VISIBLE, 304, 272, 90, 30, hwnd, reinterpret_cast<HMENU>(1007), nullptr, nullptr);
        HWND status = CreateWindowW(L"STATIC", initialStatusText().c_str(), WS_CHILD | WS_VISIBLE | SS_SUNKEN | SS_EDITCONTROL,
            0, 330, 540, 58, hwnd, reinterpret_cast<HMENU>(3001), nullptr, nullptr);
        g.statusFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(g.statusFont), TRUE);
        applyUiFont(hwnd);
        SendMessageW(timer, WM_SETFONT, reinterpret_cast<WPARAM>(g.timerFont), TRUE);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(g.statusFont), TRUE);
        registerAppHotkeys();
        updateControls();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1001: createOverlay(); break;
        case 1004: g.recording ? stopRecording() : startRecording(); break;
        case 1005: takeScreenshot(); break;
        case 1006: showCompressionPanel(hwnd); break;
        case 1007: showSimpleSettings(hwnd); break;
        case 1101: g.recordFormat = RecordFormat::Gif; saveSettings(); updateControls(); break;
        case 1102: g.recordFormat = RecordFormat::Webp; saveSettings(); updateControls(); break;
        case 1201: g.imageFormat = ImageFormat::Jpg; saveSettings(); updateControls(); break;
        case 1202: g.imageFormat = ImageFormat::Png; saveSettings(); updateControls(); break;
        case 1301: g.preset = QualityPreset::High; saveSettings(); updateControls(); break;
        case 1302: g.preset = QualityPreset::Medium; saveSettings(); updateControls(); break;
        case 1303: g.preset = QualityPreset::Low; saveSettings(); updateControls(); break;
        case 1401:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = static_cast<int>(SendDlgItemMessageW(hwnd, 1401, CB_GETCURSEL, 0, 0));
                const int values[] = {8, 16, 24, 30, 60};
                if (sel >= 0 && sel < 5) {
                    g.recordFps = values[sel];
                    saveSettings();
                }
            }
            break;
        }
        return 0;
    case WM_HOTKEY:
        if (wp == kHotkeyRecord) g.recording ? stopRecording() : startRecording();
        if (wp == kHotkeyScreenshot) takeScreenshot();
        return 0;
    case WM_TIMER:
        if (wp == kTimerRecord) recordFrame();
        if (wp == kTimerBlink) {
            g.blink = !g.blink;
            if (g.border) InvalidateRect(g.border, nullptr, TRUE);
            updateRecordingTimer();
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lp) == GetDlgItem(hwnd, 1008)) {
            SetTextColor(reinterpret_cast<HDC>(wp), RGB(220, 0, 0));
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_CTLCOLORBTN:
        SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_CTLCOLORDLG:
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_CTLCOLORLISTBOX:
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        break;
    case WM_DESTROY:
        if (g.recording) stopRecording();
        if (g.uiFont) DeleteObject(g.uiFont);
        if (g.statusFont) DeleteObject(g.statusFont);
        if (g.timerFont) DeleteObject(g.timerFont);
        if (g.appIcon) DestroyIcon(g.appIcon);
        if (g.appIconSmall) DestroyIcon(g.appIconSmall);
        UnregisterHotKey(hwnd, kHotkeyRecord);
        UnregisterHotKey(hwnd, kHotkeyScreenshot);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void registerClasses(HINSTANCE h) {
    WNDCLASSW wc{};
    wc.hInstance = h;
    wc.lpszClassName = L"Webp_DesktopCamMain";
    wc.lpfnWndProc = MainProc;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = g.appIcon ? g.appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);

    wc = {};
    wc.hInstance = h;
    wc.lpszClassName = L"Webp_DesktopCamOverlay";
    wc.lpfnWndProc = OverlayProc;
    wc.hbrBackground = nullptr;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    RegisterClassW(&wc);

    wc = {};
    wc.hInstance = h;
    wc.lpszClassName = L"Webp_DesktopCamBorder";
    wc.lpfnWndProc = BorderProc;
    wc.hbrBackground = nullptr;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    wc = {};
    wc.hInstance = h;
    wc.lpszClassName = L"Webp_DesktopCamSettings";
    wc.lpfnWndProc = SettingsWndProc;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    wc = {};
    wc.hInstance = h;
    wc.lpszClassName = L"Webp_DesktopCamCompress";
    wc.lpfnWndProc = CompressWndProc;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    g.uiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g.appIcon = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPCON), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR));
    g.appIconSmall = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPCON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    loadSettings();
    saveSettings();
    registerClasses(hInstance);
    HWND hwnd = CreateWindowW(L"Webp_DesktopCamMain", L"Webp_DesktopCam", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 450, nullptr, nullptr, hInstance, nullptr);
    g.main = hwnd;
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g.appIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g.appIconSmall));
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        forwardHotkeyCaptureMessage(&msg);
        if (msg.message == WM_NULL) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
