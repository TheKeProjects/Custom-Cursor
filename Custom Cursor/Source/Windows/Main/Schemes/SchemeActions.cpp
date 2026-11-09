#include <algorithm>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Cursors/ElevatedInstaller.h"
#include "Services/Cursors/SchemeQueries.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/InstallProgressOverlay.h"
#include "Windows/Main/Overlays/ReplacementPickerOverlay.h"
#include "Windows/Main/Schemes/SchemesView.h"

// MainWindow.xaml.cs's #region Actions: ApplyCursorScheme, UninstallCursorScheme and
// InstallSelectedButton_Click. The work (and the wait for the elevated copy behind the UAC prompt)
// runs on a worker thread, so the window keeps drawing; the cards' buttons wait meanwhile.
namespace CustomCursor::Main {

namespace {

namespace Cursors = Services::Cursors;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;

void OpenSettingsOn(MessageResult result) {
    if (result == MessageResult::Yes) MainWindow::Instance()->OpenWindowsCursorSettings();
}

} // namespace

void SchemesView::ApplyCursorScheme(const Models::CursorScheme& scheme) {
    if (busy) return;
    busy = true;
    struct Result {
        bool success = false;
        std::wstring error;
    };
    Async::Run(
        [scheme] {
            Result r;
            r.success = Cursors::ElevatedInstaller::InstallCursorScheme(scheme, r.error);
            return r;
        },
        [this, name = scheme.name](Result r) {
            busy = false;
            if (!r.success) {
                Log::Error(L"Apply " + name + L" failed: " + r.error);
                Dialogs::Message(Loc::Fmt("apply_failed", name, r.error), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
                return;
            }
            Log::Info(L"Applied " + name);
            // Refresh the display to update the buttons' state, then ask about the settings
            state.RefreshSchemes();
            Dialogs::Message(Loc::Fmt("apply_success", name), Loc::Get("dialog_success"), MessageButtons::YesNo, MessageIcon::Information, OpenSettingsOn);
        });
}

void SchemesView::UninstallCursorScheme(const Models::CursorScheme& scheme) {
    if (busy) return;
    Dialogs::Message(Loc::Fmt("uninstall_confirm", scheme.name), Loc::Get("uninstall_confirm_title"), MessageButtons::YesNo, MessageIcon::Warning,
                     [this, scheme](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         // The active scheme: pick what to switch to (the other installed schemes, all
                         // of them, not only this app's); with none left the default is used without asking
                         std::wstring safeName = Cursors::SchemeQueries::InstalledName(scheme.name);
                         if (safeName.empty()) safeName = Paths::SchemeInstallName(scheme.name);
                         const auto active = Cursors::SchemeQueries::GetActiveScheme();
                         if (active && !active->empty() && Text::Equals(*active, safeName, true)) {
                             auto installed = Cursors::SchemeQueries::GetInstalledSchemes();
                             // installedSchemes.Remove(safeSchemeName): the first exact match
                             if (const auto it = std::find(installed.begin(), installed.end(), safeName); it != installed.end()) installed.erase(it);
                             if (!installed.empty()) {
                                 main.Replacement().Show(scheme.name, std::move(installed),
                                                         [this, scheme](std::optional<std::wstring> replacement) { Uninstall(scheme, replacement); });
                                 return;
                             }
                         }
                         Uninstall(scheme, std::nullopt);
                     });
}

void SchemesView::Uninstall(const Models::CursorScheme& scheme, const std::optional<std::wstring>& replacement) {
    if (busy) return;
    busy = true;
    struct Result {
        bool success = false;
        std::wstring error;
    };
    Async::Run(
        [scheme, replacement] {
            Result r;
            r.success = Cursors::ElevatedInstaller::UninstallCursorScheme(scheme, replacement, r.error);
            return r;
        },
        [this, scheme](Result r) { FinishUninstall(scheme, r.success, r.error); });
}

void SchemesView::FinishUninstall(const Models::CursorScheme& scheme, bool success, const std::wstring& error) {
    busy = false;
    if (!success) {
        Log::Error(L"Uninstall " + scheme.name + L" failed: " + error);
        Dialogs::Message(Loc::Fmt("uninstall_failed", scheme.name, error), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
        return;
    }
    Log::Info(L"Uninstalled " + scheme.name);
    Dialogs::Message(Loc::Fmt("uninstall_success", scheme.name), Loc::Get("dialog_success"), MessageButtons::Ok, MessageIcon::Information);
    state.RefreshSchemes();
}

void SchemesView::InstallSelected() {
    const auto schemes = state.SelectedSchemes();
    if (schemes.empty() || busy) return;
    std::vector<std::wstring> names;
    for (const auto& s : schemes) names.push_back(L"• " + s.name);
    const long long count = static_cast<long long>(schemes.size());
    Dialogs::Message(Loc::Fmt(count == 1 ? "install_confirm_one" : "install_confirm_many", count, Text::Join(names, L"\n")), Loc::Get("install_confirm_title"),
                     MessageButtons::YesNo, MessageIcon::Information, [this, schemes](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         busy = true;
                         auto& progress = main.InstallProgress();
                         progress.Show(static_cast<int>(schemes.size()));
                         Async::Run(
                             [schemes, &progress] {
                                 return Cursors::ElevatedInstaller::InstallMultipleCursorSchemes(
                                     schemes, [&progress](int current, int total, const std::wstring& name) {
                                         Async::Post([&progress, current, total, name] { progress.Update(current, total, name); });
                                     });
                             },
                             [this, &progress](std::vector<Cursors::SchemeInstaller::Result> results) {
                                 busy = false;
                                 progress.Close();
                                 const long long ok = std::count_if(results.begin(), results.end(), [](const auto& r) { return r.success; });
                                 const long long failed = static_cast<long long>(results.size()) - ok;
                                 std::wstring text, title;
                                 MessageIcon icon = MessageIcon::Information;
                                 if (failed == 0) {
                                     text = Loc::Fmt(ok == 1 ? "install_all_one" : "install_all_many", ok);
                                     title = Loc::Get("install_complete_title");
                                 } else {
                                     std::vector<std::wstring> details;
                                     for (const auto& r : results)
                                         if (!r.success) details.push_back(Loc::Fmt("install_failed_item", r.schemeName, r.errorMessage));
                                     text = Loc::Fmt(ok == 1 ? "install_partial_one" : "install_partial_many", ok, failed, Text::Join(details, L"\n\n"));
                                     title = Loc::Get("install_partial_title");
                                     icon = MessageIcon::Warning;
                                 }
                                 Log::Info(L"Installed " + std::to_wstring(ok) + L" scheme(s), " + std::to_wstring(failed) + L" failed");
                                 // Clear the selection and refresh, then ask about the settings
                                 state.RefreshSchemes();
                                 Dialogs::Message(text + L"\n\n" + Loc::Get("open_settings_question"), title, MessageButtons::YesNo, icon, OpenSettingsOn);
                             });
                     });
}

} // namespace CustomCursor::Main
