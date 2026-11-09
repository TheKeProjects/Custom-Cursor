#pragma once
#include <optional>
#include <string>
#include "Core/Json.h"

// Small settings files of the C++ port (language.cfg) in %LOCALAPPDATA%\CustomCursor.
// Every read/write swallows errors, like the try { } catch { } wrappers in the old code.
namespace CustomCursor::Config {

std::wstring FilePath(const std::wstring& fileName);

// Plain text .cfg files (trimmed contents)
std::optional<std::wstring> ReadText(const std::wstring& fileName);
void WriteText(const std::wstring& fileName, const std::wstring& value);

// JSON files (System.Text.Json compatible: PascalCase property names)
Json::Value ReadJson(const std::wstring& fileName, bool* exists = nullptr);
void WriteJson(const std::wstring& fileName, const Json::Value& value, bool indented = false);

} // namespace CustomCursor::Config
