#pragma once
#include "Graphics/Color.h"
#include "Graphics/Graphics.h"
#include "Graphics/Rect.h"

// What's left of the old launcher styles the shared widgets use (the TextBox defaults, the Classic
// scroll bar), and the Segoe MDL2 glyphs of the window buttons.
namespace CustomCursor::UI {

namespace Palette {
inline constexpr Color Teal = Hex(0x00CCAA);
inline constexpr Color Faint = Hex(0x1A3A55);
} // namespace Palette

// Text styles ("Cascadia Code, Consolas, Courier New")
inline TextStyle Mono(float size, bool bold = false) {
    TextStyle s;
    s.size = size;
    s.font = Font::Mono;
    if (bold) s.weight = DWRITE_FONT_WEIGHT_BOLD;
    return s;
}
inline TextStyle IconFont(float size) {
    TextStyle s;
    s.size = size;
    s.font = Font::Icons;
    s.align = HAlign::Center;
    return s;
}

// Segoe MDL2 Assets glyphs
namespace Glyphs {
inline constexpr wchar_t Minimize = 0xE921;
inline constexpr wchar_t Maximize = 0xE922;
inline constexpr wchar_t Restore = 0xE923;
inline constexpr wchar_t ChromeClose = 0xE8BB;
} // namespace Glyphs

} // namespace CustomCursor::UI
