#include "Windows/Main/Updates/ManualUpdateOverlay.h"
#include <cmath>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/VersionCompare.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
// The window's own status colours (AccentColor #00CCAA, ErrorColor #E65100)
constexpr Color kAccent = Hex(0x00CCAA), kError = Hex(0xE65100);
} // namespace

void ManualUpdateOverlay::Show() {
    if (IsOpen()) return;
    status = Status::Checking;
    release = {};
    progress = 0;
    onClosed = nullptr;
    Open();
    Check();
}

Color ManualUpdateOverlay::Accent() const { return status == Status::Failed ? kError : kAccent; }

void ManualUpdateOverlay::Check() {
    Async::Run([] { return Services::Updates::LatestReleaseInfo(); },
               [this](Services::Updates::UpdateInfo info) {
                   release = std::move(info);
                   status = Text::Trim(release.downloadUrl).empty() ? Status::Failed : Status::Ready;
                   // A published version older than this one would be a downgrade (the C# replaced
                   // the exe anyway); the same version can still be reinstalled
                   if (status == Status::Ready && !Text::Trim(release.latestVersion).empty() && Versions::IsNewer(release.latestVersion, Versions::Current()))
                       status = Status::Older;
               });
}

void ManualUpdateOverlay::Download() {
    if (downloading || status != Status::Ready) return;
    downloading = true;
    progress = 0;
    Async::Run(
        [this] {
            // Created on the UI thread in the C# (Progress<int>): the percentage goes back there
            return Services::Updates::DownloadAndInstallLatestExe([this](int percent) { Async::Post([this, percent] { progress = percent; }); });
        },
        [this](bool ok) {
            // On success the app restarts itself and this process exits before reaching here
            if (ok) return;
            downloading = false;
            status = Status::Failed;
        });
}

void ManualUpdateOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    // The title bar (48): the dot, "MANUAL UPDATE" and ✕
    const Rect header{ card.x, card.y, card.w, 48.f };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        ui.g.FillEllipse(header.x + kPad + 4.f, header.CY(), 4.f, 4.f, kAccent);
        TextStyle title = Kit::Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        ui.g.Text(Loc::Get("manual_title"), { header.x + kPad + 16.f, header.y, header.w, header.h }, Kit::Palette::TextPrimary, title);
        const Rect close{ header.R() - kPad - 28.f, header.CY() - 14.f, 28.f, 28.f };
        if (Kit::GlassButton(ui, ui.MakeId("manual-close"), close, L"✕", !downloading)) Close();   // Close_Click
        ui.g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Kit::Palette::PanelBorder);
    }
    const Rect c{ card.x + kPad, header.B() + 18.f, card.w - kPad * 2.f, card.B() - 60.f - header.B() - 18.f };
    float y = c.y;
    {
        // IntroText
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const TextStyle intro = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
        const std::wstring text = Loc::Get("manual_intro");
        const float h = Kit::TextH(ui, text, intro, c.w);
        ui.g.Text(text, { c.x, y, c.w, h }, Kit::Palette::TextSecondary, intro);
        y += h + 20.f;
    }
    {
        // The status: the sync icon (turning while it checks) and the coloured title, the message under it
        M::Motion::Enter enter(ui, Reveal(ui, 2));
        const Color color = status == Status::Failed ? kError : kAccent;
        std::wstring title, message;
        if (status == Status::Checking) title = Loc::Get("manual_checking");
        else if (status == Status::Older) {
            title = Loc::Fmt("manual_older", release.latestVersion, Versions::Current());
            message = release.fileName + L"  |  " + Text::FileSize(release.fileSize);
        } else if (status == Status::Ready) {
            const std::wstring version = Text::Trim(release.latestVersion).empty() ? Loc::Get("manual_the_latest") : Loc::Fmt("manual_version", release.latestVersion);
            title = Loc::Fmt("manual_ready", version);
            message = release.fileName + L"  |  " + Text::FileSize(release.fileSize);
        } else {
            title = Loc::Get("manual_failed");
        }
        const float angle = status == Status::Checking ? static_cast<float>(std::fmod(ui.Time() * 240.0, 360.0)) : 0.f;
        if (status == Status::Checking) ui.KeepAnimating();
        TextStyle icon = Kit::Style(20.f);
        icon.align = HAlign::Center;
        icon.valign = VAlign::Center;
        const Rect iconR{ c.x, y, 28.f, 28.f };
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(angle, { iconR.CX(), iconR.CY() }));
        ui.g.Text(status == Status::Failed ? L"⚠️" : L"🔄", iconR, Colors::White, icon);
        ui.g.PopTransform();
        const TextStyle titleStyle = Kit::Style(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
        const float th = Kit::TextH(ui, title, titleStyle, c.w - 40.f);
        ui.g.Text(title, { c.x + 40.f, y + 4.f, c.w - 40.f, th }, color, titleStyle);
        if (!message.empty()) ui.g.Text(message, { c.x + 40.f, y + 8.f + th, c.w - 40.f, 18.f }, Kit::Palette::TextSecondary, Kit::Style(11.5f));
        y += std::max(28.f, th + 4.f) + 30.f;
    }
    if (downloading) {
        // DownloadProgressPanel: "Downloading...  N%" and the bar (220 x 6)
        TextStyle t = Kit::Style(12.f);
        t.valign = VAlign::Center;
        const std::wstring label = Loc::Fmt("manual_downloading", static_cast<long long>(progress));
        const float w = std::ceil(ui.g.Measure(label, t).w) + 2.f;
        ui.g.Text(label, { c.x, y, w, 20.f }, Kit::Palette::TextPrimary, t);
        Kit::ProgressBar(ui, { c.x + w + 12.f, y + 7.f, 220.f, 6.f }, ui.Animate(ui.MakeId("manual-progress"), progress / 100.f, 12.f));
        ui.KeepAnimatingThrottled();
    }

    // The buttons (36 tall): DOWNLOAD & REPLACE APP (MinWidth 220) and OPEN DOWNLOAD PAGE IN BROWSER
    M::Motion::Enter enter(ui, Reveal(ui, 3));
    const float by = card.B() - kPad - 36.f;
    const std::wstring download = Loc::Get("manual_download"), browser = Loc::Get("manual_browser");
    const float dw = std::max(220.f, Kit::ApplyWidth(ui, download, 12.f));
    if (Kit::ApplyButton(ui, ui.MakeId("manual-download"), { card.x + kPad, by, dw, 36.f }, download, status == Status::Ready && !downloading, 12.f)) Download();
    const float bw = std::min(card.R() - kPad - (card.x + kPad + dw + 10.f), std::max(140.f, Kit::GlassWidth(ui, browser, 12.f)));
    Kit::GlassLook look;
    look.fontSize = 12.f;
    if (Kit::GlassButton(ui, ui.MakeId("manual-browser"), { card.R() - kPad - bw, by, bw, 36.f }, browser, !downloading, look)) {
        // BrowserBtn_Click: the release page, or the repository's latest release
        Process::OpenUrl(Text::Trim(release.releaseUrl).empty() ? Services::Updates::ReleasesUrl() : release.releaseUrl);
    }
}

} // namespace CustomCursor::Main
