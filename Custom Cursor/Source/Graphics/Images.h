#pragma once
#include <d2d1_3.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace CustomCursor {

// A GPU bitmap usable by every window (created on the shared D2D device).
struct Image {
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    float w = 0.f, h = 0.f;   // pixels
};
using ImagePtr = std::shared_ptr<Image>;

// An image with several frames (an animated GIF), each shown until its end time; a still image is
// one frame. FrameAt loops over the whole duration.
struct Animation {
    std::vector<ImagePtr> frames;
    std::vector<double> ends;   // seconds since the start at which each frame stops showing
    double duration = 0.0;
    bool Animated() const { return frames.size() > 1; }
    ImagePtr FrameAt(double seconds) const;
};
using AnimationPtr = std::shared_ptr<Animation>;

namespace Images {

// Cached by path; nullptr when the file is missing or can't be decoded (also cached).
ImagePtr Load(const std::wstring& path, unsigned maxSide = 2048);
// Every frame of the file, composed as a browser shows them (offsets, disposal, delays); any format,
// a single frame when it isn't animated. Cached by path like Load (Forget drops it too).
AnimationPtr LoadAnimation(const std::wstring& path, unsigned maxSide = 2048);
ImagePtr FromMemory(const std::string& bytes, unsigned maxSide = 2048);
// Premultiplied BGRA pixels (0xAARRGGBB in memory order B,G,R,A)
ImagePtr FromPixels(unsigned width, unsigned height, const uint32_t* bgra);
// Puts an image decoded elsewhere (a worker thread) in the cache, as Load(path, maxSide) would.
void Store(const std::wstring& path, unsigned maxSide, ImagePtr image);
void Forget(const std::wstring& path);
// Drops the cached failures (files that weren't there), so they're tried again: once the startup
// has finished unpacking the assets
void ForgetMissing();
void Clear();

// An animation's frames as pixels (worker-thread friendly), turned into bitmaps on the UI thread
struct AnimationPixels {
    unsigned width = 0, height = 0;
    std::vector<std::vector<uint32_t>> frames;
    std::vector<double> ends;
    double duration = 0.0;
};
bool DecodeAnimationPixels(const std::wstring& path, unsigned maxSide, AnimationPixels& out);
AnimationPtr FromAnimationPixels(const AnimationPixels& pixels);

// Decodes on the calling thread into premultiplied BGRA pixels (worker-thread friendly).
bool Decode(const std::wstring& path, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels);
bool DecodeMemory(const std::string& bytes, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels);

} // namespace Images
} // namespace CustomCursor
