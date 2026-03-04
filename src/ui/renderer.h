#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class Renderer {
public:
    bool Init(HWND hwnd);
    void Resize(UINT width, UINT height);
    void HandleDpiChange(UINT dpi);
    void DiscardDeviceResources();
    bool EnsureTarget();

    // Begin/End frame
    void BeginDraw();
    HRESULT EndDraw();

    // Drawing primitives
    void Clear(const D2D1_COLOR_F& color);
    void FillRect(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);
    void FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color);
    void DrawRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, float width = 1.0f);
    void FillCircle(float cx, float cy, float r, const D2D1_COLOR_F& color);
    void DrawLine(float x1, float y1, float x2, float y2, const D2D1_COLOR_F& color, float width = 1.0f);

    // Text drawing
    void DrawText(const std::wstring& text, const D2D1_RECT_F& rect,
                  const D2D1_COLOR_F& color, IDWriteTextFormat* format,
                  DWRITE_TEXT_ALIGNMENT hAlign = DWRITE_TEXT_ALIGNMENT_LEADING,
                  DWRITE_PARAGRAPH_ALIGNMENT vAlign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Text measurement
    D2D1_SIZE_F MeasureText(const std::wstring& text, IDWriteTextFormat* format, float maxWidth = 10000.0f);

    // Font creation
    IDWriteTextFormat* CreateFont(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR);

    // Clip
    void PushClip(const D2D1_RECT_F& rect);
    void PopClip();

    // Getters
    ID2D1HwndRenderTarget* Target() const { return target_.Get(); }
    IDWriteFactory* DWrite() const { return dwrite_.Get(); }
    float DpiScale() const { return dpiScale_; }

    // Standard fonts (created on init)
    IDWriteTextFormat* fontTitle   = nullptr;
    IDWriteTextFormat* fontHeader  = nullptr;
    IDWriteTextFormat* fontNormal  = nullptr;
    IDWriteTextFormat* fontSmall   = nullptr;
    IDWriteTextFormat* fontBadge   = nullptr;

private:
    bool CreateDeviceResources();
    void CreateFonts();

    HWND hwnd_ = nullptr;
    float dpiScale_ = 1.0f;

    ComPtr<ID2D1Factory1>          factory_;
    ComPtr<ID2D1HwndRenderTarget>  target_;
    ComPtr<IDWriteFactory>         dwrite_;

    // Owned font objects
    ComPtr<IDWriteTextFormat> fontTitle_;
    ComPtr<IDWriteTextFormat> fontHeader_;
    ComPtr<IDWriteTextFormat> fontNormal_;
    ComPtr<IDWriteTextFormat> fontSmall_;
    ComPtr<IDWriteTextFormat> fontBadge_;

    // Reusable brush
    ComPtr<ID2D1SolidColorBrush>   brush_;
};
