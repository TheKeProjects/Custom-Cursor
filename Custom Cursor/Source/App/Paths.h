#pragma once
#include <string>

// Utilities\PathHelper.cs: the well-known folders.
namespace CustomCursor::Paths {

// %TEMP%\CustomCursor — the embedded assets, schemes and tutorial are unpacked here on startup
// (GetWorkingDirectory)
const std::wstring& TempDir();
std::wstring Asset(const std::wstring& relative);          // TempDir\Assets\<relative>
std::wstring Translation(const std::wstring& fileName);    // TempDir\Translations\<file>
// TempDir\Cursores (GetCursorDirectory: created when missing)
std::wstring CursorDirectory();
// TempDir\Tutorial (GetTutorialDirectory)
std::wstring TutorialDirectory();

// %LOCALAPPDATA%\CustomCursor — language.cfg, logs\ (new in the C++ port)
const std::wstring& DataDir();
std::wstring DataFile(const std::wstring& name);

// %WINDIR%\Cursors (GetWindowsCursorsDirectory)
std::wstring WindowsCursorsDirectory();

// Path.GetInvalidFileNameChars() on Windows: " < > | : * ? \ / and the characters 0-31
bool IsInvalidFileNameChar(wchar_t c);
bool HasInvalidFileNameChars(const std::wstring& s);

// SanitizeSchemeName: the scheme's name without the invalid file name characters, then without
// anything but [a-zA-Z0-9 _-], trimmed. It's both the folder in %WINDIR%\Cursors and the value
// name in ...\Cursors\Schemes, so it has to come out exactly as the C#'s (a scheme installed by one
// version is recognised and uninstalled by the other).
std::wstring SanitizeSchemeName(const std::wstring& schemeName);

// The name a scheme is installed under (its folder and its Schemes value): SanitizeSchemeName when
// that keeps the whole name (every scheme the C# could install without trouble: the same name, so
// both versions recognise each other's); when characters had to go (accents, other alphabets,
// emoji) it gets "-" and 8 hex digits of the full name's hash, so two such schemes don't land on the
// same name and a name with nothing left still gets one ("Scheme-…") instead of %WINDIR%\Cursors itself
std::wstring SchemeInstallName(const std::wstring& schemeName);
// The C#'s name for a scheme whose install name differs (to find what it installed); "" otherwise
std::wstring LegacyInstallName(const std::wstring& schemeName);

} // namespace CustomCursor::Paths
