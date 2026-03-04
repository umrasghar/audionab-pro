#pragma once
#include <d2d1.h>
#include <string>

class Renderer;

// Base state for interactive controls
enum class ControlState { Normal, Hovered, Pressed, Disabled };

// Base control — all UI elements derive from this
struct Control {
    D2D1_RECT_F bounds = {};
    bool visible = true;
    bool enabled = true;
    ControlState state = ControlState::Normal;

    bool HitTest(float x, float y) const {
        return visible && enabled &&
               x >= bounds.left && x <= bounds.right &&
               y >= bounds.top  && y <= bounds.bottom;
    }

    void SetBounds(float x, float y, float w, float h) {
        bounds = { x, y, x + w, y + h };
    }

    float Width() const  { return bounds.right - bounds.left; }
    float Height() const { return bounds.bottom - bounds.top; }
};

// Button with text + optional icon
struct Button : Control {
    std::wstring text;
    D2D1_COLOR_F bgColor     = {};
    D2D1_COLOR_F hoverColor  = {};
    D2D1_COLOR_F pressColor  = {};
    D2D1_COLOR_F textColor   = {};
    float cornerRadius = 8.0f;
    bool isPrimary = false;

    void Draw(Renderer& r) const;
};

// Stat card (shows number + label)
struct StatCard : Control {
    std::wstring value;
    std::wstring label;
    D2D1_COLOR_F accentColor = {};
    D2D1_COLOR_F bgColor     = {};

    void Draw(Renderer& r) const;
};

// Progress bar (determinate + indeterminate)
struct ProgressBar : Control {
    float progress = 0.0f;       // 0.0 to 1.0
    bool indeterminate = false;
    D2D1_COLOR_F trackColor = {};
    D2D1_COLOR_F fillColor  = {};
    float animOffset = 0.0f;     // For indeterminate animation

    void Draw(Renderer& r) const;
};

// Toast notification
struct Toast : Control {
    enum class Type { Success, Error, Warning, Info };
    Type type = Type::Info;
    std::wstring message;
    float opacity = 0.0f;
    float timer = 0.0f;
    float duration = 3.0f; // seconds
    bool active = false;

    void Draw(Renderer& r) const;
    void Show(Type t, const std::wstring& msg, float dur = 3.0f);
    void Update(float dt); // Call each frame
};

// Status dot
struct StatusDot : Control {
    D2D1_COLOR_F color = {};
    float radius = 5.0f;

    void Draw(Renderer& r) const;
};
