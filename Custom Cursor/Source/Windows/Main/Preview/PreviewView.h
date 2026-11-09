#pragma once
#include <vector>
#include "Models/CursorInfo.h"
#include "Models/CursorScheme.h"
#include "UI/Ui.h"

namespace CustomCursor::Main {

class MainWindow;

// The preview page (ShowDetailedPreview): a hero with Back, the big stage showing the chosen cursor
// of the scheme, the scheme's name, its folder and count, Apply and Uninstall; under it a card per
// cursor (CursorPreviewCard.cpp), scrolling when they don't fit, a click sends one to the stage.
class PreviewView {
public:
    explicit PreviewView(MainWindow& window) : main(window) {}
    // The scheme's cursors decoded in the background while the transition plays
    static void PreloadCursors(const Models::CursorScheme& scheme);
    // ShowDetailedPreview: the scheme's cursors listed (the previous preview's were freed by Back)
    void Show(const Models::CursorScheme& scheme);
    void Draw(UI::Ui& ui, const Rect& content);
    // The page has faded away: the animations stopped and freed (BackFromPreviewButton_Click's Cleanup)
    void Hidden();

private:
    void Hero(UI::Ui& ui, const Rect& r);
    // CreateCursorPreviewCard
    void CursorCard(UI::Ui& ui, const Rect& r, const Models::CursorInfo& info, int index);
    // The cursor's picture, or the pointer icon for files that aren't cursors
    void DrawCursor(UI::Ui& ui, const Models::CursorInfo& info, const Rect& r);

    MainWindow& main;
    Models::CursorScheme scheme;
    std::vector<Models::CursorInfo> infos;   // _currentCursorInfos
    int chosen = 0;
    double shownAt = -1.0;
};

} // namespace CustomCursor::Main
