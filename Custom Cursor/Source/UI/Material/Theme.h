#pragma once
#include <string>
#include "Graphics/Color.h"
#include "Graphics/Graphics.h"

// App.xaml's design tokens (the MainWindow redesign): the #1B1F27 -> #272E3A backdrop, the #EA141821
// panels, the #F51E2330 cards and the icy-blue accent (#6FC6F2 -> #3D7DD8, light #88DDFF, dark
// #4499FF). Every colour the screens use comes from here; the MaterialDesign brushes the shared
// widgets read (scroll bars, text boxes, popups) are laid out over the same tokens. Each season has
// its own (Theme.cpp); all of them are dark.
namespace CustomCursor::UI::Material {

// App.xaml, by key: these are the classic theme's; RefreshSeason sets the season's
namespace Tokens {
inline Color BgTop = Hex(0x1B1F27);               // BgTopColor
inline Color BgBottom = Hex(0x272E3A);            // BgBottomColor
inline Color Panel = Argb(0xEA141821);            // PanelBrush
inline Color PanelBorder = Argb(0x14FFFFFF);      // PanelBorderBrush
inline Color Card = Argb(0xF51E2330);             // CardBrush
inline Color CardBorder = Argb(0x1FFFFFFF);       // CardBorderBrush
inline Color CardHoverBorder = Argb(0x3DFFFFFF);  // CardHoverBorderBrush
inline Color AccentLight = Hex(0x88DDFF);         // AccentLightColor
inline Color AccentDark = Hex(0x4499FF);          // AccentDarkColor
inline Color AccentStart = Hex(0x6FC6F2);         // AccentGradientBrush, offset 0
inline Color AccentEnd = Hex(0x3D7DD8);           // AccentGradientBrush, offset 1
inline Color Surface = Hex(0x1E2330);             // the opaque colour under cards and popups
inline Color TextPrimary = Hex(0xF5F7FA);         // TextPrimaryBrush
inline Color TextSecondary = Hex(0x9AA5B1);       // TextSecondaryBrush
inline Color TextMuted = Hex(0x6B7480);           // TextMutedBrush
inline Color Danger = Hex(0xE5484D);              // DangerBrush
inline Color DangerHover = Hex(0xF1666B);         // DangerHoverBrush
inline Color NeutralDisabled = Hex(0x4B5563);     // NeutralDisabledBrush
// Shared corner radii (PanelRadius / CardRadius / ControlRadius)
inline constexpr float PanelRadius = 14.f;
inline constexpr float CardRadius = 12.f;
inline constexpr float ControlRadius = 8.f;
} // namespace Tokens

// A MaterialDesign ColorPair: the color and the contrasting text color on it.
struct ColorPair {
    Color color;
    Color foreground;
};

// The MaterialDesign brushes (MaterialDesign.Brush.*), named as in the toolkit.
struct Palette {
    bool baseDark = true;
    Color background;               // MaterialDesignPaper
    Color foreground;               // MaterialDesignBody
    Color foregroundLight;          // MaterialDesignBodyLight / TextBox.Border
    Color divider;                  // MaterialDesignDivider
    Color cardBackground;
    Color toolkitCard;              // popups, menus
    Color cardBorder;
    ColorPair primaryLight, primaryMid, primaryDark;
    ColorPair secondaryLight, secondaryMid, secondaryDark;
    Color validationError;
    // Base-theme brushes
    Color textBoxOutlineBorder, textBoxOutlineInactiveBorder, textBoxHoverBorder, textBoxFilledBackground;
    Color comboOutlineBorder, comboOutlineInactiveBorder, comboHoverBorder;
    Color comboPopupBackground;
    Color checkBoxUncheckedBorder, checkBoxDisabled;
    Color switchTrackOff;
    Color flatClick, buttonRipple;
    Color separator;
    Color scrollBarForeground;      // the slim ScrollBar thumb
    Color snackbarBackground, snackbarMouseOver;
    Color toolTipBackground;
    Color listBoxItemSelected, listViewHover;
    Color tabDivider;
    Color sectionHeader;
};

// The tokens and the palette of the chosen theme (Settings::IconTheme), after it changes
void RefreshSeason();
// The palette (built on first use).
const Palette& P();
// Increments whenever the palette changes (screens can cache derived values).
unsigned Revision();

// MaterialDesign ColorAssist.ContrastingForegroundColor
Color ContrastingForeground(Color c);

// ── Typography (Segoe UI, see Graphics/Fonts.h) ──
TextStyle Headline4();   // 34 Regular
TextStyle Headline5();   // 24 Regular
TextStyle Headline6();   // 20 Medium
TextStyle Subtitle1();   // 16 Regular
TextStyle Subtitle2();   // 14 Medium
TextStyle Body1();       // 16 Regular
TextStyle Body2();       // 14 Regular
TextStyle Caption();     // 12 Regular
TextStyle Text(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);
inline TextStyle Medium(float size) { return Text(size, DWRITE_FONT_WEIGHT_MEDIUM); }
inline TextStyle SemiBold(float size) { return Text(size, DWRITE_FONT_WEIGHT_SEMI_BOLD); }
inline TextStyle Bold(float size) { return Text(size, DWRITE_FONT_WEIGHT_BOLD); }

} // namespace CustomCursor::UI::Material
