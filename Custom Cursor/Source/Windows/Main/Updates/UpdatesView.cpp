#include "Windows/Main/Updates/UpdatesView.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/UpdateService.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/PageBanner.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Updates/ManualUpdateOverlay.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kGap = 12.f;
} // namespace

void UpdatesView::Recheck() {
    if (rechecking) return;
    rechecking = true;
    main.State().CheckForUpdates([this] { rechecking = false; });
}

void UpdatesView::Draw(UI::Ui& ui, const Rect& content, double shownAt) {
    const MainState& state = main.State();
    const Rect banner{ content.x, content.y, content.w, kPageBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.45f), 14.f);
        PageBannerSpec spec;
        spec.icon = M::Icon::CloudDownload;
        spec.motion = PageBannerSpec::Motion::Float;
        spec.title = Loc::Get("updates_title");
        spec.subtitle = Loc::Get("updates_subtitle");
        spec.pill = AppVersion::DisplayVersion();
        spec.pillIcon = state.UpdateAvailable() ? M::Icon::ArrowUpBold : M::Icon::CheckDecagram;
        PageBanner(ui, banner, spec, shownAt);
    }
    const float top = banner.B() + kGap;
    const float leftW = std::clamp(content.w * 0.38f, 280.f, 400.f);
    const Rect left{ content.x, top, leftW, content.B() - top };
    const Rect right{ left.R() + kGap, top, content.R() - left.R() - kGap, content.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.08, 0, 0.f, 0.45f), 0.f, -24.f);
        Status(ui, left, shownAt);
    }
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.14, 0, 0.f, 0.45f), 0.f, 24.f);
    WhatsNew(ui, right);
}

// The status's badge: rings going out while it's checking or there's an update
void UpdatesView::Badge(UI::Ui& ui, float cx, float cy, bool busy, bool available, bool upToDate) {
    const Color color = busy || available ? Kit::Palette::AccentLight : upToDate ? Hex(0x4CAF50) : Kit::Palette::Danger;
    const float t = static_cast<float>(ui.Time());
    const float er = 38.f;
    ui.g.FillRadial({ cx - er * 2.f, cy - er * 2.f, er * 4.f, er * 4.f }, { { 0.f, color.Fade(0.26f) }, { 1.f, color.Fade(0.f) } });
    if (busy || available) {
        UI::AnimatedCursor::RequestAmbientTicks();
        for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(t * 0.45f + i * 0.5f, 1.f);
            ui.g.StrokeEllipse(cx, cy, er + 18.f * k, er + 18.f * k, color.Fade(0.5f * (1.f - k)), 1.5f);
        }
    }
    ui.g.FillEllipse(cx, cy, er, er, color.Fade(0.18f));
    ui.g.StrokeEllipse(cx, cy, er, er, color.Fade(0.6f), 1.5f);
    if (busy) {
        for (int i = 0; i < 2; ++i) Kit::Arc(ui, cx, cy, er, t * 220.f + 180.f * static_cast<float>(i), 80.f, color, 3.f);
        M::DrawIcon(ui.g, M::Icon::CloudSearch, cx, cy, 36.f, Lerp(color, Colors::White, 0.3f));
    } else if (available) {
        const float bob = 3.5f * std::sin(t * 2.6f);
        M::DrawIcon(ui.g, M::Icon::ArrowUpBold, cx, cy + bob, 38.f, Lerp(color, Colors::White, 0.3f));
    } else {
        // Pops when the answer arrives
        const float pop = M::Motion::Pulse(ui, ui.MakeId("upd-badge"), true, 0.45f);
        if (pop > 0.f) ui.KeepAnimating();
        M::DrawIcon(ui.g, upToDate ? M::Icon::CheckDecagram : M::Icon::WifiOff, cx, cy, 36.f * (1.f + 0.25f * pop), Lerp(color, Colors::White, 0.25f));
    }
}

void UpdatesView::Status(UI::Ui& ui, const Rect& r, double shownAt) {
    MainState& state = main.State();
    Kit::Panel(ui, r);
    UI::ScrollArea scroll(ui, ui.MakeId("upd-status-scroll"), r);
    const Rect c = r.Inset(Edges(18.f, 16.f, 18.f, 16.f));
    const bool busy = rechecking || state.checkingUpdates;
    const auto& info = state.updateInfo;
    const bool available = info && info->isUpdateAvailable;
    const bool gameBanana = !available && state.gbUpdateInfo && state.gbUpdateInfo->isUpdateAvailable;
    const bool upToDate = info && !available && !info->latestVersion.empty();
    const bool checked = info.has_value();
    float y = c.y + 22.f;

    Badge(ui, c.CX(), y + 38.f, busy || !checked, available || gameBanana, upToDate);
    y += 76.f + 26.f;

    // The title and the message
    const std::wstring latest = available ? info->latestVersion : gameBanana ? state.gbUpdateInfo->latestVersion : L"";
    std::wstring title, message;
    if (busy || !checked) {
        title = Loc::Get("updates_checking");
        message = Loc::Get("updates_checking_message");
    } else if (available || gameBanana) {
        title = Loc::Get(gameBanana ? "updates_gb_available" : "updates_available");
        message = Loc::Fmt("updates_available_message", latest);
    } else if (upToDate) {
        title = Loc::Get("updates_up_to_date");
        message = Loc::Fmt("updates_up_to_date_message", AppVersion::DisplayVersion());
    } else {
        title = Loc::Get("updates_check_failed");
        message = Loc::Get("updates_check_failed_message");
    }
    const TextStyle titleStyle = Kit::Style(17.f, DWRITE_FONT_WEIGHT_BOLD, true, HAlign::Center);
    const float titleH = Kit::TextH(ui, title, titleStyle, c.w);
    ui.g.Text(title, { c.x, y, c.w, titleH }, Kit::Palette::TextPrimary, titleStyle);
    y += titleH + 4.f;
    const TextStyle body = Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    const float bodyH = Kit::TextH(ui, message, body, c.w);
    ui.g.Text(message, { c.x, y, c.w, bodyH }, Kit::Palette::TextSecondary, body);
    y += bodyH + 16.f;

    // current -> latest
    {
        TextStyle v = Kit::Style(12.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        v.valign = VAlign::Center;
        const std::wstring from = AppVersion::DisplayVersion(), to = L"v" + latest;
        const float fromW = std::ceil(ui.g.Measure(from, v).w) + 22.f, toW = latest.empty() ? 0.f : std::ceil(ui.g.Measure(to, v).w) + 22.f;
        const float total = fromW + (latest.empty() ? 0.f : 32.f + toW);
        const Rect a{ c.CX() - total * 0.5f, y, fromW, 26.f };
        ui.g.Border(a, 13.f, Argb(0x14FFFFFF), Kit::Palette::CardBorder, 1.f);
        ui.g.Text(from, a, Kit::Palette::TextPrimary, v);
        if (!latest.empty()) {
            const float nudge = 2.f * std::sin(static_cast<float>(ui.Time()) * 3.f);
            M::DrawIcon(ui.g, M::Icon::ArrowRight, a.R() + 16.f + nudge, a.CY(), 18.f, Kit::Palette::AccentLight);
            const Rect b{ a.R() + 32.f, y, toW, 26.f };
            Kit::FillAccent(ui, b, 13.f);
            ui.g.Text(to, b, Colors::White, v);
        }
        y += 26.f + 16.f;
    }

    // Update now / the download's progress
    if (downloading) {
        TextStyle t = Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(Loc::Fmt("updates_downloading", static_cast<long long>(progress)), { c.x, y, c.w, 18.f }, Kit::Palette::TextPrimary, t);
        Kit::ProgressBar(ui, { c.x, y + 22.f, c.w, 8.f }, ui.Animate(ui.MakeId("upd-progress"), progress / 100.f, 12.f));
        y += 30.f + 14.f;
    } else if (available || gameBanana) {
        if (Kit::ApplyButton(ui, ui.MakeId("upd-accept"), { c.x, y, c.w, 42.f }, Loc::Get("updates_update_now"))) AcceptInstall();
        y += 42.f + 8.f;
    }
    {
        Kit::GlassLook look;
        if (Kit::GlassButton(ui, ui.MakeId("upd-recheck"), { c.x, y, c.w, 36.f }, Loc::Get(busy ? "updates_checking" : "updates_check_again"), !busy && !downloading, look)) Recheck();
        y += 36.f + 14.f;
    }

    // DETAILS: the file and its size
    if (available) {
        const float boxH = 34.f + 2.f * 20.f;
        const Rect box{ c.x, y, c.w, boxH };
        ui.g.Border(box, Kit::Palette::ControlRadius, Argb(0x17FFFFFF), Kit::Palette::CardBorder, 1.f);
        ui.g.Text(Loc::Get("updates_details"), { box.x + 14.f, box.y + 10.f, box.w, 14.f }, Kit::Palette::TextMuted, Kit::Style(10.f, DWRITE_FONT_WEIGHT_BOLD));
        const std::wstring values[] = { info->fileName, Text::FileSize(info->fileSize) };
        const char* labels[] = { "updates_file", "updates_size" };
        for (int i = 0; i < 2; ++i) {
            const float ry = box.y + 30.f + i * 20.f;
            ui.g.Text(Loc::Get(labels[i]), { box.x + 14.f, ry, 70.f, 18.f }, Kit::Palette::TextSecondary, Kit::Style(11.5f));
            TextStyle v = Kit::Style(11.5f);
            v.ellipsis = true;
            ui.g.Text(values[i], { box.x + 84.f, ry, box.w - 98.f, 18.f }, Kit::Palette::TextPrimary, v);
        }
        y += boxH + 12.f;
    }

    // ManualUpdateLink
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.3, 0, 0.f, 0.4f));
    const std::wstring link = Loc::Get("updates_manual_link");
    const TextStyle linkStyle = Kit::Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    const float linkH = Kit::TextH(ui, link, linkStyle, c.w);
    if (Kit::Link(ui, ui.MakeId("upd-manual"), { c.x, y, c.w, linkH }, link, 11.f, Kit::Palette::TextMuted, true)) main.ManualUpdate().Show();
    scroll.SetContentSize(y + linkH + 16.f - r.y);
}

void UpdatesView::WhatsNew(UI::Ui& ui, const Rect& r) {
    const MainState& state = main.State();
    Kit::Panel(ui, r);
    const Rect c = r.Inset(14.f);
    const auto& info = state.updateInfo;
    if (info && info->isUpdateAvailable && !Text::Trim(info->changelog).empty()) {
        cards.Draw(ui, c, info->changelog, AppVersion::CurrentVersion, info->latestVersion);
        return;
    }
    // GameBanana's changelog is free text
    if (!(info && info->isUpdateAvailable) && state.gbUpdateInfo && state.gbUpdateInfo->isUpdateAvailable) {
        ui.g.Text(Loc::Get("updates_whats_new"), { c.x + 4.f, c.y, c.w, 16.f }, Kit::Palette::AccentLight, Kit::Style(11.f, DWRITE_FONT_WEIGHT_BOLD));
        const Rect text{ c.x + 4.f, c.y + 26.f, c.w - 4.f, c.h - 26.f };
        const std::wstring& changelog = state.gbUpdateInfo->changelog;
        const std::wstring shown = Text::Trim(changelog).empty() ? Loc::Get("updates_no_changelog") : changelog;
        const TextStyle body = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
        UI::ScrollArea scroll(ui, ui.MakeId("upd-changelog"), text);
        const float h = Kit::TextH(ui, shown, body, text.w - 10.f);
        ui.g.Text(shown, { text.x, text.y, text.w - 10.f, h }, Kit::Palette::TextSecondary, body);
        scroll.SetContentSize(h);
        return;
    }
    if (history.empty()) history = Services::Changelog::EmbeddedText();
    cards.DrawHistory(ui, c, history, AppVersion::CurrentVersion);
}

void UpdatesView::AcceptInstall() {
    if (downloading) return;
    const MainState& state = main.State();
    downloading = true;
    progress = 0;
    const auto report = [this](int percent) { Async::Post([this, percent] { progress = percent; }); };
    const auto gbInfo = state.gbUpdateInfo;
    const auto info = state.updateInfo;
    const bool gb = !(info && info->isUpdateAvailable) && gbInfo && gbInfo->isUpdateAvailable;
    Async::Run(
        [gb, gbInfo, info, report] {
            if (gb) return Services::Updates::DownloadAndInstallGameBanana(*gbInfo, report);
            return info && Services::Updates::DownloadAndInstall(*info, report);
        },
        [this](bool success) {
            // On success the app restarts itself and this process exits before reaching here
            if (success) return;
            downloading = false;
            Dialogs::Message(Loc::Get("updates_failed_message"), Loc::Get("updates_failed_title"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        });
}

} // namespace CustomCursor::Main
