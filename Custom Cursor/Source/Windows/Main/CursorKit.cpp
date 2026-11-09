#include "Windows/Main/CursorKit.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/TextBox.h"

namespace CustomCursor::Main::CursorKit {

namespace {

namespace M = UI::Material;

TextStyle Centered(TextStyle t) {
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    return t;
}

// A label too long for its button shrinks to fit (a longer language), down to 10 px; past that "…"
TextStyle Fitted(UI::Ui& ui, const std::wstring& text, TextStyle t, float room) {
    const float w = ui.g.Measure(text, t).w;
    if (w > room && room > 0.f) {
        t.size = std::max(10.f, t.size * room / w);
        t.ellipsis = true;
    }
    return t;
}

} // namespace

TextStyle Style(float size, DWRITE_FONT_WEIGHT weight, bool wrap, HAlign align) {
    TextStyle t = M::Text(size, weight);
    t.align = align;
    t.valign = VAlign::Top;
    t.wrap = wrap;
    t.colorFont = true;
    return t;
}

float LineH(UI::Ui& ui, const TextStyle& style) { return std::ceil(ui.g.Measure(L"Ag", style).h); }

float TextH(UI::Ui& ui, const std::wstring& text, const TextStyle& style, float width) {
    return std::ceil(ui.g.Measure(text, style, style.wrap ? width : 100000.f).h);
}

void FillAccent(UI::Ui& ui, const Rect& r, float radius, float light) {
    ui.g.FillLinear(r, radius, { { 0.f, Lerp(Palette::AccentStart, Colors::White, light) }, { 1.f, Lerp(Palette::AccentEnd, Colors::White, light) } },
                    { 0.f, 0.f }, { 1.f, 1.f });
}

void FillAccentSheen(UI::Ui& ui, const Rect& r, float radius) {
    ui.g.FillLinear(r, radius,
                    { { 0.f, Lerp(Palette::AccentLight, Colors::White, 0.5f) }, { 0.25f, Palette::AccentLight }, { 0.5f, Palette::AccentDark }, { 0.75f, Palette::AccentLight }, { 1.f, Lerp(Palette::AccentLight, Colors::White, 0.5f) } },
                    { 0.f, 0.f }, { 1.f, 1.f });
}

void Panel(UI::Ui& ui, const Rect& r, float radius) { ui.g.Border(r, radius, Palette::Panel, Palette::PanelBorder, 1.f); }

bool GlassButton(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool enabled, const GlassLook& look) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held);
    if (!enabled) ui.g.PushOpacity(0.45f);
    ui.PushScale(1.f + 0.03f * fb.hover - 0.07f * fb.press, r.CX(), r.CY());
    if (look.accentFill) {
        if (fb.hover > 0.f) ui.g.GlowRound(r, look.radius, Palette::AccentLight, 12.f, 0.5f * fb.hover);
        FillAccent(ui, r, look.radius, 0.12f * fb.hover);
    } else if (!look.fill.Transparent()) {
        if (fb.hover > 0.f) ui.g.GlowRound(r, look.radius, look.fill, 12.f, 0.45f * fb.hover);
        ui.g.Border(r, look.radius, Lerp(look.fill, Colors::White, 0.12f * fb.hover), Lerp(Palette::CardBorder, Palette::CardHoverBorder, fb.hover), 1.f);
    } else {
        ui.g.Border(r, look.radius, Lerp(Argb(0x1FFFFFFF), Argb(0x33FFFFFF), fb.hover), Lerp(Palette::CardBorder, Palette::CardHoverBorder, fb.hover), 1.f);
    }
    const TextStyle t = Fitted(ui, text, Centered(Style(look.fontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD)), r.w - 12.f);
    ui.g.Text(text, r, Palette::TextPrimary, t);
    ui.PopTransform();
    if (!enabled) ui.g.PopLayer();
    return it.clicked && enabled;
}

float GlassWidth(UI::Ui& ui, const std::wstring& text, float fontSize) {
    return std::ceil(ui.g.Measure(text, Style(fontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD)).w) + 30.f;
}

bool ApplyButton(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool enabled, float fontSize) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held);
    if (!enabled) ui.g.PushOpacity(0.4f);
    ui.PushScale(1.f + 0.02f * fb.hover - 0.04f * fb.press, r.CX(), r.CY());
    if (enabled) {
        M::Elevation(ui.g, r, 9.f, 2.f + 3.f * fb.hover);
        ui.g.GlowRound(r, 9.f, Palette::AccentLight, 16.f, 0.15f + 0.4f * fb.hover);
    }
    FillAccentSheen(ui, r, 9.f);
    const Rect inner = r.Inset(1.5f);
    FillAccent(ui, inner, 7.f, 0.1f * fb.hover);
    // A light crossing the button while hovered
    if (fb.hover > 0.f) {
        const float k = std::fmod(static_cast<float>(ui.Time()) * 0.7f, 1.6f) - 0.3f;
        ui.g.FillLinear(inner, 7.f,
                        { { std::clamp(k - 0.15f, 0.f, 1.f), Colors::White.Fade(0.f) },
                          { std::clamp(k, 0.f, 1.f), Colors::White.Fade(0.28f * fb.hover) },
                          { std::clamp(k + 0.15f, 0.f, 1.f), Colors::White.Fade(0.f) } },
                        { 0.f, 0.f }, { 1.f, 0.4f });
        ui.KeepAnimatingThrottled();
    }
    const TextStyle t = Fitted(ui, text, Centered(Style(fontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD)), inner.w - 16.f);
    ui.g.Text(text, inner, Colors::White, t);
    ui.PopTransform();
    if (!enabled) ui.g.PopLayer();
    return it.clicked && enabled;
}

float ApplyWidth(UI::Ui& ui, const std::wstring& text, float fontSize) {
    return std::ceil(ui.g.Measure(text, Style(fontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD)).w) + 34.f;
}

UI::Interaction RowButton(UI::Ui& ui, UI::Id id, const Rect& r, bool enabled, Color fill, bool accentFill) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held);
    const float radius = Palette::ControlRadius;
    if (accentFill) {
        if (fb.hover > 0.f) ui.g.GlowRound(r, radius, Palette::AccentLight, 14.f, 0.45f * fb.hover);
        FillAccent(ui, r, radius, 0.08f * fb.hover);
        ui.g.StrokeRound(r.Inset(0.5f), radius - 0.5f, Palette::CardBorder, 1.f);
    } else {
        const Color base = fill.Transparent() ? Argb(0x17FFFFFF) : fill;
        const Color hover = fill.Transparent() ? Argb(0x22FFFFFF) : Lerp(fill, Colors::White, 0.08f);
        ui.g.Border(r, radius, Lerp(base, hover, fb.hover), Palette::CardBorder, 1.f);
    }
    return it;
}

bool IconButton(UI::Ui& ui, UI::Id id, const Rect& r, M::Icon icon, Color tint, bool enabled) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const bool tinted = !tint.Transparent();
    if (!enabled) ui.g.PushOpacity(0.4f);
    ui.PushScale(1.f + 0.05f * fb.hover - 0.08f * fb.press, r.CX(), r.CY());
    const Color fill = tinted ? Lerp(Argb(0x1FFFFFFF), tint, fb.hover) : Lerp(Argb(0x1FFFFFFF), Argb(0x33FFFFFF), fb.hover);
    ui.g.Border(r, Palette::ControlRadius, fill, Lerp(Palette::CardBorder, tinted ? tint : Palette::CardHoverBorder, fb.hover), 1.f);
    M::DrawIcon(ui.g, icon, r.CX(), r.CY(), std::min(r.w, r.h) * 0.5f, Lerp(Palette::TextPrimary, Colors::White, fb.hover));
    ui.PopTransform();
    if (!enabled) ui.g.PopLayer();
    return it.clicked;
}

bool Link(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, float fontSize, Color color, bool wrapCentered) {
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
    TextStyle t = Style(fontSize, DWRITE_FONT_WEIGHT_NORMAL, wrapCentered, wrapCentered ? HAlign::Center : HAlign::Left);
    t.valign = VAlign::Center;
    t.underline = true;
    ui.g.Text(text, r, Lerp(color, Palette::AccentLight, hover), t);
    return it.clicked;
}

bool SearchBox(UI::Ui& ui, UI::Id id, const Rect& r, std::wstring& value, const std::wstring& placeholder) {
    UI::TextBoxStyle s;
    s.text = M::Text(13.f);
    s.foreground = Palette::TextPrimary;
    s.background = Argb(0x1FFFFFFF);
    s.border = Palette::CardBorder;
    s.focusBorder = Palette::AccentLight;
    s.caret = Palette::TextPrimary;
    s.selection = Palette::AccentDark.Fade(0.45f);
    s.placeholderColor = Palette::TextMuted;
    s.placeholder = placeholder;
    s.padding = Edges(10.f, 6.f);
    s.radius = Palette::ControlRadius;
    if (ui.Focus() == id) ui.g.GlowRound(r, Palette::ControlRadius, Palette::AccentLight, 10.f, 0.25f);
    return UI::TextBox(ui, id, r, value, s);
}

bool CheckBox(UI::Ui& ui, UI::Id id, const Rect& r, bool& value, const std::wstring& label, float fontSize) {
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    if (it.clicked) value = !value;
    const float on = ui.Tween(UI::HashId(id, 0xC4E), value ? 1.f : 0.f, 0.16f);
    if (on > 0.f && on < 1.f) ui.KeepAnimating();
    TextStyle t = Style(fontSize);
    t.valign = VAlign::Center;
    const float labelW = std::ceil(ui.g.Measure(label, t).w);
    const float box = 16.f;
    const float x = r.CX() - (box + 7.f + labelW) * 0.5f;
    const Rect b{ x, r.CY() - box * 0.5f, box, box };
    ui.PushScale(1.f - 0.1f * fb.press, b.CX(), b.CY());
    if (fb.hover > 0.f) ui.g.GlowRound(b, 4.f, Palette::AccentLight, 8.f, 0.4f * fb.hover);
    ui.g.Border(b, 4.f, Argb(0x1FFFFFFF), Lerp(Palette::TextSecondary, Palette::AccentLight, fb.hover), 1.5f);
    if (on > 0.f) {
        ui.g.PushOpacity(on);
        FillAccent(ui, b, 4.f);
        // The check draws itself in
        const float k = Anim::EaseOutCubic(on);
        const float x0 = b.x + 3.8f, y0 = b.y + 8.3f, x1 = b.x + 6.8f, y1 = b.y + 11.3f, x2 = b.x + 12.4f, y2 = b.y + 4.9f;
        const float first = std::min(1.f, k * 2.f), second = std::max(0.f, k * 2.f - 1.f);
        ui.g.Line(x0, y0, x0 + (x1 - x0) * first, y0 + (y1 - y0) * first, Colors::White, 2.f, true);
        if (second > 0.f) ui.g.Line(x1, y1, x1 + (x2 - x1) * second, y1 + (y2 - y1) * second, Colors::White, 2.f, true);
        ui.g.PopLayer();
    }
    ui.PopTransform();
    ui.g.Text(label, { b.R() + 7.f, r.y, labelW + 2.f, r.h }, Palette::TextPrimary, t);
    return it.clicked;
}

void ProgressBar(UI::Ui& ui, const Rect& r, float value) {
    const float radius = r.h * 0.5f;
    ui.g.FillRound(r, radius, Colors::White.Fade(0.08f));
    const float w = r.w * std::clamp(value, 0.f, 1.f);
    if (w < 0.5f) return;
    const Rect bar{ r.x, r.y, std::max(r.h, w), r.h };
    ui.g.GlowRound(bar, radius, Palette::AccentLight, 10.f, 0.5f);
    FillAccent(ui, bar, radius);
    const float c = -0.3f + 1.6f * std::fmod(static_cast<float>(ui.Time()) / 1.3f, 1.f);
    ui.g.FillLinear(bar, radius,
                    { { std::clamp(c - 0.2f, 0.f, 1.f), Colors::White.Fade(0.f) },
                      { std::clamp(c, 0.f, 1.f), Colors::White.Fade(0.4f) },
                      { std::clamp(c + 0.2f, 0.f, 1.f), Colors::White.Fade(0.f) } },
                    { 0.f, 0.f }, { 1.f, 0.f });
    ui.KeepAnimatingThrottled();
}

void Arc(UI::Ui& ui, float cx, float cy, float radius, float startDeg, float sweepDeg, Color c, float width) {
    constexpr float kPi = 3.14159265f;
    const auto At = [&](float deg) {
        const float a = deg * kPi / 180.f;
        return D2D1::Point2F(cx + radius * std::sin(a), cy - radius * std::cos(a));
    };
    const auto geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(At(startDeg), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc(D2D1::ArcSegment(At(startDeg + sweepDeg), D2D1::SizeF(radius, radius), 0.f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                      sweepDeg > 180.f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    });
    ui.g.StrokeGeometry(geometry.Get(), c, width);
}

void Badge(UI::Ui& ui, float cx, float cy) {
    // A still dot with a soft halo (a breathing one kept the window redrawing all the time)
    ui.g.FillEllipse(cx, cy, 6.5f, 6.5f, Palette::Danger.Fade(0.25f));
    ui.g.FillEllipse(cx, cy, 4.f, 4.f, Palette::Danger);
}

void DrawTooltip(Graphics& g, const std::wstring& text, float mouseX, float mouseY, float age, float left) {
    TextStyle style = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const TextSize size = g.Measure(text, style, 360.f);
    const float w = std::ceil(size.w) + 20.f, h = std::ceil(size.h) + 12.f;
    float x = mouseX, y = mouseY + 22.f;
    x = std::clamp(x, 4.f, std::max(4.f, g.Width() - w - 4.f));
    if (y + h > g.Height() - 4.f) y = mouseY - h - 6.f;
    const float fade = std::min({ 1.f, age / 0.15f, left / 0.15f });
    const Rect box{ std::round(x), std::round(y + 4.f * (1.f - fade)), w, h };
    g.PushOpacity(std::max(0.f, fade));
    g.Shadow(box, 8.f, 8.f, Colors::Black.Fade(0.35f));
    g.Border(box, 8.f, Palette::Surface, Palette::CardHoverBorder, 1.f);
    g.Text(text, box.Inset(10.f, 6.f), Palette::TextPrimary, style);
    g.PopLayer();
}

} // namespace CustomCursor::Main::CursorKit
