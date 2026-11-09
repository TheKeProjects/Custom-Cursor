#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// .cur / .ico decoding in memory (the C# loaded them with LoadImage / CreateIconFromResourceEx and
// CreateBitmapSourceFromHIcon, writing each .ani frame to a temporary file when the in-memory call
// refused it). Worker-thread friendly: no GPU objects here (Images::FromPixels makes the bitmap).
namespace CustomCursor::CursorImage {

// Premultiplied BGRA pixels (0xAARRGGBB, B G R A in memory), top row first
struct Bitmap {
    unsigned width = 0, height = 0;
    std::vector<uint32_t> pixels;
    bool Empty() const { return pixels.empty(); }
};

// A .cur or .ico file: the biggest of its images. PNG images (Vista-style) go through WIC; bitmaps
// (1, 4, 8, 24 or 32 bpp) use their alpha when they have one, else the AND mask. A pixel the mask
// marks as inverted (the classic black-and-white cursors) is drawn white, as it shows on a dark card.
std::optional<Bitmap> Decode(const std::string& bytes);
std::optional<Bitmap> DecodeFile(const std::wstring& path);

// BitmapFromHCursor's size cap: a bigger bitmap shrunk to fit `maxSide` (keeping its shape), averaging
// the pixels it covers; a smaller one comes back as it is
Bitmap Downscale(const Bitmap& bitmap, unsigned maxSide);

} // namespace CustomCursor::CursorImage
