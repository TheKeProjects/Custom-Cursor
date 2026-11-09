#include "Services/Cursors/SchemeQueries.h"
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Text.h"
#include "Services/Cursors/Registry.h"

namespace CustomCursor::Services::Cursors::SchemeQueries {

std::wstring InstalledName(const std::wstring& schemeName) {
    try {
        const RegistryKey key = RegistryKey::Open(Config::SchemesKey, false);
        const std::wstring name = Paths::SchemeInstallName(schemeName);
        if (key.HasValue(name)) return name;
        const std::wstring legacy = Paths::LegacyInstallName(schemeName);
        if (!legacy.empty() && key.HasValue(legacy)) return legacy;
    } catch (const CursorError&) {
        // Not installed as far as we can tell
    }
    return L"";
}

bool IsSchemeInstalled(const std::wstring& schemeName) { return !InstalledName(schemeName).empty(); }

std::optional<std::wstring> GetActiveScheme() {
    try {
        const RegistryKey key = RegistryKey::Open(Config::CursorsKey, false);
        if (auto name = key.GetString(L"")) return name;
        return key.GetString(Config::LegacySchemeNameValue);
    } catch (const CursorError&) {
        return std::nullopt;
    }
}

std::vector<std::wstring> GetInstalledSchemes() {
    std::vector<std::wstring> schemes;
    try {
        for (const std::wstring& name : RegistryKey::Open(Config::SchemesKey, false).ValueNames())
            if (!Text::TrimDotNet(name).empty()) schemes.push_back(name);
    } catch (const CursorError&) {
        // Ignore errors
    }
    return schemes;
}

} // namespace CustomCursor::Services::Cursors::SchemeQueries
