#pragma once
#include <optional>
#include <string>
#include <vector>

// Services\TutorialManager.cs: the tutorial videos unpacked to %TEMP%\CustomCursor\Tutorial.
namespace CustomCursor::Services::TutorialService {

// GetTutorialVideos: every .mp4 / .webm / .avi / .mov / .mkv / .wmv under the folder (subfolders
// too), ordered by file name; empty when the folder is missing
std::vector<std::wstring> GetTutorialVideos();
std::optional<std::wstring> GetFirstTutorialVideo();
bool HasTutorialVideos();

} // namespace CustomCursor::Services::TutorialService
