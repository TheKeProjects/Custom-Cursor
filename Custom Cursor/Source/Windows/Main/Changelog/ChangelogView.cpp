#include "Windows/Main/Changelog/ChangelogView.h"
#include <algorithm>
#include <cmath>
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "Services/App/ChangelogData.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/PageBanner.h"
#include "Windows/Main/MainWindow.h"

namespace CustomCursor::Main {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;

constexpr float kGap = 12.f;

TextStyle Caps() {
    TextStyle t = Kit::Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    return t;
}

} // namespace

// Counts the changes of every "## " section across the history
void ChangelogView::Summarize() {
    summarized = true;
    text = Services::Changelog::EmbeddedText();
    const auto& entries = Services::Changelog::Entries();
    versions = static_cast<int>(entries.size());
    if (!entries.empty()) {
        latestVersion = entries.front().version;
        latestLabel = entries.front().label;
    }
    for (const auto& entry : entries) {
        std::wstring section;
        for (const auto& line : entry.lines) {
            if (line.rfind(L"## ", 0) == 0) {
                section = line.substr(3);
                continue;
            }
            if (line.rfind(L"- ", 0) != 0 || section.empty()) continue;
            ++changes;
            auto it = std::find_if(types.begin(), types.end(), [&](const TypeCount& t) { return t.title == section; });
            if (it == types.end()) types.push_back({ section, 1 });
            else ++it->count;
        }
    }
    std::stable_sort(types.begin(), types.end(), [](const TypeCount& a, const TypeCount& b) { return a.count > b.count; });
}

void ChangelogView::Draw(UI::Ui& ui, const Rect& content, double shownAt) {
    if (!summarized) Summarize();
    const Rect banner{ content.x, content.y, content.w, kPageBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.45f), 14.f);
        PageBannerSpec spec;
        spec.icon = M::Icon::ClipboardTextClockOutline;
        spec.motion = PageBannerSpec::Motion::Wobble;
        spec.title = Loc::Get("changelogs_title");
        spec.subtitle = Loc::Get("changelogs_subtitle");
        spec.pill = Loc::Fmt(versions == 1 ? "changelogs_pill_one" : "changelogs_pill", versions, changes);
        spec.pillIcon = M::Icon::FileDocumentOutline;
        PageBanner(ui, banner, spec, shownAt);
    }
    const float top = banner.B() + kGap;
    const float rightW = std::clamp(content.w * 0.3f, 240.f, 340.f);
    const Rect right{ content.R() - rightW, top, rightW, content.B() - top };
    const Rect left{ content.x, top, right.x - kGap - content.x, content.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.1, 0, 0.f, 0.45f), 16.f);
        Kit::Panel(ui, left);
        cards.DrawHistory(ui, left.Inset(14.f), text, AppVersion::CurrentVersion);
    }
    const Rect glance{ right.x, right.y, right.w, Glance(ui, right, false, shownAt) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.16, 0, 0.f, 0.45f), 0.f, 24.f);
        Glance(ui, glance, true, shownAt);
    }
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.22, 0, 0.f, 0.45f), 0.f, 24.f);
    ByType(ui, { right.x, glance.B() + kGap, right.w, right.B() - glance.B() - kGap }, shownAt);
}

// Versions and changes (counting up), the installed version
float ChangelogView::Glance(UI::Ui& ui, const Rect& r, bool draw, double shownAt) {
    constexpr float kPad = 14.f, kH = kPad * 2.f + 20.f + 10.f + 58.f + 10.f + 32.f;
    if (!draw) return kH;
    Kit::Panel(ui, r);
    const Rect c = r.Inset(kPad);
    ui.g.Text(Loc::Get("changelogs_glance"), { c.x, c.y, c.w, 20.f }, Kit::Palette::AccentLight, Caps());
    float y = c.y + 30.f;

    // Two big numbers counting up
    const float k = M::Motion::Stagger(ui, shownAt + 0.3, 0, 0.f, 1.f);
    if (k < 1.f) ui.KeepAnimating();
    const auto Stat = [&](float x, float w, int value, const std::wstring& label, M::Icon icon) {
        const Rect box{ x, y, w, 58.f };
        ui.g.FillRound(box, 10.f, Kit::Palette::AccentLight.Fade(0.09f));
        M::DrawIcon(ui.g, icon, box.x + 19.f, box.CY(), 20.f, Kit::Palette::AccentLight);
        TextStyle number = Kit::Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
        number.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(value * k))), { box.x + 36.f, box.y + 6.f, w - 40.f, 28.f }, Kit::Palette::TextPrimary, number);
        TextStyle caption = Kit::Style(10.5f);
        caption.valign = VAlign::Center;
        caption.ellipsis = true;
        ui.g.Text(label, { box.x + 36.f, box.y + 33.f, w - 40.f, 16.f }, Kit::Palette::TextSecondary, caption);
    };
    const float half = (c.w - 8.f) * 0.5f;
    Stat(c.x, half, versions, Loc::Get(versions == 1 ? "changelogs_versions_one" : "changelogs_versions"), M::Icon::TrophyAward);
    Stat(c.x + half + 8.f, half, changes, Loc::Get("changelogs_changes"), M::Icon::FileMultiple);
    y += 58.f + 10.f;

    // The installed version
    TextStyle name = Kit::Style(12.f);
    name.valign = VAlign::Center;
    ui.g.Text(Loc::Get("updates_current_version"), { c.x, y, c.w * 0.6f, 32.f }, Kit::Palette::TextSecondary, name);
    TextStyle v = Kit::Style(14.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Right);
    v.valign = VAlign::Center;
    ui.g.Text(AppVersion::DisplayVersion(), { c.x, y, c.w, 32.f }, Kit::Palette::TextPrimary, v);
    return kH;
}

// One bar per type of change, growing in turn from the left
void ChangelogView::ByType(UI::Ui& ui, const Rect& r, double shownAt) {
    if (r.h < 80.f) return;
    Kit::Panel(ui, r);
    const Rect c = r.Inset(14.f);
    ui.g.Text(Loc::Get("changelogs_by_type"), { c.x, c.y, c.w, 20.f }, Kit::Palette::AccentLight, Caps());
    float y = c.y + 30.f;
    const int most = types.empty() ? 1 : std::max(1, types.front().count);
    TextStyle name = Kit::Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    TextStyle count = Kit::Style(12.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Right);
    count.valign = VAlign::Center;
    for (size_t i = 0; i < types.size() && y + 38.f <= c.B(); ++i) {
        const TypeCount& type = types[i];
        const Color color = ChangelogCards::SectionColor(type.title);
        const float grow = M::Motion::Stagger(ui, shownAt + 0.4, static_cast<int>(i), 0.08f, 0.7f);
        if (grow < 1.f) ui.KeepAnimating();
        const UI::Id id = ui.MakeId("chl-type", static_cast<int64_t>(i));
        const Rect row{ c.x, y, c.w, 36.f };
        const float hover = M::Motion::Hover(ui, id, ui.MouseOver(row), false).hover;
        if (hover > 0.f) ui.g.FillRound(row.Inset(-4.f), 8.f, color.Fade(0.08f * hover));
        M::DrawIcon(ui.g, ChangelogCards::SectionIcon(type.title), c.x + 9.f, y + 10.f, 16.f * (1.f + 0.15f * hover), ChangelogCards::ReadableColor(color));
        ui.g.Text(ChangelogCards::SectionName(type.title), { c.x + 24.f, y, c.w - 60.f, 20.f }, Kit::Palette::TextPrimary, name);
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(type.count * grow))), { c.R() - 40.f, y, 40.f, 20.f }, ChangelogCards::ReadableColor(color), count);
        const Rect track{ c.x, y + 23.f, c.w, 7.f };
        ui.g.FillRound(track, 3.5f, Colors::White.Fade(0.07f));
        const float w = track.w * static_cast<float>(type.count) / static_cast<float>(most) * grow;
        if (w > 0.5f) ui.g.FillRound({ track.x, track.y, std::max(7.f, w), track.h }, 3.5f, color);
        y += 40.f;
    }
}

} // namespace CustomCursor::Main
