#include "Services/Cursors/SchemeInstaller.h"
#include <windows.h>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Data/CursorRoles.h"
#include "Services/App/Log.h"
#include "Services/Cursors/Registry.h"
#include "Services/Cursors/SchemeScanner.h"
#include "Services/Cursors/SchemeSwitcher.h"

namespace CustomCursor::Services::Cursors::SchemeInstaller {

namespace {

// IOException / UnauthorizedAccessException's message for a path
std::wstring PathError(DWORD code, const std::wstring& path) {
    if (code == ERROR_ACCESS_DENIED) return L"Access to the path '" + path + L"' is denied.";
    if (code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS)
        return L"Cannot create '" + path + L"' because a file or directory with the same name already exists.";
    return ErrorText(code) + L" : '" + path + L"'";
}

// Directory.CreateDirectory: the missing folders from the top down; the error names the one that
// couldn't be created. Throws CursorError
void CreateDirectoryOrThrow(const std::wstring& path) {
    if (Files::DirectoryExists(path)) return;
    for (size_t at = path.find(L'\\', 3); ; at = path.find(L'\\', at + 1)) {
        const std::wstring part = at == std::wstring::npos ? path : path.substr(0, at);
        if (!Files::DirectoryExists(part) && !CreateDirectoryW(part.c_str(), nullptr)) {
            const DWORD code = GetLastError();
            if (code != ERROR_ALREADY_EXISTS || !Files::DirectoryExists(part)) throw CursorError{ PathError(code, part), L"" };
        }
        if (at == std::wstring::npos) break;
    }
}

// CursorFileService.CopyCursorFile: File.Copy(source, destination, overwrite: true)
void CopyCursorFile(const std::wstring& sourceFile, const std::wstring& destinationFile) {
    CreateDirectoryOrThrow(Files::DirectoryName(destinationFile));
    if (!CopyFileW(sourceFile.c_str(), destinationFile.c_str(), FALSE)) {
        const DWORD code = GetLastError();
        throw CursorError{ L"Failed to copy cursor file from " + sourceFile + L" to " + destinationFile, PathError(code, destinationFile) };
    }
}

// RegisterScheme: the string as a REG_SZ value of ...\Cursors\Schemes, the key created when missing
void RegisterScheme(const std::wstring& schemeName, const std::wstring& schemeString) {
    try {
        RegistryKey key = RegistryKey::Open(Config::SchemesKey, true);
        if (!key.Valid()) key = RegistryKey::Create(Config::SchemesKey);
        key.SetString(schemeName, schemeString);
    } catch (const CursorError& e) {
        throw CursorError{ L"Failed to register cursor scheme: " + schemeName, e.message };
    }
}

// ApplyScheme: reads the string back from ...\Schemes and sets the individual values
void ApplyScheme(const std::wstring& schemeName) {
    try {
        const auto schemeString = RegistryKey::Open(Config::SchemesKey, false).GetString(schemeName);
        if (!schemeString || schemeString->empty()) throw CursorError{ L"Scheme '" + schemeName + L"' not found in registry", L"" };
        SchemeSwitcher::WriteActiveScheme(schemeName, *schemeString);
    } catch (const CursorError& e) {
        throw CursorError{ L"Failed to apply cursor scheme: " + schemeName, e.message };
    }
}

} // namespace

bool ApplyCursorScheme(const Models::CursorScheme& scheme, std::wstring& errorMessage) {
    errorMessage.clear();
    try {
        const std::wstring safeSchemeName = Paths::SchemeInstallName(scheme.name);
        const std::wstring systemCursorPath = Files::DotNetCombine(Paths::WindowsCursorsDirectory(), safeSchemeName);
        CreateDirectoryOrThrow(systemCursorPath);

        // Copy the roles' cursor files, skipping special tokens and invalid names (the C# went through
        // every entry of the map, CUR_DIR, SCHEME_NAME and signature included)
        for (const auto& role : Data::CursorRoles::All()) {
            const std::wstring* found = scheme.Find(role.infKey);
            if (!found) continue;
            const std::wstring& filename = *found;
            if (!SchemeScanner::IsCopyableFileName(filename)) {
                Log::Info(L"Skipping invalid filename: " + filename);
                continue;
            }
            const std::wstring sourceFile = Files::DotNetCombine(scheme.path, filename);
            const std::wstring destFile = Files::DotNetCombine(systemCursorPath, filename);
            if (!Files::Exists(sourceFile)) {
                Log::Info(L"Source file not found (skipping): " + sourceFile);
                continue;
            }
            try {
                CopyCursorFile(sourceFile, destFile);
            } catch (const CursorError& e) {
                Log::Warning(L"Error copying " + filename + L": " + e.message);   // continue with the other files
            }
        }

        // The scheme string: "%SystemRoot%\Cursors\<scheme>\<file>" or "" per role, joined with ','
        std::wstring fullSchemeString;
        bool first = true;
        for (const auto& role : Data::CursorRoles::All()) {
            if (!first) fullSchemeString += L',';
            first = false;
            const std::wstring* filename = scheme.Find(role.infKey);
            if (filename && SchemeScanner::IsCopyableFileName(*filename))
                fullSchemeString += Config::SchemeFilePrefix + safeSchemeName + L"\\" + *filename;
        }

        RegisterScheme(safeSchemeName, fullSchemeString);
        ApplyScheme(safeSchemeName);
        return true;
    } catch (const CursorError& e) {
        errorMessage = L"Error: " + e.message + L"\n\nDetails: " + (e.inner.empty() ? std::wstring(L"N/A") : e.inner);
        Log::Error(L"Error applying cursor scheme: " + errorMessage);
        return false;
    }
}

std::vector<Result> ApplyMultipleCursorSchemes(const std::vector<Models::CursorScheme>& schemes, const Progress& progress) {
    std::vector<Result> results;
    const int total = static_cast<int>(schemes.size());
    for (int i = 0; i < total; ++i) {
        const auto& scheme = schemes[static_cast<size_t>(i)];
        if (progress) progress(i + 1, total, scheme.name);
        Result r{ scheme.name };
        r.success = ApplyCursorScheme(scheme, r.errorMessage);
        results.push_back(std::move(r));
    }
    return results;
}

} // namespace CustomCursor::Services::Cursors::SchemeInstaller
