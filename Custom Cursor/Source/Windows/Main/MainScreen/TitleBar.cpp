#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Services/App/AppVersion.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"

// The title bar: the app's icon, "Custom Cursor Manager" and the window buttons (minimize, maximize /
// restore, close) in the app's look.
namespace CustomCursor::Main::TitleBar {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;

// The window buttons: a rounded glass tile that lights up on hover (the close one in DangerBrush) and
// an icon drawn with lines, each with its own little move: the minimize bar drops, the maximize
// frame grows, the close cross turns.
enum class WindowGlyph { Minimize, Maximize, Restore, Close };

void DrawWindowGlyph(UI::Ui& ui, WindowGlyph glyph, float cx, float cy, float hover, Color color) {
    constexpr float kW = 1.4f;
    switch (glyph) {
        case WindowGlyph::Minimize: {
            const float y = cy + 1.f + 2.f * hover;
            ui.g.Line(cx - 5.f, y, cx + 5.f, y, color, kW, true);
            break;
        }
        case WindowGlyph::Maximize: {
            const float h = 4.5f + 0.8f * hover;
            ui.g.StrokeRound({ cx - h, cy - h, h * 2.f, h * 2.f }, 2.f, color, kW);
            break;
        }
        case WindowGlyph::Restore: {
            const float d = 1.f + 0.8f * hover;
            ui.g.StrokeRound({ cx - 3.f + d, cy - 6.f - d * 0.5f, 9.f, 9.f }, 2.f, color.Fade(0.6f), kW);
            ui.g.StrokeRound({ cx - 6.f, cy - 3.f, 9.f, 9.f }, 2.f, color, kW);
            break;
        }
        case WindowGlyph::Close: {
            const float turn = 90.f * Anim::EaseOutBack(hover, 0.6f);
            ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(turn, D2D1::Point2F(cx, cy)));
            ui.g.Line(cx - 5.f, cy - 5.f, cx + 5.f, cy + 5.f, color, kW, true);
            ui.g.Line(cx + 5.f, cy - 5.f, cx - 5.f, cy + 5.f, color, kW, true);
            ui.g.PopTransform();
            break;
        }
    }
}

// True when clicked
bool WindowButton(UI::Ui& ui, const char* name, const Rect& r, WindowGlyph glyph) {
    const bool close = glyph == WindowGlyph::Close;
    const UI::Id id = ui.MakeId(name);
    const UI::Interaction it = ui.Interact(id, r);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Rect tile = r.Inset(Edges(5.f, 5.f));
    if (fb.hover > 0.f) {
        ui.PushScale(0.9f + 0.1f * Anim::EaseOutBack(fb.hover, 0.8f) - 0.05f * fb.press, tile.CX(), tile.CY());
        if (close) {
            ui.g.GlowRound(tile, 7.f, Kit::Palette::Danger, 12.f, 0.45f * fb.hover);
            ui.g.FillRound(tile, 7.f, Lerp(Kit::Palette::Danger, Kit::Palette::DangerHover, fb.press).Fade(fb.hover));
        } else {
            ui.g.Border(tile, 7.f, Lerp(Argb(0x1FFFFFFF), Argb(0x33FFFFFF), fb.press).Fade(fb.hover), Kit::Palette::CardBorder.Fade(fb.hover), 1.f);
        }
        ui.PopTransform();
    }
    const Color color = Lerp(Kit::Palette::TextSecondary, close ? Colors::White : Kit::Palette::TextPrimary, fb.hover);
    ui.PushScale(1.f - 0.1f * fb.press, r.CX(), r.CY());
    DrawWindowGlyph(ui, glyph, r.CX(), r.CY(), fb.hover, color);
    ui.PopTransform();
    return it.clicked;
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar) {
    // The theme's logo, the name (not translated: the app's name) and the version in a pill
    const Rect icon{ bar.x + 12.f, bar.CY() - 10.f, 20.f, 20.f };
    ui.g.DrawImage(Images::Load(Paths::Asset(Settings::IconTheme::Current().LogoAsset()), 256), icon);
    TextStyle title = Kit::Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    title.valign = VAlign::Center;
    const std::wstring name = L"Custom Cursor Manager";
    const float nameW = std::ceil(ui.g.Measure(name, title).w) + 2.f;
    ui.g.Text(name, { icon.R() + 8.f, bar.y, nameW, bar.h }, Kit::Palette::TextSecondary, title);
    TextStyle version = Kit::Style(10.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    version.valign = VAlign::Center;
    const std::wstring shown = AppVersion::DisplayVersion();
    const float pillW = std::ceil(ui.g.Measure(shown, version).w) + 16.f;
    const Rect pill{ icon.R() + 8.f + nameW + 8.f, bar.CY() - 9.f, pillW, 18.f };
    ui.g.Border(pill, 9.f, Kit::Palette::AccentLight.Fade(0.12f), Kit::Palette::AccentLight.Fade(0.3f), 1.f);
    ui.g.Text(shown, pill, Kit::Palette::AccentLight, version);

    const float bw = MainLayout::WindowButtonWidth;
    Rect r{ bar.R() - bw * 3.f, bar.y, bw, bar.h };
    if (WindowButton(ui, "win-min", r, WindowGlyph::Minimize)) main.Minimize();
    r.x += bw;
    // The frame, or two frames while maximized
    if (WindowButton(ui, "win-max", r, main.IsMaximized() ? WindowGlyph::Restore : WindowGlyph::Maximize)) main.ToggleMaximize();
    r.x += bw;
    if (WindowButton(ui, "win-close", r, WindowGlyph::Close)) main.Quit();
}

} // namespace CustomCursor::Main::TitleBar
