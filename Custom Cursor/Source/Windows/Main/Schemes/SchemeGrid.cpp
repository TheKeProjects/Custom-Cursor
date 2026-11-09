#include <algorithm>
#include <cmath>
#include <functional>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Schemes/SchemesView.h"

// The cards of the category: as many per row as fit, stretched to fill it, coming in one after the
// other each time the list changes.
namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kMinCardW = 208.f, kMaxCardW = 280.f, kCardH = 232.f, kGap = 14.f;
} // namespace

void SchemesView::SchemeGrid(UI::Ui& ui, const Rect& r, float bottomRoom) {
    if (seenRevision != state.revision) {
        seenRevision = state.revision;
        revealAt = ui.Time();
        UI::ScrollArea::ScrollTo(ui, ui.MakeId("schemes-scroll"), 0.f);
    }
    // (the cards' hover glow reaches past the area's edge)
    const Rect list = r.Inset(Edges(-6.f, -6.f, -8.f, 0.f));
    UI::ScrollArea scroll(ui, ui.MakeId("schemes-scroll"), list);
    const auto schemes = state.ShownSchemes();
    if (schemes.empty()) {
        // The category is empty (an empty Cursores folder shows nothing at all, as in the C#)
        if (state.Loaded() && !state.categories.empty()) {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, revealAt, 0, 0.f, 0.35f));
            const float cy = list.y + std::min(list.h * 0.36f, 150.f);
            M::DrawIcon(ui.g, M::Icon::Magnify, list.CX(), cy, 46.f, Kit::Palette::TextMuted);
            TextStyle t = Kit::Style(14.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
            ui.g.Text(Loc::Get("schemes_empty"), { list.x, cy + 34.f, list.w, 24.f }, Kit::Palette::TextSecondary, t);
        }
        scroll.SetContentSize(0.f);
        return;
    }
    const float left = r.x, width = r.w - 4.f;
    const int count = static_cast<int>(schemes.size());
    const int perRow = std::max(1, static_cast<int>((width + kGap) / (kMinCardW + kGap)));
    const float cardW = std::min(kMaxCardW, (width - kGap * (perRow - 1)) / perRow);
    const int rows = (count + perRow - 1) / perRow;
    const float top = r.y + 2.f;
    for (int i = 0; i < count; ++i) {
        const Rect card{ left + (i % perRow) * (cardW + kGap), top + (i / perRow) * (kCardH + kGap), cardW, kCardH };
        if (!ui.Visible(card, 24.f)) continue;
        ui.PushId(static_cast<int64_t>(std::hash<std::wstring>{}(schemes[static_cast<size_t>(i)]->path)));
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, revealAt + 0.08, i, 0.03f, 0.4f, 20), 20.f);
        SchemeCard(ui, card, *schemes[static_cast<size_t>(i)]);
        ui.PopId();
    }
    scroll.SetContentSize(rows * (kCardH + kGap) + 10.f + (state.SelectedCount() > 0 ? bottomRoom : 0.f));
}

} // namespace CustomCursor::Main
