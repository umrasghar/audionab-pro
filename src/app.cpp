#include "app.h"
#include "constants.h"
#include "helpers.h"
#include "ui/theme.h"
#include <commdlg.h>
#include <shellapi.h>
#include <process.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

App& App::Instance() {
    static App instance;
    return instance;
}

bool App::Init(HINSTANCE hInst, int nCmdShow) {
    hInst_ = hInst;

    // Load config
    Config::Load(config_);

    // Open database
    if (!db_.Open()) {
        MessageBoxW(nullptr, L"Failed to open database.", App::NAME, MB_ICONERROR);
        return false;
    }

    // Init converter
    Converter::Init();

    // Create cancel event
    cancelEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // Create window
    if (!RegisterWindowClass(hInst)) return false;
    if (!CreateAppWindow(hInst, nCmdShow)) return false;

    // Init renderer
    if (!renderer_.Init(hwnd_)) {
        MessageBoxW(nullptr, L"Failed to initialize Direct2D.", App::NAME, MB_ICONERROR);
        return false;
    }

    // Initialize controls
    InitializeControls();

    // Load history
    RefreshHistory();

    return true;
}

void App::InitializeControls() {
    // "Nab Audio" primary button
    btnNab_.text = L"\xD83C\xDFB5  Nab Audio";  // Musical note emoji
    btnNab_.bgColor    = Theme::Accent;
    btnNab_.hoverColor = Theme::AccentHover;
    btnNab_.pressColor = Theme::AccentPress;
    btnNab_.textColor  = { 1, 1, 1, 1 };
    btnNab_.cornerRadius = Theme::CornerRadius;
    btnNab_.isPrimary = true;

    // Settings button
    btnSettings_.text = L"\x2699  Settings";  // Gear
    btnSettings_.bgColor    = Theme::BgButton;
    btnSettings_.hoverColor = Theme::BgBtnHover;
    btnSettings_.pressColor = Theme::BgCardAlt;
    btnSettings_.textColor  = Theme::TextPrimary;
    btnSettings_.cornerRadius = Theme::CornerSmall;

    // Open folder button
    btnOpenFolder_.text = L"\xD83D\xDCC2  Open Folder";
    btnOpenFolder_.bgColor    = Theme::BgButton;
    btnOpenFolder_.hoverColor = Theme::BgBtnHover;
    btnOpenFolder_.pressColor = Theme::BgCardAlt;
    btnOpenFolder_.textColor  = Theme::TextPrimary;
    btnOpenFolder_.cornerRadius = Theme::CornerSmall;

    // Stat cards
    cardTotal_.accentColor = Theme::Accent;
    cardTotal_.bgColor     = Theme::BgCard;
    cardTotal_.label       = L"Total";

    cardSuccess_.accentColor = Theme::Success;
    cardSuccess_.bgColor     = Theme::BgCard;
    cardSuccess_.label       = L"Success";

    cardFailed_.accentColor = Theme::Error;
    cardFailed_.bgColor     = Theme::BgCard;
    cardFailed_.label       = L"Failed";

    // Progress bar
    progressBar_.trackColor = Theme::BgInput;
    progressBar_.fillColor  = Theme::Accent;
    progressBar_.visible = false;

    // Status dot
    statusDot_.color = Theme::Success;
    statusDot_.radius = Theme::StatusDotRadius;

    // Toast
    toast_.active = false;
}

int App::Run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void App::Shutdown() {
    CancelConversion();
    Config::Save(config_);
    db_.Close();
    if (cancelEvent_) { CloseHandle(cancelEvent_); cancelEvent_ = nullptr; }
}

// --- Window creation ---

bool App::RegisterWindowClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInst;
    wc.hIcon          = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName  = ::App::WINDOW_CLASS;
    return RegisterClassExW(&wc) != 0;
}

bool App::CreateAppWindow(HINSTANCE hInst, int nCmdShow) {
    int x = config_.windowX >= 0 ? config_.windowX : CW_USEDEFAULT;
    int y = config_.windowY >= 0 ? config_.windowY : CW_USEDEFAULT;
    int w = config_.windowW > 0  ? config_.windowW : ::App::INITIAL_WIDTH;
    int h = config_.windowH > 0  ? config_.windowH : ::App::INITIAL_HEIGHT;

    hwnd_ = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        ::App::WINDOW_CLASS,
        ::App::NAME,
        WS_OVERLAPPEDWINDOW,
        x, y, w, h,
        nullptr, nullptr, hInst, this);

    if (!hwnd_) return false;

    // Dark title bar on Windows 10+
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

    ShowWindow(hwnd_, config_.startMinimized ? SW_SHOWMINIMIZED : nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

// --- WndProc ---

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* app = nullptr;

    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app) {
        return app->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_GETMINMAXINFO: {
        auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = ::App::MIN_WIDTH;
        mmi->ptMinTrackSize.y = ::App::MIN_HEIGHT;
        return 0;
    }

    case WM_MOUSEMOVE:
        OnMouseMove(static_cast<float>(LOWORD(lParam)) / renderer_.DpiScale(),
                     static_cast<float>(HIWORD(lParam)) / renderer_.DpiScale());
        return 0;

    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);
        OnMouseDown(static_cast<float>(LOWORD(lParam)) / renderer_.DpiScale(),
                     static_cast<float>(HIWORD(lParam)) / renderer_.DpiScale());
        return 0;

    case WM_LBUTTONUP:
        ReleaseCapture();
        OnMouseUp(static_cast<float>(LOWORD(lParam)) / renderer_.DpiScale(),
                   static_cast<float>(HIWORD(lParam)) / renderer_.DpiScale());
        return 0;

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        scrollTarget_ -= delta * 0.5f;
        if (scrollTarget_ < 0) scrollTarget_ = 0;
        if (scrollTarget_ > maxScroll_) scrollTarget_ = maxScroll_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;

    case WM_DROPFILES:
        OnDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wParam);
        renderer_.HandleDpiChange(dpi);
        auto rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, rect->left, rect->top,
                     rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_TIMER:
        if (wParam == ::App::TIMER_TOAST) {
            toast_.Update(1.0f / 60.0f);
            if (!toast_.active) KillTimer(hwnd_, ::App::TIMER_TOAST);
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (wParam == ::App::TIMER_SCROLL) {
            // Smooth scroll interpolation
            scrollY_ += (scrollTarget_ - scrollY_) * 0.2f;
            if (fabsf(scrollTarget_ - scrollY_) < 0.5f) {
                scrollY_ = scrollTarget_;
                KillTimer(hwnd_, ::App::TIMER_SCROLL);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;

    case ::App::WM_APP_PROGRESS: {
        float p = *reinterpret_cast<float*>(&wParam);
        convertProgress_ = p;
        progressBar_.progress = p;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case ::App::WM_APP_DONE: {
        converting_ = false;
        progressBar_.visible = false;
        convertThread_ = nullptr;
        RefreshHistory();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_DESTROY: {
        // Save window position
        RECT rc;
        GetWindowRect(hwnd_, &rc);
        config_.windowX = rc.left;
        config_.windowY = rc.top;
        config_.windowW = rc.right - rc.left;
        config_.windowH = rc.bottom - rc.top;
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

// --- Painting ---

void App::OnPaint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);

    renderer_.BeginDraw();
    renderer_.Clear(Theme::BgWindow);

    float s = renderer_.DpiScale();
    RECT rc;
    GetClientRect(hwnd_, &rc);
    float w = (rc.right - rc.left) / s;
    float h = (rc.bottom - rc.top) / s;

    Layout(w, h);

    // Header area
    float px = Theme::PaddingX;
    float py = Theme::PaddingY;

    // Title
    D2D1_RECT_F titleRect = { px, py, w - px, py + 36 };
    renderer_.DrawText(::App::NAME, titleRect, Theme::TextPrimary, renderer_.fontTitle,
                       DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Tagline
    D2D1_RECT_F tagRect = { px, py + 36, w - px, py + 52 };
    renderer_.DrawText(::App::TAGLINE, tagRect, Theme::TextMuted, renderer_.fontSmall,
                       DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Version badge
    std::wstring verText = std::wstring(L"v") + ::App::VERSION;
    D2D1_SIZE_F verSize = renderer_.MeasureText(verText, renderer_.fontBadge);
    D2D1_RECT_F verBg = { w - px - verSize.width - 16, py + 4,
                           w - px, py + 4 + verSize.height + 8 };
    renderer_.FillRoundedRect(verBg, 4, Theme::BgButton);
    renderer_.DrawText(verText, verBg, Theme::Purple, renderer_.fontBadge,
                       DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Status dot + converter status
    statusDot_.color = Converter::IsAvailable() ? Theme::Success : Theme::Error;
    statusDot_.Draw(renderer_);

    std::wstring statusText = Converter::IsAvailable() ? L"Ready" : L"FFmpeg not found";
    if (converting_) statusText = convertStatus_;
    D2D1_RECT_F statusTextRect = { statusDot_.bounds.right + 6, statusDot_.bounds.top - 2,
                                    w - px, statusDot_.bounds.bottom + 2 };
    renderer_.DrawText(statusText, statusTextRect, Theme::TextMuted, renderer_.fontSmall);

    // Buttons
    btnNab_.Draw(renderer_);
    btnSettings_.Draw(renderer_);

    // Stat cards
    cardTotal_.Draw(renderer_);
    cardSuccess_.Draw(renderer_);
    cardFailed_.Draw(renderer_);

    // Progress bar
    progressBar_.Draw(renderer_);

    // History section header
    float histY = statsRect_.bottom + 16;
    D2D1_RECT_F histHeader = { px, histY, w - px, histY + 24 };
    renderer_.DrawText(L"Conversion History", histHeader, Theme::TextPrimary, renderer_.fontHeader);

    // History list
    float listY = histY + 32;
    D2D1_RECT_F listRect = { px, listY, w - px, h - 8 };
    renderer_.PushClip(listRect);

    if (history_.empty()) {
        D2D1_RECT_F emptyRect = { px, listY + 40, w - px, listY + 80 };
        renderer_.DrawText(L"No conversions yet. Click \"Nab Audio\" or drag files here.",
                           emptyRect, Theme::TextDim, renderer_.fontNormal,
                           DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    } else {
        float cardY = listY - scrollY_;
        for (size_t i = 0; i < history_.size(); i++) {
            if (cardY + Theme::CardHeight < listY) { cardY += Theme::CardHeight + Theme::CardGap; continue; }
            if (cardY > h) break;

            const auto& entry = history_[i];
            D2D1_RECT_F cardRect = { px, cardY, w - px, cardY + Theme::CardHeight };

            // Card background (alternating)
            D2D1_COLOR_F cardBg = (i % 2 == 0) ? Theme::BgCard : Theme::BgCardAlt;
            renderer_.FillRoundedRect(cardRect, Theme::CornerSmall, cardBg);

            // Status indicator
            D2D1_COLOR_F dotColor = entry.success ? Theme::Success : Theme::Error;
            renderer_.FillCircle(px + 14, cardY + Theme::CardHeight / 2, 4, dotColor);

            // File name
            std::wstring fname = Helpers::GetFileName(entry.inputPath);
            D2D1_RECT_F nameRect = { px + 28, cardY + 6, w - px - 120, cardY + 28 };
            renderer_.DrawText(fname, nameRect, Theme::TextPrimary, renderer_.fontNormal);

            // Format badge
            D2D1_RECT_F fmtRect = { w - px - 110, cardY + 8, w - px - 60, cardY + 26 };
            renderer_.FillRoundedRect(fmtRect, 4, Theme::BgButton);
            renderer_.DrawText(entry.outputFormat, fmtRect, Theme::Accent, renderer_.fontBadge,
                               DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            // Size + time info
            std::wstring info = Helpers::FormatSize(entry.inputSize) + L" → " +
                                Helpers::FormatSize(entry.outputSize);
            D2D1_RECT_F infoRect = { px + 28, cardY + 30, w * 0.5f, cardY + 48 };
            renderer_.DrawText(info, infoRect, Theme::TextMuted, renderer_.fontSmall);

            std::wstring timeAgo = Helpers::FormatTimeAgo(entry.timestamp);
            D2D1_RECT_F timeRect = { w * 0.5f, cardY + 30, w - px - 8, cardY + 48 };
            renderer_.DrawText(timeAgo, timeRect, Theme::TextDim, renderer_.fontSmall,
                               DWRITE_TEXT_ALIGNMENT_TRAILING);

            cardY += Theme::CardHeight + Theme::CardGap;
        }
        maxScroll_ = (history_.size() * (Theme::CardHeight + Theme::CardGap)) - (h - listY);
        if (maxScroll_ < 0) maxScroll_ = 0;
    }

    renderer_.PopClip();

    // Toast (drawn last, on top)
    toast_.Draw(renderer_);

    HRESULT hr = renderer_.EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        renderer_.DiscardDeviceResources();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    EndPaint(hwnd_, &ps);
}

void App::OnResize(UINT width, UINT height) {
    renderer_.Resize(width, height);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::Layout(float w, float h) {
    float px = Theme::PaddingX;
    float py = Theme::PaddingY;
    float s = renderer_.DpiScale();

    // Header region
    float headerBottom = py + 60;

    // Buttons row
    float btnY = headerBottom + 8;
    float btnW = 160;
    btnNab_.SetBounds(px, btnY, btnW, Theme::ButtonHeight);
    btnSettings_.SetBounds(px + btnW + 10, btnY, 120, Theme::ButtonHeight - 6);

    // Status dot (right side of header)
    statusDot_.SetBounds(w - px - 120, py + 12, 10, 10);

    // Stats row
    float statsY = btnY + Theme::ButtonHeight + 16;
    float cardW = (w - 2 * px - 16) / 3;
    cardTotal_.SetBounds(px, statsY, cardW, Theme::CardHeight);
    cardSuccess_.SetBounds(px + cardW + 8, statsY, cardW, Theme::CardHeight);
    cardFailed_.SetBounds(px + 2 * (cardW + 8), statsY, cardW, Theme::CardHeight);
    statsRect_ = { px, statsY, w - px, statsY + Theme::CardHeight };

    // Progress bar (below stats, only visible when converting)
    progressBar_.SetBounds(px, statsY + Theme::CardHeight + 8,
                           w - 2 * px, Theme::ProgressHeight);

    // Toast (top center)
    float toastW = 400;
    toast_.SetBounds((w - toastW) / 2, 8, toastW, 44);
}

// --- Input handling ---

void App::OnMouseMove(float x, float y) {
    bool needRepaint = false;
    auto updateState = [&](Control& ctrl) {
        ControlState newState = ctrl.HitTest(x, y) ? ControlState::Hovered : ControlState::Normal;
        if (ctrl.state == ControlState::Pressed) return; // Don't change while pressed
        if (ctrl.state != newState) { ctrl.state = newState; needRepaint = true; }
    };

    updateState(btnNab_);
    updateState(btnSettings_);
    updateState(btnOpenFolder_);

    if (needRepaint) InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::OnMouseDown(float x, float y) {
    auto press = [&](Button& btn) {
        if (btn.HitTest(x, y)) { btn.state = ControlState::Pressed; return true; }
        return false;
    };

    bool needRepaint = false;
    needRepaint |= press(btnNab_);
    needRepaint |= press(btnSettings_);
    needRepaint |= press(btnOpenFolder_);

    if (needRepaint) InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::OnMouseUp(float x, float y) {
    // Check which button was pressed and is still hovered
    if (btnNab_.state == ControlState::Pressed && btnNab_.HitTest(x, y)) {
        OpenFileDialog();
    }
    if (btnSettings_.state == ControlState::Pressed && btnSettings_.HitTest(x, y)) {
        // TODO: Open settings window
    }
    if (btnOpenFolder_.state == ControlState::Pressed && btnOpenFolder_.HitTest(x, y)) {
        // TODO: Open output folder
    }

    // Reset all states
    btnNab_.state = btnNab_.HitTest(x, y) ? ControlState::Hovered : ControlState::Normal;
    btnSettings_.state = btnSettings_.HitTest(x, y) ? ControlState::Hovered : ControlState::Normal;
    btnOpenFolder_.state = btnOpenFolder_.HitTest(x, y) ? ControlState::Hovered : ControlState::Normal;

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::OnKeyDown(WPARAM vk) {
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        switch (vk) {
        case 'O': OpenFileDialog(); break;
        case VK_OEM_COMMA: /* TODO: Settings */ break;
        }
    }
    if (vk == VK_F5) RefreshHistory();
    if (vk == VK_ESCAPE) CancelConversion();
}

void App::OnDropFiles(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> files;
    wchar_t path[MAX_PATH];

    for (UINT i = 0; i < count; i++) {
        DragQueryFileW(hDrop, i, path, MAX_PATH);
        std::wstring ext = Helpers::GetFileExtension(path);
        if (Converter::IsSupportedInput(ext)) {
            files.push_back(path);
        }
    }
    DragFinish(hDrop);

    if (!files.empty()) {
        ConvertFiles(files);
    } else {
        ShowToast(Toast::Type::Warning, L"No supported files found in dropped items.");
    }
}

// --- Actions ---

void App::OpenFileDialog() {
    if (converting_) return;

    // Build filter string
    std::wstring filter = L"Media Files\0";
    std::wstring exts;
    for (auto e : ::App::VIDEO_EXTS) { exts += L"*"; exts += e; exts += L";"; }
    for (auto e : ::App::AUDIO_EXTS) { exts += L"*"; exts += e; exts += L";"; }
    exts += L"*.mp3";
    filter += exts + L"\0All Files\0*.*\0\0";

    // Replace null separators
    std::vector<wchar_t> filterBuf(filter.begin(), filter.end());
    for (size_t i = 0; i < filterBuf.size(); i++) {
        if (filterBuf[i] == L'\0' && i + 1 < filterBuf.size() && filterBuf[i + 1] == L'\0') {
            break;
        }
    }

    wchar_t fileBuf[4096] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = hwnd_;
    ofn.lpstrFilter  = filterBuf.data();
    ofn.lpstrFile    = fileBuf;
    ofn.nMaxFile     = 4096;
    ofn.Flags        = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        std::vector<std::wstring> files;

        // Check if multiple files selected (directory + files)
        if (fileBuf[wcslen(fileBuf) + 1] != L'\0') {
            std::wstring dir = fileBuf;
            const wchar_t* p = fileBuf + dir.size() + 1;
            while (*p) {
                files.push_back(dir + L"\\" + p);
                p += wcslen(p) + 1;
            }
        } else {
            files.push_back(fileBuf);
        }

        ConvertFiles(files);
    }
}

void App::ConvertFiles(const std::vector<std::wstring>& paths) {
    if (converting_ || paths.empty()) return;

    converting_ = true;
    convertProgress_ = 0.0f;
    convertStatus_ = L"Starting...";
    progressBar_.visible = true;
    progressBar_.progress = 0.0f;
    ResetEvent(cancelEvent_);

    // Copy paths for thread
    auto* threadPaths = new std::vector<std::wstring>(paths);

    convertThread_ = reinterpret_cast<HANDLE>(_beginthreadex(
        nullptr, 0, ConvertThreadProc, threadPaths, 0, nullptr));

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::ConvertFile(const std::wstring& path) {
    ConvertFiles({ path });
}

void App::CancelConversion() {
    if (converting_ && cancelEvent_) {
        SetEvent(cancelEvent_);
    }
}

unsigned __stdcall App::ConvertThreadProc(void* param) {
    auto* paths = static_cast<std::vector<std::wstring>*>(param);
    App& app = App::Instance();
    AppConfig& cfg = app.GetConfig();

    int total = static_cast<int>(paths->size());
    int success = 0;

    for (int i = 0; i < total; i++) {
        const auto& path = (*paths)[i];

        // Check cancel
        if (WaitForSingleObject(app.cancelEvent_, 0) == WAIT_OBJECT_0) break;

        // Progress callback — post to UI thread
        auto progressCb = [&app, i, total](float p, const std::wstring& status) {
            float overall = (i + p) / total;
            WPARAM wp = *reinterpret_cast<WPARAM*>(&overall);
            PostMessageW(app.GetHwnd(), ::App::WM_APP_PROGRESS, wp, 0);
        };

        std::wstring outputDir = Helpers::GetDirectory(path);
        AudioFormat fmt = static_cast<AudioFormat>(cfg.formatIndex);
        std::string bitrate = ::App::BITRATE_OPTIONS[cfg.bitrateIndex];

        ConvertResult result = Converter::Convert(
            path, outputDir, fmt, bitrate, progressCb, app.cancelEvent_);

        // Record in database
        HistoryEntry entry;
        entry.inputPath = path;
        entry.outputPath = result.outputPath;
        entry.outputFormat = Converter::FormatName(fmt);
        entry.bitrate = Helpers::Utf8ToWide(bitrate);
        entry.inputSize = Helpers::GetFileSize(path);
        entry.outputSize = result.outputSize;
        entry.duration = result.duration;
        entry.success = result.success;
        entry.error = result.error;
        entry.timestamp = Helpers::GetUnixTime();

        app.GetDatabase().AddEntry(entry);

        if (result.success) success++;
    }

    delete paths;

    // Notify UI thread
    PostMessageW(app.GetHwnd(), ::App::WM_APP_DONE, success, total);
    return 0;
}

void App::RefreshHistory() {
    history_ = db_.GetAll(searchText_, filterIndex_);
    statTotal_   = db_.GetTotalCount();
    statSuccess_ = db_.GetSuccessCount();
    statFailed_  = db_.GetFailedCount();

    cardTotal_.value   = std::to_wstring(statTotal_);
    cardSuccess_.value = std::to_wstring(statSuccess_);
    cardFailed_.value  = std::to_wstring(statFailed_);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::ShowToast(Toast::Type type, const std::wstring& msg) {
    toast_.Show(type, msg);
    SetTimer(hwnd_, ::App::TIMER_TOAST, 16, nullptr); // ~60 FPS updates
    InvalidateRect(hwnd_, nullptr, FALSE);
}
