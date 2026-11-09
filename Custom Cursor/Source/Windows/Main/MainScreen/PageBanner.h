#pragma once
#include <string>
#include "Graphics/Rect.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

namespace CustomCursor::Main {

inline constexpr float kPageBannerHeight = 92.f;

struct PageBannerSpec {
    enum class Motion { Float, Wobble, Spin, Bob };
    UI::Material::Icon icon = UI::Material::Icon::CursorPointer;
    Motion motion = Motion::Float;
    std::wstring title, subtitle;
    std::wstring pill;                                        // on the right (empty: none)
    UI::Material::Icon pillIcon = UI::Material::Icon::Star;
};

// The banner on top of every page (the Inazuma Eleven VR Save Editor's): the accent gradient drifting,
// sparks rising, a light crossing it now and then, the emblem inside two turning arcs, the title
// dropping in letter by letter when `shownAt` is recent, the subtitle and a pill. It moves on the
// low-rate ambient ticks, not on the refresh rate.
void PageBanner(UI::Ui& ui, const Rect& r, const PageBannerSpec& spec, double shownAt);

} // namespace CustomCursor::Main
