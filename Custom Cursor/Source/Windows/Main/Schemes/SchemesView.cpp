#include "Windows/Main/Schemes/SchemesView.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/PageBanner.h"
#include "Windows/Main/MainWindow.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kBarH = 58.f, kGap = 12.f;
} // namespace

SchemesView::SchemesView(MainWindow& window) : main(window), state(window.State()) {}

void SchemesView::Draw(UI::Ui& ui, const Rect& content, double shownAt) {
    const Rect banner{ content.x, content.y, content.w, kPageBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.45f), 14.f);
        PageBannerSpec spec;
        spec.icon = M::Icon::CursorPointer;
        spec.motion = PageBannerSpec::Motion::Bob;
        spec.title = Loc::Get("schemes_title");
        const int total = state.SchemeCount(Config::AllCategory);
        spec.subtitle = Loc::Fmt(total == 1 ? "schemes_subtitle_one" : "schemes_subtitle_many", total, static_cast<int>(state.categories.size()));
        for (const auto& category : state.categories)
            for (const auto& scheme : category.schemes)
                if (state.IsActive(scheme)) spec.pill = scheme.name;
        spec.pillIcon = M::Icon::CheckCircle;
        PageBanner(ui, banner, spec, shownAt);
    }
    const float chipsY = banner.B() + kGap;
    float chipsH = 0.f;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.08, 0, 0.f, 0.45f), 12.f);
        chipsH = CategoryChips(ui, { content.x, chipsY, content.w, 0.f }, shownAt);
    }
    const float gridY = chipsY + chipsH + kGap;
    SchemeGrid(ui, { content.x, gridY, content.w, content.B() - gridY }, kBarH + kGap);
    SelectionBar(ui, content);
}

// Slides up from the bottom while there's a selection: "N selected", Clear, Install
void SchemesView::SelectionBar(UI::Ui& ui, const Rect& content) {
    const size_t count = state.SelectedCount();
    if (count > 0) barCount = count;
    const float raw = ui.Tween(ui.MakeId("sel-bar"), count > 0 ? 1.f : 0.f, 0.26f, 0.f);
    if (raw > 0.f && raw < 1.f) ui.KeepAnimating();
    if (raw <= 0.f) return;
    const float k = count > 0 ? Anim::EaseOutBack(raw, 0.7f) : Anim::EaseOutCubic(raw);
    const float w = std::min(content.w, 620.f);
    const Rect bar{ content.CX() - w * 0.5f, content.B() - kBarH + (kBarH + 20.f) * (1.f - k), w, kBarH };
    ui.Block(bar);
    M::Elevation(ui.g, bar, Kit::Palette::PanelRadius, 10.f);
    ui.g.GlowRound(bar, Kit::Palette::PanelRadius, Kit::Palette::AccentLight, 18.f, 0.4f);
    ui.g.Border(bar, Kit::Palette::PanelRadius, Kit::Palette::Surface, Kit::Palette::AccentLight.Fade(0.55f), 1.f);

    const Rect c = bar.Inset(Edges(18.f, 10.f));
    // The count in a circle, popping each time it changes
    size_t& seen = ui.Data<size_t>(ui.MakeId("sel-seen"));
    float& pop = ui.Slot(ui.MakeId("sel-pop"));
    if (seen != barCount) {
        seen = barCount;
        pop = 1.f;
    }
    pop = std::max(0.f, pop - ui.Dt() / 0.3f);
    if (pop > 0.f) ui.KeepAnimating();
    const float r = 15.f * (1.f + 0.22f * pop);
    Kit::FillAccent(ui, { c.x + 15.f - r, c.CY() - r, r * 2.f, r * 2.f }, r);
    TextStyle number = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    number.valign = VAlign::Center;
    ui.g.Text(std::to_wstring(barCount), { c.x, c.y, 30.f, c.h }, Colors::White, number);

    const std::wstring install = Loc::Get("selection_install"), clear = Loc::Get("selection_clear");
    const float installW = std::max(120.f, Kit::ApplyWidth(ui, install)), clearW = std::max(80.f, Kit::GlassWidth(ui, clear));
    const Rect installR{ c.R() - installW, c.y, installW, c.h };
    const Rect clearR{ installR.x - 8.f - clearW, c.y, clearW, c.h };
    TextStyle label = Kit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    label.valign = VAlign::Center;
    label.ellipsis = true;
    ui.g.Text(Loc::Get(barCount == 1 ? "selection_count_one" : "selection_count_many"), { c.x + 40.f, c.y, std::max(0.f, clearR.x - c.x - 48.f), c.h }, Kit::Palette::TextPrimary, label);
    if (Kit::GlassButton(ui, ui.MakeId("sel-clear"), clearR, clear)) state.ClearSelection();
    if (Kit::ApplyButton(ui, ui.MakeId("sel-install"), installR, install, count > 0 && !busy)) InstallSelected();
}

} // namespace CustomCursor::Main
