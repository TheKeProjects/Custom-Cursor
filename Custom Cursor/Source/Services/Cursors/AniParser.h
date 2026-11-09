#pragma once
#include <optional>
#include <string>
#include <vector>

// AnimatedCursorControl.ParseAniFile: the RIFF / ACON structure of an .ani, read exactly as the C#
// read it. Each "icon" chunk of the LIST "fram" is a complete .cur file (Graphics/CursorImage decodes it).
namespace CustomCursor::Services::Cursors::AniParser {

struct AniData {
    int frameCount = 0;            // anih's frame count (0 without an anih chunk)
    int defaultRateMs = 60;        // anih's display rate (1/60 s units) in milliseconds
    std::vector<std::string> frames;   // the "icon" chunks' bytes, in file order
};

// An empty AniData for a file that isn't RIFF / ACON; nullopt when the structure runs past the end
// (the C#'s BinaryReader threw and the cursor fell back to its first frame)
std::optional<AniData> Parse(const std::string& bytes);

} // namespace CustomCursor::Services::Cursors::AniParser
