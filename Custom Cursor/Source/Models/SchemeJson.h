#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Core/Json.h"
#include "Models/CursorScheme.h"

// The temporary JSON files ElevatedInstaller hands to the elevated copy of the exe, in
// System.Text.Json's own format (JsonSerializer.Serialize with WriteIndented = false): a C# exe and a
// C++ one can read each other's files.
//   install:   [{"Name":..,"Path":..,"RelativePath":..,"PreviewImagePath":..|null,"CursorMap":{..},"Category":..}, ...]
//   uninstall: {"Scheme":{...},"ReplacementScheme":"name"|null}
namespace CustomCursor::Models::SchemeJson {

Json::Value ToJson(const CursorScheme& scheme);
// JsonSerializer.Deserialize<CursorScheme>: property names are case-sensitive, a missing one keeps
// the model's default; nullopt when the value isn't an object
std::optional<CursorScheme> FromJson(const Json::Value& value);

std::string SerializeInstall(const std::vector<CursorScheme>& schemes);
std::string SerializeUninstall(const CursorScheme& scheme, const std::optional<std::wstring>& replacement);

// nullopt for malformed JSON or not a list (Deserialize<List<CursorScheme>> throwing / null)
std::optional<std::vector<CursorScheme>> ParseInstall(const std::string& json);
struct UninstallRequest {
    CursorScheme scheme;
    std::optional<std::wstring> replacement;
};
// nullopt when "Scheme" is missing or not an object, or "ReplacementScheme" isn't a string / null
std::optional<UninstallRequest> ParseUninstall(const std::string& json);

} // namespace CustomCursor::Models::SchemeJson
