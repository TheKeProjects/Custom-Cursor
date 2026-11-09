#pragma once
#include "Graphics/Rect.h"
#include "UI/Ui.h"

namespace CustomCursor::Main {

class MainWindow;

// ═══ TITLE BAR ═══ the theme's logo, "Custom Cursor Manager", the version and the — / □ (❐) / ✕
// window buttons. Dragging and double-click to maximize come from the window's caption hit test
// (MainLayout::Caption).
namespace TitleBar {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar);
}

// ═══ NAVIGATION RAIL ═══ the big floating logo, the pages (Schemes, Tutorial, Updates, Changelog)
// with a highlight sliding between them, and at the bottom Windows' cursor settings, the theme and
// the language.
namespace NavRail {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& rail);
}

} // namespace CustomCursor::Main
