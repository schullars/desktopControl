#include "private/screenCapturePrivate.h"

#include "platform/windows/windowsText.h"

#include <Windows.h>
#include <algorithm>
#include <objidl.h>

namespace Gdiplus {
using std::max;
using std::min;
}

#include <dwmapi.h>

#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>

namespace desktopctl {

ScreenCapturePrivate::ScreenCapturePrivate()
{
    // All public coordinates and enumerated window bounds use physical desktop pixels.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

ScreenCapturePrivate::~ScreenCapturePrivate() = default;

OperationResult ScreenCapturePrivate::windowsFailure(const std::string& operation)
{
    return {false,
            operation + " failed with Win32 error "
                + std::to_string(static_cast<unsigned long>(GetLastError())) + "."};
}

bool ScreenCapturePrivate::isSupportedImageExtension(const std::filesystem::path& path)
{
    const std::wstring extension = path.extension().wstring();
    return _wcsicmp(extension.c_str(), L".jpg") == 0
        || _wcsicmp(extension.c_str(), L".jpeg") == 0
        || _wcsicmp(extension.c_str(), L".bmp") == 0;
}

BOOL CALLBACK ScreenCapturePrivate::enumerateWindow(HWND window, LPARAM context)
{
    auto* enumeration = reinterpret_cast<EnumerationContext*>(context);
    enumeration->owner->collectWindow(window, *enumeration->windows);
    return TRUE;
}

void ScreenCapturePrivate::collectWindow(HWND window, std::vector<WindowRecord>& windows)
{
    if (IsWindowVisible(window) == FALSE || window == GetShellWindow()) {
        return;
    }
    if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) {
        return;
    }
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
        && cloaked != 0) {
        return;
    }

    const int titleLength = GetWindowTextLengthW(window);
    if (titleLength <= 0) {
        return;
    }
    std::wstring title(static_cast<std::size_t>(titleLength) + 1U, L'\0');
    const int copied = GetWindowTextW(window, title.data(), titleLength + 1);
    if (copied <= 0) {
        return;
    }
    title.resize(static_cast<std::size_t>(copied));

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    std::wstring processName;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process != nullptr) {
        std::wstring processPath(32768, L'\0');
        DWORD pathLength = static_cast<DWORD>(processPath.size());
        if (QueryFullProcessImageNameW(process, 0, processPath.data(), &pathLength) != FALSE) {
            processPath.resize(pathLength);
            processName = std::filesystem::path(processPath).filename().wstring();
        }
        CloseHandle(process);
    }
    if (processName.empty()) {
        processName = L"unknown";
    }

    RECT bounds{};
    if (GetWindowRect(window, &bounds) == FALSE || bounds.right <= bounds.left
        || bounds.bottom <= bounds.top) {
        return;
    }

    std::ostringstream id;
    id << "0x" << std::uppercase << std::hex
       << reinterpret_cast<std::uintptr_t>(window);
    WindowRecord record;
    record.handle = window;
    record.title = title;
    record.processName = processName;
    record.info.id = id.str();
    record.info.title = WindowsText::utf8FromWide(title);
    record.info.processId = processId;
    record.info.processName = WindowsText::utf8FromWide(processName);
    record.info.minimized = IsIconic(window) != FALSE;
    record.info.active = GetForegroundWindow() == window;
    record.info.x = bounds.left;
    record.info.y = bounds.top;
    record.info.width = bounds.right - bounds.left;
    record.info.height = bounds.bottom - bounds.top;
    windows.push_back(std::move(record));
}

OperationResult ScreenCapturePrivate::discoverWindows(std::vector<WindowRecord>& windows)
{
    windows.clear();
    EnumerationContext context{this, &windows};
    if (EnumWindows(&ScreenCapturePrivate::enumerateWindow, reinterpret_cast<LPARAM>(&context))
        == FALSE) {
        return windowsFailure("EnumWindows");
    }
    std::sort(windows.begin(), windows.end(), [](const WindowRecord& left,
                                                  const WindowRecord& right) {
        const int processOrder = _wcsicmp(left.processName.c_str(), right.processName.c_str());
        return processOrder == 0 ? _wcsicmp(left.title.c_str(), right.title.c_str()) < 0
                                 : processOrder < 0;
    });
    if (windows.empty()) {
        return {false, "No visible titled application windows were found."};
    }
    return {true, {}};
}

WindowListResult ScreenCapturePrivate::listWindows()
{
    std::vector<WindowRecord> records;
    const OperationResult operation = discoverWindows(records);
    WindowListResult result;
    result.operation = operation;
    if (!operation.success) {
        return result;
    }
    result.windows.reserve(records.size());
    for (WindowRecord& record : records) {
        result.windows.push_back(std::move(record.info));
    }
    return result;
}

OperationResult ScreenCapturePrivate::resolveWindow(
    const std::string& idOrName, std::vector<WindowRecord>& windows,
    const WindowRecord*& selected)
{
    selected = nullptr;
    const OperationResult discovery = discoverWindows(windows);
    if (!discovery.success) {
        return discovery;
    }
    if (idOrName.empty()) {
        return {false, "Window ID, title, or process name cannot be empty."};
    }

    for (const WindowRecord& window : windows) {
        if (_stricmp(window.info.id.c_str(), idOrName.c_str()) == 0) {
            selected = &window;
            return {true, {}};
        }
    }

    const std::wstring query = WindowsText::wideFromUtf8(idOrName);
    if (query.empty()) {
        return {false, "Window query is not valid UTF-8."};
    }
    for (const WindowRecord& window : windows) {
        const bool titleMatches = CompareStringOrdinal(window.title.c_str(), -1, query.c_str(),
                                                       -1, TRUE)
            == CSTR_EQUAL;
        const bool processMatches = CompareStringOrdinal(window.processName.c_str(), -1,
                                                         query.c_str(), -1, TRUE)
            == CSTR_EQUAL;
        if (!titleMatches && !processMatches) {
            continue;
        }
        if (selected != nullptr) {
            return {false, "Window name is ambiguous; use the ID returned by window list."};
        }
        selected = &window;
    }
    if (selected != nullptr) {
        return {true, {}};
    }

    for (const WindowRecord& window : windows) {
        const bool titleMatches = FindStringOrdinal(FIND_FROMSTART, window.title.c_str(), -1,
                                                    query.c_str(), -1, TRUE)
            >= 0;
        const bool processMatches = FindStringOrdinal(FIND_FROMSTART, window.processName.c_str(),
                                                      -1, query.c_str(), -1, TRUE)
            >= 0;
        if (!titleMatches && !processMatches) {
            continue;
        }
        if (selected != nullptr) {
            return {false, "Window query is ambiguous; use the ID returned by window list."};
        }
        selected = &window;
    }
    return selected != nullptr ? OperationResult{true, {}}
                               : OperationResult{false, "Window was not found: " + idOrName};
}

OperationResult ScreenCapturePrivate::requestWindowActivation(HWND window)
{
    if (IsWindow(window) == FALSE) {
        return {false, "The selected window no longer exists."};
    }

    if (IsIconic(window) != FALSE) {
        ShowWindow(window, SW_RESTORE);
    } else {
        ShowWindow(window, SW_SHOW);
    }

    HWND foreground = GetForegroundWindow();
    const DWORD currentThread = GetCurrentThreadId();
    const DWORD foregroundThread = foreground != nullptr
        ? GetWindowThreadProcessId(foreground, nullptr)
        : 0;
    const DWORD targetThread = GetWindowThreadProcessId(window, nullptr);
    const bool attachedForeground = foregroundThread != 0 && foregroundThread != currentThread
        && AttachThreadInput(currentThread, foregroundThread, TRUE) != FALSE;
    const bool attachedTarget = targetThread != 0 && targetThread != currentThread
        && targetThread != foregroundThread
        && AttachThreadInput(currentThread, targetThread, TRUE) != FALSE;

    BringWindowToTop(window);
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(window);
    SetActiveWindow(window);
    SetFocus(window);

    if (attachedTarget) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
    if (attachedForeground) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }

    for (int attempt = 0; attempt < 10 && GetForegroundWindow() != window; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (GetForegroundWindow() != window) {
        return {false,
                "Windows did not grant foreground activation. The target may be elevated, on "
                "another desktop, or blocked by foreground-lock rules."};
    }
    return {true, {}};
}

OperationResult ScreenCapturePrivate::activateWindow(const std::string& idOrName)
{
    std::vector<WindowRecord> windows;
    const WindowRecord* selected = nullptr;
    const OperationResult resolution = resolveWindow(idOrName, windows, selected);
    if (!resolution.success) {
        return resolution;
    }
    const std::string selectedId = selected->info.id;
    const std::string selectedTitle = selected->info.title;
    const OperationResult activation = requestWindowActivation(selected->handle);
    return activation.success
        ? OperationResult{true, "Window activated: " + selectedTitle + " (" + selectedId + ")"}
        : activation;
}

OperationResult ScreenCapturePrivate::resolveOutputPath(
    const std::optional<std::string>& requestedPath, const std::wstring& prefix,
    std::filesystem::path& outputPath)
{
    if (requestedPath && !requestedPath->empty()) {
        const std::filesystem::path requested = std::filesystem::u8path(*requestedPath);
        if (!isSupportedImageExtension(requested)) {
            return {false, "Screenshot output must use a .jpg, .jpeg, or .bmp extension."};
        }
        std::error_code pathError;
        outputPath = std::filesystem::absolute(requested, pathError);
        if (pathError) {
            return {false, "Unable to resolve the screenshot output path: "
                               + pathError.message()};
        }
        return {true, {}};
    }

    std::wstring executablePath(32768, L'\0');
    const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
                                                static_cast<DWORD>(executablePath.size()));
    if (pathLength == 0 || pathLength >= executablePath.size()) {
        return windowsFailure("GetModuleFileName");
    }
    executablePath.resize(pathLength);
    const std::filesystem::path directory =
        std::filesystem::path(executablePath).parent_path() / L"screenshots";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);
    if (directoryError) {
        return {false, "Unable to create the default screenshots directory: "
                           + directoryError.message()};
    }

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::wostringstream fileName;
    fileName << prefix << L'_' << std::setfill(L'0') << std::setw(4) << time.wYear
             << std::setw(2) << time.wMonth << std::setw(2) << time.wDay << L'_'
             << std::setw(2) << time.wHour << std::setw(2) << time.wMinute
             << std::setw(2) << time.wSecond << L'_' << std::setw(3) << time.wMilliseconds;
    const std::wstring baseName = fileName.str();
    outputPath = directory / (baseName + L".jpg");
    for (int suffix = 1; std::filesystem::exists(outputPath) && suffix <= 999; ++suffix) {
        outputPath = directory / (baseName + L'_' + std::to_wstring(suffix) + L".jpg");
    }
    if (std::filesystem::exists(outputPath)) {
        return {false, "Unable to allocate a unique timestamped screenshot filename."};
    }
    return {true, {}};
}

OperationResult ScreenCapturePrivate::readBitmap(
    HDC memoryDc, HBITMAP bitmap, int width, int height, std::vector<std::uint8_t>& pixels)
{
    const std::uint64_t pixelBytes = static_cast<std::uint64_t>(width)
        * static_cast<std::uint64_t>(height) * 4ULL;
    if (width <= 0 || height <= 0
        || pixelBytes > std::numeric_limits<DWORD>::max() - sizeof(BITMAPFILEHEADER)
                - sizeof(BITMAPINFOHEADER)
        || pixelBytes > std::numeric_limits<std::size_t>::max()) {
        return {false, "Screenshot dimensions are too large."};
    }
    pixels.resize(static_cast<std::size_t>(pixelBytes));
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage = static_cast<DWORD>(pixelBytes);
    if (GetDIBits(memoryDc, bitmap, 0, static_cast<UINT>(height), pixels.data(), &bitmapInfo,
                  DIB_RGB_COLORS)
        == 0) {
        return windowsFailure("GetDIBits");
    }
    return {true, {}};
}

OperationResult ScreenCapturePrivate::writeImage(
    const std::filesystem::path& outputPath, int width, int height,
    const std::vector<std::uint8_t>& pixels)
{
    return _wcsicmp(outputPath.extension().c_str(), L".bmp") == 0
        ? writeBmp(outputPath, width, height, pixels)
        : writeJpeg(outputPath, width, height, pixels);
}

OperationResult ScreenCapturePrivate::writeBmp(
    const std::filesystem::path& outputPath, int width, int height,
    const std::vector<std::uint8_t>& pixels)
{
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER bitmapHeader{};
    bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapHeader.biWidth = width;
    bitmapHeader.biHeight = -height;
    bitmapHeader.biPlanes = 1;
    bitmapHeader.biBitCount = 32;
    bitmapHeader.biCompression = BI_RGB;
    bitmapHeader.biSizeImage = static_cast<DWORD>(pixels.size());

    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        return {false, "Unable to open screenshot output file."};
    }
    output.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    output.write(reinterpret_cast<const char*>(&bitmapHeader), sizeof(bitmapHeader));
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    if (!output) {
        return {false, "Failed while writing screenshot output file."};
    }
    return {true, {}};
}

OperationResult ScreenCapturePrivate::writeJpeg(
    const std::filesystem::path& outputPath, int width, int height,
    const std::vector<std::uint8_t>& pixels)
{
    if (width > std::numeric_limits<int>::max() / 4) {
        return {false, "Screenshot width is too large for JPEG encoding."};
    }

    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR token = 0;
    const Gdiplus::Status startupStatus =
        Gdiplus::GdiplusStartup(&token, &startupInput, nullptr);
    if (startupStatus != Gdiplus::Ok) {
        return {false, "GDI+ startup failed with status "
                           + std::to_string(static_cast<int>(startupStatus)) + "."};
    }

    Gdiplus::Status status = Gdiplus::GenericError;
    {
        Gdiplus::Bitmap image(width, height, width * 4, PixelFormat32bppRGB,
                              const_cast<BYTE*>(pixels.data()));
        status = image.GetLastStatus();
        if (status == Gdiplus::Ok) {
            const CLSID jpegEncoder = {0x557cf401,
                                       0x1a04,
                                       0x11d3,
                                       {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
            ULONG quality = 95;
            Gdiplus::EncoderParameters parameters{};
            parameters.Count = 1;
            parameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
            parameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
            parameters.Parameter[0].NumberOfValues = 1;
            parameters.Parameter[0].Value = &quality;
            status = image.Save(outputPath.c_str(), &jpegEncoder, &parameters);
        }
    }
    Gdiplus::GdiplusShutdown(token);

    return status == Gdiplus::Ok
        ? OperationResult{true, {}}
        : OperationResult{false,
                          "JPEG encoding failed with GDI+ status "
                              + std::to_string(static_cast<int>(status)) + "."};
}

ScreenshotResult ScreenCapturePrivate::createScreenshotResult(
    const std::filesystem::path& outputPath)
{
    ScreenshotResult result;
    result.operation = {true, {}};
    result.path = outputPath.u8string();
    result.directory = outputPath.parent_path().u8string();

    std::uint64_t count = 0;
    std::error_code enumerationError;
    std::filesystem::directory_iterator iterator(outputPath.parent_path(), enumerationError);
    const std::filesystem::directory_iterator end;
    while (!enumerationError && iterator != end) {
        const bool regularFile = iterator->is_regular_file(enumerationError);
        if (enumerationError) {
            break;
        }
        if (regularFile && isSupportedImageExtension(iterator->path())) {
            ++count;
        }
        iterator.increment(enumerationError);
    }

    if (enumerationError) {
        result.message = "Screenshot saved, but the image count could not be read: "
            + enumerationError.message();
        return result;
    }

    result.imageCount = count;
    result.cleanupRecommended = count > 50;
    result.message = "Screenshot saved. The output directory contains "
        + std::to_string(count) + " image file(s).";
    if (result.cleanupRecommended) {
        result.message += " More than 50 images are present; remove screenshots you no longer need.";
    } else {
        result.message += " No cleanup is currently recommended.";
    }
    return result;
}

ScreenshotResult ScreenCapturePrivate::captureDesktopRegion(
    const ScreenRegion& region, const std::filesystem::path& outputPath)
{
    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        return {windowsFailure("GetDC")};
    }
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = memoryDc != nullptr
        ? CreateCompatibleBitmap(screenDc, region.width, region.height)
        : nullptr;
    HGDIOBJ previousObject = bitmap != nullptr ? SelectObject(memoryDc, bitmap) : nullptr;
    if (memoryDc == nullptr || bitmap == nullptr || previousObject == nullptr
        || previousObject == HGDI_ERROR) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (memoryDc != nullptr) {
            DeleteDC(memoryDc);
        }
        ReleaseDC(nullptr, screenDc);
        return {windowsFailure("Create screenshot bitmap")};
    }

    OperationResult operation;
    if (BitBlt(memoryDc, 0, 0, region.width, region.height, screenDc, region.x, region.y,
               SRCCOPY | CAPTUREBLT)
        == FALSE) {
        operation = windowsFailure("BitBlt");
    } else {
        std::vector<std::uint8_t> pixels;
        operation = readBitmap(memoryDc, bitmap, region.width, region.height, pixels);
        if (operation.success) {
            operation = writeImage(outputPath, region.width, region.height, pixels);
        }
    }

    SelectObject(memoryDc, previousObject);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return operation.success ? createScreenshotResult(outputPath)
                             : ScreenshotResult{operation};
}

ScreenshotResult ScreenCapturePrivate::captureWindowTarget(
    HWND window, const std::filesystem::path& outputPath)
{
    RECT bounds{};
    if (GetWindowRect(window, &bounds) == FALSE) {
        return {windowsFailure("GetWindowRect")};
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) {
        return {{false, "The target window has invalid bounds."}};
    }

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = screenDc != nullptr ? CreateCompatibleDC(screenDc) : nullptr;
    HBITMAP bitmap = memoryDc != nullptr ? CreateCompatibleBitmap(screenDc, width, height)
                                         : nullptr;
    HGDIOBJ previousObject = bitmap != nullptr ? SelectObject(memoryDc, bitmap) : nullptr;
    if (screenDc == nullptr || memoryDc == nullptr || bitmap == nullptr
        || previousObject == nullptr || previousObject == HGDI_ERROR) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (memoryDc != nullptr) {
            DeleteDC(memoryDc);
        }
        if (screenDc != nullptr) {
            ReleaseDC(nullptr, screenDc);
        }
        return {windowsFailure("Create window screenshot bitmap")};
    }

    OperationResult operation;
    if (PrintWindow(window, memoryDc, PW_RENDERFULLCONTENT) == FALSE) {
        operation = windowsFailure("PrintWindow");
    } else {
        std::vector<std::uint8_t> pixels;
        operation = readBitmap(memoryDc, bitmap, width, height, pixels);
        if (operation.success) {
            operation = writeImage(outputPath, width, height, pixels);
        }
    }

    SelectObject(memoryDc, previousObject);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return operation.success ? createScreenshotResult(outputPath)
                             : ScreenshotResult{operation};
}

ScreenshotResult ScreenCapturePrivate::captureDesktop(
    const std::optional<std::string>& outputPath)
{
    const ScreenRegion desktop{GetSystemMetrics(SM_XVIRTUALSCREEN),
                               GetSystemMetrics(SM_YVIRTUALSCREEN),
                               GetSystemMetrics(SM_CXVIRTUALSCREEN),
                               GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    return captureRegion(desktop, outputPath);
}

ScreenshotResult ScreenCapturePrivate::captureRegion(
    const ScreenRegion& region, const std::optional<std::string>& outputPath)
{
    const int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const std::int64_t right = static_cast<std::int64_t>(region.x) + region.width;
    const std::int64_t bottom = static_cast<std::int64_t>(region.y) + region.height;
    if (region.width <= 0 || region.height <= 0 || region.x < virtualX || region.y < virtualY
        || right > static_cast<std::int64_t>(virtualX) + virtualWidth
        || bottom > static_cast<std::int64_t>(virtualY) + virtualHeight) {
        return {{false, "Screenshot region is outside the current virtual desktop."}};
    }
    std::filesystem::path path;
    const OperationResult resolved = resolveOutputPath(outputPath, L"screenshot", path);
    return resolved.success ? captureDesktopRegion(region, path) : ScreenshotResult{resolved};
}

ScreenshotResult ScreenCapturePrivate::captureWindow(
    const std::string& idOrName, const std::optional<std::string>& outputPath)
{
    std::vector<WindowRecord> windows;
    const WindowRecord* selected = nullptr;
    const OperationResult resolution = resolveWindow(idOrName, windows, selected);
    if (!resolution.success) {
        return {resolution};
    }
    std::filesystem::path path;
    const OperationResult outputResolution = resolveOutputPath(outputPath, L"window", path);
    return outputResolution.success ? captureWindowTarget(selected->handle, path)
                                    : ScreenshotResult{outputResolution};
}

} // namespace desktopctl
