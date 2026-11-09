#include "App/Paths.h"
#include <windows.h>
#include <cstdint>
#include <cwchar>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace CustomCursor::Paths {

// PathHelper.IsRunningAsSingleFileExe compared the process path with %TEMP%, which a published exe
// never matched, so the C# actually read the schemes from .NET's own extraction folder. The C++ exe
// always unpacks to %TEMP%\CustomCursor, the folder the C# meant to use.
const std::wstring& TempDir() {
    static const std::wstring dir = Files::Combine(Files::TempDirectory(), Config::WorkFolder);
    return dir;
}

std::wstring Asset(const std::wstring& relative) { return Files::Combine(TempDir(), L"Assets", relative); }

std::wstring Translation(const std::wstring& fileName) { return Files::Combine(TempDir(), L"Translations", fileName); }

std::wstring CursorDirectory() {
    const std::wstring dir = Files::Combine(TempDir(), Config::CursorFolderName);
    if (!Files::DirectoryExists(dir)) Files::CreateDirectories(dir);
    return dir;
}

std::wstring TutorialDirectory() { return Files::Combine(TempDir(), Config::TutorialFolderName); }

const std::wstring& DataDir() {
    static const std::wstring dir = Files::Combine(Files::LocalAppData(), Config::DataFolder);
    return dir;
}

std::wstring DataFile(const std::wstring& name) { return Files::Combine(DataDir(), name); }

std::wstring WindowsCursorsDirectory() {
    wchar_t winDir[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"WINDIR", winDir, MAX_PATH);
    const std::wstring root = n > 0 && n < MAX_PATH ? std::wstring(winDir) : std::wstring(Config::DefaultWindowsDir);
    return Files::Combine(root, Config::WindowsCursorsFolder);
}

bool IsInvalidFileNameChar(wchar_t c) {
    return c < 32 || c == L'"' || c == L'<' || c == L'>' || c == L'|' || c == L':' || c == L'*' || c == L'?' || c == L'\\' || c == L'/';
}

bool HasInvalidFileNameChars(const std::wstring& s) {
    for (wchar_t c : s)
        if (IsInvalidFileNameChar(c)) return true;
    return false;
}

std::wstring SanitizeSchemeName(const std::wstring& schemeName) {
    // The invalid characters go first, then Regex.Replace(safe, @"[^a-zA-Z0-9 _-]", "") removes the
    // rest (UTF-16 units, so both halves of an emoji), then Trim(): only spaces can be left to trim
    std::wstring safe;
    for (wchar_t c : schemeName) {
        if (IsInvalidFileNameChar(c)) continue;
        const bool kept = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L' ' || c == L'_' || c == L'-';
        if (kept) safe += c;
    }
    const size_t first = safe.find_first_not_of(L' ');
    if (first == std::wstring::npos) return L"";
    return safe.substr(first, safe.find_last_not_of(L' ') - first + 1);
}

namespace {

// Whether sanitizing dropped anything but surrounding white space
bool Lossless(const std::wstring& schemeName, const std::wstring& safe) { return !safe.empty() && safe == Text::TrimDotNet(schemeName); }

} // namespace

std::wstring SchemeInstallName(const std::wstring& schemeName) {
    const std::wstring safe = SanitizeSchemeName(schemeName);
    if (Lossless(schemeName, safe)) return safe;
    // FNV-1a over the UTF-16 units of the full name
    uint32_t hash = 2166136261u;
    for (wchar_t c : schemeName) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }
    wchar_t hex[9]{};
    swprintf_s(hex, L"%08x", hash);
    return (safe.empty() ? std::wstring(L"Scheme") : safe) + L"-" + hex;
}

std::wstring LegacyInstallName(const std::wstring& schemeName) {
    const std::wstring safe = SanitizeSchemeName(schemeName);
    return Lossless(schemeName, safe) ? std::wstring() : safe;
}

} // namespace CustomCursor::Paths
