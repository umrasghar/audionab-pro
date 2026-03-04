#include "tray.h"
#include "constants.h"
#include <strsafe.h>

bool TrayIcon::Init(HWND hwnd, HINSTANCE hInst) {
    hwnd_ = hwnd;

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    StringCchCopyW(nid_.szTip, ARRAYSIZE(nid_.szTip), AppDef::NAME);
    nid_.uVersion = NOTIFYICON_VERSION_4;

    initialized_ = true;
    return true;
}

void TrayIcon::Destroy() {
    if (visible_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        visible_ = false;
    }
    if (nid_.hIcon) {
        DestroyIcon(nid_.hIcon);
        nid_.hIcon = nullptr;
    }
    initialized_ = false;
}

void TrayIcon::Show() {
    if (!initialized_) return;
    if (!visible_) {
        Shell_NotifyIconW(NIM_ADD, &nid_);
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
        visible_ = true;
    }
}

void TrayIcon::Hide() {
    if (visible_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        visible_ = false;
    }
}

void TrayIcon::SetTooltip(const wchar_t* text) {
    StringCchCopyW(nid_.szTip, ARRAYSIZE(nid_.szTip), text);
    if (visible_) {
        Shell_NotifyIconW(NIM_MODIFY, &nid_);
    }
}

void TrayIcon::ShowBalloon(const wchar_t* title, const wchar_t* msg, DWORD flags) {
    nid_.uFlags |= NIF_INFO;
    StringCchCopyW(nid_.szInfoTitle, ARRAYSIZE(nid_.szInfoTitle), title);
    StringCchCopyW(nid_.szInfo, ARRAYSIZE(nid_.szInfo), msg);
    nid_.dwInfoFlags = flags;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags &= ~NIF_INFO;
}

void TrayIcon::HandleMessage(WPARAM wParam, LPARAM lParam) {
    UINT msg = LOWORD(lParam);

    switch (msg) {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
        // Restore window
        ShowWindow(hwnd_, SW_SHOW);
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        break;

    case WM_RBUTTONUP:
        ShowContextMenu();
        break;
    }
}

void TrayIcon::ShowContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SHOW, L"Show AudioNab");
    AppendMenuW(hMenu, MF_STRING, IDM_CONVERT, L"Open File...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}
