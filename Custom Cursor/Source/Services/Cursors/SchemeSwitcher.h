#pragma once
#include <string>

// CursorRegistry.cs, the part that makes a scheme the active one: writes HKCU\Control Panel\Cursors
// and tells Windows to reload the cursors.
namespace CustomCursor::Services::Cursors::SchemeSwitcher {

// The values of a registered scheme: "Scheme Source" = 1 and (Default) = the name first, as the
// Control Panel does, then each role in CursorRoles' order from the comma-separated Schemes string
// (REG_EXPAND_SZ, or an empty REG_SZ), then SPI_SETCURSORS. Nothing is written when the Cursors key
// doesn't exist (OpenSubKey returned null), but the cursors are still reloaded. Throws CursorError.
void WriteActiveScheme(const std::wstring& schemeName, const std::wstring& schemeString);

// SystemParametersInfo(SPI_SETCURSORS, 0, null, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)
void ReloadCursors();

// ApplySchemeByName: a scheme of ...\Cursors\Schemes made the active one. Errors:
// "Scheme '<name>' not found in registry" / "Error applying scheme: <message>"
bool ApplySchemeByName(const std::wstring& schemeName, std::wstring& errorMessage);

// ApplyDefaultCursorScheme: "Scheme Source" = 0, (Default) = "", every role "" (Windows' own
// cursors). Error: "Error applying default cursor: <message>"
bool ApplyDefaultCursorScheme(std::wstring& errorMessage);

} // namespace CustomCursor::Services::Cursors::SchemeSwitcher
