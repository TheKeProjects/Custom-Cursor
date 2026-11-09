#pragma once
#include <string>
#include "Core/Text.h"
#include "Version.h"

// The manager's version and how versions compare (the Inazuma Eleven VR Save Editor's
// AppVersion.h / VersionCompare.h).
namespace CustomCursor::Versions {

inline const std::wstring& Current() {
    static const std::wstring version = Text::Widen(APP_VERSION_STRING);
    return version;
}

struct Parsed {
    int major = 0, minor = 0, patch = 0;
};

// Keeps digits and dots only ("v3.0.1-beta" → 3.0.1).
inline Parsed Parse(const std::wstring& version) {
    std::wstring clean;
    for (wchar_t c : version)
        if ((c >= L'0' && c <= L'9') || c == L'.') clean += c;
    const auto parts = Text::Split(clean, L'.');
    Parsed p;
    if (parts.size() > 0) p.major = static_cast<int>(Text::ToInt(parts[0]).value_or(0));
    if (parts.size() > 1) p.minor = static_cast<int>(Text::ToInt(parts[1]).value_or(0));
    if (parts.size() > 2) p.patch = static_cast<int>(Text::ToInt(parts[2]).value_or(0));
    return p;
}

// True when `latest` is a newer version than `current`
inline bool IsNewer(const std::wstring& current, const std::wstring& latest) {
    const Parsed c = Parse(current), l = Parse(latest);
    if (l.major != c.major) return l.major > c.major;
    if (l.minor != c.minor) return l.minor > c.minor;
    return l.patch > c.patch;
}

inline bool Same(const std::wstring& a, const std::wstring& b) { return !IsNewer(a, b) && !IsNewer(b, a); }

// "1.0.1" → "v1.0.1" (left alone when it already has its v)
inline std::wstring WithV(const std::wstring& version) {
    if (!version.empty() && (version[0] == L'v' || version[0] == L'V')) return L"v" + version.substr(1);
    return L"v" + version;
}

} // namespace CustomCursor::Versions
