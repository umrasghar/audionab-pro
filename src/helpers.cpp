#include "helpers.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <chrono>
#include <ctime>

#pragma comment(lib, "shlwapi.lib")

namespace Helpers {

// --- UTF conversion ---

std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8 || !*utf8) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result.data(), len);
    return result;
}

std::wstring Utf8ToWide(const std::string& utf8) {
    return Utf8ToWide(utf8.c_str());
}

std::string WideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), len, nullptr, nullptr);
    return result;
}

std::string WideToUtf8(const std::wstring& wide) {
    return WideToUtf8(wide.c_str());
}

// --- Formatting ---

std::wstring FormatSize(int64_t bytes) {
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB" };
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }
    wchar_t buf[64];
    if (unit == 0)
        swprintf(buf, 64, L"%lld B", bytes);
    else
        swprintf(buf, 64, L"%.1f %s", size, units[unit]);
    return buf;
}

std::wstring FormatTimeAgo(int64_t unixTime) {
    int64_t now = GetUnixTime();
    int64_t diff = now - unixTime;

    if (diff < 60)       return L"Just now";
    if (diff < 3600)     { auto m = diff / 60;   wchar_t b[64]; swprintf(b, 64, L"%lld min ago", m);  return b; }
    if (diff < 86400)    { auto h = diff / 3600;  wchar_t b[64]; swprintf(b, 64, L"%lld hr ago", h);   return b; }
    if (diff < 604800)   { auto d = diff / 86400; wchar_t b[64]; swprintf(b, 64, L"%lld day%s ago", d, d > 1 ? L"s" : L""); return b; }

    // Format as date
    time_t t = static_cast<time_t>(unixTime);
    struct tm tm;
    localtime_s(&tm, &t);
    wchar_t buf[64];
    wcsftime(buf, 64, L"%b %d, %Y", &tm);
    return buf;
}

std::wstring FormatDuration(double seconds) {
    int total = static_cast<int>(seconds);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    wchar_t buf[64];
    if (h > 0)
        swprintf(buf, 64, L"%d:%02d:%02d", h, m, s);
    else
        swprintf(buf, 64, L"%d:%02d", m, s);
    return buf;
}

// --- File utilities ---

std::wstring GetFileExtension(const std::wstring& path) {
    auto pos = path.rfind(L'.');
    if (pos == std::wstring::npos) return {};
    std::wstring ext = path.substr(pos);
    for (auto& c : ext) c = towlower(c);
    return ext;
}

std::wstring GetFileName(const std::wstring& path) {
    auto pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

std::wstring GetFileNameNoExt(const std::wstring& path) {
    std::wstring name = GetFileName(path);
    auto pos = name.rfind(L'.');
    return (pos == std::wstring::npos) ? name : name.substr(0, pos);
}

std::wstring GetDirectory(const std::wstring& path) {
    auto pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
}

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

int64_t GetFileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
        return -1;
    LARGE_INTEGER li;
    li.HighPart = data.nFileSizeHigh;
    li.LowPart  = data.nFileSizeLow;
    return li.QuadPart;
}

// --- App data ---

std::wstring GetAppDataDir() {
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        std::wstring dir = std::wstring(path) + L"\\AudioNab";
        CoTaskMemFree(path);
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L".";
}

// --- DPI ---

float GetDpiScale(HWND hwnd) {
    UINT dpi = 96;
    // Try GetDpiForWindow (Win10 1607+)
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static auto fn = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn && hwnd)
        dpi = fn(hwnd);
    else {
        HDC hdc = GetDC(nullptr);
        dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
    }
    return dpi / 96.0f;
}

// --- Time ---

int64_t GetUnixTime() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace Helpers
