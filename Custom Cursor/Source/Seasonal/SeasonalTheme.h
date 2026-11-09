#pragma once
#include <string>

// ThemeService.ActiveSeason / SeasonChanged for the seasonal art. The colors of each season live in
// UI/Material/Theme (the MaterialDesign palettes); this side only knows which season is drawn.
namespace CustomCursor::Seasonal {

enum class Season { Classic, Halloween, Summer, Christmas };

// The season follows the icon theme setting; themes without visuals use the classic look.
Season Active();
bool IsSeasonal();
std::wstring Name(Season season);   // "classic" | "halloween" | "summer" | "christmas"
void Refresh();                     // re-read after the icon theme changes
// Light / dark mode of the app (set by UI::Material::Theme). The night scenery is only drawn in
// dark mode, and the art switches some pens to dark ones on light backgrounds (SyncArt).
void SetDarkMode(bool dark);
bool DarkMode();
// Increments when the season or the mode changes (ThemeService.SeasonChanged)
unsigned Revision();

// Clock of the seasonal animations (scenery, decorations, particles). While `paused` it slows down
// smoothly to a stop over ~1.6 s, and speeds back up the same way.
double AnimationTime(double realTime, bool paused);
double LastAnimationTime();   // value returned by the last AnimationTime call
bool AnimationStopped();      // fully stopped: nothing seasonal moves, no need to redraw for it

// Game mode (the editor is attached to the game): the critters (bats, ghosts, spiders, seagulls,
// crabs...) flee off screen, then the clock winds down to a stop so the editor leaves the GPU / CPU
// to the game. When it ends they fly back in and everything moves again.
void SetGameMode(bool on);
bool GameMode();
// 0 = every critter in place, 1 = all gone (eased: they speed up as they flee). Advances with
// AnimationTime.
float HideProgress();

} // namespace CustomCursor::Seasonal
