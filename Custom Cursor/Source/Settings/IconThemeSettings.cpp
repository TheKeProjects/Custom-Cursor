#include "Settings/IconThemeSettings.h"
#include "Resource.h"
#include "Services/App/AppVersion.h"
#include "Settings/ConfigFile.h"

namespace CustomCursor::Settings {

namespace {
constexpr const wchar_t* kFile = L"icon_theme_settings.json";
}

IconTheme& IconTheme::Current() {
    static IconTheme current = [] {
        IconTheme t;
        const Json::Value json = Config::ReadJson(kFile);
        if (json.Has("Theme")) t.theme = json["Theme"].WStr();
        return t;
    }();
    return current;
}

void IconTheme::Save() {
    Json::Value json = Json::Value::MakeObject();
    json.Set("Theme", theme);
    Config::WriteJson(kFile, json);
    Current() = *this;
}

std::wstring IconTheme::EffectiveTheme() const { return theme == L"default" ? AppVersion::BuildDefaultTheme() : theme; }

int IconTheme::IconResourceId() const {
    const std::wstring t = EffectiveTheme();
    if (t == L"lgbtq") return IDI_ICON_LGBTQ;
    if (t == L"summer") return IDI_ICON_SUMMER;
    if (t == L"halloween") return IDI_ICON_HALLOWEEN;
    if (t == L"christmas") return IDI_ICON_CHRISTMAS;
    return IDI_ICON_CLASSIC;   // "classic" and unrecognized values use the plain icon
}

std::wstring IconTheme::LogoAsset() const {
    const std::wstring t = EffectiveTheme();
    if (t == L"lgbtq") return L"Logo\\logo LGBTQ+.png";
    if (t == L"summer") return L"Logo\\logo Summer.png";
    if (t == L"halloween") return L"Logo\\logo Halloween.png";
    if (t == L"christmas") return L"Logo\\logo Christmas.png";
    return L"Logo\\logo.png";
}

} // namespace CustomCursor::Settings
