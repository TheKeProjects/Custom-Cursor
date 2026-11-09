#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HeaderPopups.h"

namespace CustomCursor::Main::NavRail {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;
using View = MainState::View;

constexpr float kPi = 3.14159265f;
constexpr float kItemH = 62.f, kItemGap = 4.f;

struct Page {
    View view;
    M::Icon icon;
    const char* label;
    const char* tooltip;
};
constexpr Page kPages[] = {
    { View::Schemes, M::Icon::CursorPointer, "nav_schemes", "nav_schemes_tooltip" },
    { View::Tutorial, M::Icon::PlayCircle, "nav_tutorial", "nav_tutorial_tooltip" },
    { View::Updates, M::Icon::CloudDownload, "nav_updates", "nav_updates_tooltip" },
    { View::Changelog, M::Icon::ClipboardTextClockOutline, "nav_changelog", "nav_changelog_tooltip" },
};

// The page's row in the rail (the preview belongs to the schemes)
int RowOf(View view) {
    switch (view) {
        case View::Tutorial: return 1;
        case View::Updates: return 2;
        case View::Changelog: return 3;
        default: return 0;
    }
}

void Open(MainWindow& main, View view) {
    switch (view) {
        case View::Tutorial: main.ShowTutorial(); break;
        case View::Updates: main.ShowUpdates(); break;
        case View::Changelog: main.ShowChangelog(); break;
        default: main.ShowSchemes(); break;
    }
}

// The theme's logo floating gently; on hover it grows and glows, a click hops back to the schemes
void Logo(UI::Ui& ui, MainWindow& main, const Rect& r, float size) {
    const UI::Id id = ui.MakeId("rail-logo");
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const float hop = M::Motion::Pulse(ui, UI::HashId(id, 0x401), it.held, 0.45f);
    const float t = static_cast<float>(ui.Time());
    UI::AnimatedCursor::RequestAmbientTicks();
    const float bob = 2.5f * std::sin(t * 1.7f) - 7.f * std::sin((1.f - hop) * kPi) * (hop > 0.f ? 1.f : 0.f);
    if (hop > 0.f) ui.KeepAnimating();
    const float scale = 1.f + 0.08f * fb.hover - 0.05f * fb.press;
    const Rect logo = r.Center(size, size);
    ui.g.FillRadial(logo.Inset(-16.f), { { 0.f, Kit::Palette::AccentLight.Fade(0.16f + 0.14f * fb.hover) }, { 1.f, Kit::Palette::AccentLight.Fade(0.f) } });
    ui.PushTranslate(0.f, bob);
    ui.PushScale(scale, logo.CX(), logo.CY());
    ui.g.DrawImage(Images::Load(Paths::Asset(Settings::IconTheme::Current().LogoAsset()), 256), logo);
    ui.PopTransform();
    ui.PopTransform();
    if (it.clicked) main.ShowSchemes();
}

// A page's row: the icon over its label; the highlight is drawn by the caller, under it
void Item(UI::Ui& ui, MainWindow& main, const Rect& r, const Page& page, int index, bool selected) {
    const UI::Id id = ui.MakeId("rail-item", index);
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const float on = ui.Tween(UI::HashId(id, 0x402), selected ? 1.f : 0.f, 0.2f);
    if (on > 0.f && on < 1.f) ui.KeepAnimating();
    if (fb.hover > 0.f && !selected) ui.g.FillRound(r, 12.f, Argb(0x12FFFFFF).Fade(fb.hover));

    // The icon jumps when its page opens
    const float pop = M::Motion::Pulse(ui, UI::HashId(id, 0x403), selected, 0.5f);
    const float jump = pop > 0.f ? std::sin((1.f - pop) * kPi) : 0.f;
    const float cx = r.CX(), cy = r.y + 24.f - 3.f * fb.hover * (1.f - on) - 5.f * jump;
    const Color ink = Lerp(Lerp(Kit::Palette::TextSecondary, Kit::Palette::TextPrimary, fb.hover), Colors::White, on);
    const float size = 24.f * (1.f + 0.22f * jump + 0.06f * fb.hover - 0.08f * fb.press);
    M::DrawIcon(ui.g, page.icon, cx, cy, size, ink);
    if (page.view == View::Updates && main.State().UpdateAvailable()) Kit::Badge(ui, cx + 13.f, cy - 9.f);

    TextStyle label = Kit::Style(10.5f, selected ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    label.valign = VAlign::Center;
    label.ellipsis = true;
    ui.g.Text(Loc::Get(page.label), { r.x + 2.f, r.y + 39.f, r.w - 4.f, 16.f }, ink, label);
    ui.Tooltip(id, Loc::Get(page.tooltip));
    if (it.clicked) Open(main, page.view);
}

// A small tool at the bottom; returns the interaction. The icon is drawn by the caller
UI::Interaction Tool(UI::Ui& ui, UI::Id id, const Rect& r, M::Motion::Feedback& fb) {
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    if (fb.hover > 0.f) ui.g.FillRound(r, 10.f, Argb(0x14FFFFFF).Fade(fb.hover));
    return it;
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& rail) {
    const double at = main.ShownAt();
    ui.g.FillRect(rail, Kit::Palette::Panel.Fade(0.88f));
    ui.g.FillRect({ rail.R() - 1.f, rail.y, 1.f, rail.h }, Kit::Palette::PanelBorder);
    const Rect inner = rail.Inset(Edges(8.f, 6.f, 8.f, 10.f));
    // A short window: a smaller logo and tools, so they all still fit
    const bool compact = rail.h < 580.f;
    const float kLogo = compact ? 42.f : 60.f, kToolH = compact ? 30.f : 40.f;

    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at, 0, 0.f, 0.5f), 0.f, -18.f);
        Logo(ui, main, { inner.x, inner.y, inner.w, kLogo + 12.f }, kLogo);
    }

    // The highlight slides between the rows, stretching a little on its way
    const float top = inner.y + kLogo + (compact ? 14.f : 22.f);
    const int selected = RowOf(main.State().view);
    const float targetY = top + selected * (kItemH + kItemGap);
    const UI::Id pillId = ui.MakeId("rail-pill");
    const float y = ui.Animate(pillId, targetY, 16.f);
    if (std::fabs(y - targetY) > 0.2f) ui.KeepAnimating();
    const float stretch = std::min(14.f, std::fabs(y - targetY) * 0.18f);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at + 0.15, 0, 0.f, 0.45f));
        const Rect pill{ inner.x, y - stretch * 0.5f, inner.w, kItemH + stretch };
        ui.g.GlowRound(pill, 12.f, Kit::Palette::AccentLight, 14.f, 0.32f);
        Kit::FillAccent(ui, pill, 12.f);
        ui.g.FillRound(pill.Inset(1.f), 11.f, Colors::Black.Fade(0.12f));
    }
    for (int i = 0; i < static_cast<int>(std::size(kPages)); ++i) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at + 0.08, i, 0.06f, 0.42f), 0.f, -18.f);
        Item(ui, main, { inner.x, top + i * (kItemH + kItemGap), inner.w, kItemH }, kPages[i], i, i == selected);
    }

    // The tools at the bottom: settings (the cog turns), theme (the palette tilts), language (its flag)
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at + 0.32, 0, 0.f, 0.42f), 0.f, -18.f);
    float toolY = inner.B() - kToolH * 3.f - (compact ? 0.f : 8.f);
    ui.g.FillRect({ inner.x + 12.f, toolY - 8.f, inner.w - 24.f, 1.f }, Kit::Palette::PanelBorder);
    M::Motion::Feedback fb;
    {
        const UI::Id id = ui.MakeId("rail-settings");
        const Rect r{ inner.x, toolY, inner.w, kToolH };
        const UI::Interaction it = Tool(ui, id, r, fb);
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * Anim::EaseOutCubic(fb.hover), D2D1::Point2F(r.CX(), r.CY())));
        M::DrawIcon(ui.g, M::Icon::Cog, r.CX(), r.CY(), 20.f, Lerp(Kit::Palette::TextSecondary, Kit::Palette::TextPrimary, fb.hover));
        ui.g.PopTransform();
        ui.Tooltip(id, Loc::Get("nav_settings_tooltip"));
        if (it.clicked) main.OpenWindowsCursorSettings();
    }
    toolY += kToolH + (compact ? 0.f : 4.f);
    {
        const UI::Id id = ui.MakeId("rail-theme");
        const Rect r{ inner.x, toolY, inner.w, kToolH };
        const UI::Interaction it = Tool(ui, id, r, fb);
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(-18.f * Anim::EaseOutBack(fb.hover, 0.8f), D2D1::Point2F(r.CX(), r.CY())));
        M::DrawIcon(ui.g, M::Icon::Palette, r.CX(), r.CY(), 20.f, Lerp(Kit::Palette::AccentLight, Colors::White, fb.hover));
        ui.g.PopTransform();
        ui.Tooltip(id, Loc::Get("nav_theme_tooltip"));
        if (it.clicked) main.Themes().Toggle(ui.ToScreenRect(r));
    }
    toolY += kToolH + (compact ? 0.f : 4.f);
    {
        const UI::Id id = ui.MakeId("rail-language");
        const Rect r{ inner.x, toolY, inner.w, kToolH };
        const UI::Interaction it = Tool(ui, id, r, fb);
        const float s = 1.f + 0.08f * fb.hover - 0.06f * fb.press;
        LanguagePopup::Banner(ui, { r.CX() - 15.f * s, r.CY() - 10.f * s, 30.f * s, 20.f * s }, LanguagePopup::Flag(Loc::CurrentLang()), fb.hover, 0.f);
        ui.Tooltip(id, Loc::Get("language_tooltip"));
        if (it.clicked) main.Languages().Toggle(ui.ToScreenRect(r));
    }
}

} // namespace CustomCursor::Main::NavRail
