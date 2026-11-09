#include "Services/Cursors/SchemeUninstaller.h"
#include <windows.h>
#include <filesystem>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Services/Cursors/Registry.h"
#include "Services/Cursors/SchemeQueries.h"
#include "Services/Cursors/SchemeSwitcher.h"

namespace CustomCursor::Services::Cursors::SchemeUninstaller {

namespace {

// Directory.Delete(path, recursive: true): stops at the first file it can't delete and throws
void DeleteDirectoryOrThrow(const std::wstring& path) {
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::path(path), ec);
    if (!ec) return;
    if (ec.value() == ERROR_ACCESS_DENIED) throw CursorError{ L"Access to the path '" + path + L"' is denied.", L"" };
    throw CursorError{ ErrorText(static_cast<DWORD>(ec.value())) + L" : '" + path + L"'", L"" };
}

} // namespace

bool UninstallCursorScheme(const Models::CursorScheme& scheme, const std::optional<std::wstring>& replacement, std::wstring& errorMessage) {
    errorMessage.clear();
    try {
        // The name it's installed under (what the C# installed keeps the C#'s name)
        std::wstring safeSchemeName = SchemeQueries::InstalledName(scheme.name);
        if (safeSchemeName.empty()) safeSchemeName = Paths::SchemeInstallName(scheme.name);
        const std::wstring systemCursorPath = Files::DotNetCombine(Paths::WindowsCursorsDirectory(), safeSchemeName);

        // The active scheme: switch to the replacement or to the default first
        const auto activeScheme = SchemeQueries::GetActiveScheme();
        if (activeScheme && !activeScheme->empty() && Text::Equals(*activeScheme, safeSchemeName, true)) {
            std::wstring switchError;
            if (replacement && !replacement->empty()) {
                if (!SchemeSwitcher::ApplySchemeByName(*replacement, switchError)) {
                    errorMessage = L"Failed to switch to replacement cursor: " + switchError;
                    return false;
                }
            } else if (!SchemeSwitcher::ApplyDefaultCursorScheme(switchError)) {
                errorMessage = L"Failed to switch to default cursor: " + switchError;
                return false;
            }
        }

        {
            RegistryKey key = RegistryKey::Open(Config::SchemesKey, true);
            if (key.Valid()) key.DeleteValue(safeSchemeName);
        }
        // A name that sanitizes to nothing would make that %WINDIR%\Cursors itself: the C# deleted
        // the whole folder (Windows' own cursors too); the C++ port leaves it alone
        if (!safeSchemeName.empty() && Files::DirectoryExists(systemCursorPath)) DeleteDirectoryOrThrow(systemCursorPath);
        return true;
    } catch (const CursorError& e) {
        errorMessage = L"Error uninstalling cursor: " + e.message;
        return false;
    }
}

} // namespace CustomCursor::Services::Cursors::SchemeUninstaller
