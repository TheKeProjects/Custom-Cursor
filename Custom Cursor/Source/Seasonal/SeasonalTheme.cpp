#include "Seasonal/SeasonalTheme.h"
#include <algorithm>
#include "Seasonal/SeasonalArt.h"
#include "Settings/IconThemeSettings.h"

namespace CustomCursor::Seasonal {

namespace {

Season Resolve() {
    const std::wstring t = Settings::IconTheme::Current().EffectiveTheme();
    if (t == L"halloween") return Season::Halloween;
    if (t == L"summer") return Season::Summer;
    if (t == L"christmas") return Season::Christmas;
    return Season::Classic;
}

Season active = Season::Classic;
bool resolved = false;
bool dark = true;   // ThemeService starts dark
unsigned revision = 0;

double clockReal = -1.0, clockTime = 0.0;
float clockRamp = 1.f;   // 1 = full speed, 0 = stopped
bool gameMode = false;
float hideRamp = 0.f;    // 0 = critters in place, 1 = all gone (linear)

} // namespace

Season Active() {
    if (!resolved) {
        active = Resolve();
        resolved = true;
    }
    return active;
}

bool IsSeasonal() { return Active() != Season::Classic; }

std::wstring Name(Season season) {
    switch (season) {
        case Season::Halloween: return L"halloween";
        case Season::Summer: return L"summer";
        case Season::Christmas: return L"christmas";
        default: return L"classic";
    }
}

void Refresh() {
    const Season season = Resolve();
    resolved = true;
    if (season == active) return;
    active = season;
    ++revision;
}

void SetDarkMode(bool on) {
    Art::SetLightMode(!on);
    if (on == dark) return;
    dark = on;
    ++revision;
}

bool DarkMode() { return dark; }

unsigned Revision() { return revision; }

double AnimationTime(double realTime, bool paused) {
    constexpr float kRampSeconds = 1.6f, kFleeSeconds = 1.4f;
    const float dt = clockReal < 0.0 ? 0.f : std::clamp(static_cast<float>(realTime - clockReal), 0.f, 0.1f);
    clockReal = realTime;
    // Game mode: the critters flee first (the clock still running), and only once they are all gone
    // does the clock wind down. Leaving it, the clock speeds up while they fly back in.
    hideRamp = std::clamp(hideRamp + (gameMode ? dt : -dt) / kFleeSeconds, 0.f, 1.f);
    paused = paused || (gameMode && hideRamp >= 1.f);
    clockRamp = std::clamp(clockRamp + (paused ? -dt : dt) / kRampSeconds, 0.f, 1.f);
    // Smoothstep on the speed: eases out of full speed and settles gently at the stop (and back)
    const float speed = clockRamp * clockRamp * (3.f - 2.f * clockRamp);
    clockTime += dt * speed;
    return clockTime;
}

double LastAnimationTime() { return clockTime; }
bool AnimationStopped() { return clockRamp <= 0.f; }

void SetGameMode(bool on) { gameMode = on; }
bool GameMode() { return gameMode; }
float HideProgress() { return hideRamp * hideRamp; }

} // namespace CustomCursor::Seasonal
