#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "App/Config.h"

namespace CustomCursor::Models {

// Models\CursorScheme.cs: a complete cursor scheme (a folder with a scheme.inf).
struct CursorScheme {
    std::wstring name;                               // SCHEME_NAME ("Unknown Scheme" without one)
    std::wstring path;                               // the scheme's folder
    std::wstring relativePath;                       // from Cursores ("Arcane\Jinx")
    std::optional<std::wstring> previewImagePath;    // FindPreviewImage (null when there's none)
    // Dictionary<string, string>: every key = "value" of the .inf, in the order the keys first
    // appeared (a later one replaces the value in place, like the indexer); keys are case-sensitive
    std::vector<std::pair<std::wstring, std::wstring>> cursorMap;
    std::wstring category = Config::DefaultCategory;

    // CursorMap.TryGetValue (ordinal)
    const std::wstring* Find(const std::wstring& key) const {
        for (const auto& [k, v] : cursorMap)
            if (k == key) return &v;
        return nullptr;
    }
    // CursorMap[key] = value
    void Set(const std::wstring& key, const std::wstring& value) {
        for (auto& [k, v] : cursorMap)
            if (k == key) {
                v = value;
                return;
            }
        cursorMap.emplace_back(key, value);
    }
};

} // namespace CustomCursor::Models
