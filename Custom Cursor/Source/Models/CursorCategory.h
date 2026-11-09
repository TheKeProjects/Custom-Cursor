#pragma once
#include <string>
#include <vector>
#include "Models/CursorScheme.h"

namespace CustomCursor::Models {

// Models\CursorCategory.cs: the schemes of one top-level folder of Cursores ("General" for a
// scheme.inf right in Cursores). The scanner keeps the categories in the order it found them (the
// order of _cursorCategories, which "All" shows).
struct CursorCategory {
    std::wstring name;
    std::vector<CursorScheme> schemes;
};

} // namespace CustomCursor::Models
