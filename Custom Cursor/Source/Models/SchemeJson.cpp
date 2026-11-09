#include "Models/SchemeJson.h"
#include "Core/Text.h"

namespace CustomCursor::Models::SchemeJson {

namespace {

// A string property: a JSON string, or null (the C# string becomes null; here "" / nullopt)
bool ReadString(const Json::Value& object, const char* name, std::wstring& out) {
    if (!object.Has(name)) return true;
    const Json::Value& v = object[name];
    if (v.IsNull()) {
        out.clear();
        return true;
    }
    if (!v.IsString()) return false;
    out = v.WStr();
    return true;
}

// File.ReadAllText drops a UTF-8 byte order mark
std::string_view WithoutBom(const std::string& json) {
    std::string_view view(json);
    if (view.size() >= 3 && view.substr(0, 3) == "\xEF\xBB\xBF") view.remove_prefix(3);
    return view;
}

} // namespace

Json::Value ToJson(const CursorScheme& scheme) {
    Json::Value map = Json::Value::MakeObject();
    for (const auto& [key, value] : scheme.cursorMap) map.Set(Text::Narrow(key), value);
    Json::Value v = Json::Value::MakeObject();
    v.Set("Name", scheme.name);
    v.Set("Path", scheme.path);
    v.Set("RelativePath", scheme.relativePath);
    v.Set("PreviewImagePath", scheme.previewImagePath ? Json::Value(*scheme.previewImagePath) : Json::Value(nullptr));
    v.Set("CursorMap", std::move(map));
    v.Set("Category", scheme.category);
    return v;
}

std::optional<CursorScheme> FromJson(const Json::Value& value) {
    if (!value.IsObject()) return std::nullopt;
    CursorScheme s;
    if (!ReadString(value, "Name", s.name) || !ReadString(value, "Path", s.path) || !ReadString(value, "RelativePath", s.relativePath) ||
        !ReadString(value, "Category", s.category))
        return std::nullopt;
    if (value.Has("PreviewImagePath")) {
        const Json::Value& preview = value["PreviewImagePath"];
        if (preview.IsString()) s.previewImagePath = preview.WStr();
        else if (!preview.IsNull()) return std::nullopt;
    }
    if (value.Has("CursorMap")) {
        const Json::Value& map = value["CursorMap"];
        if (map.IsObject()) {
            for (const auto& [key, item] : map.Items()) {
                if (!item.IsString() && !item.IsNull()) return std::nullopt;
                s.Set(Text::Widen(key), item.WStr());
            }
        } else if (map.IsNull()) {
            s.cursorMap.clear();
        } else {
            return std::nullopt;
        }
    }
    return s;
}

std::string SerializeInstall(const std::vector<CursorScheme>& schemes) {
    Json::Value list = Json::Value::MakeArray();
    for (const auto& scheme : schemes) list.Push(ToJson(scheme));
    return list.SerializeSystemTextJson(false);
}

std::string SerializeUninstall(const CursorScheme& scheme, const std::optional<std::wstring>& replacement) {
    Json::Value v = Json::Value::MakeObject();
    v.Set("Scheme", ToJson(scheme));
    v.Set("ReplacementScheme", replacement ? Json::Value(*replacement) : Json::Value(nullptr));
    return v.SerializeSystemTextJson(false);
}

std::optional<std::vector<CursorScheme>> ParseInstall(const std::string& json) {
    bool ok = false;
    const Json::Value root = Json::Parse(WithoutBom(json), &ok);
    if (!ok || !root.IsArray()) return std::nullopt;
    std::vector<CursorScheme> schemes;
    for (const auto& item : root.Elements()) {
        auto scheme = FromJson(item);
        if (!scheme) return std::nullopt;
        schemes.push_back(std::move(*scheme));
    }
    return schemes;
}

std::optional<UninstallRequest> ParseUninstall(const std::string& json) {
    bool ok = false;
    const Json::Value root = Json::Parse(WithoutBom(json), &ok);
    if (!ok || !root.IsObject() || !root.Has("Scheme")) return std::nullopt;
    auto scheme = FromJson(root["Scheme"]);
    if (!scheme) return std::nullopt;
    UninstallRequest request{ std::move(*scheme), std::nullopt };
    if (root.Has("ReplacementScheme")) {
        const Json::Value& replacement = root["ReplacementScheme"];
        if (replacement.IsString()) request.replacement = replacement.WStr();
        else if (!replacement.IsNull()) return std::nullopt;
    }
    return request;
}

} // namespace CustomCursor::Models::SchemeJson
