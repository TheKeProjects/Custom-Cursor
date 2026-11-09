#pragma once
#include "UI/Ui.h"
#include "Windows/Main/Updates/ChangelogCards.h"

namespace CustomCursor::Main {

class MainWindow;

// The updates page: the banner; on the left the status (an animated badge, "Update available" /
// "You're up to date" / "Couldn't check for updates", current -> latest, Update now with its progress,
// Check again, the details, "App not updating? ..."); on the right what's new in the update, or the
// history when there's none.
class UpdatesView {
public:
    explicit UpdatesView(MainWindow& window) : main(window), cards("upd-cards") {}
    void Draw(UI::Ui& ui, const Rect& content, double shownAt);

private:
    void Status(UI::Ui& ui, const Rect& r, double shownAt);
    void WhatsNew(UI::Ui& ui, const Rect& r);
    void Badge(UI::Ui& ui, float cx, float cy, bool busy, bool available, bool upToDate);
    void Recheck();         // CheckUpdatesBanner_Click
    void AcceptInstall();   // AcceptInstallBtn_Click

    MainWindow& main;
    ChangelogCards cards;
    std::wstring history;
    bool downloading = false;   // _isDownloading
    bool rechecking = false;
    int progress = 0;
};

} // namespace CustomCursor::Main
