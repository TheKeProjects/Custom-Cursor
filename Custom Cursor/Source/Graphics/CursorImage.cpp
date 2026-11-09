#include "Graphics/CursorImage.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "Core/Files.h"
#include "Graphics/Images.h"

namespace CustomCursor::CursorImage {

namespace {

uint16_t U16(const std::string& b, size_t at) { return static_cast<uint16_t>(static_cast<unsigned char>(b[at]) | static_cast<unsigned char>(b[at + 1]) << 8); }

uint32_t U32(const std::string& b, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}

uint32_t Premultiply(uint32_t a, uint32_t r, uint32_t g, uint32_t bl) {
    return a << 24 | (r * a + 127) / 255 << 16 | (g * a + 127) / 255 << 8 | (bl * a + 127) / 255;
}

// One image of the directory: a PNG stream, or a BITMAPINFOHEADER with the XOR bitmap and the AND mask
std::optional<Bitmap> DecodeImage(const std::string& file, size_t offset, size_t size) {
    if (offset >= file.size() || size < 8) return std::nullopt;
    size = std::min(size, file.size() - offset);
    const std::string image = file.substr(offset, size);
    if (static_cast<unsigned char>(image[0]) == 0x89 && image.compare(1, 3, "PNG") == 0) {
        Bitmap bmp;
        if (!Images::DecodeMemory(image, 4096, bmp.width, bmp.height, bmp.pixels)) return std::nullopt;
        return bmp;
    }
    if (image.size() < 40) return std::nullopt;
    const uint32_t headerSize = U32(image, 0);
    const int32_t width = static_cast<int32_t>(U32(image, 4));
    const int32_t fullHeight = static_cast<int32_t>(U32(image, 8));
    const unsigned bits = U16(image, 14);
    const uint32_t compression = U32(image, 16);
    uint32_t colorsUsed = U32(image, 32);
    if (width <= 0 || width > 1024 || fullHeight == 0 || compression != 0 || headerSize < 40 || headerSize > image.size()) return std::nullopt;
    const bool bottomUp = fullHeight > 0;
    const unsigned w = static_cast<unsigned>(width), h = static_cast<unsigned>(std::abs(fullHeight)) / 2;
    if (h == 0) return std::nullopt;

    size_t at = headerSize;
    std::vector<uint32_t> palette;
    if (bits <= 8) {
        if (colorsUsed == 0) colorsUsed = 1u << bits;
        if (at + colorsUsed * 4 > image.size()) return std::nullopt;
        for (uint32_t i = 0; i < colorsUsed; ++i) palette.push_back(U32(image, at + i * 4));
        at += colorsUsed * 4;
    } else if (bits != 24 && bits != 32) {
        return std::nullopt;
    }
    const size_t xorStride = ((static_cast<size_t>(w) * bits + 31) / 32) * 4;
    const size_t andStride = ((static_cast<size_t>(w) + 31) / 32) * 4;
    const size_t xorAt = at, andAt = at + xorStride * h;
    if (xorAt + xorStride * h > image.size()) return std::nullopt;
    const bool hasMask = andAt + andStride * h <= image.size();
    const auto* data = reinterpret_cast<const unsigned char*>(image.data());

    Bitmap bmp;
    bmp.width = w;
    bmp.height = h;
    bmp.pixels.assign(static_cast<size_t>(w) * h, 0);
    // A 32-bit bitmap with any alpha uses it; else the mask decides
    bool alpha = false;
    if (bits == 32)
        for (unsigned y = 0; y < h && !alpha; ++y)
            for (unsigned x = 0; x < w; ++x)
                if (data[xorAt + y * xorStride + x * 4 + 3]) {
                    alpha = true;
                    break;
                }
    for (unsigned y = 0; y < h; ++y) {
        const unsigned row = bottomUp ? h - 1 - y : y;
        const unsigned char* src = data + xorAt + row * xorStride;
        const unsigned char* mask = data + andAt + row * andStride;
        for (unsigned x = 0; x < w; ++x) {
            uint32_t r = 0, g = 0, b = 0, a = 255;
            if (bits == 32) {
                b = src[x * 4];
                g = src[x * 4 + 1];
                r = src[x * 4 + 2];
                if (alpha) a = src[x * 4 + 3];
            } else if (bits == 24) {
                b = src[x * 3];
                g = src[x * 3 + 1];
                r = src[x * 3 + 2];
            } else {
                const unsigned bitIndex = x * bits;
                const unsigned index = (src[bitIndex / 8] >> (8 - bits - bitIndex % 8)) & ((1u << bits) - 1);
                const uint32_t c = index < palette.size() ? palette[index] : 0;
                b = c & 0xFF;
                g = (c >> 8) & 0xFF;
                r = (c >> 16) & 0xFF;
            }
            if (!alpha && hasMask && ((mask[x / 8] >> (7 - x % 8)) & 1)) {
                // AND 1: transparent, unless the XOR colour inverts the screen there
                if (r | g | b) r = g = b = 255;
                else a = 0;
            }
            bmp.pixels[static_cast<size_t>(y) * w + x] = Premultiply(a, r, g, b);
        }
    }
    return bmp;
}

} // namespace

std::optional<Bitmap> Decode(const std::string& bytes) {
    // ICONDIR: reserved 0, type 1 (icon) or 2 (cursor), count; then 16-byte entries
    if (bytes.size() < 6 || U16(bytes, 0) != 0) return std::nullopt;
    const uint16_t type = U16(bytes, 2), count = U16(bytes, 4);
    if ((type != 1 && type != 2) || count == 0 || 6 + static_cast<size_t>(count) * 16 > bytes.size()) return std::nullopt;
    // The biggest image (a 0 in the entry means 256)
    int best = -1;
    unsigned bestArea = 0;
    for (int i = 0; i < count; ++i) {
        const size_t e = 6 + static_cast<size_t>(i) * 16;
        const unsigned w = static_cast<unsigned char>(bytes[e]) ? static_cast<unsigned char>(bytes[e]) : 256u;
        const unsigned h = static_cast<unsigned char>(bytes[e + 1]) ? static_cast<unsigned char>(bytes[e + 1]) : 256u;
        if (w * h > bestArea) {
            bestArea = w * h;
            best = i;
        }
    }
    const size_t e = 6 + static_cast<size_t>(best) * 16;
    if (auto image = DecodeImage(bytes, U32(bytes, e + 12), U32(bytes, e + 8))) return image;
    // A broken entry: try the others in order
    for (int i = 0; i < count; ++i) {
        if (i == best) continue;
        const size_t other = 6 + static_cast<size_t>(i) * 16;
        if (auto image = DecodeImage(bytes, U32(bytes, other + 12), U32(bytes, other + 8))) return image;
    }
    return std::nullopt;
}

std::optional<Bitmap> DecodeFile(const std::wstring& path) {
    const auto bytes = Files::ReadBytes(path);
    if (!bytes) return std::nullopt;
    return Decode(*bytes);
}

Bitmap Downscale(const Bitmap& bitmap, unsigned maxSide) {
    if (bitmap.Empty() || (bitmap.width <= maxSide && bitmap.height <= maxSide)) return bitmap;
    const double scale = std::min(static_cast<double>(maxSide) / bitmap.width, static_cast<double>(maxSide) / bitmap.height);
    Bitmap out;
    out.width = std::max(1u, static_cast<unsigned>(std::lround(bitmap.width * scale)));
    out.height = std::max(1u, static_cast<unsigned>(std::lround(bitmap.height * scale)));
    out.pixels.resize(static_cast<size_t>(out.width) * out.height);
    // Box filter over the source pixels each destination pixel covers (premultiplied, so averaging is exact)
    for (unsigned y = 0; y < out.height; ++y) {
        const unsigned y0 = y * bitmap.height / out.height, y1 = std::max(y0 + 1, (y + 1) * bitmap.height / out.height);
        for (unsigned x = 0; x < out.width; ++x) {
            const unsigned x0 = x * bitmap.width / out.width, x1 = std::max(x0 + 1, (x + 1) * bitmap.width / out.width);
            uint32_t sum[4] = {};
            for (unsigned sy = y0; sy < y1; ++sy)
                for (unsigned sx = x0; sx < x1; ++sx) {
                    const uint32_t p = bitmap.pixels[static_cast<size_t>(sy) * bitmap.width + sx];
                    for (int c = 0; c < 4; ++c) sum[c] += (p >> (c * 8)) & 0xFF;
                }
            const uint32_t n = (y1 - y0) * (x1 - x0);
            uint32_t p = 0;
            for (int c = 0; c < 4; ++c) p |= ((sum[c] + n / 2) / n) << (c * 8);
            out.pixels[static_cast<size_t>(y) * out.width + x] = p;
        }
    }
    return out;
}

} // namespace CustomCursor::CursorImage
