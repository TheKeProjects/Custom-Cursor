#include "Windows/Main/Preview/PreviewTransition.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Preview/PreviewView.h"
#include "Windows/Main/Schemes/SchemesView.h"

namespace CustomCursor::Main {

namespace {
namespace Kit = CursorKit;
constexpr double kMove = 0.5, kHold = 0.3, kFade = 0.3;
constexpr float kSize = 200.f;
} // namespace

void PreviewTransition::Start(const Models::CursorScheme& target, const Rect& card) {
    if (scheme) return;
    scheme = target;
    from = card;
    PreviewView::PreloadCursors(target);
    startedAt = -1.0;
    shown = false;
}

void PreviewTransition::Draw(UI::Ui& ui, const Rect& screen) {
    if (!scheme) return;
    const double now = ui.Time();
    if (startedAt < 0.0) startedAt = now;
    const double t = now - startedAt;
    ui.KeepAnimating();
    ui.Block(screen);

    // Phase 3: the preview is there; the dark layer fades away over it
    if (t >= kMove + kHold && !shown) {
        shown = true;
        main.ShowPreview(*scheme);
    }
    const float out = t > kMove + kHold ? Anim::Clamp01(static_cast<float>((t - kMove - kHold) / kFade)) : 0.f;
    if (out >= 1.f) {
        scheme.reset();
        return;
    }

    // Phase 1: Background black 0 -> 1 (the page fading away), the card to the middle, scale 1 -> 2
    const float k = Anim::EaseInOutCubic(Anim::Clamp01(static_cast<float>(t / kMove)));
    const float dark = Anim::Clamp01(static_cast<float>(t / kMove));
    ui.g.PushOpacity(1.f - out);
    ui.g.FillRect(screen, Colors::Black.Fade(dark));
    // Canvas.SetLeft(card.x), SetTop(card.y + 10): the 200 x 200 card over the image part
    const float startX = from.x, startY = from.y + 10.f;
    const float endX = screen.CX() - kSize * 0.5f, endY = screen.CY() - kSize * 0.5f;
    const Rect box{ Anim::Lerp(startX, endX, k), Anim::Lerp(startY, endY, k), kSize, kSize };
    const float scale = Anim::Lerp(1.f, 2.f, k);
    ui.PushScale(scale, box.CX(), box.CY());
    // DropShadowEffect Color #88DDFF, BlurRadius 0 -> 100: a wide glow fading in (one blur, its
    // opacity growing, rather than a new blur every frame)
    if (k > 0.f) ui.g.GlowRound(box, 12.f, Kit::Palette::AccentLight, 60.f, k);
    ui.g.FillRound(box, 12.f, Kit::Palette::Card);
    // The card's preview (the VisualBrush of its image part), Padding 10, Stretch Uniform
    const Rect image = box.Inset(10.f);
    const float fit = std::min(image.w / 174.f, image.h / 85.f);
    SchemesView::DrawPreview(ui, main.State(), *scheme, image.Center(174.f * fit, 85.f * fit));
    ui.PopTransform();
    ui.g.PopLayer();
}

} // namespace CustomCursor::Main
