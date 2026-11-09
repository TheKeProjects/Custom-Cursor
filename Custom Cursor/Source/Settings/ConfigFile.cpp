#include "Settings/ConfigFile.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace CustomCursor::Config {

std::wstring FilePath(const std::wstring& fileName) { return Paths::DataFile(fileName); }

std::optional<std::wstring> ReadText(const std::wstring& fileName) {
    auto text = Files::ReadText(FilePath(fileName));
    if (!text) return std::nullopt;
    return Text::Trim(*text);
}

void WriteText(const std::wstring& fileName, const std::wstring& value) { Files::WriteText(FilePath(fileName), value); }

Json::Value ReadJson(const std::wstring& fileName, bool* exists) {
    auto text = Files::ReadBytes(FilePath(fileName));
    if (exists) *exists = text.has_value();
    if (!text) return {};
    return Json::Parse(std::string_view(*text));
}

void WriteJson(const std::wstring& fileName, const Json::Value& value, bool indented) {
    const std::wstring path = FilePath(fileName);
    Files::CreateDirectories(Files::DirectoryName(path));
    Files::WriteBytes(path, value.Serialize(indented));
}

} // namespace CustomCursor::Config
