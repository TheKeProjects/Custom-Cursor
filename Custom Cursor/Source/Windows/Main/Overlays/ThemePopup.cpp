#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HeaderPopups.h"

namespace CustomCursor::Main {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;

constexpr float kPadding = 10.f, kHeadingH = 22.f, kRowH = 42.f, kRowGap = 2.f, kWidth = 214.f;

struct Look {
    const wchar_t* theme;
    const wchar_t* logo;
    const char* label;
};
// "default" follows the season baked into the build (the .vcxproj's <ChristmasMode>...): its logo is
// filled in from that season's row
constexpr Look kLooks[] = {
    { L"default", nullptr, "theme_default" },
    { L"classic", L"Logo\\logo.png", "theme_classic" },
    { L"halloween", L"Logo\\logo Halloween.png", "theme_halloween" },
    { L"summer", L"Logo\\logo Summer.png", "theme_summer" },
    { L"christmas", L"Logo\\logo Christmas.png", "theme_christmas" },
    { L"lgbtq", L"Logo\\logo LGBTQ+.png", "theme_lgbtq" },
};

const Look& LookOf(const std::wstring& theme) {
    for (const Look& look : kLooks)
        if (look.logo && theme == look.theme) return look;
    return kLooks[1];   // classic
}

} // namespace

Color ThemePopup::Fill() const { return Kit::Palette::Surface; }

Color ThemePopup::Edge() const { return Kit::Palette::AccentLight.Fade(0.7f); }

float ThemePopup::Width(UI::Ui&) { return kWidth; }

float ThemePopup::Height(UI::Ui&, float) {
    const float n = static_cast<float>(std::size(kLooks));
    return kPadding * 2.f + kHeadingH + n * kRowH + (n - 1.f) * kRowGap + 4.f;
}

void ThemePopup::DrawContent(UI::Ui& ui, const Rect& box) {
    const Rect inner = box.Inset(kPadding);
    {
        M::Motion::Enter enter(ui, RowReveal(ui, 0), 0.f, 16.f);
        ui.g.Text(Text::Upper(Loc::Get("theme_title")), { inner.x + 2.f, inner.y, inner.w, 16.f }, Kit::Palette::AccentLight, Kit::Style(11.f, DWRITE_FONT_WEIGHT_BOLD));
    }
    // The stored choice, not the effective one: "default" and the season it resolves to are different rows
    const std::wstring current = Settings::IconTheme::Current().theme;
    const Look& buildLook = LookOf(AppVersion::BuildDefaultTheme());
    float y = inner.y + kHeadingH;
    int index = 0;
    for (const Look& look : kLooks) {
        const Rect r{ inner.x, y, inner.w, kRowH };
        y += kRowH + kRowGap;
        M::Motion::Enter enter(ui, RowReveal(ui, ++index), 0.f, 20.f);
        const UI::Id id = ui.MakeId("theme", index);
        const UI::Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const bool selected = current == look.theme;
        if (selected) ui.g.FillRound(r, 9.f, Kit::Palette::AccentLight.Fade(0.14f));
        else if (fb.hover > 0.f) ui.g.FillRound(r, 9.f, Argb(0x14FFFFFF).Fade(fb.hover));

        // The logo hops a little under the mouse
        const Rect logo{ r.x + 6.f, r.CY() - 16.f, 32.f, 32.f };
        ui.PushScale(1.f + 0.14f * Anim::EaseOutBack(fb.hover, 0.9f) - 0.08f * fb.press, logo.CX(), logo.CY());
        const bool isDefault = look.logo == nullptr;
        ui.g.DrawImage(Images::Load(Paths::Asset(isDefault ? buildLook.logo : look.logo), 256), logo);
        ui.PopTransform();

        TextStyle name = Kit::Style(12.5f, selected ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        const Color nameColor = selected ? Kit::Palette::TextPrimary : Lerp(Kit::Palette::TextSecondary, Kit::Palette::TextPrimary, fb.hover);
        const Rect text{ logo.R() + 10.f, r.y, r.R() - logo.R() - 36.f, r.h };
        if (isDefault) {
            // Two lines: "Default" and, under it, the season the build follows
            ui.g.Text(Loc::Get(look.label), { text.x, r.y + 4.f, text.w, 18.f }, nameColor, name);
            TextStyle hint = Kit::Style(10.5f, DWRITE_FONT_WEIGHT_NORMAL);
            hint.valign = VAlign::Center;
            hint.ellipsis = true;
            ui.g.Text(Loc::Get(buildLook.label), { text.x, r.y + 21.f, text.w, 16.f }, Kit::Palette::TextSecondary.Fade(0.8f), hint);
        } else {
            ui.g.Text(Loc::Get(look.label), text, nameColor, name);
        }
        if (selected) M::DrawIcon(ui.g, M::Icon::Check, r.R() - 16.f, r.CY(), 16.f, Kit::Palette::AccentLight);
        if (it.clicked) {
            if (!selected) main.ChangeTheme(look.theme);
            Close();
        }
    }
}

} // namespace CustomCursor::Main
