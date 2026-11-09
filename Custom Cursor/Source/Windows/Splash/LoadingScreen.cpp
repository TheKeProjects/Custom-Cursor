#include "Windows/Splash/LoadingScreen.h"
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/App/ResourceExtractor.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"

namespace CustomCursor::Main {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;

constexpr float kPi = 3.14159265f;

// SimulateProgress's steps (seconds from the start, progress, status). The timeline waits before
// kWaitUnpack until the schemes are unpacked, and before kWaitStartup until the start-up is done.
struct Step {
    double at;
    double progress;
    const char* status;
};
constexpr Step kSteps[] = {
    { 0.0, 20, "loading_step_resources" }, { 0.3, 40, "loading_step_paths" }, { 0.6, 60, "loading_step_extracting" },
    { 0.9, 80, "loading_step_cursors" },   { 1.2, 100, "loading_step_ready" },
};
constexpr int kStepCount = static_cast<int>(std::size(kSteps));
constexpr double kWaitUnpack = 0.85, kWaitStartup = 1.15;
constexpr double kComplete = 1.6;                       // CompleteLoading: "Complete!", then the logo pops
constexpr double kFinalScale = kComplete + 0.15;
constexpr double kFadeStart = kFinalScale + 0.45, kFadeEnd = kFadeStart + 0.35;

constexpr float kLogo = 148.f;

TextStyle Centered(TextStyle t) {
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    return t;
}

// Sparks drifting up the screen, each on its own column, speed and phase
void Sparks(UI::Ui& ui, const Rect& screen, Color color, float t) {
    for (int i = 0; i < 26; ++i) {
        const float seed = static_cast<float>(i) * 12.9898f;
        const float fx = std::fmod(std::sin(seed) * 43758.5453f, 1.f);
        const float speed = 0.04f + 0.05f * std::fmod(std::fabs(std::cos(seed)) * 9.1f, 1.f);
        const float k = std::fmod(t * speed + static_cast<float>(i) * 0.137f, 1.f);
        const float x = screen.x + screen.w * std::fabs(fx) + 18.f * std::sin(t * 0.8f + seed);
        const float y = screen.B() - (screen.h + 40.f) * k;
        const float size = 1.5f + 2.f * std::fabs(std::sin(seed * 3.f));
        const float alpha = 0.5f * std::sin(k * kPi);
        ui.g.FillEllipse(x, y, size, size, color.Fade(alpha));
    }
}

} // namespace

void LoadingOverlay::Advance(double now) {
    const bool unpacked = Services::ResourceExtractor::Ready();
    const bool done = main.StartupDone();
    if (lastNow < 0.0) lastNow = now;
    const double dt = now - lastNow;
    lastNow = now;
    double limit = 1e9;
    if (!unpacked) limit = kWaitUnpack;
    else if (!done) limit = kWaitStartup;
    elapsed = std::min(elapsed + dt, std::max(elapsed, limit));
    while (step < kStepCount && elapsed >= kSteps[step].at) {
        targetProgress = kSteps[step].progress;
        ++step;
        stepAt = now;
    }
    // Extracting: the bar follows the unpacking from 40 to 60
    if (!unpacked && step >= 3) targetProgress = std::max(targetProgress, 40.0 + 20.0 * Services::ResourceExtractor::Progress());
    // _progressTimer: every 16 ms the bar closes 10% of the remaining distance
    while (now - progressClock >= 0.016) {
        progressClock += 0.016;
        if (currentProgress < targetProgress) {
            currentProgress += (targetProgress - currentProgress) * 0.1;
            if (targetProgress - currentProgress < 0.5) currentProgress = targetProgress;
        }
    }
}

void LoadingOverlay::Draw(UI::Ui& ui, const Rect& screen) {
    if (!visible) return;
    const double now = ui.Time();
    if (startedAt < 0.0) {
        startedAt = now;
        progressClock = now;
    }
    Advance(now);
    const double t = elapsed;
    if (t >= kFadeEnd) {
        // fadeOut.Completed: Close()
        visible = false;
        main.LoadingFinished();
        return;
    }
    fading = t > kFadeStart;
    ui.KeepAnimating();
    const float ft = static_cast<float>(now - startedAt);
    const float opacity = fading ? 1.f - static_cast<float>((t - kFadeStart) / (kFadeEnd - kFadeStart)) : 1.f;
    if (opacity < 1.f) ui.g.PushOpacity(opacity);
    ui.Block(screen);

    // Background: the window's gradient, two wandering glows, sparks
    ui.g.FillLinear(screen, 0.f, { { 0.f, Kit::Palette::BgTop }, { 1.f, Kit::Palette::BgBottom } }, { 0.f, 0.f }, { 1.f, 1.f });
    const float g1x = screen.x + screen.w * (0.3f + 0.12f * std::sin(ft * 0.35f)), g1y = screen.y + screen.h * (0.35f + 0.08f * std::cos(ft * 0.3f));
    ui.g.FillRadial({ g1x - 420.f, g1y - 320.f, 840.f, 640.f }, { { 0.f, Kit::Palette::AccentLight.Fade(0.16f) }, { 1.f, Kit::Palette::AccentLight.Fade(0.f) } });
    const float g2x = screen.x + screen.w * (0.72f + 0.1f * std::cos(ft * 0.27f)), g2y = screen.y + screen.h * (0.68f + 0.07f * std::sin(ft * 0.4f));
    ui.g.FillRadial({ g2x - 360.f, g2y - 280.f, 720.f, 560.f }, { { 0.f, Kit::Palette::AccentDark.Fade(0.16f) }, { 1.f, Kit::Palette::AccentDark.Fade(0.f) } });
    Sparks(ui, screen, Lerp(Kit::Palette::AccentLight, Colors::White, 0.3f), ft);

    // The logo, the name, the bar and the status, centred, rising in
    const float titleH = 36.f, statusH = 22.f, orbit = kLogo * 0.5f + 26.f;
    const float contentH = orbit * 2.f + 16.f + titleH + 14.f + 10.f + 14.f + statusH;
    const float rise = Anim::EaseOutBack(Anim::Clamp01(ft / 0.7f), 0.8f);
    float y = screen.CY() - contentH * 0.5f;
    ui.g.PushOpacity(Anim::Clamp01(ft / 0.4f));
    ui.PushTranslate(0.f, 30.f * (1.f - rise));

    // The logo: breathing inside its arcs; at the end the arcs close into a ring that bursts out and
    // the logo pops
    {
        const float cx = screen.CX(), cy = y + orbit;
        const float pop = Anim::Clamp01(static_cast<float>((t - kFinalScale) / 0.4));
        const float close = Anim::Clamp01(static_cast<float>((t - kComplete) / 0.2));
        const float breathe = 1.f + 0.04f * std::sin(ft * 2.6f);
        const float scale = t >= kFinalScale ? 1.f + 0.16f * Anim::EaseOutBack(pop, 1.4f) : breathe;
        const Color light = Kit::Palette::AccentLight;
        ui.g.FillRadial({ cx - orbit * 1.9f, cy - orbit * 1.9f, orbit * 3.8f, orbit * 3.8f }, { { 0.f, light.Fade(0.26f + 0.2f * pop) }, { 1.f, light.Fade(0.f) } });
        for (int i = 0; i < 3; ++i) {
            const float sweep = Anim::Lerp(70.f, 119.9f, close);
            Kit::Arc(ui, cx, cy, orbit, ft * 150.f + 120.f * static_cast<float>(i), sweep, light.Fade(0.9f - 0.2f * i), 3.f);
        }
        for (int i = 0; i < 2; ++i) Kit::Arc(ui, cx, cy, orbit - 9.f, -ft * 90.f + 180.f * static_cast<float>(i), 60.f, Kit::Palette::AccentDark.Fade(0.8f), 2.f);
        if (pop > 0.f && pop < 1.f) ui.g.StrokeEllipse(cx, cy, orbit + 70.f * pop, orbit + 70.f * pop, light.Fade(0.7f * (1.f - pop)), 2.5f);
        const Rect logo{ cx - kLogo * 0.5f, cy - kLogo * 0.5f, kLogo, kLogo };
        ui.PushScale(scale, cx, cy);
        ui.g.DrawImage(Images::Load(Paths::Asset(Settings::IconTheme::Current().LogoAsset()), 256), logo);
        ui.PopTransform();
        y += orbit * 2.f + 16.f;
    }

    // "Custom Cursor Manager"
    ui.g.Text(L"Custom Cursor Manager", { screen.x, y, screen.w, titleH }, Colors::White, Centered(Kit::Style(26.f, DWRITE_FONT_WEIGHT_BOLD)));
    y += titleH + 14.f;

    // Progress: the accent, with its glow and a shimmer
    {
        const float barW = std::min(420.f, screen.w - 80.f);
        Kit::ProgressBar(ui, { screen.CX() - barW * 0.5f, y, barW, 10.f }, static_cast<float>(currentProgress / 100.0));
        y += 10.f + 14.f;
    }

    // The status: the step reached, or "Complete!"
    {
        const char* key = t >= kComplete ? "loading_step_complete" : step == 0 ? "loading_initializing" : kSteps[step - 1].status;
        const double changed = t >= kComplete ? now - (t - kComplete) : stepAt;
        M::Motion::Enter enter(ui, Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((now - changed) / 0.3))), 0.f, 8.f);
        ui.g.Text(Loc::Get(key), { screen.x, y, screen.w, statusH }, Argb(0xAAFFFFFF), Centered(Kit::Style(14.f)));
    }
    ui.PopTransform();
    ui.g.PopLayer();
    if (opacity < 1.f) ui.g.PopLayer();
}

} // namespace CustomCursor::Main
