#pragma once
#include <string>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace CustomCursor::Main {

// The "Installing Cursors" window InstallSelectedButton_Click built in code, as a card that can't be
// closed: "Installing cursors... (current/total)", the progress bar and the scheme being installed.
// The installation runs on a worker thread, so the window keeps drawing meanwhile.
class InstallProgressOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show(int total);
    void Update(int current, int total, const std::wstring& schemeName);

protected:
    float CardWidth() const override { return 380.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    bool Closable() const override { return false; }

private:
    int current = 0, total = 0;
    std::wstring name;
};

} // namespace CustomCursor::Main
