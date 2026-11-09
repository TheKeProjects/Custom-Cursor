#pragma once
#include "Models/CursorScheme.h"
#include "UI/Ui.h"
#include "Windows/Main/MainState.h"

namespace CustomCursor::Main {

class MainWindow;

// The schemes page and the handlers behind it. Split:
//   SchemesView.cpp     the banner, the chips row and the bar of the selected schemes
//   CategoryChips.cpp   the categories as chips with their counts, the highlight sliding between them
//   SchemeGrid.cpp      the cards, as wide as the page allows, with their entrance
//   SchemeCard.cpp      a card: the cursor on its stage, the badges, name, "N cursors", Apply, 👁, 🗑
//   SchemeActions.cpp   apply (directly or elevated), uninstall (with the replacement picker), install
//                       the selected ones (with the progress card), the success / error cards
class SchemesView {
public:
    explicit SchemesView(MainWindow& window);
    void Draw(UI::Ui& ui, const Rect& content, double shownAt);

    // The card's preview (the preview image, an animated .ani, or the icon on the accent), also
    // drawn by the transition into the preview
    static void DrawPreview(UI::Ui& ui, const MainState& state, const Models::CursorScheme& scheme, const Rect& r, float opacity = 1.f);

    // ── SchemeActions.cpp ──
    void ApplyCursorScheme(const Models::CursorScheme& scheme);
    void UninstallCursorScheme(const Models::CursorScheme& scheme);
    void InstallSelected();
    // An action is running on a worker (the cards' buttons wait)
    bool Busy() const { return busy; }

private:
    // Lays out and draws the chips (the search box at the end of the first row); returns their height
    float CategoryChips(UI::Ui& ui, const Rect& r, double shownAt);
    void SchemeGrid(UI::Ui& ui, const Rect& r, float bottomRoom);
    void SchemeCard(UI::Ui& ui, const Rect& r, const Models::CursorScheme& scheme);
    void SelectionBar(UI::Ui& ui, const Rect& content);
    // SchemeActions.cpp: after the elevated work, on the UI thread
    void FinishUninstall(const Models::CursorScheme& scheme, bool success, const std::wstring& error);
    void Uninstall(const Models::CursorScheme& scheme, const std::optional<std::wstring>& replacement);

    MainWindow& main;
    MainState& state;
    double revealAt = -1.0;       // when the cards started coming in (state.revision changed)
    unsigned seenRevision = ~0u;
    size_t barCount = 0;          // what the selection bar shows while it slides away
    bool busy = false;
};

} // namespace CustomCursor::Main
