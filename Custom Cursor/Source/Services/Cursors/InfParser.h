#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

// CursorParser.ParseInfFile: a scheme.inf read as .NET reads it and matched with the C#'s two regular
// expressions, done by hand (std::wregex doesn't share .NET's \w and \s).
namespace CustomCursor::Services::Cursors::InfParser {

// File.ReadAllText: the byte order mark picks UTF-8, UTF-16 LE / BE or UTF-32 LE / BE; without one
// the bytes are UTF-8, or the ANSI code page when they aren't valid UTF-8 (the C# turned an ANSI
// .inf's accented letters into U+FFFD). nullopt when the file can't be read.
std::optional<std::wstring> ReadAllText(const std::wstring& path);

struct Result {
    std::wstring schemeName;                                      // "Unknown Scheme" without SCHEME_NAME
    std::vector<std::pair<std::wstring, std::wstring>> cursorMap;  // in first-appearance order, last value wins
};

// Every (\w+)\s*=\s*"(.+?)" match goes into the map (CUR_DIR, SCHEME_NAME and signature too), the
// first SCHEME_NAME = "..." that is a whole word gives the name. nullopt when the file can't be read (the C# threw
// "Error parsing INF file: <path>" and the scheme was skipped).
std::optional<Result> Parse(const std::wstring& infPath);
// The same on text already read
Result ParseText(const std::wstring& content);

} // namespace CustomCursor::Services::Cursors::InfParser
