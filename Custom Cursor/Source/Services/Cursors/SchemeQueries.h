#pragma once
#include <optional>
#include <string>
#include <vector>

// CursorRegistry.cs, the questions about the installed schemes (errors are swallowed as there).
namespace CustomCursor::Services::Cursors::SchemeQueries {

// The Schemes value the scheme is installed under: its install name, or the C#'s name for it
// (Paths::LegacyInstallName); "" when it isn't installed
std::wstring InstalledName(const std::wstring& schemeName);
// IsSchemeInstalled
bool IsSchemeInstalled(const std::wstring& schemeName);

// GetActiveScheme: (Default) of HKCU\Control Panel\Cursors, or "Scheme Name" (what older versions of
// the app wrote) only when (Default) isn't set; nullopt when neither is
std::optional<std::wstring> GetActiveScheme();

// GetInstalledSchemes: every value name of ...\Cursors\Schemes but blank ones, in the registry's order
std::vector<std::wstring> GetInstalledSchemes();

} // namespace CustomCursor::Services::Cursors::SchemeQueries
