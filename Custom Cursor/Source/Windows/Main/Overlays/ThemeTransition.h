#pragma once
#include <string>
#include "Graphics/Graphics.h"

namespace CustomCursor::Main {

// Changing the theme: a snapshot of the window in the old look covers it and a slanted edge sweeps
// across it from the top right to the bottom left, uncovering the new look behind it, a light in the
// new accent running along the edge (the Marvel Rivals Mod Manager's).
class ThemeTransition {
public:
    // The next frame takes the snapshot (drawn in the old look) and only then switches the theme
    void Request(const std::wstring& theme) {
        if (!Animating()) pending = theme;
    }
    bool Pending() const { return !pending.empty(); }
    const std::wstring& PendingTheme() const { return pending; }
    bool Animating() const { return Pending() || active; }
    void Begin(ComPtr<ID2D1Bitmap1> snapshot, double now);
    // Draws what's left of the snapshot over the new look
    void Draw(Graphics& g, const Rect& screen, double now);

private:
    std::wstring pending;
    bool active = false;
    double startedAt = -10.0;
    ComPtr<ID2D1Bitmap1> snapshot;
};

} // namespace CustomCursor::Main
