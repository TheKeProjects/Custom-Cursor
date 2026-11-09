#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Preview/PreviewTransition.h"
#include "Windows/Main/Schemes/SchemesView.h"

// A scheme's card: the cursor floating on a stage lit with the accent, ACTIVE / INSTALLED in its
// corner and the selection box in the other (it shows on hover or while selected); the name,
// "Category · N cursors"; Apply, preview and, when installed, uninstall. The hover lifts the card and
// lights its edge; a click on the card (not on its buttons) opens the preview.
namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kInfoH = 106.f, kPad = 12.f, kButtonH = 32.f;

// A little pill on the stage
void StatusPill(UI::Ui& ui, float x, float y, const std::wstring& text, Color color, bool filled) {
    TextStyle t = Kit::Style(9.5f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    const float w = std::ceil(ui.g.Measure(text, t).w) + 26.f;
    const Rect pill{ x, y, w, 20.f };
    if (filled) ui.g.FillRound(pill, 10.f, color);
    else ui.g.Border(pill, 10.f, color.Fade(0.2f), color.Fade(0.6f), 1.f);
    const Color ink = filled ? Colors::White : Lerp(color, Colors::White, 0.3f);
    M::DrawIcon(ui.g, M::Icon::Check, pill.x + 11.f, pill.CY(), 12.f, ink);
    ui.g.Text(text, { pill.x + 19.f, pill.y, w - 21.f, pill.h }, ink, t);
}
} // namespace

void SchemesView::DrawPreview(UI::Ui& ui, const MainState& state, const Models::CursorScheme& scheme, const Rect& r, float opacity) {
    const auto& preview = state.Card(scheme).previewPath;
    // The picture / animated cursor, Stretch Uniform
    if (preview && UI::AnimatedCursor::Draw(ui, *preview, r.Inset(5.f), opacity)) return;
    // CreatePlaceholderPreview: the pointer on an accent circle
    if (opacity < 1.f) ui.g.PushOpacity(opacity);
    const float radius = std::min(r.w, r.h) * 0.36f;
    Kit::FillAccent(ui, { r.CX() - radius, r.CY() - radius, radius * 2.f, radius * 2.f }, radius);
    M::DrawIcon(ui.g, M::Icon::CursorPointer, r.CX(), r.CY(), radius * 1.2f, Colors::White);
    if (opacity < 1.f) ui.g.PopLayer();
}

void SchemesView::SchemeCard(UI::Ui& ui, const Rect& card, const Models::CursorScheme& scheme) {
    const float radius = Kit::Palette::PanelRadius;
    const UI::Id id = ui.MakeId("card");
    const UI::Interaction it = ui.Interact(id, card);
    // (the hover look stays while the mouse is on the card's own buttons)
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, ui.MouseOver(card), it.held);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const bool selected = state.IsSelected(scheme), installed = state.IsInstalled(scheme), active = state.IsActive(scheme);
    const float on = ui.Tween(ui.MakeId("card-selected"), selected ? 1.f : 0.f, 0.16f);
    if (on > 0.f && on < 1.f) ui.KeepAnimating();

    // The card lifts and glows; an accent edge when it's hovered, selected or the active scheme
    const float lit = std::max({ fb.hover, on, active ? 0.55f : 0.f });
    ui.PushTranslate(0.f, -4.f * fb.hover);
    ui.PushScale(1.f - 0.012f * fb.press, card.CX(), card.CY());
    M::Elevation(ui.g, card, radius, 2.f + 7.f * fb.hover);
    if (lit > 0.f) ui.g.GlowRound(card, radius, Kit::Palette::AccentLight, 16.f, 0.42f * lit);
    ui.g.Border(card, radius, Kit::Palette::Card, Lerp(Kit::Palette::CardBorder, Kit::Palette::AccentLight.Fade(0.8f), lit), lit > 0.5f ? 1.5f : 1.f);

    // The stage
    const Rect stage{ card.x + 1.f, card.y + 1.f, card.w - 2.f, card.h - kInfoH - 1.f };
    ui.g.FillRound(stage, radius - 1.f, radius - 1.f, 0.f, 0.f, Colors::Black.Fade(0.2f));
    ui.PushClip(stage);
    ui.g.FillRadial({ stage.CX() - stage.w * 0.5f, stage.CY() - stage.h * 0.62f, stage.w, stage.h * 1.24f },
                    { { 0.f, Kit::Palette::AccentLight.Fade(0.14f + 0.2f * lit) }, { 1.f, Kit::Palette::AccentLight.Fade(0.f) } });
    ui.PopClip();
    // The cursor floats while the card is hovered, each one to its own beat
    float bob = 0.f;
    if (fb.hover > 0.f) {
        UI::AnimatedCursor::RequestAmbientTicks();
        bob = 3.5f * fb.hover * std::sin(static_cast<float>(ui.Time()) * 3.2f);
    }
    const Rect preview = stage.Inset(Edges(14.f, 18.f));
    ui.PushTranslate(0.f, bob);
    ui.PushScale(1.f + 0.08f * Anim::EaseOutBack(fb.hover, 0.8f), preview.CX(), preview.CY());
    DrawPreview(ui, state, scheme, preview);
    ui.PopTransform();
    ui.PopTransform();

    // The badge
    if (active) StatusPill(ui, stage.x + 9.f, stage.y + 9.f, Loc::Get("card_active"), Kit::Palette::AccentEnd, true);
    else if (installed) StatusPill(ui, stage.x + 9.f, stage.y + 9.f, Loc::Get("card_installed"), Hex(0x4CAF50), false);

    // The selection box
    const float boxShown = std::max(fb.hover, on);
    if (boxShown > 0.f) {
        ui.g.PushOpacity(boxShown);
        bool value = selected;
        const Rect box{ stage.R() - 34.f, stage.y + 6.f, 28.f, 28.f };
        const UI::Id boxId = ui.MakeId("select");
        if (Kit::CheckBox(ui, boxId, box, value, L"")) state.SetSelected(scheme, value);
        ui.Tooltip(boxId, Loc::Get("card_select"));
        ui.g.PopLayer();
    }

    // The name and "Category · N cursors"
    const Rect info{ card.x + kPad, stage.B() + 9.f, card.w - kPad * 2.f, kInfoH - 9.f - kPad };
    TextStyle nameStyle = Kit::Style(13.5f, DWRITE_FONT_WEIGHT_BOLD);
    nameStyle.ellipsis = true;
    const float nameH = Kit::LineH(ui, nameStyle);
    const UI::Id nameId = ui.MakeId("name");
    const Rect nameR{ info.x, info.y, info.w, nameH };
    ui.g.Text(scheme.name, nameR, Kit::Palette::TextPrimary, nameStyle);
    const UI::Interaction nameIt = ui.Interact(nameId, nameR);
    ui.Tooltip(nameId, scheme.relativePath);
    const int count = state.Card(scheme).cursorCount;
    const std::wstring sub = scheme.category + L"  \x00B7  " + Loc::Fmt(count == 1 ? "card_cursors_one" : "card_cursors_many", static_cast<long long>(count));
    TextStyle subStyle = Kit::Style(11.f);
    subStyle.ellipsis = true;
    const float subY = info.y + nameH + 2.f;
    const Rect subR{ info.x, subY, info.w, Kit::LineH(ui, subStyle) };
    ui.g.Text(sub, subR, Kit::Palette::TextSecondary, subStyle);
    const UI::Id countId = ui.MakeId("count");
    const UI::Interaction countIt = ui.Interact(countId, subR);
    ui.Tooltip(countId, Loc::Get("card_cursors_tooltip"));
    if (nameIt.hovered || countIt.hovered) ui.SetCursor(IDC_HAND);

    // Apply, preview, uninstall
    const float rowY = card.B() - kPad - kButtonH;
    float right = info.R();
    if (installed) {
        const Rect trash{ right - kButtonH, rowY, kButtonH, kButtonH };
        const UI::Id statusId = ui.MakeId("status");
        if (Kit::IconButton(ui, statusId, trash, M::Icon::Delete, Kit::Palette::Danger, !busy)) UninstallCursorScheme(scheme);
        ui.Tooltip(statusId, Loc::Get("card_uninstall_tooltip"));
        right = trash.x - 6.f;
    }
    const Rect eye{ right - kButtonH, rowY, kButtonH, kButtonH };
    const UI::Id previewId = ui.MakeId("preview");
    if (Kit::IconButton(ui, previewId, eye, M::Icon::Eye)) main.Transition().Start(scheme, ui.ToScreenRect(card));
    ui.Tooltip(previewId, Loc::Get("card_preview_tooltip"));
    const UI::Id applyId = ui.MakeId("apply");
    if (Kit::ApplyButton(ui, applyId, { info.x, rowY, eye.x - 6.f - info.x, kButtonH }, Loc::Get("card_apply"), !busy, 12.5f)) ApplyCursorScheme(scheme);
    ui.Tooltip(applyId, Loc::Get("card_install_tooltip"));
    ui.PopTransform();
    ui.PopTransform();

    // The card itself (its buttons and the box take their own clicks)
    if (it.clicked || nameIt.clicked || countIt.clicked) main.Transition().Start(scheme, ui.ToScreenRect(card));
}

} // namespace CustomCursor::Main
