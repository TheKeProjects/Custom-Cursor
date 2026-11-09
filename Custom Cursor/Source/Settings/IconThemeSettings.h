#pragma once
#include <string>

namespace CustomCursor::Settings {

// The app's theme and logo (icon_theme_settings.json, like the Inazuma Eleven VR Save Editor's).
// "default" follows the mode baked into the build (AppVersion::BuildDefaultTheme); any other value
// overrides it.
struct IconTheme {
    std::wstring theme = L"default";   // default | classic | lgbtq | summer | halloween | christmas

    std::wstring EffectiveTheme() const;
    // Window icon resource id
    int IconResourceId() const;
    // The logo of the theme, relative to Assets ("Logo\\logo Halloween.png")
    std::wstring LogoAsset() const;

    void Save();
    static IconTheme& Current();
};

} // namespace CustomCursor::Settings
