#pragma once
#include <string>

namespace CustomCursor::Models {

// Models\CursorInfo.cs: one cursor of a scheme, for the preview. `Name` was the English label
// (CursorLabels); here it's the label's translation key (shown with Loc::Get).
struct CursorInfo {
    std::string labelKey;    // "cursor_label_pointer"
    std::wstring fileName;   // as in the .inf
    std::wstring key;        // the .inf key ("pointer")
};

} // namespace CustomCursor::Models
