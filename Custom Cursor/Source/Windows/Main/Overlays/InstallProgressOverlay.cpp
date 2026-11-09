#include "Windows/Main/Overlays/InstallProgressOverlay.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/CursorKit.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
} // namespace

void InstallProgressOverlay::Show(int count) {
    current = 0;
    total = count;
    name.clear();
    onClosed = nullptr;
    Open();
}

void InstallProgressOverlay::Update(int done, int count, const std::wstring& schemeName) {
    current = done;
    total = count;
    name = schemeName;
}

float InstallProgressOverlay::CardHeight(UI::Ui&, float) { return kPad + 24.f + 10.f + 10.f + 10.f + 40.f + kPad; }

void InstallProgressOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    TextStyle label = Kit::Style(14.f);
    label.align = HAlign::Center;
    label.valign = VAlign::Center;
    {
        // progressLabel: "Installing cursors... (current/total)"
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        ui.g.Text(Loc::Fmt("install_progress", static_cast<long long>(current), static_cast<long long>(total)), { inner.x, inner.y, inner.w, 24.f },
                  Kit::Palette::TextPrimary, label);
    }
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const float shown = ui.Animate(ui.MakeId("install-bar"), total > 0 ? static_cast<float>(current) / static_cast<float>(total) : 0.f, 10.f);
        ui.KeepAnimating();
        Kit::ProgressBar(ui, { inner.x, inner.y + 34.f, inner.w, 10.f }, shown);
    }
    // currentLabel: the scheme being installed (12, secondary, wrapping)
    M::Motion::Enter enter(ui, Reveal(ui, 2));
    const TextStyle nameStyle = Kit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    ui.g.Text(name, { inner.x, inner.y + 54.f, inner.w, 40.f }, Kit::Palette::TextSecondary, nameStyle);
}

} // namespace CustomCursor::Main
