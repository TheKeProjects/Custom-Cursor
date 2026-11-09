#pragma once
#include <string>
#include "Controls/MoviePlayer.h"
#include "UI/Ui.h"

namespace CustomCursor::Main {

class MainWindow;

// The tutorial page: the banner and the video in a frame (TutorialMediaPlayer, Stretch Uniform; a
// click pauses / plays it, it stops at the end). Leaving the page stops it.
class TutorialView {
public:
    explicit TutorialView(MainWindow& window) : main(window) {}
    // TutorialButton_Click: "No Tutorials" when there's no video, else ShowTutorialVideo
    void Open();
    void Draw(UI::Ui& ui, const Rect& content, bool videoShown);
    // ShowMainView: TutorialMediaPlayer.Stop(), Source = null
    void Stop();

private:
    MainWindow& main;
    Controls::MoviePlayer player;
    double shownAt = -1.0;
};

} // namespace CustomCursor::Main
