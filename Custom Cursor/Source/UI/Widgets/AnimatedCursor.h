#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Graphics/Rect.h"
#include "UI/Ui.h"

// Controls\AnimatedCursorControl.cs (and CursorImageExtractor's still cursors), drawn with Direct2D:
//   .ani      its frames (at most 10, evenly skipped, each shrunk to 32 x 32 at most), one every
//             150 ms whatever the file's own rate (App\Config.h: CursorFrameMs / CursorMaxFrames /
//             CursorMaxSize / CursorFullAnimation); an .ani that can't be read shows its first frame
//   .cur/.ico the image at its own size (ExtractCursorPreview)
//   .gif/.png/.jpg/.jpeg  the scheme's preview picture (a GIF plays its frames)
// fitted inside the rectangle (Stretch="Uniform"). Files are decoded on worker threads (Preload, or
// the first Draw) and show once ready, so drawing never waits on a file. Every animation runs off one
// shared timer, which stops when nothing animated has been drawn for a moment. Decoded frames are
// kept per `group` until Release(group) (BackFromPreviewButton_Click's Cleanup).
namespace CustomCursor::UI::AnimatedCursor {

// Decodes `paths` in the background (a few worker threads); `done` runs on the UI thread once every
// one is ready (straight away when they all are)
void Preload(const std::vector<std::wstring>& paths, const char* group, std::function<void()> done = {});
// Draws the file; true while it's still being decoded too (nothing drawn yet), false when it can't be
// shown (the caller draws its placeholder instead)
bool Draw(Ui& ui, const std::wstring& path, const Rect& r, float opacity = 1.f, const char* group = "cards");
// Stops and frees everything drawn under `group`
void Release(const char* group);
// Decoration that moves on its own (the banners, the logo, the seasonal scenery) asks for this every
// frame it draws: the shared timer then ticks at about 30 fps, stopping when nobody asks any more
void RequestAmbientTicks();
// While the window isn't the active one the decoration stands still (nothing asks for those ticks)
void SetAmbientPaused(bool paused);
// Called on every tick of the shared timer (the window redraws)
void SetTickHandler(std::function<void()> onTick);

} // namespace CustomCursor::UI::AnimatedCursor
