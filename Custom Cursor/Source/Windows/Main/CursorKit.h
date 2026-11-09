#pragma once
#include <string>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

// MainWindow.xaml's Window.Resources redrawn in the redesign's motion (the controls of App.xaml's
// identity): ModernButton (the "glass" button), ApplyButton (the gradient call to action),
// UpdateRowButton, the GlassTextBox and the scheme / cursor cards' frame. Every colour comes from Material::Tokens.
namespace CustomCursor::Main::CursorKit {

namespace Palette = UI::Material::Tokens;

// The emoji in the labels ("🔄 Update", "⚡ Install Selected") are drawn in colour: the XAML's
// FontFamily="Segoe UI Emoji" for the whole button. `wrap` / `align` as TextBlock.
TextStyle Style(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, bool wrap = false, HAlign align = HAlign::Left);
float LineH(UI::Ui& ui, const TextStyle& style);
float TextH(UI::Ui& ui, const std::wstring& text, const TextStyle& style, float width);

// AccentGradientBrush (StartPoint 0,0 EndPoint 1,1) over a rounded rectangle; `light` brightens it (hover)
void FillAccent(UI::Ui& ui, const Rect& r, float radius, float light = 0.f);
// AccentBorderGradientBrush: the white / icy blue sheen of the ApplyButton's 2 px border (the fill
// drawn inset over it leaves the ring)
void FillAccentSheen(UI::Ui& ui, const Rect& r, float radius);
// A panel (PanelBrush, PanelBorderBrush, PanelRadius): the side panels, the header bars
void Panel(UI::Ui& ui, const Rect& r, float radius = Palette::PanelRadius);

// ── Buttons (true when clicked) ──
// ModernButton: #1FFFFFFF glass with the card border; hover #33FFFFFF + CardHoverBorder and 1.03,
// pressed 0.96, disabled 45 %. `fill` replaces the glass (the scheme card's red / grey status button)
struct GlassLook {
    float fontSize = 13.f;
    Color fill = Colors::Transparent;   // transparent = the glass
    bool accentFill = false;            // AccentGradientBrush background (the ✨ button)
    float radius = Palette::ControlRadius;
};
bool GlassButton(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool enabled = true, const GlassLook& look = {});
float GlassWidth(UI::Ui& ui, const std::wstring& text, float fontSize = 13.f);   // Padding 15,8

// ApplyButton: the accent gradient inside the 2 px sheen border, white text; hover 1.02 (and a light
// crossing it), pressed 0.98, disabled 40 %
bool ApplyButton(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool enabled = true, float fontSize = 13.f);
float ApplyWidth(UI::Ui& ui, const std::wstring& text, float fontSize = 13.f);

// UpdateRowButton: #17FFFFFF with the card border, #22FFFFFF on hover (the Updates card's rows).
// Returns the interaction; draw the content over `r` afterwards
UI::Interaction RowButton(UI::Ui& ui, UI::Id id, const Rect& r, bool enabled = true, Color fill = Colors::Transparent, bool accentFill = false);

// A round-cornered glass square with an icon; `tint` colours it on hover (the danger red of the trash)
bool IconButton(UI::Ui& ui, UI::Id id, const Rect& r, UI::Material::Icon icon, Color tint = Colors::Transparent, bool enabled = true);

// A text link (TextDecorations="Underline"), brighter on hover
bool Link(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, float fontSize, Color color, bool wrapCentered = false);

// GlassTextBox: #1FFFFFFF, card border (AccentLight while focused), ControlRadius, 36 tall, 13 px
// text, Padding 10,6, a placeholder in the muted colour. True when the text changed
bool SearchBox(UI::Ui& ui, UI::Id id, const Rect& r, std::wstring& value, const std::wstring& placeholder);
inline constexpr float kSearchHeight = 36.f;

// CheckBox with its label ("Select"): a rounded box that fills with the accent and a check
bool CheckBox(UI::Ui& ui, UI::Id id, const Rect& r, bool& value, const std::wstring& label, float fontSize = 12.f);

// A progress bar: track and accent gradient fill with a shimmer (0..1)
void ProgressBar(UI::Ui& ui, const Rect& r, float value);

// An arc of a circle (angles in degrees, 0 = up, clockwise)
void Arc(UI::Ui& ui, float cx, float cy, float radius, float startDeg, float sweepDeg, Color c, float width);

// The red dot of an available update (UpdateAvailableBadge)
void Badge(UI::Ui& ui, float cx, float cy);

// Ui::tooltipRenderer: a dark card with the card border under the cursor, fading in
void DrawTooltip(Graphics& g, const std::wstring& text, float mouseX, float mouseY, float age, float left);

} // namespace CustomCursor::Main::CursorKit
