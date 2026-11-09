#include "Services/Cursors/SchemeScanner.h"
#include <windows.h>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Data/CursorRoles.h"
#include "Services/App/Log.h"
#include "Services/Cursors/InfParser.h"

namespace CustomCursor::Services::Cursors::SchemeScanner {

namespace {

struct Entry {
    std::wstring path;
    DWORD attributes = 0;
};

// Directory.GetFiles / GetDirectories: the folder's entries in the file system's order
std::vector<Entry> List(const std::wstring& folder, bool directories) {
    std::vector<Entry> out;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(Files::Combine(folder, L"*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return out;
    do {
        const bool isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory != directories) continue;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        out.push_back({ Files::Combine(folder, data.cFileName), data.dwFileAttributes });
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return out;
}

// Path.GetExtension(file).ToLowerInvariant()
std::wstring LowerExtension(const std::wstring& path) { return Text::Lower(Files::Extension(path)); }

// GetCategoryFromPath: the first folder of the relative path, "General" for the root itself
std::wstring CategoryFromPath(const std::wstring& relativePath) {
    if (relativePath == L".") return Config::DefaultCategory;
    return relativePath.substr(0, relativePath.find(L'\\'));
}

void AddScheme(std::vector<Models::CursorCategory>& categories, Models::CursorScheme scheme) {
    for (auto& category : categories)
        if (category.name == scheme.category) {
            category.schemes.push_back(std::move(scheme));
            return;
        }
    categories.push_back({ scheme.category, {} });
    categories.back().schemes.push_back(std::move(scheme));
}

void ScanDirectory(const std::wstring& currentPath, const std::wstring& basePath, std::vector<Models::CursorCategory>& categories) {
    const std::wstring infPath = Files::Combine(currentPath, Config::SchemeFileName);
    if (Files::Exists(infPath)) {
        if (auto parsed = InfParser::Parse(infPath)) {
            Models::CursorScheme scheme;
            scheme.name = parsed->schemeName;
            scheme.path = currentPath;
            // Path.GetRelativePath(basePath, currentPath)
            scheme.relativePath = currentPath.size() > basePath.size() ? currentPath.substr(basePath.size() + 1) : L".";
            scheme.previewImagePath = FindPreviewImage(currentPath);
            scheme.cursorMap = std::move(parsed->cursorMap);
            scheme.category = CategoryFromPath(scheme.relativePath);
            AddScheme(categories, std::move(scheme));
        } else {
            Log::Warning(L"Error processing " + infPath + L": Error parsing INF file: " + infPath);
        }
    }
    // Recursively scan the subdirectories, skipping hidden ones
    for (const Entry& sub : List(currentPath, true))
        if (!(sub.attributes & FILE_ATTRIBUTE_HIDDEN)) ScanDirectory(sub.path, basePath, categories);
}

} // namespace

std::vector<Models::CursorCategory> FindCursorSets(const std::wstring& cursorDirectory) {
    std::vector<Models::CursorCategory> categories;
    if (!Files::DirectoryExists(cursorDirectory)) return categories;
    ScanDirectory(cursorDirectory, cursorDirectory, categories);
    return categories;
}

std::optional<std::wstring> FindPreviewImage(const std::wstring& folderPath) {
    for (const Entry& file : List(folderPath, false)) {
        const std::wstring ext = LowerExtension(file.path);
        for (const wchar_t* preview : Config::PreviewExtensions)
            if (ext == preview) return file.path;
    }
    return std::nullopt;
}

bool IsCopyableFileName(const std::wstring& fileName) {
    if (Text::TrimDotNet(fileName).empty()) return false;   // string.IsNullOrWhiteSpace
    return fileName.find(L'$') == std::wstring::npos && fileName.find(L"..") == std::wstring::npos && !Paths::HasInvalidFileNameChars(fileName);
}

int CountChangedCursors(const Models::CursorScheme& scheme) {
    int count = 0;
    for (const auto& role : Data::CursorRoles::All()) {
        const std::wstring* fileName = scheme.Find(role.infKey);
        if (!fileName || !IsCopyableFileName(*fileName)) continue;
        if (Files::Exists(Files::DotNetCombine(scheme.path, *fileName))) ++count;
    }
    return count;
}

std::vector<Models::CursorInfo> LoadCursorInfos(const Models::CursorScheme& scheme) {
    std::vector<Models::CursorInfo> infos;
    for (const auto& role : Data::CursorRoles::All()) {
        const std::wstring* fileName = scheme.Find(role.infKey);
        if (!fileName) continue;
        if (Files::Exists(Files::DotNetCombine(scheme.path, *fileName))) infos.push_back({ role.labelKey, *fileName, role.infKey });
    }
    return infos;
}

std::optional<std::wstring> PreviewPath(const Models::CursorScheme& scheme) {
    if (scheme.previewImagePath && !scheme.previewImagePath->empty() && Files::Exists(*scheme.previewImagePath)) return scheme.previewImagePath;
    const std::vector<Entry> files = List(scheme.path, false);
    // Directory.GetFiles(schemePath, "*.gif"), then "*.ani", then "*.png" + "*.jpg" + "*.jpeg"
    for (const wchar_t* ext : { L".gif", L".ani", L".png", L".jpg", L".jpeg" })
        for (const Entry& file : files)
            if (LowerExtension(file.path) == ext) return file.path;
    return std::nullopt;
}

} // namespace CustomCursor::Services::Cursors::SchemeScanner
