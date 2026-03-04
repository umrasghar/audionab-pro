#include "dropdown.h"
#include "renderer.h"
#include "theme.h"

void Dropdown::Draw(Renderer& r) const {
    if (!visible) return;

    D2D1_COLOR_F bg = bgColor;
    if (state == ControlState::Hovered || isOpen)
        bg = hoverColor;

    r.FillRoundedRect(bounds, cornerRadius, bg);

    // Border
    if (isOpen) {
        r.DrawRoundedRect(bounds, cornerRadius, Theme::Accent, 1.5f);
    } else {
        r.DrawRoundedRect(bounds, cornerRadius, borderColor, 1.0f);
    }

    // Selected text
    D2D1_RECT_F textRect = { bounds.left + 12, bounds.top,
                              bounds.right - 28, bounds.bottom };
    r.DrawText(SelectedText(), textRect, textColor, r.fontNormal,
               DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Arrow indicator
    float arrowX = bounds.right - 18;
    float arrowY = (bounds.top + bounds.bottom) / 2;
    float sz = 4.0f;
    if (isOpen) {
        // Up arrow
        r.DrawLine(arrowX - sz, arrowY + 2, arrowX, arrowY - 2, Theme::TextMuted, 1.5f);
        r.DrawLine(arrowX, arrowY - 2, arrowX + sz, arrowY + 2, Theme::TextMuted, 1.5f);
    } else {
        // Down arrow
        r.DrawLine(arrowX - sz, arrowY - 2, arrowX, arrowY + 2, Theme::TextMuted, 1.5f);
        r.DrawLine(arrowX, arrowY + 2, arrowX + sz, arrowY - 2, Theme::TextMuted, 1.5f);
    }
}

void Dropdown::DrawDropdown(Renderer& r) const {
    if (!isOpen || items.empty()) return;

    float totalH = items.size() * itemHeight;
    if (totalH > maxDropHeight) totalH = maxDropHeight;

    D2D1_RECT_F dropRect = {
        bounds.left,
        bounds.bottom + 4,
        bounds.right,
        bounds.bottom + 4 + totalH
    };

    // Shadow (offset dark rect)
    D2D1_RECT_F shadow = { dropRect.left + 2, dropRect.top + 2,
                            dropRect.right + 2, dropRect.bottom + 2 };
    D2D1_COLOR_F shadowColor = { 0, 0, 0, 0.3f };
    r.FillRoundedRect(shadow, cornerRadius, shadowColor);

    // Background
    r.FillRoundedRect(dropRect, cornerRadius, dropBg);
    r.DrawRoundedRect(dropRect, cornerRadius, borderColor, 1.0f);

    // Items
    r.PushClip(dropRect);
    for (int i = 0; i < (int)items.size(); i++) {
        float y = dropRect.top + i * itemHeight;
        if (y + itemHeight < dropRect.top) continue;
        if (y > dropRect.bottom) break;

        D2D1_RECT_F itemRect = { dropRect.left, y, dropRect.right, y + itemHeight };

        // Hover or selected highlight
        if (i == hoverIndex) {
            r.FillRect(itemRect, hoverColor);
        } else if (i == selectedIndex) {
            D2D1_COLOR_F selBg = Theme::Accent;
            selBg.a = 0.15f;
            r.FillRect(itemRect, selBg);
        }

        // Text
        D2D1_RECT_F textRect = { itemRect.left + 12, itemRect.top,
                                  itemRect.right - 8, itemRect.bottom };
        D2D1_COLOR_F tc = (i == selectedIndex) ? Theme::Accent : textColor;
        r.DrawText(items[i], textRect, tc, r.fontNormal,
                   DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    r.PopClip();
}

D2D1_RECT_F Dropdown::GetDropdownBounds() const {
    if (!isOpen) return bounds;
    float totalH = items.size() * itemHeight;
    if (totalH > maxDropHeight) totalH = maxDropHeight;
    return {
        bounds.left, bounds.top,
        bounds.right, bounds.bottom + 4 + totalH
    };
}

bool Dropdown::OnClick(float x, float y) {
    // Click on the button itself
    if (HitTest(x, y)) {
        isOpen = !isOpen;
        hoverIndex = -1;
        return true;
    }

    // Click inside dropdown
    if (isOpen) {
        D2D1_RECT_F dropBounds = GetDropdownBounds();
        if (x >= dropBounds.left && x <= dropBounds.right &&
            y >= bounds.bottom + 4 && y <= dropBounds.bottom) {
            int idx = static_cast<int>((y - bounds.bottom - 4) / itemHeight);
            if (idx >= 0 && idx < (int)items.size()) {
                selectedIndex = idx;
                if (onChanged) onChanged(idx);
            }
            isOpen = false;
            hoverIndex = -1;
            return true;
        }
        // Click outside — close
        isOpen = false;
        hoverIndex = -1;
        return false;
    }

    return false;
}

void Dropdown::OnMouseMove(float x, float y) {
    if (!isOpen) return;
    if (y >= bounds.bottom + 4) {
        int idx = static_cast<int>((y - bounds.bottom - 4) / itemHeight);
        if (idx >= 0 && idx < (int)items.size())
            hoverIndex = idx;
        else
            hoverIndex = -1;
    } else {
        hoverIndex = -1;
    }
}
