#pragma once
#include <functional>
#include <string>

// Services\UpdateService.cs (on the Inazuma Eleven VR Save Editor's C++ UpdateService): version.txt and
// changelog.txt from the GitHub repository (Config::GitHub), the release zip CustomCursorv<x-y-z>.zip
// (automatic update) or the bare exe (manual update) from its latest release, and GameBanana (off
// while Config::GameBanana::ToolId is 0). Every function blocks: call them from worker threads.
namespace CustomCursor::Services::Updates {

struct UpdateInfo {
    std::wstring latestVersion;
    std::wstring changelog;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring releaseUrl;
};

struct GbUpdateInfo {
    std::wstring latestVersion;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring changelog;
};

// CheckForUpdatesAsync: failures are swallowed, the result then says "no update" with an empty
// latest version (`reachable` tells whether GitHub answered with a version)
UpdateInfo CheckForUpdates(bool* reachable = nullptr);
// GetLatestReleaseInfoAsync (the manual update): the latest release's exe whatever the version
// comparison says; an empty downloadUrl when the exe can't be reached (the version is cosmetic)
UpdateInfo LatestReleaseInfo();
// CheckGameBananaUpdateAsync
GbUpdateInfo CheckGameBanana();

using Progress = std::function<void(int percent)>;
// DownloadAndInstallUpdateAsync: downloads the zip, unpacks it, finds the exe and has a helper batch
// copy that folder over the app's once it has closed, then start it again; exits the process.
// Returns false (and keeps running) on failure.
bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress);
bool DownloadAndInstallGameBanana(const GbUpdateInfo& info, const Progress& progress);
// DownloadAndInstallLatestExeAsync: CustomCursor.exe straight to <exe>.update.exe next to the
// running exe, moved over it once the app has closed; exits the process. False on failure.
bool DownloadAndInstallLatestExe(const Progress& progress);

std::wstring RepoUrl();       // GetGitHubRepoUrl
std::wstring ReleasesUrl();   // the latest release's page

} // namespace CustomCursor::Services::Updates
