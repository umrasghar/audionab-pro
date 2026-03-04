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
        MessageBoxW(nullptr, L"Failed to open database.", AppDef::NAME, MB_ICONERROR);
        return false;
    }

    // Init converter
    Converter::Init();

    // Create cancel event
    cancelEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // Create window
    if (!RegisterWindowClass(hInst)) {
        MessageBoxW(nullptr, L"Failed to register window class.", AppDef::NAME, MB_ICONERROR);
        return false;
    }
    if (!CreateAppWindow(hInst, nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to create window.", AppDef::NAME, MB_ICONERROR);
        return false;
    }

    // Init renderer
    if (!renderer_.Init(hwnd_)) {
        MessageBoxW(nullptr, L"Failed to initialize Direct2D.", AppDef::NAME, MB_ICONERROR);
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
    btnNab_.text = L"Nab Audio";
    btnNab_.bgColor    = Theme::Accent;
    btnNab_.hoverColor = Theme::AccentHover;
    btnNab_.pressColor = Theme::AccentPress;
    btnNab_.textColor  = { 1, 1, 1, 1 };
    btnNab_.cornerRadius = Theme::CornerRadius;
    btnNab_.isPrimary = true;

    // Cancel button (only visible during conversion)
    btnCancel_.text = L"Cancel";
    btnCancel_.bgColor    = Theme::Error;
    btnCancel_.hoverColor = { 0.97f, 0.33f, 0.33f, 1 };
    btnCancel_.pressColor = { 0.85f, 0.25f, 0.25f, 1 };
    btnCancel_.textColor  = { 1, 1, 1, 1 };
    btnCancel_.cornerRadius = Theme::CornerSmall;
    btnCancel_.visible = false;

    // Open folder button
    btnOpenFolder_.text = L"Open Output";
    btnOpenFolder_.bgColor    = Theme::BgButton;
    btnOpenFolder_.hoverColor = Theme::BgBtnHover;
    btnOpenFolder_.pressColor = Theme::BgCardAlt;
    btnOpenFolder_.textColor  = Theme::TextPrimary;
    btnOpenFolder_.cornerRadius = Theme::CornerSmall;

    // Format dropdown
    ddFormat_.items = { L"MP3", L"FLAC", L"WAV", L"AAC", L"Opus" };
    ddFormat_.selectedIndex = config_.formatIndex;
    ddFormat_.bgColor     = Theme::BgInput;
    ddFormat_.hoverColor  = Theme::BgBtnHover;
    ddFormat_.textColor   = Theme::TextPrimary;
    ddFormat_.borderColor = Theme::Border;
    ddFormat_.dropBg      = Theme::BgCard;
    ddFormat_.onChanged = [this](int idx) {
        config_.formatIndex = idx;
        Config::Save(config_);
    };

    // Bitrate dropdown
    ddBitrate_.items = { L"64k", L"96k", L"128k", L"160k", L"192k", L"256k", L"320k" };
    ddBitrate_.selectedIndex = config_.bitrateIndex;
    ddBitrate_.bgColor     = Theme::BgInput;
    ddBitrate_.hoverColor  = Theme::BgBtnHover;
    ddBitrate_.textColor   = Theme::TextPrimary;
    ddBitrate_.borderColor = Theme::Border;
    ddBitrate_.dropBg      = Theme::BgCard;
    ddBitrate_.onChanged = [this](int idx) {
        config_.bitrateIndex = idx;
        Config::Save(config_);
    };

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
    wc.lpszClassName  = AppDef::WINDOW_CLASS;
    return RegisterClassExW(&wc) != 0;
}

bool App::CreateAppWindow(HINSTANCE hInst, int nCmdShow) {
    int x = config_.windowX >= 0 ? config_.windowX : CW_USEDEFAULT;
    int y = config_.windowY >= 0 ? config_.windowY : CW_USEDEFAULT;
    int w = config_.windowW > 0  ? config_.windowW : AppDef::INITIAL_WIDTH;
    int h = config_.windowH > 0  ? config_.windowH : AppDef::INITIAL_HEIGHT;

    hwnd_ = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        AppDef::WINDOW_CLASS,
        AppDef::NAME,
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
        app->hwnd_ = hwnd; // Store hwnd early
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
        mmi->ptMinTrackSize.x = AppDef::MIN_WIDTH;
        mmi->ptMinTrackSize.y = AppDef::MIN_HEIGHT;
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
        if (wParam == AppDef::TIMER_TOAST) {
            toast_.Update(1.0f / 60.0f);
            if (!toast_.active) KillTimer(hwnd_, AppDef::TIMER_TOAST);
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (wParam == AppDef::TIMER_SCROLL) {
            // Smooth scroll interpolation
            scrollY_ += (scrollTarget_ - scrollY_) * 0.2f;
            if (fabsf(scrollTarget_ - scrollY_) < 0.5f) {
                scrollY_ = scrollTarget_;
                KillTimer(hwnd_, AppDef::TIMER_SCROLL);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;

    case AppDef::WM_APP_PROGRESS: {
        float p = *reinterpret_cast<float*>(&wParam);
        convertProgress_ = p;
        progressBar_.progress = p;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case AppDef::WM_APP_DONE: {
        converting_ = false;
        progressBar_.visible = false;
        btnCancel_.visible = false;
        convertThread_ = nullptr;
        RefreshHistory();
        // Show result toast
        int ok = static_cast<int>(wParam);
        int total = static_cast<int>(lParam);
        if (ok == total && total > 0) {
            ShowToast(Toast::Type::Success, std::to_wstring(ok) + L" file(s) converted successfully!");
        } else if (ok > 0) {
            ShowToast(Toast::Type::Warning, std::to_wstring(ok) + L"/" + std::to_wstring(total) + L" files converted.");
        } else if (total > 0) {
            ShowToast(Toast::Type::Error, L"Conversion failed.");
        }
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

    // Guard: renderer not ready yet (WM_PAINT can fire during CreateWindowEx)
    if (!renderer_.Target()) {
        EndPaint(hwnd_, &ps);
        return;
    }

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
    renderer_.DrawText(AppDef::NAME, titleRect, Theme::TextPrimary, renderer_.fontTitle,
                       DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Tagline
    D2D1_RECT_F tagRect = { px, py + 36, w - px, py + 52 };
    renderer_.DrawText(AppDef::TAGLINE, tagRect, Theme::TextMuted, renderer_.fontSmall,
                       DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Version badge
    std::wstring verText = std::wstring(L"v") + AppDef::VERSION;
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

    // Buttons and controls
    btnNab_.Draw(renderer_);
    ddFormat_.Draw(renderer_);
    ddBitrate_.Draw(renderer_);
    btnOpenFolder_.Draw(renderer_);
    btnCancel_.Draw(renderer_);

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

    // Dropdown popups (drawn on top of everything else)
    ddFormat_.DrawDropdown(renderer_);
    ddBitrate_.DrawDropdown(renderer_);

    // Drag-drop overlay
    if (dragOver_) {
        D2D1_RECT_F fullRect = { 0, 0, w, h };
        renderer_.FillRect(fullRect, Theme::DragOverlay);
        // Dashed border effect
        D2D1_RECT_F borderRect = { 12, 12, w - 12, h - 12 };
        renderer_.DrawRoundedRect(borderRect, Theme::CornerRadius, Theme::DragBorder, 2.0f);
        // Center text
        D2D1_RECT_F dropText = { 0, h / 2 - 20, w, h / 2 + 20 };
        renderer_.DrawText(L"Drop files to convert", dropText, Theme::Accent, renderer_.fontHeader,
                           DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

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

    // Header region
    float headerBottom = py + 60;

    // Controls row: [Nab Audio] [Format v] [Bitrate v] [Open Output]  ... [Cancel]
    float btnY = headerBottom + 8;
    float nabW = 130;
    btnNab_.SetBounds(px, btnY, nabW, Theme::ButtonHeight);

    float ddX = px + nabW + 12;
    float ddW = 90;
    ddFormat_.SetBounds(ddX, btnY + 4, ddW, Theme::ButtonHeight - 8);
    ddBitrate_.SetBounds(ddX + ddW + 8, btnY + 4, ddW, Theme::ButtonHeight - 8);

    float oX = ddX + 2 * (ddW + 8);
    btnOpenFolder_.SetBounds(oX, btnY + 4, 110, Theme::ButtonHeight - 8);

    // Cancel button (right side, only during conversion)
    btnCancel_.SetBounds(w - px - 90, btnY + 4, 80, Theme::ButtonHeight - 8);

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
        if (ctrl.state == ControlState::Pressed) return;
        if (ctrl.state != newState) { ctrl.state = newState; needRepaint = true; }
    };

    updateState(btnNab_);
    updateState(btnCancel_);
    updateState(btnOpenFolder_);
    updateState(ddFormat_);
    updateState(ddBitrate_);

    // Dropdown hover tracking
    ddFormat_.OnMouseMove(x, y);
    ddBitrate_.OnMouseMove(x, y);
    if (ddFormat_.isOpen || ddBitrate_.isOpen) needRepaint = true;

    if (needRepaint) InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::OnMouseDown(float x, float y) {
    // Dropdowns get first priority
    bool consumed = false;
    consumed |= ddFormat_.OnClick(x, y);
    consumed |= ddBitrate_.OnClick(x, y);
    if (consumed) { InvalidateRect(hwnd_, nullptr, FALSE); return; }

    // Close any open dropdown when clicking elsewhere
    if (ddFormat_.isOpen)  { ddFormat_.Close();  InvalidateRect(hwnd_, nullptr, FALSE); }
    if (ddBitrate_.isOpen) { ddBitrate_.Close(); InvalidateRect(hwnd_, nullptr, FALSE); }

    auto press = [&](Button& btn) {
        if (btn.HitTest(x, y)) { btn.state = ControlState::Pressed; return true; }
        return false;
    };

    bool needRepaint = false;
    needRepaint |= press(btnNab_);
    needRepaint |= press(btnCancel_);
    needRepaint |= press(btnOpenFolder_);

    if (needRepaint) InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::OnMouseUp(float x, float y) {
    // Check which button was pressed and is still hovered
    if (btnNab_.state == ControlState::Pressed && btnNab_.HitTest(x, y)) {
        OpenFileDialog();
    }
    if (btnCancel_.state == ControlState::Pressed && btnCancel_.HitTest(x, y)) {
        CancelConversion();
    }
    if (btnOpenFolder_.state == ControlState::Pressed && btnOpenFolder_.HitTest(x, y)) {
        // Open last output directory
        if (!history_.empty() && !history_[0].outputPath.empty()) {
            std::wstring dir = Helpers::GetDirectory(history_[0].outputPath);
            ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    // Reset all states
    btnNab_.state = btnNab_.HitTest(x, y) ? ControlState::Hovered : ControlState::Normal;
    btnCancel_.state = btnCancel_.HitTest(x, y) ? ControlState::Hovered : ControlState::Normal;
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
    for (auto e : AppDef::VIDEO_EXTS) { exts += L"*"; exts += e; exts += L";"; }
    for (auto e : AppDef::AUDIO_EXTS) { exts += L"*"; exts += e; exts += L";"; }
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
    btnCancel_.visible = true;
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
            PostMessageW(app.GetHwnd(), AppDef::WM_APP_PROGRESS, wp, 0);
        };

        std::wstring outputDir = Helpers::GetDirectory(path);
        AudioFormat fmt = static_cast<AudioFormat>(cfg.formatIndex);
        std::string bitrate = AppDef::BITRATE_OPTIONS[cfg.bitrateIndex];

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
    PostMessageW(app.GetHwnd(), AppDef::WM_APP_DONE, success, total);
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
    SetTimer(hwnd_, AppDef::TIMER_TOAST, 16, nullptr); // ~60 FPS updates
    InvalidateRect(hwnd_, nullptr, FALSE);
}
