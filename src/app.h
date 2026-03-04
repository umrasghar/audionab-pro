#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "config.h"
#include "database.h"
#include "converter.h"
#include "tray.h"
#include "watcher.h"
#include "ui/renderer.h"
#include "ui/controls.h"
#include "ui/dropdown.h"

class App {
public:
    static App& Instance();

    bool Init(HINSTANCE hInst, int nCmdShow);
    int  Run();
    void Shutdown();

    // Public access
    Renderer& GetRenderer()  { return renderer_; }
    Database& GetDatabase()  { return db_; }
    AppConfig& GetConfig()   { return config_; }
    HWND       GetHwnd()     { return hwnd_; }

    // Actions
    void OpenFileDialog();
    void ConvertFile(const std::wstring& path);
    void ConvertFiles(const std::vector<std::wstring>& paths);
    void CancelConversion();
    void RefreshHistory();
    void ShowToast(Toast::Type type, const std::wstring& msg);

    // Tray / Watch
    void StartWatcher();
    void StopWatcher();

    // State
    bool IsConverting() const { return converting_; }
    float ConvertProgress() const { return convertProgress_; }
    const std::wstring& ConvertStatus() const { return convertStatus_; }

    // Stats
    int StatTotal()   const { return statTotal_; }
    int StatSuccess() const { return statSuccess_; }
    int StatFailed()  const { return statFailed_; }
    const std::vector<HistoryEntry>& History() const { return history_; }

    // Window message handler
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    App() = default;

    // Window setup
    bool RegisterWindowClass(HINSTANCE hInst);
    bool CreateAppWindow(HINSTANCE hInst, int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Painting
    void OnPaint();
    void OnResize(UINT width, UINT height);
    void Layout(float width, float height);

    // Input
    void OnMouseMove(float x, float y);
    void OnMouseDown(float x, float y);
    void OnMouseUp(float x, float y);
    void OnKeyDown(WPARAM vk);
    void OnDropFiles(HDROP hDrop);

    // Initialization
    void InitializeControls();

    // Conversion thread
    static unsigned __stdcall ConvertThreadProc(void* param);

    // UI controls
    Button      btnNab_;
    Button      btnCancel_;
    Button      btnOpenFolder_;
    Button      btnSettings_;
    Dropdown    ddFormat_;
    Dropdown    ddBitrate_;
    StatCard    cardTotal_;
    StatCard    cardSuccess_;
    StatCard    cardFailed_;
    ProgressBar progressBar_;
    Toast       toast_;
    StatusDot   statusDot_;

    // Drag-drop overlay
    bool        dragOver_ = false;

    // Layout regions
    D2D1_RECT_F headerRect_  = {};
    D2D1_RECT_F statsRect_   = {};
    D2D1_RECT_F historyRect_ = {};
    D2D1_RECT_F statusRect_  = {};

    // State
    HWND        hwnd_ = nullptr;
    HINSTANCE   hInst_ = nullptr;
    Renderer    renderer_;
    Database    db_;
    AppConfig   config_;
    TrayIcon    tray_;
    FolderWatcher watcher_;

    bool        converting_ = false;
    float       convertProgress_ = 0.0f;
    std::wstring convertStatus_;
    HANDLE      cancelEvent_ = nullptr;
    HANDLE      convertThread_ = nullptr;

    std::vector<HistoryEntry> history_;
    int statTotal_   = 0;
    int statSuccess_ = 0;
    int statFailed_  = 0;

    // Scroll state
    float scrollY_ = 0.0f;
    float scrollTarget_ = 0.0f;
    float maxScroll_ = 0.0f;

    // Search/filter
    std::wstring searchText_;
    int filterIndex_ = 0; // 0=All, 1=Success, 2=Failed
};
