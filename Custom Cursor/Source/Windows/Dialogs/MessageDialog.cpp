#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/MessageOverlay.h"

namespace CustomCursor::Dialogs {

void Message(const std::wstring& text, const std::wstring& title, MessageButtons buttons, MessageIcon icon,
             std::function<void(MessageResult)> done) {
    if (auto* window = Main::MainWindow::Instance()) window->Messages().Show(text, title, buttons, icon, std::move(done));
}

} // namespace CustomCursor::Dialogs
