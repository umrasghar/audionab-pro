#pragma once
#include <windows.h>
#include <string>

namespace ContextMenu {
    // Install context menu entries for supported file types
    bool Install(const std::wstring& exePath);

    // Remove all context menu entries
    bool Uninstall();

    // Check if context menu is installed
    bool IsInstalled();
}
