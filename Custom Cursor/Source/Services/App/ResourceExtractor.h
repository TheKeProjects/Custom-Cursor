#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

// Utilities\ResourceExtractor.cs: unpacks what the exe embeds to %TEMP%\CustomCursor. Unchanged files
// of a previous run of the same build are kept; a different build starts from an empty folder (the
// C# never refreshed it, so the schemes a new version added didn't show up).
namespace CustomCursor::Services::ResourceExtractor {

// Startup in two parts, so the window and its loading screen show at once: ExtractLight unpacks the
// small folders (Assets, Translations) right away, ExtractHeavyInBackground the schemes and the
// tutorial (Cursores, Tutorial: ~20 MB) on a worker thread, then runs `done` on the UI thread.
void ExtractLight();
void ExtractHeavyInBackground(std::function<void()> done);
// On exit: stops the background part after the file it is writing and waits for it (at most 5 s)
void StopBackground();
// True once every asset is on disk; Progress is 0..1 of the background part
bool Ready();
float Progress();

// Bytes of one embedded asset ("Assets", "Flags\\gb.png"), without extracting anything.
std::optional<std::string> ReadEmbedded(const std::wstring& root, const std::wstring& path);

} // namespace CustomCursor::Services::ResourceExtractor
