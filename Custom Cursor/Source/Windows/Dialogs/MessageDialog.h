#pragma once
#include <functional>
#include <string>

// Replacement for System.Windows.MessageBox: a card inside the main window (MessageOverlay) with
// the title, the text and the buttons. Asynchronous: `done` runs on the UI thread once it closes;
// Enter is the first button, Escape / a click outside the last one.
namespace CustomCursor::Dialogs {

enum class MessageButtons { Ok, YesNo, YesNoCancel };
enum class MessageIcon { None, Information, Warning, Error };
enum class MessageResult { Ok, Yes, No, Cancel };

void Message(const std::wstring& text, const std::wstring& title, MessageButtons buttons = MessageButtons::Ok,
             MessageIcon icon = MessageIcon::None, std::function<void(MessageResult)> done = {});

} // namespace CustomCursor::Dialogs
