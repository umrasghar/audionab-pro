#pragma once
#include <windows.h>
#include <shellapi.h>

class App;

class TrayIcon {
public:
    bool Init(HWND hwnd, HINSTANCE hInst);
    void Destroy();
    void SetTooltip(const wchar_t* text);
    void ShowBalloon(const wchar_t* title, const wchar_t* msg, DWORD flags = NIIF_INFO);
    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }

    // Handle tray messages (WM_APP+50)
    void HandleMessage(WPARAM wParam, LPARAM lParam);

    // Tray menu IDs
    static constexpr UINT IDM_SHOW    = 5001;
    static constexpr UINT IDM_CONVERT = 5002;
    static constexpr UINT IDM_EXIT    = 5003;

    static constexpr UINT WM_TRAYICON = WM_APP + 50;

private:
    void ShowContextMenu();

    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    bool visible_ = false;
    bool initialized_ = false;
};
