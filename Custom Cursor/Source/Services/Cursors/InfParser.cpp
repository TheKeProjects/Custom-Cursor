#include "Services/Cursors/InfParser.h"
#include <windows.h>
#include "App/Config.h"
#include "Core/Files.h"

namespace CustomCursor::Services::Cursors::InfParser {

namespace {

// .NET's \w: letters, nonspacing marks, decimal digits and connector punctuation (_ and the like)
bool IsWord(wchar_t c) {
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_') return true;
    if (c < 0x80) return false;
    WORD type1 = 0, type3 = 0;
    if (!GetStringTypeW(CT_CTYPE1, &c, 1, &type1) || !GetStringTypeW(CT_CTYPE3, &c, 1, &type3)) return false;
    if (type1 & (C1_ALPHA | C1_DIGIT)) return true;
    if (type3 & C3_NONSPACING) return true;
    // Pc: the undertie, the character tie and the wide / small low lines
    return c == 0x203F || c == 0x2040 || c == 0x2054 || c == 0xFE33 || c == 0xFE34 || (c >= 0xFE4D && c <= 0xFE4F) || c == 0xFF3F;
}

// .NET's \s: \t \n \v \f \r, U+0085 and the Unicode separators (Zs, Zl, Zp)
bool IsSpace(wchar_t c) {
    return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
           c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

// \s*=\s*"(.+?)" from `i`: the value, and where the match ends; false when it doesn't match there
bool MatchValue(const std::wstring& s, size_t i, std::wstring& value, size_t& end) {
    while (i < s.size() && IsSpace(s[i])) ++i;
    if (i >= s.size() || s[i] != L'=') return false;
    ++i;
    while (i < s.size() && IsSpace(s[i])) ++i;
    if (i >= s.size() || s[i] != L'"') return false;
    const size_t start = ++i;
    // .+? : at least one character, none of them \n, up to the first " after it
    for (size_t j = start + 1; j < s.size(); ++j) {
        if (s[j - 1] == L'\n') return false;
        if (s[j] == L'"') {
            value = s.substr(start, j - start);
            end = j + 1;
            return true;
        }
    }
    return false;
}

void Set(std::vector<std::pair<std::wstring, std::wstring>>& map, const std::wstring& key, const std::wstring& value) {
    for (auto& [k, v] : map)
        if (k == key) {
            v = value;
            return;
        }
    map.emplace_back(key, value);
}

} // namespace

std::optional<std::wstring> ReadAllText(const std::wstring& path) {
    const auto bytes = Files::ReadBytes(path);
    if (!bytes) return std::nullopt;
    const auto* b = reinterpret_cast<const unsigned char*>(bytes->data());
    const size_t n = bytes->size();
    const auto utf16 = [&](size_t skip, bool bigEndian) {
        std::wstring out;
        out.reserve((n - skip) / 2);
        for (size_t i = skip; i + 1 < n; i += 2)
            out += static_cast<wchar_t>(bigEndian ? (b[i] << 8 | b[i + 1]) : (b[i] | b[i + 1] << 8));
        if ((n - skip) % 2) out += static_cast<wchar_t>(0xFFFD);
        return out;
    };
    const auto utf32 = [&](bool bigEndian) {
        std::wstring out;
        for (size_t i = 4; i + 3 < n; i += 4) {
            uint32_t cp = bigEndian ? (uint32_t(b[i]) << 24 | b[i + 1] << 16 | b[i + 2] << 8 | b[i + 3])
                                    : (uint32_t(b[i + 3]) << 24 | b[i + 2] << 16 | b[i + 1] << 8 | b[i]);
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
            if (cp >= 0x10000) {
                cp -= 0x10000;
                out += static_cast<wchar_t>(0xD800 + (cp >> 10));
                out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
            } else {
                out += static_cast<wchar_t>(cp);
            }
        }
        return out;
    };
    if (n >= 4 && b[0] == 0xFF && b[1] == 0xFE && b[2] == 0 && b[3] == 0) return utf32(false);
    if (n >= 4 && b[0] == 0 && b[1] == 0 && b[2] == 0xFE && b[3] == 0xFF) return utf32(true);
    if (n >= 2 && b[0] == 0xFF && b[1] == 0xFE) return utf16(2, false);
    if (n >= 2 && b[0] == 0xFE && b[1] == 0xFF) return utf16(2, true);
    const size_t skip = n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF ? 3 : 0;
    if (n == skip) return std::wstring();
    // UTF-8 when the bytes are valid UTF-8, else the system's ANSI code page (Windows' own .inf files;
    // .NET turned their accented letters into U+FFFD)
    const char* text = bytes->data() + skip;
    const int length = static_cast<int>(n - skip);
    UINT codePage = CP_UTF8;
    int wide = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, nullptr, 0);
    if (wide == 0) {
        codePage = CP_ACP;
        wide = MultiByteToWideChar(CP_ACP, 0, text, length, nullptr, 0);
    }
    std::wstring out(static_cast<size_t>(wide), L'\0');
    MultiByteToWideChar(codePage, 0, text, length, out.data(), wide);
    return out;
}

Result ParseText(const std::wstring& content) {
    Result result;
    // Regex.Matches(content, @"(\w+)\s*=\s*""(.+?)"""): scanned left to right, matches don't overlap
    for (size_t i = 0; i < content.size();) {
        if (!IsWord(content[i])) {
            ++i;
            continue;
        }
        size_t wordEnd = i;
        while (wordEnd < content.size() && IsWord(content[wordEnd])) ++wordEnd;
        std::wstring value;
        size_t end = 0;
        if (MatchValue(content, wordEnd, value, end)) {
            Set(result.cursorMap, content.substr(i, wordEnd - i), value);
            i = end;
        } else {
            ++i;   // a match starting later inside the word ends the same way: it fails too
        }
    }
    // Regex.Match(content, @"SCHEME_NAME\s*=\s*""(.+?)"""): the first one anywhere
    result.schemeName = Config::UnknownSchemeName;
    const std::wstring key = L"SCHEME_NAME";
    for (size_t at = content.find(key); at != std::wstring::npos; at = content.find(key, at + 1)) {
        // A whole word only: the C#'s regex also took the end of a longer key (XSCHEME_NAME)
        if ((at > 0 && IsWord(content[at - 1])) || (at + key.size() < content.size() && IsWord(content[at + key.size()]))) continue;
        std::wstring value;
        size_t end = 0;
        if (MatchValue(content, at + key.size(), value, end)) {
            result.schemeName = value;
            break;
        }
    }
    return result;
}

std::optional<Result> Parse(const std::wstring& infPath) {
    const auto content = ReadAllText(infPath);
    if (!content) return std::nullopt;
    return ParseText(*content);
}

} // namespace CustomCursor::Services::Cursors::InfParser
