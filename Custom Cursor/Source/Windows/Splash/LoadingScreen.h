#pragma once
#include "Windows/Main/Overlays/Overlay.h"

namespace CustomCursor::Main {

// Views\LoadingWindow.xaml(.cs) as the first in-window layer, redesigned like the Inazuma Eleven VR
// Save Editor's loading screen: the theme's backdrop with drifting glows and rising sparks, its logo
// breathing inside orbiting arcs, "Custom Cursor Manager", the glowing progress bar and the status.
// The steps are SimulateProgress's, but the timeline waits at "Extracting resources..." until the
// schemes are unpacked (the bar follows the unpacking) and at "Loading cursor sets..." until the
// start-up work is done. CompleteLoading's ending follows ("Complete!", the arcs closing into a ring
// and the logo popping), then it fades out and hands over to MainWindow::LoadingFinished.
class LoadingOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return visible; }
    // Fading out: the main view shows through
    bool Revealing() const { return visible && fading; }
    void Draw(UI::Ui& ui, const Rect& screen) override;

private:
    void Advance(double now);

    bool visible = true;
    bool fading = false;
    double startedAt = -1.0;
    double elapsed = 0.0;         // the timeline; it stops at the steps that wait for real work
    double lastNow = -1.0;
    double progressClock = 0.0;   // the 16 ms _progressTimer
    double currentProgress = 0.0, targetProgress = 0.0;
    int step = 0;                 // steps reached; the status shows the last one ("Initializing..." before)
    double stepAt = -10.0;        // when the last step was reached
};

} // namespace CustomCursor::Main
