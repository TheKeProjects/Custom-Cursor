#include "UI/Material/Theme.h"
#include <cmath>
#include "Seasonal/SeasonalTheme.h"
#include "Settings/IconThemeSettings.h"

namespace CustomCursor::UI::Material {

namespace {

Palette palette;
unsigned revision = 0;
bool built = false;

ColorPair Pair(Color c) { return { c, ContrastingForeground(c) }; }

// The look of each theme: backdrop, accent gradient, its light and dark ends, the panels and the cards
struct Look {
    Color bgTop, bgBottom, start, end, light, dark, panel, card, surface;
};

Look LookOf(const std::wstring& theme) {
    if (theme == L"halloween")
        return { Hex(0x171019), Hex(0x2A1830), Hex(0xFFA94D), Hex(0xD9480F), Hex(0xFFC78A), Hex(0xF76707), Argb(0xE0190F1D), Argb(0xF5261828), Hex(0x261828) };
    if (theme == L"summer")
        return { Hex(0x0B2233), Hex(0x124055), Hex(0x4FE0D2), Hex(0x0E8FA8), Hex(0x8FF3E6), Hex(0x18B7C4), Argb(0xE00A2130), Argb(0xF5113040), Hex(0x113040) };
    if (theme == L"christmas")
        return { Hex(0x0C1A2B), Hex(0x14293F), Hex(0xF2545B), Hex(0xB81D2A), Hex(0xFFA3A8), Hex(0xE5383B), Argb(0xE00B1B2A), Argb(0xF5142638), Hex(0x142638) };
    if (theme == L"lgbtq")
        return { Hex(0x17132B), Hex(0x271D42), Hex(0xE879C9), Hex(0x6D5AE6), Hex(0xF3B3E4), Hex(0xA855F7), Argb(0xE016112A), Argb(0xF5221A3B), Hex(0x221A3B) };
    return { Hex(0x1B1F27), Hex(0x272E3A), Hex(0x6FC6F2), Hex(0x3D7DD8), Hex(0x88DDFF), Hex(0x4499FF), Argb(0xEA141821), Argb(0xF51E2330), Hex(0x1E2330) };
}

void ApplyTokens() {
    const Look l = LookOf(Settings::IconTheme::Current().EffectiveTheme());
    Tokens::BgTop = l.bgTop;
    Tokens::BgBottom = l.bgBottom;
    Tokens::AccentStart = l.start;
    Tokens::AccentEnd = l.end;
    Tokens::AccentLight = l.light;
    Tokens::AccentDark = l.dark;
    Tokens::Panel = l.panel;
    Tokens::Card = l.card;
    Tokens::Surface = l.surface;
}

// MaterialDesignTheme.Dark's brushes over App.xaml's tokens
void Build() {
    ApplyTokens();
    using namespace Tokens;
    Palette p;
    p.baseDark = true;
    p.background = BgTop;
    p.foreground = TextPrimary;
    p.foregroundLight = TextSecondary;
    p.divider = PanelBorder;
    p.cardBackground = Card;
    p.toolkitCard = Surface;
    p.cardBorder = CardBorder;
    p.primaryLight = Pair(AccentLight);
    p.primaryMid = Pair(AccentStart);
    p.primaryDark = Pair(AccentDark);
    p.secondaryLight = Pair(Hex(0x323A48));
    p.secondaryMid = Pair(Hex(0x272E3A));
    p.secondaryDark = Pair(Hex(0x1B1F27));
    p.validationError = Danger;
    p.textBoxOutlineBorder = CardBorder;          // GlassTextBox BorderBrush
    p.textBoxOutlineInactiveBorder = CardBorder;
    p.textBoxHoverBorder = CardHoverBorder;
    p.textBoxFilledBackground = Argb(0x1FFFFFFF);  // GlassTextBox Background
    p.comboOutlineBorder = CardBorder;
    p.comboOutlineInactiveBorder = CardBorder;
    p.comboHoverBorder = CardHoverBorder;
    p.comboPopupBackground = Surface;
    p.checkBoxUncheckedBorder = TextSecondary;
    p.checkBoxDisabled = NeutralDisabled;
    p.switchTrackOff = NeutralDisabled;
    p.flatClick = Argb(0x19FFFFFF);
    p.buttonRipple = Hex(0xB9B9B9);
    p.separator = PanelBorder;
    p.scrollBarForeground = AccentLight.Fade(0.55f);   // the ScrollBar style's thumb: AccentLightBrush, Opacity 0.55
    p.snackbarBackground = Surface;
    p.snackbarMouseOver = Hex(0x2A3040);
    p.toolTipBackground = Surface;
    p.listBoxItemSelected = Argb(0x19FFFFFF);
    p.listViewHover = Argb(0x14FFFFFF);
    p.tabDivider = PanelBorder;
    p.sectionHeader = AccentLight;
    palette = p;
    built = true;
    ++revision;
}

} // namespace

void RefreshSeason() {
    Build();
    Seasonal::Refresh();
}

const Palette& P() {
    if (!built) Build();
    return palette;
}

unsigned Revision() {
    if (!built) Build();
    return revision;
}

Color ContrastingForeground(Color c) {
    const auto lin = [](float v) { return v > 0.03928f ? std::pow((v + 0.055f) / 1.055f, 2.4f) : v / 12.92f; };
    const double luminance = 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
    return luminance > 0.179 ? Colors::Black : Colors::White;
}

TextStyle Text(float size, DWRITE_FONT_WEIGHT weight) {
    TextStyle s;
    s.size = size;
    s.font = Font::Ui;
    s.weight = weight;
    return s;
}

TextStyle Headline4() { return Text(34.f); }
TextStyle Headline5() { return Text(24.f); }
TextStyle Headline6() { return Text(20.f, DWRITE_FONT_WEIGHT_MEDIUM); }
TextStyle Subtitle1() { return Text(16.f); }
TextStyle Subtitle2() { return Text(14.f, DWRITE_FONT_WEIGHT_MEDIUM); }
TextStyle Body1() { return Text(16.f); }
TextStyle Body2() { return Text(14.f); }
TextStyle Caption() { return Text(12.f); }

} // namespace CustomCursor::UI::Material
