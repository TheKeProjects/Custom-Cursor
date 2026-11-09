#include "Services/App/UpdateService.h"
#include <windows.h>
#include <shellapi.h>
#include <optional>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/Log.h"
#include "Services/App/VersionCompare.h"

namespace CustomCursor::Services::Updates {

namespace {

std::wstring RawUrl(const wchar_t* file) {
    return std::wstring(L"https://raw.githubusercontent.com/") + Config::GitHub::Owner + L"/" + Config::GitHub::Repo + L"/" +
           Config::GitHub::Branch + L"/" + file;
}

// version.txt without \r \n \t, trimmed; nullopt on a network error or an empty file
std::optional<std::wstring> FetchVersionText() {
    const Http::Response r = Http::Get(RawUrl(L"version.txt"));
    if (!r.Ok()) return std::nullopt;
    std::wstring v;
    for (wchar_t c : Text::Widen(r.body))
        if (c != L'\r' && c != L'\n' && c != L'\t') v += c;
    v = Text::Trim(v);
    if (v.empty()) return std::nullopt;
    return v;
}

// changelog.txt, trimmed; the embedded changelog when it can't be read
std::wstring FetchChangelog() {
    const Http::Response r = Http::Get(RawUrl(L"changelog.txt"));
    const std::wstring text = r.Ok() ? Text::Trim(Text::Widen(r.body)) : L"";
    return text.empty() ? Changelog::EmbeddedText() : text;
}

// The response headers of `url` (HEAD, redirects followed): nullopt when it isn't reachable
std::optional<Http::Response> Head(const std::wstring& url) {
    Http::Request head;
    head.method = L"HEAD";
    head.url = url;
    Http::Response r = Http::Send(head);
    if (!r.Ok()) return std::nullopt;
    return r;
}

bool DownloadTo(const std::wstring& url, const std::wstring& path, const Progress& progress) {
    return Http::Download(url, path, [&](uint64_t received, uint64_t total) {
        if (progress && total > 0) progress(static_cast<int>(received * 100 / total));
        return true;
    });
}

std::wstring UpdateTempDir() { return Files::Combine(Files::TempDirectory(), Config::UpdateFolder); }

// FindExecutable: the running exe's name, then CustomCursor.exe, then any exe (anywhere under `dir`)
std::wstring FindExecutable(const std::wstring& dir) {
    const std::wstring names[] = { Files::FileName(Files::ExecutablePath()), Config::GitHub::ExeAsset };
    for (const auto& name : names) {
        const auto found = Files::ListFiles(dir, name, true);
        if (!found.empty()) return found.front();
    }
    const auto any = Files::ListFiles(dir, L"*.exe", true);
    return any.empty() ? std::wstring() : any.front();
}

// ZipFile.ExtractToDirectory: every entry under `dir` (names that would leave it are refused)
bool ExtractAll(const std::wstring& zipPath, const std::wstring& dir) {
    const auto reader = Zip::Reader::Open(zipPath);
    if (!reader) return false;
    for (const auto& entry : reader->Entries()) {
        const std::wstring relative = Text::Replace(entry.fullName, L"/", L"\\");
        if (relative.find(L"..") != std::wstring::npos || (relative.size() > 1 && relative[1] == L':') || relative.starts_with(L"\\")) return false;
        const std::wstring target = Files::Combine(dir, relative);
        if (entry.name.empty()) {
            Files::CreateDirectories(target);
            continue;
        }
        Files::CreateDirectories(Files::DirectoryName(target));
        if (!reader->ExtractToFile(entry, target)) return false;
    }
    return true;
}

// Batch lines that wait (up to ~30 s) until this process is gone, then about one second more: the
// C#'s fixed "timeout /t 2" returned at once when cmd runs hidden on some PCs (ping waits instead;
// full paths: a Git / MSYS folder on PATH brings its own find)
std::wstring WaitForExitLines() {
    const std::wstring pid = std::to_wstring(GetCurrentProcessId());
    return L"set WAITS=30\r\n:wait\r\n"
           L"\"%SystemRoot%\\System32\\tasklist.exe\" /FI \"PID eq " + pid + L"\" /NH 2>nul | \"%SystemRoot%\\System32\\find.exe\" /I \".exe\" >nul\r\n"
           L"if not errorlevel 1 (\r\n    set /a WAITS-=1\r\n    if %WAITS% GTR 0 (\r\n"
           L"        ping -n 2 127.0.0.1 >nul\r\n        goto wait\r\n    )\r\n)\r\n"
           L"ping -n 2 127.0.0.1 >nul\r\n";
}

// Writes the batch, starts it hidden (cmd.exe /c) and exits: the batch waits for this process
bool RunBatchAndExit(const std::wstring& batchPath, const std::wstring& content) {
    if (!Files::WriteBytes(batchPath, Text::Narrow(content))) return false;
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.lpVerb = L"open";
    info.lpFile = L"cmd.exe";
    const std::wstring args = L"/c \"" + batchPath + L"\"";
    info.lpParameters = args.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) return false;
    Log::Info(L"Update: closing to let update.bat replace the app");
    ExitProcess(0);
}

} // namespace

std::wstring RepoUrl() { return std::wstring(L"https://github.com/") + Config::GitHub::Owner + L"/" + Config::GitHub::Repo; }

std::wstring ReleasesUrl() { return RepoUrl() + L"/releases/latest"; }

UpdateInfo CheckForUpdates(bool* reachable) {
    // The latest version stays empty when GitHub can't be reached: the Updates card then says
    // "Couldn't check for updates" (the C# kept the current version there and claimed it was the latest)
    UpdateInfo info;
    if (reachable) *reachable = false;
    const auto latest = FetchVersionText();
    if (!latest) return info;   // "version.txt is empty or invalid"
    // A real version number (X.Y...), not a 404 page or an error body
    std::wstring digits;
    for (wchar_t c : *latest)
        if ((c >= L'0' && c <= L'9') || c == L'.') digits += c;
    const auto parts = Text::Split(digits, L'.', true);
    if (parts.size() < 2) return info;
    for (const auto& p : parts)
        if (!Text::ToInt(p)) return info;
    if (reachable) *reachable = true;

    info.latestVersion = *latest;
    info.isUpdateAvailable = Versions::IsNewer(Versions::Current(), *latest);
    info.changelog = FetchChangelog();
    if (info.isUpdateAvailable) {
        info.fileName = Config::GitHub::ZipPrefix + Text::Replace(*latest, L".", L"-") + L".zip";
        info.downloadUrl = ReleasesUrl() + L"/download/" + info.fileName;
        info.releaseUrl = ReleasesUrl();
        if (const auto head = Head(info.downloadUrl)) info.fileSize = Text::ToInt(head->Header(L"content-length")).value_or(0);
    }
    return info;
}

UpdateInfo LatestReleaseInfo() {
    UpdateInfo info;
    // The version label is cosmetic (best effort): the download itself doesn't depend on it
    if (const auto latest = FetchVersionText()) info.latestVersion = *latest;
    // Only report a download once the release asset is confirmed reachable
    if (const auto head = Head(Config::GitHub::LatestExeUrl)) {
        info.downloadUrl = Config::GitHub::LatestExeUrl;
        info.fileName = Config::GitHub::ExeAsset;
        info.fileSize = Text::ToInt(head->Header(L"content-length")).value_or(0);
        info.releaseUrl = ReleasesUrl();
        info.isUpdateAvailable = true;
    } else {
        Log::Warning(L"Manual update: the latest release's exe could not be reached");
    }
    return info;
}

GbUpdateInfo CheckGameBanana() {
    GbUpdateInfo info;
    if (Config::GameBanana::ToolId <= 0) return info;
    const Http::Response r = Http::Get(std::wstring(Config::GameBanana::ApiItemUrl) + std::to_wstring(Config::GameBanana::ToolId) + Config::GameBanana::ApiFields);
    if (!r.Ok()) return info;
    // [name, { fileId: { _sVersion, _sDownloadUrl, _sFile, _nFilesize, ... }, ... }]: the first file only
    const Json::Value root = Json::Parse(std::string_view(r.body));
    if (!root.IsArray() || root.Size() < 2 || !root[1].IsObject() || root[1].Items().empty()) return info;
    const Json::Value& file = root[1].Items().front().second;
    info.latestVersion = file["_sVersion"].IsString() ? Text::Trim(file["_sVersion"].WStr()) : L"";
    info.isUpdateAvailable = Versions::IsNewer(Versions::Current(), info.latestVersion);
    if (info.isUpdateAvailable) {
        info.downloadUrl = file["_sDownloadUrl"].IsString() ? file["_sDownloadUrl"].WStr() : L"";
        info.fileName = file["_sFile"].IsString() ? file["_sFile"].WStr() : L"";
        info.fileSize = file["_nFilesize"].IsNumber() ? file["_nFilesize"].Int() : 0;
        info.changelog = FetchChangelog();
    }
    return info;
}

bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress) {
    if (Text::Trim(info.downloadUrl).empty()) return false;
    const std::wstring tempDir = UpdateTempDir();
    const auto fail = [&](const wchar_t* why) {
        Log::Error(std::wstring(L"Update failed: ") + why);
        Files::DeleteDirectory(tempDir);
        return false;
    };
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    const std::wstring zipPath = Files::Combine(tempDir, info.fileName);
    Log::Info(L"Update: downloading " + info.downloadUrl + L" (" + info.latestVersion + L")");
    if (!DownloadTo(info.downloadUrl, zipPath, progress)) return fail(L"the download did not finish");

    const std::wstring extractDir = Files::Combine(tempDir, L"extracted");
    Files::CreateDirectories(extractDir);
    if (!ExtractAll(zipPath, extractDir)) return fail(L"the zip could not be extracted");
    const std::wstring newExe = FindExecutable(extractDir);
    if (newExe.empty()) return fail(L"no exe in the zip");

    const std::wstring currentExe = Files::ExecutablePath();
    const std::wstring currentDir = Files::DirectoryName(currentExe);
    const std::wstring extractedDir = Files::DirectoryName(newExe);
    // xcopy "<extracted>\*" "<app folder>" /E /Y /I /Q, retried while the exe is still locked; its
    // output goes to update.log, and on failure the temp folder stays for diagnosis
    const std::wstring log = Files::Combine(tempDir, L"update.log");
    return RunBatchAndExit(Files::Combine(tempDir, L"update.bat"),
                           L"@echo off\r\nchcp 65001 >nul\r\n" + WaitForExitLines() + L"set RETRIES=15\r\n:copy\r\n"
                           L"xcopy \"" + extractedDir + L"\\*\" \"" + currentDir + L"\" /E /Y /I /Q >> \"" + log + L"\" 2>&1\r\n"
                           L"if not errorlevel 1 goto copied\r\n"
                           L"set /a RETRIES-=1\r\nif %RETRIES% LEQ 0 exit /b 1\r\n"
                           L"ping -n 2 127.0.0.1 >nul\r\ngoto copy\r\n:copied\r\n"
                           L"start \"\" \"" + currentExe + L"\"\r\n"
                           L"rd /s /q \"" + tempDir + L"\"\r\nexit\r\n");
}

bool DownloadAndInstallGameBanana(const GbUpdateInfo& gb, const Progress& progress) {
    UpdateInfo mapped;
    mapped.downloadUrl = gb.downloadUrl;
    mapped.fileName = Text::Trim(gb.fileName).empty() ? std::wstring(Config::GameBanana::FallbackZipName) : gb.fileName;
    mapped.isUpdateAvailable = true;
    mapped.latestVersion = gb.latestVersion;
    return DownloadAndInstall(mapped, progress);
}

bool DownloadAndInstallLatestExe(const Progress& progress) {
    const std::wstring currentExe = Files::ExecutablePath();
    const std::wstring staging = Files::Combine(Files::DirectoryName(currentExe), Files::FileNameWithoutExtension(currentExe) + L".update.exe");
    Log::Info(L"Manual update: downloading " + std::wstring(Config::GitHub::LatestExeUrl));
    // (Http::Download writes <file>.part and renames it only once complete: a truncated download
    // never replaces a working exe)
    if (!DownloadTo(Config::GitHub::LatestExeUrl, staging, progress)) {
        Log::Error(L"Manual update failed: the download did not finish");
        Files::Delete(staging);
        return false;
    }
    const std::wstring tempDir = UpdateTempDir();
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    // move /y, retried 10 times while the exe is locked; a staging file that never moved is deleted
    const bool started = RunBatchAndExit(Files::Combine(tempDir, L"update.bat"),
                                         L"@echo off\r\nchcp 65001 >nul\r\n" + WaitForExitLines() + L"set RETRIES=10\r\n:retry\r\n"
                                         L"move /y \"" + staging + L"\" \"" + currentExe + L"\" >nul 2>&1\r\n"
                                         L"if not exist \"" + staging + L"\" goto done\r\n"
                                         L"set /a RETRIES-=1\r\nif %RETRIES% GTR 0 (\r\n    ping -n 2 127.0.0.1 >nul\r\n    goto retry\r\n)\r\n"
                                         L"del /f /q \"" + staging + L"\" >nul 2>&1\r\n:done\r\n"
                                         L"start \"\" \"" + currentExe + L"\"\r\n"
                                         L"rd /s /q \"" + tempDir + L"\"\r\nexit\r\n");
    if (!started) Files::Delete(staging);
    return started;
}

} // namespace CustomCursor::Services::Updates
