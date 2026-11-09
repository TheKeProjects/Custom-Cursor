#include "Services/Cursors/SchemeSwitcher.h"
#include <windows.h>
#include "App/Config.h"
#include "Core/Text.h"
#include "Data/CursorRoles.h"
#include "Services/Cursors/Registry.h"

namespace CustomCursor::Services::Cursors::SchemeSwitcher {

void ReloadCursors() { SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE); }

void WriteActiveScheme(const std::wstring& schemeName, const std::wstring& schemeString) {
    // schemeString.Split(','): every piece, empty ones included
    const std::vector<std::wstring> cursorPaths = Text::Split(schemeString, L',');
    {
        RegistryKey key = RegistryKey::Open(Config::CursorsKey, true);
        if (key.Valid()) {
            key.SetDword(Config::SchemeSourceValue, 1);
            key.SetString(L"", schemeName);
            size_t index = 0;
            for (const auto& role : Data::CursorRoles::All()) {
                const std::wstring cursorPath = index < cursorPaths.size() ? Text::TrimDotNet(cursorPaths[index]) : L"";
                key.SetString(role.registryName, cursorPath, !cursorPath.empty());
                ++index;
            }
        }
    }
    ReloadCursors();
}

bool ApplySchemeByName(const std::wstring& schemeName, std::wstring& errorMessage) {
    errorMessage.clear();
    try {
        const auto schemeString = RegistryKey::Open(Config::SchemesKey, false).GetString(schemeName);
        if (!schemeString || schemeString->empty()) {
            errorMessage = L"Scheme '" + schemeName + L"' not found in registry";
            return false;
        }
        WriteActiveScheme(schemeName, *schemeString);
        return true;
    } catch (const CursorError& e) {
        errorMessage = L"Error applying scheme: " + e.message;
        return false;
    }
}

bool ApplyDefaultCursorScheme(std::wstring& errorMessage) {
    errorMessage.clear();
    try {
        {
            RegistryKey key = RegistryKey::Open(Config::CursorsKey, true);
            if (key.Valid()) {
                key.SetDword(Config::SchemeSourceValue, 0);
                key.SetString(L"", L"");
                // Every role empty: Windows' defaults
                for (const auto& role : Data::CursorRoles::All()) key.SetString(role.registryName, L"");
            }
        }
        ReloadCursors();
        return true;
    } catch (const CursorError& e) {
        errorMessage = L"Error applying default cursor: " + e.message;
        return false;
    }
}

} // namespace CustomCursor::Services::Cursors::SchemeSwitcher
