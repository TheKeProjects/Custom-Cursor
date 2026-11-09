#include <algorithm>
#include <cmath>
#include <vector>
#include "App/Config.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Schemes/SchemesView.h"

// The categories as chips ("All" first, the rest A to Z) wrapping over as many rows as they need, the
// highlight sliding to the one shown; the search box closes the first row.
namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kChipH = 32.f, kGap = 6.f, kSearchW = 230.f;

struct Chip {
    std::wstring name, count;
    Rect r;
};
} // namespace

float SchemesView::CategoryChips(UI::Ui& ui, const Rect& r, double shownAt) {
    TextStyle name = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    TextStyle number = Kit::Style(10.5f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    number.valign = VAlign::Center;

    // The layout: the first row leaves room for the search box
    const float searchW = std::min(kSearchW, r.w * 0.35f);
    std::vector<Chip> chips;
    float x = r.x, y = r.y;
    for (const std::wstring& category : state.ShownCategories()) {
        Chip chip;
        chip.name = category == Config::AllCategory ? Loc::Get("categories_all") : category;
        chip.count = std::to_wstring(state.SchemeCount(category));
        const float w = std::ceil(ui.g.Measure(chip.name, name).w) + 2.f + std::ceil(ui.g.Measure(chip.count, number).w) + 40.f;
        const float limit = y == r.y ? r.R() - searchW - 12.f : r.R();
        if (x + w > limit && x > r.x) {
            x = r.x;
            y += kChipH + kGap;
        }
        chip.r = { x, y, w, kChipH };
        chips.push_back(std::move(chip));
        x += w + kGap;
    }
    Kit::SearchBox(ui, ui.MakeId("scheme-search"), { r.R() - searchW, r.y - 2.f, searchW, Kit::kSearchHeight }, state.schemeSearch, Loc::Get("schemes_search"));
    const std::vector<std::wstring> categories = state.ShownCategories();

    // The highlight slides (and stretches) to the chip shown
    for (size_t i = 0; i < chips.size(); ++i) {
        if (categories[i] != state.currentCategory) continue;
        const Rect& target = chips[i].r;
        const UI::Id hx = ui.MakeId("chip-x"), hy = ui.MakeId("chip-y"), hw = ui.MakeId("chip-w");
        const Rect shown{ ui.Animate(hx, target.x, 18.f), ui.Animate(hy, target.y, 18.f), ui.Animate(hw, target.w, 18.f), kChipH };
        if (std::fabs(shown.x - target.x) + std::fabs(shown.y - target.y) + std::fabs(shown.w - target.w) > 0.3f) ui.KeepAnimating();
        ui.g.GlowRound(shown, kChipH * 0.5f, Kit::Palette::AccentLight, 10.f, 0.35f);
        Kit::FillAccent(ui, shown, kChipH * 0.5f);
    }
    for (size_t i = 0; i < chips.size(); ++i) {
        const Chip& chip = chips[i];
        const bool selected = categories[i] == state.currentCategory;
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.12, static_cast<int>(i), 0.025f, 0.35f, 24), 8.f);
        const UI::Id id = ui.MakeId("chip", static_cast<int64_t>(i));
        const UI::Interaction it = ui.Interact(id, chip.r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        ui.PushScale(1.f + 0.03f * fb.hover - 0.04f * fb.press, chip.r.CX(), chip.r.CY());
        if (!selected) ui.g.Border(chip.r, kChipH * 0.5f, Lerp(Argb(0x12FFFFFF), Argb(0x24FFFFFF), fb.hover), Lerp(Kit::Palette::PanelBorder, Kit::Palette::CardHoverBorder, fb.hover), 1.f);
        const Color ink = selected ? Colors::White : Lerp(Kit::Palette::TextSecondary, Kit::Palette::TextPrimary, fb.hover);
        const float countW = std::ceil(ui.g.Measure(chip.count, number).w) + 10.f;
        ui.g.Text(chip.name, { chip.r.x + 13.f, chip.r.y, chip.r.w - countW - 16.f, kChipH }, ink, name);
        const Rect pill{ chip.r.R() - countW - 7.f, chip.r.CY() - 9.f, countW, 18.f };
        ui.g.FillRound(pill, 9.f, selected ? Colors::Black.Fade(0.22f) : Argb(0x14FFFFFF));
        ui.g.Text(chip.count, pill, ink, number);
        ui.PopTransform();
        // (the shown one too: DisplaySchemes runs again and clears the selection)
        if (it.clicked) state.SelectCategory(categories[i]);
    }
    return y + kChipH - r.y;
}

} // namespace CustomCursor::Main
