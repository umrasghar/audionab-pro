#include "controls.h"
#include "renderer.h"
#include "theme.h"

// --- Button ---

void Button::Draw(Renderer& r) const {
    if (!visible) return;

    D2D1_COLOR_F bg;
    switch (state) {
        case ControlState::Pressed:  bg = pressColor; break;
        case ControlState::Hovered:  bg = hoverColor; break;
        case ControlState::Disabled: bg = bgColor; bg.a = 0.5f; break;
        default:                     bg = bgColor; break;
    }

    r.FillRoundedRect(bounds, cornerRadius, bg);

    D2D1_COLOR_F tc = textColor;
    if (state == ControlState::Disabled) tc.a = 0.5f;

    r.DrawText(text, bounds, tc, r.fontNormal,
               DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// --- StatCard ---

void StatCard::Draw(Renderer& r) const {
    if (!visible) return;

    r.FillRoundedRect(bounds, Theme::CornerSmall, bgColor);

    // Accent left bar
    D2D1_RECT_F bar = { bounds.left, bounds.top + 8, bounds.left + 3, bounds.bottom - 8 };
    r.FillRoundedRect(bar, 1.5f, accentColor);

    // Value text (large)
    D2D1_RECT_F valueRect = { bounds.left + 14, bounds.top + 6,
                               bounds.right - 8, bounds.top + Height() * 0.55f };
    r.DrawText(value, valueRect, Theme::TextPrimary, r.fontHeader,
               DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Label text (small)
    D2D1_RECT_F labelRect = { bounds.left + 14, bounds.top + Height() * 0.52f,
                               bounds.right - 8, bounds.bottom - 4 };
    r.DrawText(label, labelRect, Theme::TextMuted, r.fontSmall,
               DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// --- ProgressBar ---

void ProgressBar::Draw(Renderer& r) const {
    if (!visible) return;

    // Track
    r.FillRoundedRect(bounds, Height() / 2, trackColor);

    if (indeterminate) {
        // Sliding highlight
        float w = Width() * 0.3f;
        float x = bounds.left + animOffset * (Width() + w) - w;
        D2D1_RECT_F fill = { x, bounds.top, x + w, bounds.bottom };
        // Clip to track
        r.PushClip(bounds);
        r.FillRoundedRect(fill, Height() / 2, fillColor);
        r.PopClip();
    } else if (progress > 0.0f) {
        float fillWidth = Width() * progress;
        if (fillWidth < Height()) fillWidth = Height(); // Min width for rounded ends
        D2D1_RECT_F fill = { bounds.left, bounds.top,
                              bounds.left + fillWidth, bounds.bottom };
        r.PushClip(bounds);
        r.FillRoundedRect(fill, Height() / 2, fillColor);
        r.PopClip();
    }
}

// --- Toast ---

void Toast::Show(Type t, const std::wstring& msg, float dur) {
    type = t;
    message = msg;
    duration = dur;
    timer = 0.0f;
    opacity = 0.0f;
    active = true;
}

void Toast::Update(float dt) {
    if (!active) return;
    timer += dt;

    // Fade in (0-0.2s), stay, fade out (last 0.3s)
    if (timer < 0.2f) {
        opacity = timer / 0.2f;
    } else if (timer < duration - 0.3f) {
        opacity = 1.0f;
    } else if (timer < duration) {
        opacity = (duration - timer) / 0.3f;
    } else {
        opacity = 0.0f;
        active = false;
    }
}

void Toast::Draw(Renderer& r) const {
    if (!active || opacity <= 0.0f) return;

    D2D1_COLOR_F bg, text;
    switch (type) {
        case Type::Success: bg = Theme::ToastSuccessBg; text = Theme::ToastSuccessText; break;
        case Type::Error:   bg = Theme::ToastErrorBg;   text = Theme::ToastErrorText;   break;
        case Type::Warning: bg = Theme::ToastWarningBg; text = Theme::ToastWarningText; break;
        case Type::Info:    bg = Theme::ToastInfoBg;     text = Theme::ToastInfoText;    break;
    }
    bg.a *= opacity;
    text.a *= opacity;

    r.FillRoundedRect(bounds, Theme::CornerSmall, bg);
    r.DrawText(message, bounds, text, r.fontNormal,
               DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// --- StatusDot ---

void StatusDot::Draw(Renderer& r) const {
    if (!visible) return;
    float cx = (bounds.left + bounds.right) / 2;
    float cy = (bounds.top + bounds.bottom) / 2;
    r.FillCircle(cx, cy, radius, color);
}
