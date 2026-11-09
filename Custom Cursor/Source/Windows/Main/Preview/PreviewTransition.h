#pragma once
#include <optional>
#include "Models/CursorScheme.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace CustomCursor::Main {

// AnimatedTransitionToPreview: the screen goes dark while a 200 x 200 card with the scheme's preview
// flies from the scheme card to the middle, doubling in size with an icy glow growing round it
// (500 ms, CubicEase InOut); it holds 300 ms, then the preview is shown and fades in (300 ms) as
// the dark layer goes.
class PreviewTransition : public Overlay {
public:
    using Overlay::Overlay;
    // `card`: the scheme card's Border, in window coordinates
    void Start(const Models::CursorScheme& scheme, const Rect& card);
    bool Visible() const override { return scheme.has_value(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

private:
    std::optional<Models::CursorScheme> scheme;
    Rect from;
    double startedAt = -1.0;
    bool shown = false;   // ShowDetailedPreview ran
};

} // namespace CustomCursor::Main
