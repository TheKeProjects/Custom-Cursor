#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace CustomCursor::Main {

// The "Select Replacement Cursor" window UninstallCursorScheme built in code, as a card: "'<name>' is
// currently active.", "Select a cursor to switch to before uninstalling:", the list ("Windows
// Default" first, selected, then the other installed schemes), Cancel / OK. A double click or Enter
// is OK; Escape, a click outside or Cancel cancels.
class ReplacementPickerOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    // `onOk` gets the chosen scheme (nullopt = Windows Default); it isn't called when cancelled
    void Show(const std::wstring& schemeName, std::vector<std::wstring> installed,
              std::function<void(std::optional<std::wstring> replacement)> onOk);

protected:
    float CardWidth() const override { return 450.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Accept();

    std::wstring scheme;
    std::vector<std::wstring> items;   // the installed schemes (index 0 of the list is Windows Default)
    int selected = 0;
    bool accepted = false;
    std::function<void(std::optional<std::wstring>)> onOk;
};

} // namespace CustomCursor::Main
