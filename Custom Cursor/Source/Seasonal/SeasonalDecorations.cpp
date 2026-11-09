#include "Seasonal/SeasonalDecorations.h"
#include <algorithm>
#include "Seasonal/SeasonalArt.h"
#include "Seasonal/SeasonalTheme.h"

namespace CustomCursor::Seasonal::Decorations {

using namespace Art;

namespace {

// Game mode: each critter heads for the nearest side edge of what it sits on (see Art::Flee)
float Side(float x, float w) { return x < w * 0.5f ? -1.f : 1.f; }

void FleeingBat(Graphics& g, float x, float y, float scale, float w, float distance) {
    Flee(g, Side(x, w), -0.5f, distance, [&] { Art::Halloween::DrawBat(g, x, y, scale); });
}
void FleeingGhost(Graphics& g, float x, float y, float scale, float w, float distance) {
    Flee(g, Side(x, w), -0.4f, distance, [&] { Art::Halloween::DrawGhost(g, x, y, scale); });
}
void FleeingSeagull(Graphics& g, float x, float y, float scale, float w, float distance) {
    Flee(g, Side(x, w), -0.6f, distance, [&] { Art::Summer::DrawSeagull(g, x, y, scale); });
}
void FleeingCrab(Graphics& g, float x, float y, float scale, float w, float distance) {
    Flee(g, Side(x, w), 0.f, distance, [&] { Art::Summer::DrawCrab(g, x, y, scale); });
}
// The gingerbread men run off too
void FleeingGingerbread(Graphics& g, float x, float y, float scale, float angle, float w, float distance) {
    Flee(g, Side(x, w), 0.f, distance, [&] { Art::Christmas::DrawGingerbread(g, x, y, scale, angle); });
}
// A spider climbs back up its thread, then slips out of sight over the edge
void FleeingSpider(Graphics& g, float x, float length, float scale) {
    const float e = HideProgress();
    Flee(g, 0.f, -1.f, 30.f * scale, [&] { Art::Halloween::DrawSpider(g, x, 0, std::max(2.f * scale, length * (1.f - e)), scale); });
}

// HalloweenArt.DrawCard
void HalloweenCard(Graphics& g, float w, float h, float s, int variant) {
    using namespace Art::Halloween;
    if (w >= 500.f) {
        // Wide cards (headers, editor panels): richer scene along the edges
        DrawCobweb(g, 0, 0, s * 1.1f, false, false);
        FleeingSpider(g, w - 46 * s, 20 * s, s);
        FleeingSpider(g, w - 90 * s, 34 * s, s * 0.85f);
        DrawPumpkin(g, w - 20 * s, h - 14 * s, s);
        DrawPumpkin(g, w - 44 * s, h - 11 * s, s * 0.75f);
        DrawTree(g, 22 * s, h - 2, s * 0.9f);
        return;
    }
    switch (variant % 6) {
        case 0:
            DrawWitchHat(g, 18 * s, 2, s, -18);
            DrawCobweb(g, w, 0, s * 0.9f, true, false);
            break;
        case 1:
            DrawPumpkin(g, w - 17 * s, h - 14 * s, s);
            FleeingBat(g, 22 * s, 16 * s, s * 0.9f, w, 220 * s);
            break;
        case 2:
            FleeingSpider(g, w - 22 * s, 24 * s, s);
            DrawTree(g, 18 * s, h - 2, s * 0.85f);
            break;
        case 3:
            FleeingGhost(g, w - 18 * s, 18 * s, s, w, 200 * s);
            DrawPumpkin(g, 18 * s, h - 14 * s, s * 0.9f);
            break;
        case 4:
            DrawCobweb(g, 0, 0, s * 0.9f, false, false);
            DrawWitchHat(g, w - 18 * s, 2, s, 18);
            break;
        default:
            FleeingBat(g, w - 22 * s, 16 * s, s * 0.9f, w, 220 * s);
            DrawTree(g, w - 18 * s, h - 2, s * 0.85f);
            DrawPumpkin(g, 16 * s, h - 12 * s, s * 0.75f);
            break;
    }
}

// SummerArt.DrawCard
void SummerCard(Graphics& g, float w, float h, float s, int variant) {
    using namespace Art::Summer;
    if (w >= 500.f) {
        DrawSun(g, w - 34 * s, 30 * s, s * 0.9f);
        FleeingSeagull(g, w - 80 * s, 20 * s, s * 0.8f, w, 240 * s);
        FleeingSeagull(g, w - 100 * s, 30 * s, s * 0.6f, w, 240 * s);
        DrawWaves(g, 4, w - 4, h - 3, s * 0.8f);
        DrawPalm(g, 20 * s, h - 6, s * 0.9f);
        FleeingCrab(g, w - 26 * s, h - 12 * s, s * 0.9f, w, 160 * s);
        return;
    }
    switch (variant % 6) {
        case 0:
            DrawSun(g, 4, 4, s * 0.8f);
            DrawStarfish(g, w - 18 * s, h - 16 * s, s * 1.3f, 15);
            break;
        case 1:
            DrawBeachBall(g, w - 18 * s, h - 18 * s, s * 1.2f);
            FleeingSeagull(g, 24 * s, 18 * s, s * 1.1f, w, 240 * s);
            break;
        case 2:
            DrawPalm(g, 18 * s, h - 3, s * 0.85f);
            DrawShell(g, w - 18 * s, 18 * s, s * 1.3f, -20);
            break;
        case 3:
            DrawIceCream(g, w - 18 * s, 26 * s, s * 1.2f);
            FleeingCrab(g, 22 * s, h - 11 * s, s * 1.1f, w, 160 * s);
            break;
        case 4:
            DrawUmbrella(g, w - 20 * s, h - 4, s * 0.85f, 10);
            DrawStarfish(g, 18 * s, 18 * s, s * 1.1f, -10);
            break;
        default:
            DrawWaves(g, 3, w - 3, h - 3, s * 0.7f);
            FleeingSeagull(g, w - 26 * s, 18 * s, s * 1.1f, w, 240 * s);
            DrawShell(g, 18 * s, h - 22 * s, s * 1.1f, 15);
            break;
    }
}

// ChristmasArt.DrawCard
void ChristmasCard(Graphics& g, float w, float h, float s, int variant) {
    using namespace Art::Christmas;
    constexpr Color Green = Rgba(46, 125, 50);
    if (w >= 500.f) {
        DrawLights(g, 10, w - 10, 1, s * 0.9f);
        DrawTree(g, 22 * s, h - 5, s * 1.1f);
        DrawGift(g, 46 * s, h - 3, s * 0.8f, OrnRed);
        DrawGift(g, w - 22 * s, h - 3, s * 0.9f, OrnBlue);
        DrawSnowman(g, w - 48 * s, h - 3, s * 0.9f);
        return;
    }
    switch (variant % 6) {
        case 0:
            DrawSantaHat(g, 18 * s, 3, s, -18);
            DrawHolly(g, w - 16 * s, h - 14 * s, s * 1.1f, 0);
            break;
        case 1:
            DrawOrnament(g, w - 22 * s, 0, 16 * s, s, OrnRed);
            DrawOrnament(g, w - 40 * s, 0, 8 * s, s * 0.8f, OrnGold);
            DrawGift(g, 18 * s, h - 3, s, Green);
            break;
        case 2:
            DrawTree(g, 18 * s, h - 5, s * 0.9f);
            DrawSnowflake(g, w - 18 * s, 18 * s, s * 1.1f);
            break;
        case 3:
            DrawCandyCane(g, w - 20 * s, 22 * s, s * 0.9f, 15);
            DrawSnowman(g, 18 * s, h - 3, s * 0.9f);
            break;
        case 4:
            DrawHolly(g, 16 * s, 14 * s, s * 1.1f, 180);
            DrawSantaHat(g, w - 18 * s, 3, s, 18);
            FleeingGingerbread(g, w - 18 * s, h - 18 * s, s * 1.1f, 8, w, 160 * s);
            break;
        default:
            DrawGift(g, w - 18 * s, h - 3, s, OrnPurple);
            FleeingGingerbread(g, 18 * s, 20 * s, s * 1.1f, -8, w, 160 * s);
            DrawSnowflake(g, w - 20 * s, 18 * s, s * 0.9f);
            break;
    }
}

} // namespace

void Card(Graphics& g, const Rect& card, int variant, double time, float opacity) {
    const Season season = Active();
    const float w = card.w, h = card.h;
    if (season == Season::Classic || w < 60.f || h < 40.f || opacity <= 0.f) return;
    SetClock(time);
    const float s = std::clamp(std::min(w, h) / 115.f, 1.f, 1.5f);
    NoColorFilter keep(g);
    if (opacity < 1.f) g.PushOpacity(opacity);
    {
        Push origin(g, Translate(card.x, card.y));
        switch (season) {
            case Season::Halloween: HalloweenCard(g, w, h, s, variant); break;
            case Season::Summer: SummerCard(g, w, h, s, variant); break;
            case Season::Christmas: ChristmasCard(g, w, h, s, variant); break;
            default: break;
        }
    }
    if (opacity < 1.f) g.PopLayer();
}

void Header(Graphics& g, const Rect& header, double time) {
    const Season season = Active();
    const float w = header.w, h = header.h;
    if (season == Season::Classic || w < 200.f) return;
    SetClock(time);
    NoColorFilter keep(g);
    Push origin(g, Translate(header.x, header.y));
    const float cx = w * 0.5f, cy = h * 0.5f;
    switch (season) {
        case Season::Halloween:
            // Moon with bats in the middle, cobweb in the corner
            Halloween::DrawMoon(g, cx, cy, h * 0.28f);
            FleeingBat(g, cx - h * 0.75f, cy - h * 0.12f, 1.1f, w, w * 0.55f);
            FleeingBat(g, cx + h * 0.7f, cy + h * 0.05f, 0.9f, w, w * 0.55f);
            FleeingBat(g, cx + h * 0.35f, cy - h * 0.3f, 0.7f, w, w * 0.55f);
            Halloween::DrawCobweb(g, 0, 0, 0.9f, false, false);
            break;
        case Season::Summer:
            // Sun with seagulls and clouds
            Summer::DrawSun(g, cx, cy, h / 48);
            Summer::DrawCloud(g, cx - h * 0.9f, cy + h * 0.1f, h / 60);
            Summer::DrawCloud(g, cx + h * 0.95f, cy - h * 0.05f, h / 75);
            FleeingSeagull(g, cx - h * 0.45f, cy - h * 0.25f, 1.0f, w, w * 0.55f);
            FleeingSeagull(g, cx + h * 0.55f, cy + h * 0.28f, 0.8f, w, w * 0.55f);
            break;
        case Season::Christmas:
            // String lights along the bottom edge and snowflakes around the middle
            Christmas::DrawLights(g, 0, w, h - 14, 0.9f);
            Christmas::DrawSnowflake(g, cx, h * 0.42f, h / 40);
            Christmas::DrawSnowflake(g, cx - h * 0.8f, h * 0.3f, h / 70);
            Christmas::DrawSnowflake(g, cx + h * 0.75f, h * 0.35f, h / 60);
            Christmas::DrawSnowflake(g, cx + h * 0.35f, h * 0.15f, h / 110);
            Christmas::DrawSnowflake(g, cx - h * 0.4f, h * 0.6f, h / 110);
            break;
        default: break;
    }
}

} // namespace CustomCursor::Seasonal::Decorations
