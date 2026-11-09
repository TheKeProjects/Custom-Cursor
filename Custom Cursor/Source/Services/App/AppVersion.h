#pragma once
#include <string>
#include "App/Config.h"
#include "Core/Text.h"
#include "Version.h"

// Services\AppVersion.cs (the version itself lives in Resources\Version.h)
namespace CustomCursor::AppVersion {

inline const std::wstring CurrentVersion = Text::Widen(APP_VERSION_STRING);

// Repository and GameBanana id live in App/Config.h
inline constexpr const wchar_t* GitHubOwner = Config::GitHub::Owner;
inline constexpr const wchar_t* GitHubRepo = Config::GitHub::Repo;
inline constexpr const wchar_t* GitHubBranch = Config::GitHub::Branch;
inline constexpr int GameBananaToolId = Config::GameBanana::ToolId;

// The season baked into the build (<HalloweenMode>... in the .vcxproj): the theme and the exe's icon
inline std::wstring BuildDefaultTheme() {
#if defined(CHRISTMAS_MODE)
    return L"christmas";
#elif defined(HALLOWEEN_MODE)
    return L"halloween";
#elif defined(SUMMER_MODE)
    return L"summer";
#elif defined(LGBTQ_MODE)
    return L"lgbtq";
#else
    return L"classic";
#endif
}

inline std::wstring DisplayVersion() { return L"v" + CurrentVersion; }   // GetDisplayVersion
inline std::wstring GitHubRepoUrl() { return std::wstring(L"https://github.com/") + GitHubOwner + L"/" + GitHubRepo; }

} // namespace CustomCursor::AppVersion
