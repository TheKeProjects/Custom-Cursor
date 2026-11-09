#include "Windows/Main/Preview/PreviewView.h"
#include <algorithm>
#include <cmath>
#include "Core/Files.h"
#include "Localization/Localization.h"
#include "Services/Cursors/SchemeScanner.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Schemes/SchemesView.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kHeroH = 172.f, kGap = 12.f, kCardMinW = 128.f, kCardH = 124.f, kCardGap = 10.f;
} // namespace

void PreviewView::Show(const Models::CursorScheme& target) {
    // (the cursors were already preloaded by the transition: PreloadCursors)
    scheme = target;
    infos = Services::Cursors::SchemeScanner::LoadCursorInfos(scheme);
    chosen = 0;
    shownAt = -1.0;
}

void PreviewView::PreloadCursors(const Models::CursorScheme& target) {
    std::vector<std::wstring> paths;
    for (const auto& info : Services::Cursors::SchemeScanner::LoadCursorInfos(target)) paths.push_back(Files::DotNetCombine(target.path, info.fileName));
    UI::AnimatedCursor::Preload(paths, "preview");
}

void PreviewView::Hidden() {
    if (infos.empty()) return;
    UI::AnimatedCursor::Release("preview");
    infos.clear();
}

void PreviewView::Hero(UI::Ui& ui, const Rect& r) {
    Kit::Panel(ui, r);
    const Rect c = r.Inset(14.f);
    const UI::Id backId = ui.MakeId("preview-back");
    const Rect back{ c.x, c.y, 36.f, 36.f };
    if (Kit::IconButton(ui, backId, back, M::Icon::ArrowLeft) || (!ui.InputMuted() && ui.KeyPressed(VK_ESCAPE))) main.ShowSchemes();
    ui.Tooltip(backId, Loc::Get("preview_back"));

    // The stage: the chosen cursor large and floating on the accent's light, swapping with a little pop
    const Rect stage{ back.R() + 12.f, c.y, 180.f, c.h };
    ui.g.FillRound(stage, Kit::Palette::CardRadius, Colors::Black.Fade(0.24f));
    ui.PushClip(stage);
    ui.g.FillRadial(stage.Inset(-8.f), { { 0.f, Kit::Palette::AccentLight.Fade(0.3f) }, { 1.f, Kit::Palette::AccentLight.Fade(0.f) } });
    ui.PopClip();
    ui.g.StrokeRound(stage.Inset(0.5f), Kit::Palette::CardRadius - 0.5f, Kit::Palette::CardBorder, 1.f);
    if (!infos.empty()) {
        UI::AnimatedCursor::RequestAmbientTicks();
        const float pop = M::Motion::Pulse(ui, ui.MakeId("preview-chosen", chosen), true, 0.35f);
        if (pop > 0.f) ui.KeepAnimating();
        const float bob = 4.f * std::sin(static_cast<float>(ui.Time()) * 2.2f);
        const Rect big = stage.Center(96.f, 96.f);
        ui.PushTranslate(0.f, bob);
        ui.PushScale(1.f - 0.25f * pop, big.CX(), big.CY());
        DrawCursor(ui, infos[static_cast<size_t>(std::clamp(chosen, 0, static_cast<int>(infos.size()) - 1))], big);
        ui.PopTransform();
        ui.PopTransform();
    }

    // The actions on the right
    auto& schemes = main.Schemes();
    const std::wstring apply = Loc::Get("preview_apply");
    const float actionsW = std::clamp(Kit::ApplyWidth(ui, apply) + 20.f, 150.f, 210.f);
    const Rect actions{ c.R() - actionsW, c.y, actionsW, c.h };
    const bool installed = main.State().IsInstalled(scheme);
    const float buttonsH = installed ? 42.f + 8.f + 36.f : 42.f;
    const float by = actions.CY() - buttonsH * 0.5f;
    if (Kit::ApplyButton(ui, ui.MakeId("preview-apply"), { actions.x, by, actions.w, 42.f }, apply, !schemes.Busy())) schemes.ApplyCursorScheme(scheme);
    if (installed) {
        Kit::GlassLook look;
        look.fill = Kit::Palette::Danger.Fade(0.75f);
        if (Kit::GlassButton(ui, ui.MakeId("preview-uninstall"), { actions.x, by + 50.f, actions.w, 36.f }, Loc::Get("card_uninstall_tooltip"), !schemes.Busy(), look))
            schemes.UninstallCursorScheme(scheme);
    }

    // The text in the middle
    const float x = stage.R() + 18.f, w = std::max(0.f, actions.x - 14.f - x);
    TextStyle title = Kit::Style(22.f, DWRITE_FONT_WEIGHT_BOLD);
    title.ellipsis = true;
    TextStyle path = Kit::Style(12.f);
    path.ellipsis = true;
    float y = c.y + 8.f;
    const float titleH = Kit::LineH(ui, title);
    ui.g.Text(scheme.name, { x, y, w, titleH }, Kit::Palette::TextPrimary, title);
    y += titleH + 2.f;
    ui.g.Text(scheme.relativePath, { x, y, w, Kit::LineH(ui, path) }, Kit::Palette::TextSecondary, path);
    y += Kit::LineH(ui, path) + 12.f;
    if (!infos.empty()) {
        const Models::CursorInfo& info = infos[static_cast<size_t>(std::clamp(chosen, 0, static_cast<int>(infos.size()) - 1))];
        const std::wstring& label = info.labelKey.empty() ? info.key : Loc::Get(info.labelKey);
        TextStyle role = Kit::Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        role.ellipsis = true;
        ui.g.Text(label, { x, y, w, 20.f }, Kit::Palette::AccentLight, role);
        y += 21.f;
        TextStyle file = Kit::Style(11.f);
        file.ellipsis = true;
        ui.g.Text(info.fileName, { x, y, w, 16.f }, Kit::Palette::TextMuted, file);
        y += 22.f;
        const int count = static_cast<int>(infos.size());
        ui.g.Text(Loc::Fmt(count == 1 ? "card_cursors_one" : "card_cursors_many", static_cast<long long>(count)), { x, y, w, 16.f }, Kit::Palette::TextSecondary, Kit::Style(11.f, DWRITE_FONT_WEIGHT_BOLD));
    }
}

void PreviewView::Draw(UI::Ui& ui, const Rect& content) {
    if (shownAt < 0.0) shownAt = ui.Time();
    const Rect hero{ content.x, content.y, content.w, kHeroH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.4f), 14.f);
        Hero(ui, hero);
    }
    const float top = hero.B() + kGap;
    const Rect panel{ content.x, top, content.w, content.B() - top };
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.06, 0, 0.f, 0.4f), 16.f);
    Kit::Panel(ui, panel);
    const Rect area = panel.Inset(Edges(14.f, 14.f, 6.f, 14.f));
    if (infos.empty()) {
        // "No cursor data available for preview"
        TextStyle t = Kit::Style(14.f);
        t.align = HAlign::Center;
        t.valign = VAlign::Center;
        ui.g.Text(Loc::Get("preview_no_data"), area, Kit::Palette::TextSecondary, t);
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("preview-scroll"), area);
    const float width = area.w - 12.f;
    const int count = static_cast<int>(infos.size());
    const int perRow = std::max(1, static_cast<int>((width + kCardGap) / (kCardMinW + kCardGap)));
    const float cardW = (width - kCardGap * (perRow - 1)) / perRow;
    const int rows = (count + perRow - 1) / perRow;
    for (int i = 0; i < count; ++i) {
        const Rect card{ area.x + 2.f + (i % perRow) * (cardW + kCardGap), area.y + 3.f + (i / perRow) * (kCardH + kCardGap), cardW, kCardH };
        if (!ui.Visible(card, 12.f)) continue;
        M::Motion::Enter cardEnter(ui, M::Motion::Stagger(ui, shownAt + 0.12, i, 0.025f, 0.4f, 24), 16.f);
        CursorCard(ui, card, infos[static_cast<size_t>(i)], i);
    }
    scroll.SetContentSize(rows * (kCardH + kCardGap) + 6.f);
}

} // namespace CustomCursor::Main
