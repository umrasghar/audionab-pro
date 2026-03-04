#include "context_menu.h"
#include "constants.h"
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "advapi32.lib")

static const wchar_t* SHELL_KEY = L"Software\\Classes\\*\\shell\\AudioNabPro";

namespace ContextMenu {

bool Install(const std::wstring& exePath) {
    HKEY hKey = nullptr;
    HKEY hCmd = nullptr;
    bool ok = false;

    // Create shell key: *\shell\AudioNabPro
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SHELL_KEY, 0, nullptr,
                        0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {

        // Display name
        std::wstring label = L"Convert with AudioNab Pro";
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(label.c_str()),
                       static_cast<DWORD>((label.size() + 1) * sizeof(wchar_t)));

        // Icon
        RegSetValueExW(hKey, L"Icon", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(exePath.c_str()),
                       static_cast<DWORD>((exePath.size() + 1) * sizeof(wchar_t)));

        // AppliesTo: supported extensions
        std::wstring appliesTo;
        for (auto ext : AppDef::VIDEO_EXTS) {
            if (!appliesTo.empty()) appliesTo += L" OR ";
            appliesTo += L"System.FileName:\"*";
            appliesTo += ext;
            appliesTo += L"\"";
        }
        for (auto ext : AppDef::AUDIO_EXTS) {
            if (!appliesTo.empty()) appliesTo += L" OR ";
            appliesTo += L"System.FileName:\"*";
            appliesTo += ext;
            appliesTo += L"\"";
        }
        RegSetValueExW(hKey, L"AppliesTo", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(appliesTo.c_str()),
                       static_cast<DWORD>((appliesTo.size() + 1) * sizeof(wchar_t)));

        // Command
        std::wstring cmdKey = std::wstring(SHELL_KEY) + L"\\command";
        if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKey.c_str(), 0, nullptr,
                            0, KEY_WRITE, nullptr, &hCmd, nullptr) == ERROR_SUCCESS) {
            std::wstring cmd = L"\"" + exePath + L"\" \"%1\"";
            RegSetValueExW(hCmd, nullptr, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(cmd.c_str()),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hCmd);
            ok = true;
        }
        RegCloseKey(hKey);
    }

    return ok;
}

bool Uninstall() {
    // Delete the command subkey first, then the shell key
    std::wstring cmdKey = std::wstring(SHELL_KEY) + L"\\command";
    RegDeleteKeyW(HKEY_CURRENT_USER, cmdKey.c_str());
    return RegDeleteKeyW(HKEY_CURRENT_USER, SHELL_KEY) == ERROR_SUCCESS;
}

bool IsInstalled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SHELL_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

} // namespace ContextMenu
