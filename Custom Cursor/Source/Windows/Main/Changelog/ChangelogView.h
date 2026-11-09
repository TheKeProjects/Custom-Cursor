#pragma once
#include <string>
#include <vector>
#include "UI/Ui.h"
#include "Windows/Main/Updates/ChangelogCards.h"

namespace CustomCursor::Main {

class MainWindow;

// The internal changelog (the Sonic Racing Save Manager's / Inazuma Eleven VR Save Editor's): the
// banner, every version of the app as folding cards and, beside them, the totals counting up and a
// bar per type of change.
class ChangelogView {
public:
    explicit ChangelogView(MainWindow& window) : main(window), cards("chl-cards") {}
    void Draw(UI::Ui& ui, const Rect& content, double shownAt);

private:
    struct TypeCount {
        std::wstring title;
        int count = 0;
    };
    void Summarize();
    float Glance(UI::Ui& ui, const Rect& r, bool draw, double shownAt);
    void ByType(UI::Ui& ui, const Rect& r, double shownAt);

    MainWindow& main;
    ChangelogCards cards;
    std::wstring text;
    bool summarized = false;
    int versions = 0, changes = 0;
    std::wstring latestVersion, latestLabel;
    std::vector<TypeCount> types;
};

} // namespace CustomCursor::Main
