#include <algorithm>
#include <cmath>
#include "Core/Files.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/Preview/PreviewView.h"

// CreateCursorPreviewCard: a card with the cursor (an .ani animated, a .cur / .ico as it is, else the
// pointer icon), its label in bold and its file name. A click shows it on the stage.
namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
} // namespace

void PreviewView::DrawCursor(UI::Ui& ui, const Models::CursorInfo& info, const Rect& r) {
    const std::wstring path = Files::DotNetCombine(scheme.path, info.fileName);
    // (ExtractCursorPreview reads .cur, .ani and .ico; anything else gets the icon)
    const std::wstring ext = Text::Lower(Files::Extension(path));
    const bool cursor = ext == L".ani" || ext == L".cur" || ext == L".ico";
    if (!cursor || !UI::AnimatedCursor::Draw(ui, path, r, 1.f, "preview")) M::DrawIcon(ui.g, M::Icon::CursorPointer, r.CX(), r.CY(), r.h * 0.7f, Kit::Palette::TextSecondary);
}

void PreviewView::CursorCard(UI::Ui& ui, const Rect& r, const Models::CursorInfo& info, int index) {
    const UI::Id id = ui.MakeId("cursor-card", index);
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const bool selected = index == chosen;
    const float on = ui.Tween(UI::HashId(id, 0x7C1), selected ? 1.f : 0.f, 0.18f);
    if (on > 0.f && on < 1.f) ui.KeepAnimating();
    const float lit = std::max(fb.hover * 0.8f, on);
    ui.PushTranslate(0.f, -2.f * fb.hover);
    M::Elevation(ui.g, r, Kit::Palette::CardRadius, 2.f + 4.f * fb.hover);
    if (lit > 0.f) ui.g.GlowRound(r, Kit::Palette::CardRadius, Kit::Palette::AccentLight, 12.f, 0.38f * lit);
    ui.g.Border(r, Kit::Palette::CardRadius, Kit::Palette::Card, Lerp(Kit::Palette::CardBorder, Kit::Palette::AccentLight.Fade(0.85f), lit), lit > 0.5f ? 1.5f : 1.f);

    const Rect image{ r.CX() - 26.f, r.y + 10.f, 52.f, 52.f };
    ui.PushScale(1.f + 0.1f * fb.hover, image.CX(), image.CY());
    DrawCursor(ui, info, image);
    ui.PopTransform();

    TextStyle name = Kit::Style(11.5f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    name.ellipsis = true;
    const std::wstring& label = info.labelKey.empty() ? info.key : Loc::Get(info.labelKey);
    ui.g.Text(label, { r.x + 8.f, image.B() + 8.f, r.w - 16.f, 17.f }, selected ? Kit::Palette::AccentLight : Kit::Palette::TextPrimary, name);
    TextStyle file = Kit::Style(10.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
    file.ellipsis = true;
    ui.g.Text(info.fileName, { r.x + 8.f, image.B() + 26.f, r.w - 16.f, 15.f }, Kit::Palette::TextSecondary, file);
    ui.PopTransform();
    if (it.clicked) chosen = index;
}

} // namespace CustomCursor::Main
