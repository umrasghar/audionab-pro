#include "renderer.h"
#include "theme.h"
#include <d2d1_1.h>
#include <dwrite.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

bool Renderer::Init(HWND hwnd) {
    hwnd_ = hwnd;

    // Create D2D factory
    D2D1_FACTORY_OPTIONS opts = {};
#ifdef _DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                    __uuidof(ID2D1Factory1), &opts,
                                    reinterpret_cast<void**>(factory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    // Create DirectWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf()));
    if (FAILED(hr)) return false;

    // Get initial DPI
    float dpiX, dpiY;
    factory_->GetDesktopDpi(&dpiX, &dpiY);
    dpiScale_ = dpiX / 96.0f;

    // Create device resources
    if (!CreateDeviceResources()) return false;
    CreateFonts();

    return true;
}

bool Renderer::CreateDeviceResources() {
    if (target_) return true;

    RECT rc;
    GetClientRect(hwnd_, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.dpiX = 96.0f * dpiScale_;
    rtProps.dpiY = 96.0f * dpiScale_;

    HRESULT hr = factory_->CreateHwndRenderTarget(
        rtProps,
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        target_.GetAddressOf());
    if (FAILED(hr)) return false;

    // Create reusable brush
    target_->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), brush_.GetAddressOf());

    return true;
}

void Renderer::CreateFonts() {
    fontTitle_.Reset();
    fontHeader_.Reset();
    fontNormal_.Reset();
    fontSmall_.Reset();
    fontBadge_.Reset();

    auto make = [&](float size, DWRITE_FONT_WEIGHT weight) -> ComPtr<IDWriteTextFormat> {
        ComPtr<IDWriteTextFormat> fmt;
        dwrite_->CreateTextFormat(
            L"Segoe UI", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-US", fmt.GetAddressOf());
        if (fmt) {
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    fontTitle_  = make(Theme::FontTitle,  DWRITE_FONT_WEIGHT_BOLD);
    fontHeader_ = make(Theme::FontHeader, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    fontNormal_ = make(Theme::FontNormal, DWRITE_FONT_WEIGHT_REGULAR);
    fontSmall_  = make(Theme::FontSmall,  DWRITE_FONT_WEIGHT_REGULAR);
    fontBadge_  = make(Theme::FontBadge,  DWRITE_FONT_WEIGHT_SEMI_BOLD);

    // Set raw pointers for convenient access
    fontTitle  = fontTitle_.Get();
    fontHeader = fontHeader_.Get();
    fontNormal = fontNormal_.Get();
    fontSmall  = fontSmall_.Get();
    fontBadge  = fontBadge_.Get();
}

void Renderer::Resize(UINT width, UINT height) {
    if (target_) {
        target_->Resize(D2D1::SizeU(width, height));
    }
}

void Renderer::HandleDpiChange(UINT dpi) {
    dpiScale_ = dpi / 96.0f;
    DiscardDeviceResources();
}

void Renderer::DiscardDeviceResources() {
    brush_.Reset();
    target_.Reset();
}

bool Renderer::EnsureTarget() {
    if (!target_) {
        if (!CreateDeviceResources()) return false;
        CreateFonts();
    }
    return true;
}

// --- Drawing ---

void Renderer::BeginDraw() {
    if (EnsureTarget())
        target_->BeginDraw();
}

HRESULT Renderer::EndDraw() {
    HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
    return hr;
}

void Renderer::Clear(const D2D1_COLOR_F& color) {
    target_->Clear(color);
}

void Renderer::FillRect(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) {
    brush_->SetColor(color);
    target_->FillRectangle(rect, brush_.Get());
}

void Renderer::FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color) {
    brush_->SetColor(color);
    D2D1_ROUNDED_RECT rr = { rect, radius, radius };
    target_->FillRoundedRectangle(rr, brush_.Get());
}

void Renderer::DrawRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, float width) {
    brush_->SetColor(color);
    D2D1_ROUNDED_RECT rr = { rect, radius, radius };
    target_->DrawRoundedRectangle(rr, brush_.Get(), width);
}

void Renderer::FillCircle(float cx, float cy, float r, const D2D1_COLOR_F& color) {
    brush_->SetColor(color);
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush_.Get());
}

void Renderer::DrawLine(float x1, float y1, float x2, float y2, const D2D1_COLOR_F& color, float width) {
    brush_->SetColor(color);
    target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush_.Get(), width);
}

void Renderer::DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
                         const D2D1_COLOR_F& color, IDWriteTextFormat* format,
                         DWRITE_TEXT_ALIGNMENT hAlign, DWRITE_PARAGRAPH_ALIGNMENT vAlign) {
    if (!format) return;
    format->SetTextAlignment(hAlign);
    format->SetParagraphAlignment(vAlign);
    brush_->SetColor(color);
    target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.length()),
                       format, rect, brush_.Get(),
                       D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

D2D1_SIZE_F Renderer::MeasureText(const std::wstring& text, IDWriteTextFormat* format, float maxWidth) {
    if (!format || text.empty()) return { 0, 0 };

    ComPtr<IDWriteTextLayout> layout;
    dwrite_->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()),
                              format, maxWidth, 10000.0f, layout.GetAddressOf());
    if (!layout) return { 0, 0 };

    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);
    return { metrics.width, metrics.height };
}

IDWriteTextFormat* Renderer::CreateFont(float size, DWRITE_FONT_WEIGHT weight) {
    IDWriteTextFormat* fmt = nullptr;
    dwrite_->CreateTextFormat(
        L"Segoe UI", nullptr, weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        size, L"en-US", &fmt);
    return fmt; // Caller owns
}

void Renderer::PushClip(const D2D1_RECT_F& rect) {
    target_->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void Renderer::PopClip() {
    target_->PopAxisAlignedClip();
}
