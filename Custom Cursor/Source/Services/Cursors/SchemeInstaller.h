#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Models/CursorScheme.h"

// CursorRegistry.ApplyCursorScheme / ApplyMultipleCursorSchemes (and CursorFileService.cs): installs
// a scheme in this process, which needs to be elevated to write %WINDIR%\Cursors (ElevatedInstaller
// decides).
namespace CustomCursor::Services::Cursors::SchemeInstaller {

// Copies the scheme's files to %WINDIR%\Cursors\<sanitized name> (skipping the names that aren't
// copyable and the files that don't exist), registers the comma-separated string of the roles in
// ...\Cursors\Schemes and makes it the active scheme. On failure `errorMessage` is
// "Error: <message>\n\nDetails: <inner message or N/A>" (the C#'s, without the stack trace).
bool ApplyCursorScheme(const Models::CursorScheme& scheme, std::wstring& errorMessage);

struct Result {
    std::wstring schemeName;
    bool success = false;
    std::wstring errorMessage;
};
// (current, total, scheme name) before each scheme; any thread
using Progress = std::function<void(int current, int total, const std::wstring& schemeName)>;
// One after the other (without the C#'s 100 ms pause after each: the work runs on a worker thread,
// the window doesn't need it to redraw)
std::vector<Result> ApplyMultipleCursorSchemes(const std::vector<Models::CursorScheme>& schemes, const Progress& progress = {});

} // namespace CustomCursor::Services::Cursors::SchemeInstaller
