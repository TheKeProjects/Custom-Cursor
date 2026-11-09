#pragma once
#include <optional>
#include <string>
#include "Models/CursorScheme.h"

// CursorRegistry.UninstallCursorScheme: in this process (ElevatedInstaller decides whether that's
// possible or a second, elevated copy has to do it).
namespace CustomCursor::Services::Cursors::SchemeUninstaller {

// When the scheme is the active one (compared without case), first switches to `replacement` (a
// value of ...\Cursors\Schemes) or, without one, to Windows' default cursors; then removes its value
// from ...\Schemes and its folder from %WINDIR%\Cursors. Errors: "Failed to switch to replacement
// cursor: ..." / "Failed to switch to default cursor: ..." / "Error uninstalling cursor: <message>"
bool UninstallCursorScheme(const Models::CursorScheme& scheme, const std::optional<std::wstring>& replacement, std::wstring& errorMessage);

} // namespace CustomCursor::Services::Cursors::SchemeUninstaller
