#include "Windows/Main/MainScreen/PageBanner.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "Windows/Main/CursorKit.h"

namespace CustomCursor::Main {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;

constexpr float kPi = 3.14159265f, kRadius = 14.f;

// Stable pseudo-random 0..1 (the sparks need no state)
float Hash(double index, double channel) {
    const double v = std::sin(index * 12.9898 + channel * 78.233) * 43758.5453;
    return static_cast<float>(v - std::floor(v));
}

} // namespace

void PageBanner(UI::Ui& ui, const Rect& r, const PageBannerSpec& spec, double shownAt) {
    const double time = ui.Time();
    const float t = static_cast<float>(time);
    const Color ink = Colors::White, spark = Kit::Palette::AccentLight;
    UI::AnimatedCursor::RequestAmbientTicks();

    // Background
    M::Elevation(ui.g, r, kRadius, 3.f);
    ui.g.PushRoundClip(r, kRadius);
    const float drift = 0.18f * std::sin(t * 0.33f);
    const Color end = Kit::Palette::AccentEnd;
    ui.g.FillLinear(r, 0.f, { { 0.f, Lerp(end, Colors::Black, 0.22f) }, { 0.6f, end }, { 1.f, Lerp(end, Colors::Black, 0.5f) } }, { drift, 0.f }, { 1.f, 1.f - drift });
    const float gx = r.x + r.w * (0.68f + 0.16f * std::sin(t * 0.21f));
    ui.g.FillRadial({ gx - 230.f, r.CY() - 130.f, 460.f, 260.f }, { { 0.f, spark.Fade(0.3f) }, { 1.f, spark.Fade(0.f) } });
    for (int i = 0; i < 16; ++i) {
        const float period = 3.f + 4.f * Hash(i, 2);
        const float phase = static_cast<float>(std::fmod(time / period + Hash(i, 3), 1.0));
        const float x = r.x + Hash(i, 1) * r.w + 8.f * std::sin(t * 1.2f + static_cast<float>(i));
        const float y = r.B() + 6.f - phase * (r.h + 12.f);
        const float size = 1.f + 1.8f * Hash(i, 4);
        ui.g.FillEllipse(x, y, size, size, Lerp(spark, Colors::White, 0.4f).Fade(0.65f * std::sin(phase * kPi)));
    }
    const float sweep = static_cast<float>(std::fmod(time, 8.0) / 1.4);
    if (sweep < 1.f) {
        const float sx = r.x - 200.f + (r.w + 400.f) * Anim::EaseInOutCubic(sweep);
        ui.g.PushTransform(D2D1::Matrix3x2F::Skew(-24.f, 0.f, D2D1::Point2F(sx, r.CY())));
        ui.g.FillLinear({ sx, r.y, 170.f, r.h }, 0.f, { { 0.f, ink.Fade(0.f) }, { 0.5f, ink.Fade(0.13f) }, { 1.f, ink.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
        ui.g.PopTransform();
    }
    ui.g.FillRect({ r.x, r.B() - 2.f, r.w, 2.f }, spark.Fade(0.5f));
    ui.g.PopLayer();

    // Emblem: the icon inside two turning arcs, with its own motion
    const float er = 26.f, ecx = r.x + 24.f + er, ecy = r.CY();
    ui.g.FillEllipse(ecx, ecy, er, er, Colors::Black.Fade(0.22f));
    ui.g.StrokeEllipse(ecx, ecy, er, er, ink.Fade(0.28f), 1.5f);
    for (int i = 0; i < 2; ++i) Kit::Arc(ui, ecx, ecy, er + 5.f, t * 70.f + 180.f * static_cast<float>(i), 70.f, Lerp(spark, Colors::White, 0.3f).Fade(0.9f), 2.5f);
    D2D1::Matrix3x2F motion = D2D1::Matrix3x2F::Identity();
    switch (spec.motion) {
        case PageBannerSpec::Motion::Wobble: motion = D2D1::Matrix3x2F::Rotation(11.f * std::sin(t * 2.4f), D2D1::Point2F(ecx, ecy)); break;
        case PageBannerSpec::Motion::Float: motion = D2D1::Matrix3x2F::Translation(0.f, 3.f * std::sin(t * 2.f)); break;
        case PageBannerSpec::Motion::Spin: motion = D2D1::Matrix3x2F::Rotation(std::fmod(t * 40.f, 360.f), D2D1::Point2F(ecx, ecy)); break;
        case PageBannerSpec::Motion::Bob: motion = D2D1::Matrix3x2F::Translation(0.f, 4.f * std::fabs(std::sin(t * 1.8f)) - 2.f); break;
    }
    ui.g.PushTransform(motion);
    M::DrawIcon(ui.g, spec.icon, ecx, ecy, 28.f, Lerp(spark, Colors::White, 0.55f));
    ui.g.PopTransform();

    // Pill on the right
    float right = r.R() - 20.f;
    if (!spec.pill.empty()) {
        TextStyle pillText = Kit::Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        pillText.valign = VAlign::Center;
        pillText.ellipsis = true;
        const float pw = std::min(std::ceil(ui.g.Measure(spec.pill, pillText).w) + 46.f, r.w * 0.42f);
        const Rect pill{ right - pw, r.CY() - 17.f, pw, 34.f };
        ui.g.FillRound(pill, 17.f, Colors::Black.Fade(0.24f));
        ui.g.StrokeRound(pill.Inset(0.5f), 16.5f, ink.Fade(0.25f), 1.f);
        M::DrawIcon(ui.g, spec.pillIcon, pill.x + 20.f, pill.CY(), 17.f, Lerp(spark, Colors::White, 0.4f));
        ui.g.Text(spec.pill, { pill.x + 35.f, pill.y, pw - 45.f, pill.h }, ink, pillText);
        right = pill.x - 16.f;
    }

    // Title dropping in letter by letter (then the whole string: its layout is cached), the subtitle under it
    const float tx = ecx + er + 22.f;
    TextStyle heading = Kit::Style(24.f, DWRITE_FONT_WEIGHT_BLACK);
    TextStyle sub = Kit::Style(12.5f);
    sub.ellipsis = true;
    const float titleH = Kit::LineH(ui, heading), subH = Kit::LineH(ui, sub);
    const float y = r.CY() - (titleH + 2.f + subH) * 0.5f;
    const int letters = static_cast<int>(spec.title.size());
    if (time - shownAt < 0.1 + 0.03 * letters + 0.45) {
        ui.KeepAnimating();
        for (int i = 0; i < letters; ++i) {
            const float k = M::Motion::Stagger(ui, shownAt + 0.1, i, 0.03f, 0.45f, 64);
            const float cx = ui.g.Measure(spec.title.substr(0, static_cast<size_t>(i)), heading).w;
            ui.g.Text(std::wstring(1, spec.title[static_cast<size_t>(i)]), { tx + cx, y - 18.f * (1.f - Anim::EaseOutBack(k, 0.6f)), 60.f, titleH }, ink.Fade(Anim::Clamp01(k * 1.5f)), heading);
        }
    } else {
        heading.ellipsis = true;
        ui.g.Text(spec.title, { tx, y, std::max(0.f, right - tx), titleH }, ink, heading);
    }
    ui.g.Text(spec.subtitle, { tx, y + titleH + 2.f, std::max(0.f, right - tx), subH }, ink.Fade(0.82f), sub);
}

} // namespace CustomCursor::Main
