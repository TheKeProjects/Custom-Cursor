#pragma once
#include <deque>
#include <functional>
#include <string>
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace CustomCursor::Main {

// The MessageBox.Show calls (the success questions, the errors, the uninstall / install
// confirmations, "No Cursors Found", "No Tutorials") as an in-window card: an icon badge, the title,
// the text and OK or Yes / No. A message asked for while another one is shown waits for it to close.
class MessageOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show(const std::wstring& text, const std::wstring& title, Dialogs::MessageButtons buttons, Dialogs::MessageIcon icon,
              std::function<void(Dialogs::MessageResult)> done);

protected:
    float CardWidth() const override { return 480.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    Color Accent() const override;

private:
    struct Request {
        std::wstring text, title;
        Dialogs::MessageButtons buttons = Dialogs::MessageButtons::Ok;
        Dialogs::MessageIcon icon = Dialogs::MessageIcon::None;
        std::function<void(Dialogs::MessageResult)> done;
    };
    void Next();
    void Answer(Dialogs::MessageResult result);

    Request current;
    std::deque<Request> queue;
    Dialogs::MessageResult result = Dialogs::MessageResult::Ok;
};

} // namespace CustomCursor::Main
