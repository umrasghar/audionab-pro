#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

namespace Helpers {
    // UTF conversion
    std::wstring Utf8ToWide(const char* utf8);
    std::wstring Utf8ToWide(const std::string& utf8);
    std::string  WideToUtf8(const wchar_t* wide);
    std::string  WideToUtf8(const std::wstring& wide);

    // Formatting
    std::wstring FormatSize(int64_t bytes);
    std::wstring FormatTimeAgo(int64_t unixTime);
    std::wstring FormatDuration(double seconds);

    // File utilities
    std::wstring GetFileExtension(const std::wstring& path);
    std::wstring GetFileName(const std::wstring& path);
    std::wstring GetFileNameNoExt(const std::wstring& path);
    std::wstring GetDirectory(const std::wstring& path);
    bool         FileExists(const std::wstring& path);
    int64_t      GetFileSize(const std::wstring& path);

    // App data directory (%LOCALAPPDATA%\AudioNab)
    std::wstring GetAppDataDir();

    // DPI helpers
    float GetDpiScale(HWND hwnd);

    // Time
    int64_t GetUnixTime();
}
