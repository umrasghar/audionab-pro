#pragma once
#include <d2d1.h>

namespace Theme {
    // Helper to create D2D color from hex
    constexpr D2D1_COLOR_F Hex(uint32_t rgb, float a = 1.0f) {
        return {
            ((rgb >> 16) & 0xFF) / 255.0f,
            ((rgb >> 8) & 0xFF) / 255.0f,
            (rgb & 0xFF) / 255.0f,
            a
        };
    }

    // Background colors
    inline constexpr auto BgWindow   = Hex(0x121221);
    inline constexpr auto BgCard     = Hex(0x1e1e2e);
    inline constexpr auto BgCardAlt  = Hex(0x1a1a28);
    inline constexpr auto BgCardHover= Hex(0x282840);
    inline constexpr auto BgInput    = Hex(0x232333);
    inline constexpr auto BgSurface  = Hex(0x181825);
    inline constexpr auto BgButton   = Hex(0x2a2a3e);
    inline constexpr auto BgBtnHover = Hex(0x353550);

    // Accent colors (matching Python version)
    inline constexpr auto Accent     = Hex(0x7aa2f7);   // Blue
    inline constexpr auto AccentHover= Hex(0x8fb4ff);
    inline constexpr auto AccentPress= Hex(0x6690e0);
    inline constexpr auto Success    = Hex(0x2dd4bf);   // Teal
    inline constexpr auto Error      = Hex(0xf87171);   // Red
    inline constexpr auto Warning    = Hex(0xfbbf24);   // Amber
    inline constexpr auto Purple     = Hex(0xa78bfa);   // Purple

    // Text colors
    inline constexpr auto TextPrimary= Hex(0xebebef);
    inline constexpr auto TextMuted  = Hex(0x808090);
    inline constexpr auto TextDim    = Hex(0x5f5f6b);

    // Toast colors
    inline constexpr auto ToastSuccessBg   = Hex(0x065f46);
    inline constexpr auto ToastSuccessText = Hex(0xa7f3d0);
    inline constexpr auto ToastErrorBg     = Hex(0x7f1d1d);
    inline constexpr auto ToastErrorText   = Hex(0xfca5a5);
    inline constexpr auto ToastWarningBg   = Hex(0x78350f);
    inline constexpr auto ToastWarningText = Hex(0xfde68a);
    inline constexpr auto ToastInfoBg      = Hex(0x1e3a5f);
    inline constexpr auto ToastInfoText    = Hex(0xbfdbfe);

    // FFmpeg warning banner
    inline constexpr auto BannerBg   = Hex(0x7f1d1d);
    inline constexpr auto BannerText = Hex(0xfca5a5);

    // Metrics (base values at 96 DPI, scale by dpiScale)
    inline constexpr float CornerRadius   = 12.0f;
    inline constexpr float CornerSmall    = 8.0f;
    inline constexpr float PaddingX       = 24.0f;
    inline constexpr float PaddingY       = 18.0f;
    inline constexpr float CardHeight     = 62.0f;
    inline constexpr float CardGap        = 4.0f;
    inline constexpr float ButtonHeight   = 44.0f;
    inline constexpr float InputHeight    = 34.0f;
    inline constexpr float ProgressHeight = 6.0f;
    inline constexpr float StatusDotRadius= 5.0f;

    // Font sizes (in DIP)
    inline constexpr float FontTitle      = 28.0f;
    inline constexpr float FontHeader     = 15.0f;
    inline constexpr float FontNormal     = 13.0f;
    inline constexpr float FontSmall      = 11.0f;
    inline constexpr float FontBadge      = 10.0f;
}
