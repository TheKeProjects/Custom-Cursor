#include "Services/App/TutorialService.h"
#include <windows.h>
#include <algorithm>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace CustomCursor::Services::TutorialService {

std::vector<std::wstring> GetTutorialVideos() {
    const std::wstring tutorialDir = Paths::TutorialDirectory();
    if (!Files::DirectoryExists(tutorialDir)) return {};
    std::vector<std::wstring> videos;
    for (const std::wstring& file : Files::ListFiles(tutorialDir, L"*", true)) {
        const std::wstring ext = Text::Lower(Files::Extension(file));
        for (const wchar_t* video : Config::VideoExtensions)
            if (ext == video) {
                videos.push_back(file);
                break;
            }
    }
    // OrderBy(file => Path.GetFileName(file)): the current culture's string order, stable
    std::stable_sort(videos.begin(), videos.end(), [](const std::wstring& a, const std::wstring& b) {
        const std::wstring na = Files::FileName(a), nb = Files::FileName(b);
        return CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0, na.c_str(), static_cast<int>(na.size()), nb.c_str(), static_cast<int>(nb.size()), nullptr,
                               nullptr, 0) == CSTR_LESS_THAN;
    });
    return videos;
}

std::optional<std::wstring> GetFirstTutorialVideo() {
    const auto videos = GetTutorialVideos();
    if (videos.empty()) return std::nullopt;
    return videos.front();
}

bool HasTutorialVideos() { return !GetTutorialVideos().empty(); }

} // namespace CustomCursor::Services::TutorialService
