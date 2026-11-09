#include "Services/Cursors/ElevatedCommandLine.h"
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/System.h"
#include "Models/SchemeJson.h"
#include "Services/Cursors/Registry.h"
#include "Services/Cursors/SchemeInstaller.h"
#include "Services/Cursors/SchemeUninstaller.h"

namespace CustomCursor::Services::Cursors::ElevatedCommandLine {

namespace {

// --user <SID>: another administrator's account approved the prompt, so this process runs as that
// account; the cursor settings are the requesting user's (HKEY_USERS\<SID>)
void UseRequestingUser(const std::vector<std::wstring>& args) {
    for (size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == Config::UserArgument && args[i + 1] != System::CurrentUserSid()) UseUserHive(args[i + 1]);
}

int Install(const std::wstring& tempFile) {
    if (!Files::Exists(tempFile)) return 1;   // "Temp file not found"
    const auto json = Files::ReadBytes(tempFile);
    const auto schemes = json ? Models::SchemeJson::ParseInstall(*json) : std::nullopt;
    if (!schemes || schemes->empty()) return 1;   // "No cursor schemes to install" (or bad JSON)

    // The waiting copy reads the progress and each scheme's result (the C# only had the .error text)
    const auto results = SchemeInstaller::ApplyMultipleCursorSchemes(*schemes, [&](int current, int count, const std::wstring& name) {
        Files::WriteText(tempFile + Config::ProgressFileSuffix, std::to_wstring(current) + L"\t" + std::to_wstring(count) + L"\t" + name);
    });
    Json::Value list = Json::Value::MakeArray();
    bool allSuccess = true;
    std::wstring errorMessage;
    for (const auto& r : results) {
        Json::Value item = Json::Value::MakeObject();
        item.Set("Name", r.schemeName);
        item.Set("Success", r.success);
        item.Set("Error", r.errorMessage);
        list.Push(std::move(item));
        if (!r.success) {
            allSuccess = false;
            errorMessage += r.schemeName + L": " + r.errorMessage + L"\n";
        }
    }
    Files::WriteBytes(tempFile + Config::ResultsFileSuffix, list.Serialize());
    if (!allSuccess) {
        Files::WriteText(tempFile + Config::ErrorFileSuffix, errorMessage);
        return 1;
    }
    return 0;
}

int Uninstall(const std::wstring& tempFile) {
    if (!Files::Exists(tempFile)) return 1;
    const auto json = Files::ReadBytes(tempFile);
    const auto request = json ? Models::SchemeJson::ParseUninstall(*json) : std::nullopt;
    if (!request) return 1;   // "No cursor scheme to uninstall" (or bad JSON)
    std::wstring errorMessage;
    if (!SchemeUninstaller::UninstallCursorScheme(request->scheme, request->replacement, errorMessage)) {
        Files::WriteText(tempFile + Config::ErrorFileSuffix, errorMessage);
        return 1;
    }
    return 0;
}

} // namespace

int HandleCommandLineInstall(const std::vector<std::wstring>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == Config::InstallArgument && i + 1 < args.size()) {
            UseRequestingUser(args);
            return Install(args[i + 1]);
        }
        if (args[i] == Config::UninstallArgument && i + 1 < args.size()) {
            UseRequestingUser(args);
            return Uninstall(args[i + 1]);
        }
    }
    return 0;   // No command found, continue normal execution
}

bool ShouldExit(const std::vector<std::wstring>& args, int& exitCode) {
    if (args.empty()) return false;
    exitCode = HandleCommandLineInstall(args);
    if (exitCode != 0) return true;
    for (const auto& arg : args)
        if (arg == Config::InstallArgument || arg == Config::UninstallArgument) return true;
    return false;
}

} // namespace CustomCursor::Services::Cursors::ElevatedCommandLine
