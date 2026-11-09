#pragma once
#include "Graphics/Rect.h"

namespace CustomCursor::Main {

// The window (Height 846, Width 1285, MinHeight 500, MinWidth 800): the title bar on top, the
// navigation rail on the left, the page in the rest.
struct MainLayout {
    static constexpr float WindowWidth = 1285.f, WindowHeight = 846.f;
    static constexpr float MinWidth = 800.f, MinHeight = 500.f;
    static constexpr float Radius = 10.f;             // the window's rounded corners (0 maximized)
    static constexpr float TitleBarHeight = 36.f;
    static constexpr float RailWidth = 96.f;
    static constexpr float ContentMargin = 14.f;
    static constexpr float WindowButtonWidth = 46.f;

    Rect window, titleBar, body, rail, content;

    static MainLayout Compute(const Rect& window) {
        MainLayout l;
        l.window = window;
        l.titleBar = { window.x, window.y, window.w, TitleBarHeight };
        l.body = { window.x, l.titleBar.B(), window.w, window.B() - l.titleBar.B() };
        l.rail = { window.x, l.body.y, RailWidth, l.body.h };
        l.content = Rect{ l.rail.R(), l.body.y, window.R() - l.rail.R(), l.body.h }.Inset(Edges(ContentMargin, 2.f, ContentMargin, ContentMargin));
        return l;
    }
    // The title bar the window is dragged by (all of it but the three window buttons)
    Rect Caption() const { return { titleBar.x, titleBar.y, titleBar.w - WindowButtonWidth * 3.f, titleBar.h }; }
};

} // namespace CustomCursor::Main
