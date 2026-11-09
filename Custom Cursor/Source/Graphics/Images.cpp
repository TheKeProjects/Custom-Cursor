#include "Graphics/Images.h"
#include <shlwapi.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "Graphics/Device.h"

#pragma comment(lib, "shlwapi.lib")

namespace CustomCursor::Images {

namespace {

// Keyed by path + requested size: the same file asked for at 128 px (a thumbnail) and at 768 px
// (a banner) are different images — sharing one entry left the banner with the tiny version
std::unordered_map<std::wstring, ImagePtr> cache;
std::unordered_map<std::wstring, AnimationPtr> animations;   // same keys as `cache`

std::wstring CacheKey(const std::wstring& path, unsigned maxSide) { return path + L'|' + std::to_wstring(maxSide); }

// A frame's metadata value as an unsigned number (GIF fields are VT_UI1 / VT_UI2), `fallback` when missing
unsigned MetaUInt(IWICMetadataQueryReader* reader, const wchar_t* name, unsigned fallback) {
    if (!reader) return fallback;
    PROPVARIANT value;
    PropVariantInit(&value);
    unsigned out = fallback;
    if (SUCCEEDED(reader->GetMetadataByName(name, &value))) {
        if (value.vt == VT_UI1) out = value.bVal;
        else if (value.vt == VT_UI2) out = value.uiVal;
        else if (value.vt == VT_UI4) out = value.ulVal;
    }
    PropVariantClear(&value);
    return out;
}

// Premultiplied BGRA pixels of `source` (any WIC source) scaled to w x h
bool CopyScaled(IWICBitmapSource* source, UINT w, UINT h, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICBitmapSource> from = source;
    UINT sw = 0, sh = 0;
    source->GetSize(&sw, &sh);
    if (sw != w || sh != h) {
        ComPtr<IWICBitmapScaler> scaler;
        if (FAILED(wic->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(source, w, h, WICBitmapInterpolationModeFant))) return false;
        from = scaler;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(from.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut)))
        return false;
    pixels.resize(static_cast<size_t>(w) * h);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size() * 4), reinterpret_cast<BYTE*>(pixels.data())));
}

// Every frame of an animated file onto a canvas of the logical screen size, as a browser plays a GIF:
// each frame drawn over the canvas at its offset, then its area cleared (disposal 2) or put back
// (disposal 3) before the next one. Delays under 20 ms count as 100 ms, as browsers do.
bool DecodeAnimation(IWICBitmapDecoder* decoder, unsigned maxSide, AnimationPixels& anim) {
    IWICImagingFactory* wic = Device::Get().Wic();
    UINT count = 0;
    if (FAILED(decoder->GetFrameCount(&count)) || count == 0) return false;
    ComPtr<IWICMetadataQueryReader> global;
    decoder->GetMetadataQueryReader(&global);
    ComPtr<IWICBitmapFrameDecode> first;
    if (FAILED(decoder->GetFrame(0, &first))) return false;
    UINT fw = 0, fh = 0;
    first->GetSize(&fw, &fh);
    const unsigned cw = MetaUInt(global.Get(), L"/logscrdesc/Width", fw), ch = MetaUInt(global.Get(), L"/logscrdesc/Height", fh);
    if (cw == 0 || ch == 0) return false;
    UINT ow = cw, oh = ch;   // the output size, scaled down to maxSide
    if (std::max(cw, ch) > maxSide) {
        const double s = static_cast<double>(maxSide) / std::max(cw, ch);
        ow = std::max(1u, static_cast<UINT>(cw * s));
        oh = std::max(1u, static_cast<UINT>(ch * s));
    }

    anim = {};
    anim.width = ow;
    anim.height = oh;
    std::vector<uint32_t> canvas(static_cast<size_t>(cw) * ch, 0), previous, frame, scaled;
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IWICBitmapFrameDecode> decoded;
        if (FAILED(decoder->GetFrame(i, &decoded))) break;
        ComPtr<IWICMetadataQueryReader> meta;
        decoded->GetMetadataQueryReader(&meta);
        const unsigned left = MetaUInt(meta.Get(), L"/imgdesc/Left", 0), top = MetaUInt(meta.Get(), L"/imgdesc/Top", 0);
        const unsigned delay = MetaUInt(meta.Get(), L"/grctlext/Delay", 10), disposal = MetaUInt(meta.Get(), L"/grctlext/Disposal", 0);
        UINT w = 0, h = 0;
        decoded->GetSize(&w, &h);
        if (w == 0 || h == 0 || !CopyScaled(decoded.Get(), w, h, frame)) break;
        if (disposal == 3) previous = canvas;
        // Draw the frame over the canvas (premultiplied "over")
        for (UINT y = 0; y < h && top + y < ch; ++y) {
            for (UINT x = 0; x < w && left + x < cw; ++x) {
                const uint32_t src = frame[static_cast<size_t>(y) * w + x];
                const uint32_t a = src >> 24;
                uint32_t& dst = canvas[static_cast<size_t>(top + y) * cw + left + x];
                if (a == 255) {
                    dst = src;
                } else if (a > 0) {
                    uint32_t out = 0;
                    for (int shift = 0; shift < 32; shift += 8) {
                        const uint32_t s = (src >> shift) & 0xFF, d = (dst >> shift) & 0xFF;
                        out |= std::min(255u, s + d * (255 - a) / 255) << shift;
                    }
                    dst = out;
                }
            }
        }
        // This frame as shown
        ComPtr<IWICBitmap> shown;
        if (FAILED(wic->CreateBitmapFromMemory(cw, ch, GUID_WICPixelFormat32bppPBGRA, cw * 4, static_cast<UINT>(canvas.size() * 4),
                                               reinterpret_cast<BYTE*>(canvas.data()), &shown)) ||
            !CopyScaled(shown.Get(), ow, oh, scaled))
            break;
        anim.duration += (delay < 2 ? 10 : delay) / 100.0;
        anim.frames.push_back(scaled);
        anim.ends.push_back(anim.duration);
        // Disposal before the next frame
        if (disposal == 2 && left < cw) {
            for (UINT y = 0; y < h && top + y < ch; ++y)
                std::fill_n(canvas.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(top + y) * cw + left), std::min(w, cw - left), 0u);
        } else if (disposal == 3 && !previous.empty()) {
            canvas = previous;
        }
    }
    return !anim.frames.empty();
}

bool DecodeFrame(IWICBitmapDecoder* decoder, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    ComPtr<IWICBitmapSource> source = frame;
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (w == 0 || h == 0) return false;
    if (std::max(w, h) > maxSide) {
        const double s = static_cast<double>(maxSide) / std::max(w, h);
        w = std::max(1u, static_cast<UINT>(w * s));
        h = std::max(1u, static_cast<UINT>(h * s));
        ComPtr<IWICBitmapScaler> scaler;
        if (FAILED(wic->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(frame.Get(), w, h, WICBitmapInterpolationModeFant)))
            return false;
        source = scaler;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeMedianCut)))
        return false;
    width = w;
    height = h;
    pixels.resize(static_cast<size_t>(w) * h);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size() * 4),
                                           reinterpret_cast<BYTE*>(pixels.data())));
}

} // namespace

bool Decode(const std::wstring& path, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    if (!wic) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)))
        return false;
    return DecodeFrame(decoder.Get(), maxSide, width, height, pixels);
}

bool DecodeMemory(const std::string& bytes, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    if (!wic || bytes.empty()) return false;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size())));
    if (!stream) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    return DecodeFrame(decoder.Get(), maxSide, width, height, pixels);
}

ImagePtr FromPixels(unsigned width, unsigned height, const uint32_t* bgra) {
    ID2D1DeviceContext2* dc = Device::Get().ResourceContext();
    if (!dc || width == 0 || height == 0) return nullptr;
    auto image = std::make_shared<Image>();
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(dc->CreateBitmap(D2D1::SizeU(width, height), bgra, width * 4, props, &image->bitmap))) return nullptr;
    image->w = static_cast<float>(width);
    image->h = static_cast<float>(height);
    return image;
}

ImagePtr Load(const std::wstring& path, unsigned maxSide) {
    const std::wstring key = CacheKey(path, maxSide);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    unsigned w = 0, h = 0;
    std::vector<uint32_t> pixels;
    ImagePtr image;
    if (Decode(path, maxSide, w, h, pixels)) image = FromPixels(w, h, pixels.data());
    cache.emplace(key, image);  // failures are cached too, so they aren't retried every frame
    return image;
}

bool DecodeAnimationPixels(const std::wstring& path, unsigned maxSide, AnimationPixels& out) {
    out = {};
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICBitmapDecoder> decoder;
    if (!wic || FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    UINT count = 0;
    decoder->GetFrameCount(&count);
    if (count > 1) return DecodeAnimation(decoder.Get(), maxSide, out);
    std::vector<uint32_t> pixels;
    if (!DecodeFrame(decoder.Get(), maxSide, out.width, out.height, pixels)) return false;
    out.frames.push_back(std::move(pixels));
    out.ends.push_back(0.0);
    return true;
}

AnimationPtr FromAnimationPixels(const AnimationPixels& pixels) {
    auto anim = std::make_shared<Animation>();
    for (size_t i = 0; i < pixels.frames.size(); ++i) {
        ImagePtr image = FromPixels(pixels.width, pixels.height, pixels.frames[i].data());
        if (!image) return nullptr;
        anim->frames.push_back(std::move(image));
        anim->ends.push_back(pixels.ends[i]);
    }
    anim->duration = pixels.duration;
    return anim->frames.empty() ? nullptr : anim;
}

AnimationPtr LoadAnimation(const std::wstring& path, unsigned maxSide) {
    const std::wstring key = CacheKey(path, maxSide);
    if (const auto it = animations.find(key); it != animations.end()) return it->second;
    AnimationPixels pixels;
    AnimationPtr anim = DecodeAnimationPixels(path, maxSide, pixels) ? FromAnimationPixels(pixels) : nullptr;
    animations.emplace(key, anim);   // failures are cached too
    return anim;
}

ImagePtr FromMemory(const std::string& bytes, unsigned maxSide) {
    unsigned w = 0, h = 0;
    std::vector<uint32_t> pixels;
    if (!DecodeMemory(bytes, maxSide, w, h, pixels)) return nullptr;
    return FromPixels(w, h, pixels.data());
}

void Store(const std::wstring& path, unsigned maxSide, ImagePtr image) { cache[CacheKey(path, maxSide)] = std::move(image); }

void Forget(const std::wstring& path) {
    // Every size of that file
    const std::wstring prefix = path + L'|';
    for (auto it = cache.begin(); it != cache.end();) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) it = cache.erase(it);
        else ++it;
    }
    for (auto it = animations.begin(); it != animations.end();) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) it = animations.erase(it);
        else ++it;
    }
}

void ForgetMissing() {
    for (auto it = cache.begin(); it != cache.end();) it = it->second ? std::next(it) : cache.erase(it);
    for (auto it = animations.begin(); it != animations.end();) it = it->second ? std::next(it) : animations.erase(it);
}

void Clear() {
    cache.clear();
    animations.clear();
}

} // namespace CustomCursor::Images

namespace CustomCursor {

ImagePtr Animation::FrameAt(double seconds) const {
    if (frames.empty()) return nullptr;
    if (frames.size() == 1 || duration <= 0.0) return frames.front();
    const double t = std::fmod(std::max(0.0, seconds), duration);
    const auto it = std::upper_bound(ends.begin(), ends.end(), t);
    return frames[std::min(static_cast<size_t>(it - ends.begin()), frames.size() - 1)];
}

} // namespace CustomCursor
