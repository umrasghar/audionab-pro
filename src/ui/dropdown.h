#pragma once
#include "controls.h"
#include <vector>
#include <string>
#include <functional>

class Renderer;

struct Dropdown : Control {
    std::vector<std::wstring> items;
    int selectedIndex = 0;
    bool isOpen = false;
    float itemHeight = 32.0f;
    float cornerRadius = 8.0f;
    float maxDropHeight = 200.0f;

    D2D1_COLOR_F bgColor     = {};
    D2D1_COLOR_F hoverColor  = {};
    D2D1_COLOR_F textColor   = {};
    D2D1_COLOR_F borderColor = {};
    D2D1_COLOR_F dropBg      = {};

    int hoverIndex = -1; // Which dropdown item is hovered

    std::function<void(int)> onChanged;

    const std::wstring& SelectedText() const {
        static std::wstring empty;
        return (selectedIndex >= 0 && selectedIndex < (int)items.size())
            ? items[selectedIndex] : empty;
    }

    // Draw the collapsed button
    void Draw(Renderer& r) const;

    // Draw the dropdown list (call after all other controls)
    void DrawDropdown(Renderer& r) const;

    // Returns total bounds including dropdown when open
    D2D1_RECT_F GetDropdownBounds() const;

    // Handle click — returns true if consumed
    bool OnClick(float x, float y);

    // Handle mouse move in dropdown
    void OnMouseMove(float x, float y);

    // Close dropdown
    void Close() { isOpen = false; hoverIndex = -1; }
};
