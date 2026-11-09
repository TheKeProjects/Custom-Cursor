#include "Services/Cursors/ElevatedInstaller.h"
#include <windows.h>
#include <shellapi.h>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Models/SchemeJson.h"
#include "Services/Cursors/Registry.h"
#include "Services/Cursors/SchemeSwitcher.h"
#include "Services/Cursors/SchemeUninstaller.h"

namespace CustomCursor::Services::Cursors::ElevatedInstaller {

namespace {

// Writes the JSON to %TEMP%\<prefix><guid>.json, runs the exe elevated with
// "<argument> \"<json>\" --user <SID>", waits and reads the result; `failedText` is the C#'s
// "... failed with exit code: {0}" start. While it waits, `onProgress` gets what the elevated copy
// writes to <json>.progress; `results` (when given) gets <json>.results' per-scheme outcome.
bool RunElevated(const wchar_t* prefix, const wchar_t* argument, const std::string& json, const std::wstring& failedText, std::wstring& errorMessage,
                 const SchemeInstaller::Progress& onProgress = {}, std::vector<SchemeInstaller::Result>* results = nullptr) {
    errorMessage.clear();
    const std::wstring tempFile = Files::Combine(Files::TempDirectory(), prefix + System::NewGuid() + L".json");
    if (!Files::WriteBytes(tempFile, json)) {
        errorMessage = L"Elevation error: " + ErrorText(GetLastError());
        return false;
    }
    const std::wstring progressFile = tempFile + Config::ProgressFileSuffix, resultsFile = tempFile + Config::ResultsFileSuffix;
    const auto cleanUp = [&] {
        Files::Delete(tempFile);
        Files::Delete(progressFile);
        Files::Delete(resultsFile);
    };

    // Process.Start(new ProcessStartInfo { FileName = exe, Verb = "runas", UseShellExecute = true, WindowStyle = Hidden }).
    // --user: when another administrator's account approves the prompt, the elevated copy still writes
    // this user's cursor settings (a C# copy ignores the extra argument)
    const std::wstring exePath = Files::ExecutablePath();
    std::wstring arguments = std::wstring(argument) + L" \"" + tempFile + L"\"";
    if (const std::wstring sid = System::CurrentUserSid(); !sid.empty()) arguments += std::wstring(L" ") + Config::UserArgument + L" " + sid;
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = exePath.c_str();
    info.lpParameters = arguments.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        const DWORD code = GetLastError();
        errorMessage = code == ERROR_CANCELLED ? L"User cancelled elevation request" : L"Failed to elevate: " + ErrorText(code);
        cleanUp();   // (the C# left the JSON behind when the prompt was cancelled)
        return false;
    }
    if (!info.hProcess) {
        errorMessage = L"Failed to start elevated process";
        cleanUp();
        return false;
    }
    // The elevated copy's progress ("<current>\t<total>\t<name>"), read while it works
    std::wstring lastProgress;
    while (WaitForSingleObject(info.hProcess, 100) == WAIT_TIMEOUT) {
        if (!onProgress) continue;
        const std::wstring text = Files::ReadText(progressFile).value_or(L"");
        if (text.empty() || text == lastProgress) continue;
        lastProgress = text;
        const auto parts = Text::Split(text, L'\t');
        if (parts.size() == 3) onProgress(static_cast<int>(Text::ToInt(parts[0]).value_or(0)), static_cast<int>(Text::ToInt(parts[1]).value_or(0)), parts[2]);
    }
    DWORD exitCode = 1;
    GetExitCodeProcess(info.hProcess, &exitCode);
    CloseHandle(info.hProcess);

    if (results) {
        // [{"Name":..,"Success":..,"Error":..}, ...]: what each scheme did
        if (const auto bytes = Files::ReadBytes(resultsFile)) {
            bool ok = false;
            const Json::Value list = Json::Parse(std::string_view(*bytes), &ok);
            if (ok && list.IsArray())
                for (const auto& item : list.Elements()) results->push_back({ item["Name"].WStr(), item["Success"].Bool(), item["Error"].WStr() });
        }
    }
    // The elevated copy set the cursors in the registry; this session loads them (with another
    // administrator's account, its own reload only reached that account)
    if (exitCode == 0 || (results && !results->empty())) SchemeSwitcher::ReloadCursors();

    if (exitCode == 0) {
        cleanUp();
        return true;
    }
    // Failure: the error the elevated process wrote, if any
    const std::wstring errorFile = tempFile + Config::ErrorFileSuffix;
    if (Files::Exists(errorFile)) {
        errorMessage = Files::ReadText(errorFile).value_or(L"");
        Files::Delete(errorFile);
    } else {
        errorMessage = failedText + std::to_wstring(static_cast<int>(exitCode));
    }
    cleanUp();
    return false;
}

// IsRunningAsAdmin; Debug builds take CC_NO_ELEVATION=1 as yes (the actions' tests, with WINDIR
// pointing somewhere writable)
bool IsRunningAsAdmin() {
#ifdef _DEBUG
    if (GetEnvironmentVariableW(L"CC_NO_ELEVATION", nullptr, 0) > 0) return true;
#endif
    return System::IsAdministrator();
}

} // namespace

bool InstallCursorScheme(const Models::CursorScheme& scheme, std::wstring& errorMessage) {
    if (IsRunningAsAdmin()) return SchemeInstaller::ApplyCursorScheme(scheme, errorMessage);
    return RunElevated(Config::InstallJsonPrefix, Config::InstallArgument, Models::SchemeJson::SerializeInstall({ scheme }),
                       L"Installation failed with exit code: ", errorMessage);
}

std::vector<SchemeInstaller::Result> InstallMultipleCursorSchemes(const std::vector<Models::CursorScheme>& schemes,
                                                                  const SchemeInstaller::Progress& progress) {
    if (IsRunningAsAdmin()) return SchemeInstaller::ApplyMultipleCursorSchemes(schemes, progress);
    // One elevated copy (one UAC prompt) for all of them, reporting its progress and each result
    std::wstring errorMessage;
    std::vector<SchemeInstaller::Result> reported;
    const bool success = RunElevated(Config::InstallJsonPrefix, Config::InstallArgument, Models::SchemeJson::SerializeInstall(schemes),
                                     L"Installation failed with exit code: ", errorMessage, progress, &reported);
    if (reported.size() == schemes.size()) return reported;
    // No per-scheme results (it couldn't start): every scheme gets the same outcome
    std::vector<SchemeInstaller::Result> results;
    for (const auto& scheme : schemes) results.push_back({ scheme.name, success, success ? std::wstring() : errorMessage });
    return results;
}

bool UninstallCursorScheme(const Models::CursorScheme& scheme, const std::optional<std::wstring>& replacement, std::wstring& errorMessage) {
    if (IsRunningAsAdmin()) return SchemeUninstaller::UninstallCursorScheme(scheme, replacement, errorMessage);
    return RunElevated(Config::UninstallJsonPrefix, Config::UninstallArgument, Models::SchemeJson::SerializeUninstall(scheme, replacement),
                       L"Uninstall failed with exit code: ", errorMessage);
}

} // namespace CustomCursor::Services::Cursors::ElevatedInstaller
