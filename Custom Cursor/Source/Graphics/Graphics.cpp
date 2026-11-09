#include "Graphics/Graphics.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Graphics/Geometry.h"

#pragma comment(lib, "dxguid.lib")

namespace CustomCursor {

namespace {

// Cache keys are raw values packed into a string (formatting floats with sprintf every frame was a
// visible share of the frame time). Floats are quantized like the old "%.Nf" keys.
void KeyAppend(std::string& key, uint32_t v) { key.append(reinterpret_cast<const char*>(&v), sizeof(v)); }
void KeyAppend(std::wstring& key, uint32_t v) {
    key.push_back(static_cast<wchar_t>(v & 0xFFFF));
    key.push_back(static_cast<wchar_t>(v >> 16));
}
template <typename Key>
void KeyAppend(Key& key, float v, float steps) { KeyAppend(key, static_cast<uint32_t>(static_cast<int32_t>(std::lround(v * steps)))); }

// A glow's blur in 2 px steps: an animated radius (a pulsing glow, an elevation easing in) reuses a
// handful of blurred masks instead of making and blurring a new one on every frame
float QuantizeBlur(float blurRadius) { return std::max(1.f, std::round(blurRadius * 0.5f) * 2.f); }

// Characters that may have colour glyphs (emoji, the symbol blocks, U+FE0F): only those texts go
// through the colour text cache
bool HasColorGlyphs(const std::wstring& s) {
    for (wchar_t ch : s)
        if ((ch >= 0xD800 && ch <= 0xDFFF) || (ch >= 0x2100 && ch <= 0x2BFF) || ch == 0xFE0F || ch == 0x3030 || ch == 0x303D) return true;
    return false;
}

// WPF never drew colour emoji; the section icons ask for them (TextStyle::colorFont)
D2D1_DRAW_TEXT_OPTIONS DrawOptions(const TextStyle& style) {
    return style.colorFont ? D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT : D2D1_DRAW_TEXT_OPTIONS_NONE;
}

std::string StopsKey(const std::vector<GradientStop>& stops, Graphics::ColorFilter filter) {
    std::string key;
    key.reserve(stops.size() * 8);
    for (const auto& s : stops) {
        KeyAppend(key, s.offset, 1000.f);
        KeyAppend(key, (filter ? filter(s.color) : s.color).Packed());
    }
    return key;
}

} // namespace

Graphics::~Graphics() { Shutdown(); }

bool Graphics::Init(HWND window, bool isTransparent) {
    hwnd = window;
    transparent = isTransparent;
    Device& device = Device::Get();
    if (!device.Init()) return false;
    dpi = static_cast<float>(GetDpiForWindow(hwnd));
    if (dpi <= 0.f) dpi = 96.f;
    if (FAILED(device.D2D()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) return false;
    dc->SetTextAntialiasMode(transparent ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    device.D2DFactory()->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                                       D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND),
                                           nullptr, 0, &roundStroke);
    dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &solid);
    return CreateTarget();
}

void Graphics::Shutdown() {
    ReleaseTarget();
    layouts.clear();
    formats.clear();
    glowMasks.clear();
    realizations.clear();
    roundClips.clear();
    shadowEffect.Reset();
    maskBlurEffect.Reset();
    linearBrushes.clear();
    radialBrushes.clear();
    gradientStops.clear();
    visual.Reset();
    compositionTarget.Reset();
    swapChain.Reset();
    solid.Reset();
    roundStroke.Reset();
    dc.Reset();
}

bool Graphics::CreateTarget() {
    if (target) return true;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    pixelW = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
    pixelH = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));
    Device& device = Device::Get();

    if (!swapChain) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = pixelW;
        desc.Height = pixelH;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (transparent) {
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            desc.Scaling = DXGI_SCALING_STRETCH;
            if (!device.Composition() ||
                FAILED(device.DxgiFactory()->CreateSwapChainForComposition(device.D3D(), &desc, nullptr, &swapChain)))
                return false;
            if (FAILED(device.Composition()->CreateTargetForHwnd(hwnd, TRUE, &compositionTarget)) ||
                FAILED(device.Composition()->CreateVisual(&visual)))
                return false;
            visual->SetContent(swapChain.Get());
            compositionTarget->SetRoot(visual.Get());
            device.Composition()->Commit();
        } else {
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            desc.Scaling = DXGI_SCALING_NONE;
            if (FAILED(device.DxgiFactory()->CreateSwapChainForHwnd(device.D3D(), hwnd, &desc, nullptr, nullptr, &swapChain))) {
                desc.Scaling = DXGI_SCALING_STRETCH;  // older drivers
                if (FAILED(device.DxgiFactory()->CreateSwapChainForHwnd(device.D3D(), hwnd, &desc, nullptr, nullptr, &swapChain)))
                    return false;
            }
            device.DxgiFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        }
    }

    ComPtr<IDXGISurface> surface;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, transparent ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE), dpi, dpi);
    if (FAILED(dc->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target))) return false;
    dc->SetTarget(target.Get());
    dc->SetDpi(dpi, dpi);
    width = pixelW * 96.f / dpi;
    height = pixelH * 96.f / dpi;
    return true;
}

void Graphics::ReleaseTarget() {
    if (dc) dc->SetTarget(nullptr);
    target.Reset();
}

void Graphics::Resize() {
    if (!swapChain) return;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const UINT w = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
    const UINT h = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));
    if (w == pixelW && h == pixelH && target) return;
    ReleaseTarget();
    swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    CreateTarget();
}

void Graphics::SetDpi(float newDpi) {
    dpi = newDpi;
    glowMasks.clear();
    realizations.clear();
    ReleaseTarget();
    CreateTarget();
}

bool Graphics::Begin() {
    if (!CreateTarget()) return false;
    ++frame;
    dc->BeginDraw();
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    transforms.clear();
    return true;
}

void Graphics::End() {
    const HRESULT hr = dc->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseTarget();
        return;
    }
    // Interval 0: the message loop paces all windows together with DwmFlush
    const HRESULT present = swapChain->Present(0, 0);
    if (present == DXGI_ERROR_DEVICE_REMOVED || present == DXGI_ERROR_DEVICE_RESET) ReleaseTarget();
    EndFrameHousekeeping();
}

void Graphics::EndFrameHousekeeping() {
    // Geometries built on the fly go quickly (the cache holds them); realizations after a while unused
    if (frame % 30 == 0) {
        for (auto it = roundClips.begin(); it != roundClips.end();) {
            if (frame - it->second.lastUsed > 2) it = roundClips.erase(it);
            else ++it;
        }
        for (auto it = realizations.begin(); it != realizations.end();) {
            const uint64_t idle = frame - it->second.lastUsed;
            if (idle > (it->second.realization ? 600u : 2u)) it = realizations.erase(it);
            else ++it;
        }
    }
    // Drop text layouts that weren't used for a while (text that changes every frame, like timers)
    if (frame % 120 != 0) return;
    for (auto it = layouts.begin(); it != layouts.end();) {
        if (frame - it->second.lastUsed > 240) it = layouts.erase(it);
        else ++it;
    }
    for (auto it = glowMasks.begin(); it != glowMasks.end();) {
        if (frame - it->second.lastUsed > 600) it = glowMasks.erase(it);
        else ++it;
    }
    for (auto it = colorTexts.begin(); it != colorTexts.end();) {
        if (frame - it->second.lastUsed > 600) it = colorTexts.erase(it);
        else ++it;
    }
}

void Graphics::Clear(Color c) { dc->Clear(c.ToD2D()); }

void Graphics::PushTransform(const D2D1::Matrix3x2F& m) {
    D2D1::Matrix3x2F current;
    dc->GetTransform(&current);
    transforms.push_back(current);
    dc->SetTransform(m * current);
}

void Graphics::PopTransform() {
    if (transforms.empty()) return;
    dc->SetTransform(transforms.back());
    transforms.pop_back();
}

void Graphics::PushClip(const Rect& r) { dc->PushAxisAlignedClip(r.D2D(), D2D1_ANTIALIAS_MODE_ALIASED); }
void Graphics::PopClip() { dc->PopAxisAlignedClip(); }

void Graphics::PushLayer(float opacity, ID2D1Geometry* mask) {
    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1(D2D1::InfiniteRect(), mask, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                           D2D1::IdentityMatrix(), opacity, nullptr,
                                                           D2D1_LAYER_OPTIONS1_NONE);
    PushLayerKeepingText(params);
}

// ClearType can't blend onto a layer's transparent pixels (dark text vanishes): grayscale text
// while a layer is open, ClearType again once the last one is popped
void Graphics::PushLayerKeepingText(const D2D1_LAYER_PARAMETERS1& params) {
    layerTextModes.push_back(dc->GetTextAntialiasMode());
    dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    dc->PushLayer(params, nullptr);
}

void Graphics::PushOpacity(float opacity) { PushLayer(opacity, nullptr); }

void Graphics::PushMaskLayer(float opacity, ID2D1Brush* opacityMask) {
    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                           D2D1::IdentityMatrix(), opacity, opacityMask, D2D1_LAYER_OPTIONS1_NONE);
    PushLayerKeepingText(params);
}

void Graphics::PushRoundClip(const Rect& r, float radius) {
    // The same clips come back every frame: reuse their geometry
    std::wstring key;
    KeyAppend(key, r.x, 10.f);
    KeyAppend(key, r.y, 10.f);
    KeyAppend(key, r.w, 10.f);
    KeyAppend(key, r.h, 10.f);
    KeyAppend(key, radius, 10.f);
    CachedClip& clip = roundClips[key];
    clip.lastUsed = frame;
    if (!clip.geometry) clip.geometry = Geometry::RoundedRect(r.D2D(), radius, radius);
    // Bounded to the rect, so the layer isn't the size of the whole window
    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1(r.D2D(), clip.geometry.Get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), 1.f,
                                                           nullptr, D2D1_LAYER_OPTIONS1_NONE);
    PushLayerKeepingText(params);
}

void Graphics::PopLayer() {
    dc->PopLayer();
    if (!layerTextModes.empty()) {
        dc->SetTextAntialiasMode(layerTextModes.back());
        layerTextModes.pop_back();
    }
}

ID2D1SolidColorBrush* Graphics::Brush(Color c) {
    solid->SetColor(Filter(c).ToD2D());
    solid->SetOpacity(1.f);
    return solid.Get();
}

void Graphics::FillRect(const Rect& r, Color c) {
    if (c.Transparent() || r.Empty()) return;
    dc->FillRectangle(r.D2D(), Brush(c));
}

void Graphics::FillRound(const Rect& r, float radius, Color c) {
    if (c.Transparent() || r.Empty()) return;
    if (radius <= 0.f) dc->FillRectangle(r.D2D(), Brush(c));
    else dc->FillRoundedRectangle(D2D1::RoundedRect(r.D2D(), radius, radius), Brush(c));
}

void Graphics::FillRound(const Rect& r, float tl, float tr, float br, float bl, Color c) {
    if (c.Transparent() || r.Empty()) return;
    auto g = Geometry::RoundedRect(r.D2D(), tl, tr, br, bl);
    if (g) dc->FillGeometry(g.Get(), Brush(c));
}

void Graphics::StrokeRect(const Rect& r, Color c, float w) {
    if (c.Transparent()) return;
    dc->DrawRectangle(r.Inset(w * 0.5f).D2D(), Brush(c), w);
}

void Graphics::StrokeRound(const Rect& r, float radius, Color c, float w) {
    if (c.Transparent()) return;
    const Rect inner = r.Inset(w * 0.5f);
    const float rr = std::max(0.f, radius - w * 0.5f);
    if (rr <= 0.f) dc->DrawRectangle(inner.D2D(), Brush(c), w);
    else dc->DrawRoundedRectangle(D2D1::RoundedRect(inner.D2D(), rr, rr), Brush(c), w);
}

void Graphics::Border(const Rect& r, float radius, Color background, Color border, float thickness) {
    FillRound(r, radius, background);
    if (thickness > 0.f) StrokeRound(r, radius, border, thickness);
}

void Graphics::BorderSides(const Rect& r, const Edges& t, Color c) {
    if (c.Transparent()) return;
    if (t.t > 0) FillRect({ r.x, r.y, r.w, t.t }, c);
    if (t.b > 0) FillRect({ r.x, r.B() - t.b, r.w, t.b }, c);
    if (t.l > 0) FillRect({ r.x, r.y, t.l, r.h }, c);
    if (t.r > 0) FillRect({ r.R() - t.r, r.y, t.r, r.h }, c);
}

void Graphics::FillEllipse(float cx, float cy, float rx, float ry, Color c) {
    if (c.Transparent()) return;
    dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), Brush(c));
}

void Graphics::StrokeEllipse(float cx, float cy, float rx, float ry, Color c, float w) {
    if (c.Transparent()) return;
    dc->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), Brush(c), w);
}

void Graphics::Line(float x1, float y1, float x2, float y2, Color c, float w, bool roundCaps) {
    if (c.Transparent() || w <= 0.f) return;
    // A rectangle (or a capsule for round caps) turned along the line: D2D draws those on the GPU,
    // where a stroked line is tessellated on the CPU every time
    const float dx = x2 - x1, dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);
    D2D1::Matrix3x2F current;
    dc->GetTransform(&current);
    const float cosA = length > 0.f ? dx / length : 1.f, sinA = length > 0.f ? dy / length : 0.f;
    const D2D1::Matrix3x2F along(cosA, sinA, -sinA, cosA, x1, y1);
    dc->SetTransform(along * current);
    const float half = w * 0.5f;
    if (roundCaps) dc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(-half, -half, length + half, half), half, half), Brush(c));
    else if (length > 0.f) dc->FillRectangle(D2D1::RectF(0.f, -half, length, half), Brush(c));
    dc->SetTransform(current);
}

// D2D tessellates a geometry on the CPU every time it is filled or stroked. The geometries drawn
// frame after frame (icons, the seasonal scenery) are tessellated once per scale step into a
// realization instead; one seen only once or twice (built on the fly) is drawn directly.
ID2D1GeometryRealization* Graphics::Realization(ID2D1Geometry* geometry, float strokeWidth, bool round) {
    D2D1::Matrix3x2F m;
    dc->GetTransform(&m);
    const float scale = std::sqrt(std::fabs(m._11 * m._22 - m._12 * m._21));
    if (!(scale > 0.f)) return nullptr;
    // Quarter-octave scale steps, so a hover / pop animation reuses a handful of realizations
    const int step = static_cast<int>(std::lround(std::log2(scale) * 4.f));
    CachedRealization& entry = realizations[{ geometry, step, strokeWidth, round }];
    entry.lastUsed = frame;
    if (entry.realization) return entry.realization.Get();
    if (!entry.geometry) entry.geometry = geometry;   // holds it, so its address can't be reused by another geometry
    if (++entry.uses < 3) return nullptr;
    // Fine enough for the top of the step (2^(1/8) above its scale)
    const float stepScale = std::exp2(static_cast<float>(step) / 4.f);
    const float tolerance = D2D1::ComputeFlatteningTolerance(D2D1::Matrix3x2F::Scale(stepScale, stepScale), dpi, dpi, 1.0905f);
    HRESULT hr = strokeWidth > 0.f ? dc->CreateStrokedGeometryRealization(geometry, tolerance, strokeWidth, round ? roundStroke.Get() : nullptr, &entry.realization)
                                   : dc->CreateFilledGeometryRealization(geometry, tolerance, &entry.realization);
    if (FAILED(hr)) entry.realization.Reset();
    return entry.realization.Get();
}

void Graphics::FillGeometry(ID2D1Geometry* g, Color c) {
    if (!g || c.Transparent()) return;
    if (ID2D1GeometryRealization* r = Realization(g, 0.f, false)) dc->DrawGeometryRealization(r, Brush(c));
    else dc->FillGeometry(g, Brush(c));
}

void Graphics::StrokeGeometry(ID2D1Geometry* g, Color c, float w, bool round) {
    if (!g || c.Transparent() || w <= 0.f) return;
    if (ID2D1GeometryRealization* r = Realization(g, w, round)) dc->DrawGeometryRealization(r, Brush(c));
    else dc->DrawGeometry(g, Brush(c), w, round ? roundStroke.Get() : nullptr);
}

void Graphics::FillGeometry(ID2D1Geometry* g, ID2D1Brush* brush) {
    if (!g || !brush) return;
    if (ID2D1GeometryRealization* r = Realization(g, 0.f, false)) dc->DrawGeometryRealization(r, brush);
    else dc->FillGeometry(g, brush);
}

ID2D1LinearGradientBrush* Graphics::LinearBrush(const std::vector<GradientStop>& stops, D2D1_POINT_2F start, D2D1_POINT_2F end,
                                                float opacity) {
    const std::string key = StopsKey(stops, colorFilter);
    auto it = linearBrushes.find(key);
    if (it == linearBrushes.end()) {
        std::vector<D2D1_GRADIENT_STOP> d2dStops;
        for (const auto& s : stops) d2dStops.push_back({ s.offset, Filter(s.color).ToD2D() });
        ComPtr<ID2D1GradientStopCollection> collection;
        ComPtr<ID2D1LinearGradientBrush> brush;
        if (FAILED(dc->CreateGradientStopCollection(d2dStops.data(), static_cast<UINT32>(d2dStops.size()), &collection)) ||
            FAILED(dc->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(start, end), collection.Get(), &brush)))
            return nullptr;
        it = linearBrushes.emplace(key, brush).first;
    }
    it->second->SetStartPoint(start);
    it->second->SetEndPoint(end);
    it->second->SetOpacity(opacity);
    return it->second.Get();
}

ID2D1RadialGradientBrush* Graphics::RadialBrush(const std::vector<GradientStop>& stops, D2D1_POINT_2F center, float rx, float ry,
                                                float opacity, D2D1_POINT_2F originOffset) {
    const std::string key = StopsKey(stops, colorFilter);
    auto it = radialBrushes.find(key);
    if (it == radialBrushes.end()) {
        std::vector<D2D1_GRADIENT_STOP> d2dStops;
        for (const auto& s : stops) d2dStops.push_back({ s.offset, Filter(s.color).ToD2D() });
        ComPtr<ID2D1GradientStopCollection> collection;
        ComPtr<ID2D1RadialGradientBrush> brush;
        if (FAILED(dc->CreateGradientStopCollection(d2dStops.data(), static_cast<UINT32>(d2dStops.size()), &collection)) ||
            FAILED(dc->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center, originOffset, rx, ry),
                                                 collection.Get(), &brush)))
            return nullptr;
        it = radialBrushes.emplace(key, brush).first;
    }
    it->second->SetCenter(center);
    it->second->SetGradientOriginOffset(originOffset);
    it->second->SetRadiusX(rx);
    it->second->SetRadiusY(ry);
    it->second->SetOpacity(opacity);
    return it->second.Get();
}

void Graphics::FillLinear(const Rect& r, float radius, const std::vector<GradientStop>& stops, D2D1_POINT_2F start, D2D1_POINT_2F end) {
    // start / end are relative (0..1) like XAML StartPoint / EndPoint
    auto* b = LinearBrush(stops, { r.x + start.x * r.w, r.y + start.y * r.h }, { r.x + end.x * r.w, r.y + end.y * r.h });
    if (!b) return;
    if (radius <= 0.f) dc->FillRectangle(r.D2D(), b);
    else dc->FillRoundedRectangle(D2D1::RoundedRect(r.D2D(), radius, radius), b);
}

void Graphics::FillRadial(const Rect& r, const std::vector<GradientStop>& stops) {
    auto* b = RadialBrush(stops, { r.CX(), r.CY() }, r.w * 0.5f, r.h * 0.5f);
    if (b) dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(r.CX(), r.CY()), r.w * 0.5f, r.h * 0.5f), b);
}

// ── Text ────────────────────────────────────────────────────────────────────

IDWriteTextFormat* Graphics::Format(const TextStyle& style) {
    const uint64_t key = static_cast<uint64_t>(style.size * 100.f) << 32 | static_cast<uint64_t>(style.font) << 24 |
                         static_cast<uint64_t>(style.weight) << 12 | (style.italic ? 1ull : 0ull) << 11 |
                         static_cast<uint64_t>(style.lineHeight * 10.f) & 0x7FF;
    auto it = formats.find(key);
    if (it != formats.end()) return it->second.Get();
    const Fonts::Family& family = Fonts::Get(style.font);
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(Device::Get().DWrite()->CreateTextFormat(family.name.c_str(), family.collection.Get(), style.weight,
                                                        style.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                        style.font == Font::Display ? DWRITE_FONT_STRETCH_CONDENSED : DWRITE_FONT_STRETCH_NORMAL,
                                                        style.size, L"en-us", &format)))
        return nullptr;
    if (style.lineHeight > 0.f)
        format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, style.lineHeight, style.lineHeight * 0.8f);
    return formats.emplace(key, format).first->second.Get();
}

IDWriteTextLayout* Graphics::Layout(const std::wstring& s, const TextStyle& style, float maxWidth, float maxHeight) {
    std::wstring key;
    key.reserve(10 + s.size());
    KeyAppend(key, style.size, 100.f);
    KeyAppend(key, static_cast<uint32_t>(style.font) | static_cast<uint32_t>(style.weight) << 8 | static_cast<uint32_t>(style.italic) << 20 |
                       static_cast<uint32_t>(style.align) << 21 | static_cast<uint32_t>(style.valign) << 24 | static_cast<uint32_t>(style.wrap) << 27 |
                       static_cast<uint32_t>(style.ellipsis) << 28 | static_cast<uint32_t>(style.underline) << 29);
    KeyAppend(key, style.lineHeight, 10.f);
    KeyAppend(key, maxWidth, 10.f);
    KeyAppend(key, maxHeight, 10.f);
    key += s;
    auto it = layouts.find(key);
    if (it != layouts.end()) {
        it->second.lastUsed = frame;
        return it->second.layout.Get();
    }
    IDWriteTextFormat* format = Format(style);
    if (!format) return nullptr;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(Device::Get().DWrite()->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), format, std::max(0.f, maxWidth),
                                                        std::max(0.f, maxHeight), &layout)))
        return nullptr;
    switch (style.align) {
        case HAlign::Left: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); break;
        case HAlign::Center: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); break;
        case HAlign::Right: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); break;
        case HAlign::Justify: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED); break;
    }
    switch (style.valign) {
        case VAlign::Top: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR); break;
        case VAlign::Center: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); break;
        case VAlign::Bottom: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR); break;
    }
    layout->SetWordWrapping(style.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (style.ellipsis) {
        ComPtr<IDWriteInlineObject> sign;
        Device::Get().DWrite()->CreateEllipsisTrimmingSign(layout.Get(), &sign);
        const DWRITE_TRIMMING trimming{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        layout->SetTrimming(&trimming, sign.Get());
    }
    if (style.underline) layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, static_cast<UINT32>(s.size()) });
    auto& entry = layouts[key];
    entry.layout = layout;
    entry.lastUsed = frame;
    return layout.Get();
}

void Graphics::Text(const std::wstring& text, const Rect& r, Color c, const TextStyle& style) {
    if (text.empty() || c.Transparent() || r.w <= 0.f) return;
    const std::wstring& s = textFilter ? textFilter(text) : text;
    IDWriteTextLayout* layout = Layout(s, style, r.w, std::max(r.h, 0.f));
    if (!layout) return;
    if (style.colorFont && HasColorGlyphs(s) && DrawColorText(s, layout, r, c, style)) return;
    dc->DrawTextLayout(D2D1::Point2F(r.x, r.y), layout, Brush(c), DrawOptions(style));
}

bool Graphics::DrawColorText(const std::wstring& s, IDWriteTextLayout* layout, const Rect& r, Color c, const TextStyle& style) {
    const float scale = dpi / 96.f;
    std::wstring key = s;
    key.push_back(L'\x1');
    KeyAppend(key, r.w, 4.f);
    KeyAppend(key, r.h, 4.f);
    KeyAppend(key, style.size, 10.f);
    KeyAppend(key, style.lineHeight, 10.f);
    KeyAppend(key, static_cast<uint32_t>(style.weight) | static_cast<uint32_t>(style.font) << 12 | static_cast<uint32_t>(style.align) << 16 |
                       static_cast<uint32_t>(style.valign) << 18 | (style.wrap ? 1u << 20 : 0u) | (style.ellipsis ? 1u << 21 : 0u) |
                       (style.italic ? 1u << 22 : 0u) | (style.underline ? 1u << 23 : 0u));
    KeyAppend(key, Filter(c).Packed());
    KeyAppend(key, dpi, 1.f);
    auto it = colorTexts.find(key);
    if (it == colorTexts.end()) {
        if (colorTexts.size() >= 512) {
            for (auto old = colorTexts.begin(); old != colorTexts.end();) old = old->second.lastUsed + 2 < frame ? colorTexts.erase(old) : std::next(old);
        }
        // Rendered on the resource context, 4 physical pixels of room on every side
        CachedColorText cached;
        cached.pad = 4.f / scale;
        ID2D1DeviceContext2* rc = Device::Get().ResourceContext();
        const UINT pw = static_cast<UINT>(std::ceil(r.w * scale)) + 8, ph = static_cast<UINT>(std::ceil(std::max(r.h, 1.f) * scale)) + 8;
        if (!rc || pw > 4096 || ph > 4096) return false;
        const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
        if (FAILED(rc->CreateBitmap(D2D1::SizeU(pw, ph), nullptr, 0, props, &cached.bitmap))) return false;
        ComPtr<ID2D1SolidColorBrush> brush;
        rc->SetTarget(cached.bitmap.Get());
        rc->SetDpi(dpi, dpi);
        rc->CreateSolidColorBrush(Filter(c).ToD2D(), &brush);
        rc->BeginDraw();
        rc->Clear(D2D1::ColorF(0, 0, 0, 0));
        rc->SetTransform(D2D1::Matrix3x2F::Identity());
        rc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        rc->DrawTextLayout(D2D1::Point2F(cached.pad, cached.pad), layout, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        const HRESULT hr = rc->EndDraw();
        rc->SetTarget(nullptr);
        if (FAILED(hr)) return false;
        it = colorTexts.emplace(key, std::move(cached)).first;
    }
    it->second.lastUsed = frame;
    // Placed on the pixel grid when nothing scales it, so it's as sharp as text drawn directly
    float x = r.x - it->second.pad, y = r.y - it->second.pad;
    D2D1::Matrix3x2F t;
    dc->GetTransform(&t);
    if (t._11 == 1.f && t._22 == 1.f && t._12 == 0.f && t._21 == 0.f) {
        x = std::round((x + t._31) * scale) / scale - t._31;
        y = std::round((y + t._32) * scale) / scale - t._32;
    }
    const auto size = it->second.bitmap->GetSize();
    dc->DrawBitmap(it->second.bitmap.Get(), D2D1::RectF(x, y, x + size.width, y + size.height), 1.f, D2D1_INTERPOLATION_MODE_LINEAR, nullptr);
    return true;
}

ComPtr<IDWriteTextLayout> Graphics::RichLayout(const std::wstring& s, const TextStyle& style, const std::vector<TextSpan>& spans,
                                               float maxWidth, float maxHeight, bool withColors) {
    // Same paragraph settings as the cached layouts, then the ranges on a private copy
    IDWriteTextLayout* base = Layout(s, style, maxWidth, maxHeight);
    IDWriteTextFormat* format = Format(style);
    if (!base || !format) return nullptr;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(Device::Get().DWrite()->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), format, std::max(0.f, maxWidth),
                                                        std::max(0.f, maxHeight), &layout)))
        return nullptr;
    layout->SetTextAlignment(base->GetTextAlignment());
    layout->SetParagraphAlignment(base->GetParagraphAlignment());
    layout->SetWordWrapping(base->GetWordWrapping());
    DWRITE_LINE_SPACING_METHOD method;
    FLOAT spacing = 0.f, baseline = 0.f;
    if (SUCCEEDED(base->GetLineSpacing(&method, &spacing, &baseline))) layout->SetLineSpacing(method, spacing, baseline);
    if (style.underline) layout->SetUnderline(TRUE, { 0, static_cast<UINT32>(s.size()) });
    for (const auto& span : spans) {
        const DWRITE_TEXT_RANGE range{ static_cast<UINT32>(span.start), static_cast<UINT32>(span.length) };
        if (span.weight != 0) layout->SetFontWeight(span.weight, range);
        if (span.underline) layout->SetUnderline(TRUE, range);
        if (withColors && !span.color.Transparent()) {
            ComPtr<ID2D1SolidColorBrush> brush;
            if (SUCCEEDED(dc->CreateSolidColorBrush(Filter(span.color).ToD2D(), &brush))) layout->SetDrawingEffect(brush.Get(), range);
        }
    }
    return layout;
}

void Graphics::RichText(const std::wstring& s, const Rect& r, Color c, const TextStyle& style, const std::vector<TextSpan>& spans) {
    if (s.empty() || r.w <= 0.f) return;
    auto layout = RichLayout(s, style, spans, r.w, std::max(r.h, 0.f), true);
    if (layout) dc->DrawTextLayout(D2D1::Point2F(r.x, r.y), layout.Get(), Brush(c), DrawOptions(style));
}

TextSize Graphics::MeasureRich(const std::wstring& s, const TextStyle& style, const std::vector<TextSpan>& spans, float maxWidth) {
    TextStyle measure = style;
    measure.align = HAlign::Left;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    auto layout = RichLayout(s.empty() ? L" " : s, measure, spans, maxWidth, 100000.f, false);
    if (!layout) return {};
    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return { s.empty() ? 0.f : m.widthIncludingTrailingWhitespace, m.height };
}

int Graphics::HitTestRich(const std::wstring& s, const Rect& r, const TextStyle& style, const std::vector<TextSpan>& spans, float x, float y) {
    auto layout = RichLayout(s, style, spans, r.w, std::max(r.h, 0.f), false);
    if (!layout) return -1;
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS m{};
    layout->HitTestPoint(x - r.x, y - r.y, &trailing, &inside, &m);
    return inside ? static_cast<int>(m.textPosition) : -1;
}

TextSize Graphics::Measure(const std::wstring& text, const TextStyle& style, float maxWidth) {
    const std::wstring& s = textFilter && !text.empty() ? textFilter(text) : text;
    if (s.empty()) {
        // Height of an empty line still matters for layout (empty TextBlocks keep their line height)
        IDWriteTextLayout* probe = Layout(L" ", style, maxWidth, 100000.f);
        if (!probe) return {};
        DWRITE_TEXT_METRICS m{};
        probe->GetMetrics(&m);
        return { 0.f, m.height };
    }
    TextStyle measure = style;
    measure.align = HAlign::Left;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    IDWriteTextLayout* layout = Layout(s, measure, maxWidth, 100000.f);
    if (!layout) return {};
    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return { m.widthIncludingTrailingWhitespace, m.height };
}

int Graphics::HitTestText(const std::wstring& s, const TextStyle& style, float maxWidth, float x, float y) {
    TextStyle measure = style;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    IDWriteTextLayout* layout = Layout(s.empty() ? L" " : s, measure, maxWidth, 100000.f);
    if (!layout) return 0;
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS m{};
    layout->HitTestPoint(x, y, &trailing, &inside, &m);
    int index = static_cast<int>(m.textPosition) + (trailing ? static_cast<int>(m.length) : 0);
    return std::clamp(index, 0, static_cast<int>(s.size()));
}

void Graphics::CaretPosition(const std::wstring& s, const TextStyle& style, float maxWidth, int index, float& x, float& y, float& h) {
    TextStyle measure = style;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    const std::wstring text = s.empty() ? L" " : s;
    IDWriteTextLayout* layout = Layout(text, measure, maxWidth, 100000.f);
    x = y = 0.f;
    h = style.size * 1.3f;
    if (!layout) return;
    DWRITE_HIT_TEST_METRICS m{};
    FLOAT px = 0, py = 0;
    const bool atEnd = index >= static_cast<int>(s.size()) && !s.empty();
    layout->HitTestTextPosition(static_cast<UINT32>(atEnd ? s.size() - 1 : (s.empty() ? 0 : index)), atEnd ? TRUE : FALSE, &px,
                                &py, &m);
    x = s.empty() ? (style.align == HAlign::Center ? m.left : 0.f) : px;
    y = py;
    h = m.height;
    // A caret after a trailing newline sits at the start of the next line
    if (atEnd && s.back() == L'\n') {
        x = 0.f;
        y = py + m.height;
    }
}

// ── Images ──────────────────────────────────────────────────────────────────

namespace {

D2D1_RECT_F Fit(const Image& image, const Rect& r, Stretch stretch) {
    if (stretch == Stretch::Fill || image.w <= 0 || image.h <= 0) return r.D2D();
    float s = 1.f;
    if (stretch == Stretch::Uniform) s = std::min(r.w / image.w, r.h / image.h);
    else if (stretch == Stretch::UniformToFill) s = std::max(r.w / image.w, r.h / image.h);
    const float w = image.w * s, h = image.h * s;
    const float x = r.x + (r.w - w) * 0.5f, y = r.y + (r.h - h) * 0.5f;
    return D2D1::RectF(x, y, x + w, y + h);
}

} // namespace

void Graphics::DrawImage(const Image& image, const Rect& r, float opacity, Stretch stretch, bool highQuality) {
    if (!image.bitmap || r.Empty()) return;
    const D2D1_RECT_F dest = Fit(image, r, stretch);
    const auto mode = highQuality ? D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    if (stretch == Stretch::UniformToFill) {
        PushClip(r);
        dc->DrawBitmap(image.bitmap.Get(), dest, opacity, mode, nullptr, nullptr);
        PopClip();
    } else {
        dc->DrawBitmap(image.bitmap.Get(), dest, opacity, mode, nullptr, nullptr);
    }
}

void Graphics::DrawImageRounded(const Image& image, const Rect& r, float radius, float opacity, Stretch stretch) {
    if (!image.bitmap || r.Empty()) return;
    // A full radius gives a circle on a square and a capsule on anything wider (not an ellipse)
    radius = std::min(radius, std::min(r.w, r.h) * 0.5f);
    ComPtr<ID2D1Geometry> mask = Geometry::RoundedRect(r.D2D(), radius, radius);
    PushLayer(opacity, mask.Get());
    dc->DrawBitmap(image.bitmap.Get(), Fit(image, r, stretch), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr, nullptr);
    PopLayer();
}

void Graphics::DrawBitmap(ID2D1Bitmap* bitmap, const Rect& dest, float opacity, const D2D1_RECT_F* source) {
    if (!bitmap) return;
    dc->DrawBitmap(bitmap, dest.D2D(), opacity, D2D1_INTERPOLATION_MODE_LINEAR, source, nullptr);
}

ComPtr<ID2D1Bitmap1> Graphics::CreateWindowBitmap() {
    ComPtr<ID2D1Bitmap1> bitmap;
    if (!dc) return bitmap;
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    dc->CreateBitmap(D2D1::SizeU(pixelW, pixelH), nullptr, 0, props, &bitmap);
    return bitmap;
}

bool Graphics::MatchesWindow(ID2D1Bitmap1* bitmap) const {
    if (!bitmap) return false;
    const auto size = bitmap->GetPixelSize();
    return size.width == pixelW && size.height == pixelH;
}

void Graphics::BeginOffscreen(ID2D1Bitmap1* bitmap) {
    if (!bitmap || savedTarget) return;
    dc->GetTarget(&savedTarget);
    dc->SetTarget(bitmap);
}

void Graphics::EndOffscreen() {
    if (!savedTarget) return;
    dc->SetTarget(savedTarget.Get());
    savedTarget.Reset();
}

bool Graphics::SavePng(ID2D1Bitmap1* bitmap, const std::wstring& path) {
    if (!bitmap || !dc) return false;
    dc->Flush();   // the drawing into it is still batched
    // A CPU-readable copy of the bitmap, encoded by WIC
    const auto size = bitmap->GetPixelSize();
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    ComPtr<ID2D1Bitmap1> readable;
    if (FAILED(dc->CreateBitmap(size, nullptr, 0, props, &readable)) || FAILED(readable->CopyFromBitmap(nullptr, bitmap, nullptr))) return false;
    D2D1_MAPPED_RECT mapped{};
    if (FAILED(readable->Map(D2D1_MAP_OPTIONS_READ, &mapped))) return false;
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frameEncode;
    bool ok = wic && SUCCEEDED(wic->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
              SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
              SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) && SUCCEEDED(encoder->CreateNewFrame(&frameEncode, nullptr)) &&
              SUCCEEDED(frameEncode->Initialize(nullptr)) && SUCCEEDED(frameEncode->SetSize(size.width, size.height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    ok = ok && SUCCEEDED(frameEncode->SetPixelFormat(&format)) &&
         SUCCEEDED(frameEncode->WritePixels(size.height, mapped.pitch, mapped.pitch * size.height, mapped.bits)) &&
         SUCCEEDED(frameEncode->Commit()) && SUCCEEDED(encoder->Commit());
    readable->Unmap();
    return ok;
}

Graphics::GlowMask* Graphics::Mask(const std::wstring& key, float w, float h, float pad,
                                   const std::function<void(ID2D1DeviceContext*, ID2D1Brush*)>& draw) {
    auto it = glowMasks.find(key);
    if (it != glowMasks.end()) {
        it->second.lastUsed = frame;
        return it->second.bitmap ? &it->second : nullptr;
    }
    // Masks are keyed by size, so a resize / hover animation adds one per frame: past a few hundred,
    // drop the ones that weren't drawn this frame instead of waiting for the periodic cleanup
    if (glowMasks.size() >= 384) {
        for (auto old = glowMasks.begin(); old != glowMasks.end();) {
            if (old->second.lastUsed != frame) old = glowMasks.erase(old);
            else ++old;
        }
    }
    GlowMask mask;
    mask.pad = pad;
    mask.lastUsed = frame;
    ID2D1DeviceContext2* rc = Device::Get().ResourceContext();
    const UINT pw = static_cast<UINT>(std::ceil((w + pad * 2.f) * dpi / 96.f));
    const UINT ph = static_cast<UINT>(std::ceil((h + pad * 2.f) * dpi / 96.f));
    if (rc && pw > 0 && ph > 0 && pw < 8192 && ph < 8192) {
        const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                                                   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
        if (SUCCEEDED(rc->CreateBitmap(D2D1::SizeU(pw, ph), nullptr, 0, props, &mask.bitmap))) {
            ComPtr<ID2D1SolidColorBrush> white;
            rc->SetTarget(mask.bitmap.Get());
            rc->SetDpi(dpi, dpi);
            rc->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &white);
            rc->BeginDraw();
            rc->Clear(D2D1::ColorF(0, 0, 0, 0));
            rc->SetTransform(D2D1::Matrix3x2F::Translation(pad, pad));
            draw(rc, white.Get());
            rc->SetTransform(D2D1::Matrix3x2F::Identity());
            rc->EndDraw();
            rc->SetTarget(nullptr);
        }
    }
    auto& stored = glowMasks[key];
    stored = mask;
    return stored.bitmap ? &stored : nullptr;
}

// Blurs the mask once (white glow) into its own bitmap; the frames then only tint that bitmap
void Graphics::BlurMask(GlowMask& mask, float blurRadius) {
    mask.blurTried = true;
    ID2D1DeviceContext2* rc = Device::Get().ResourceContext();
    if (!rc || !mask.bitmap) return;
    if (!maskBlurEffect && FAILED(rc->CreateEffect(CLSID_D2D1Shadow, &maskBlurEffect))) return;
    const auto size = mask.bitmap->GetPixelSize();
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    ComPtr<ID2D1Bitmap1> blurred;
    if (FAILED(rc->CreateBitmap(size, nullptr, 0, props, &blurred))) return;
    maskBlurEffect->SetInput(0, mask.bitmap.Get());
    maskBlurEffect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(1.f, 1.f, 1.f, 1.f));
    // WPF's BlurRadius spans about three standard deviations
    maskBlurEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, std::max(0.1f, blurRadius / 3.f));
    rc->SetTarget(blurred.Get());
    rc->SetDpi(dpi, dpi);
    rc->BeginDraw();
    rc->Clear(D2D1::ColorF(0, 0, 0, 0));
    rc->SetTransform(D2D1::Matrix3x2F::Identity());
    rc->DrawImage(maskBlurEffect.Get(), D2D1::Point2F(0.f, 0.f));
    const HRESULT hr = rc->EndDraw();
    rc->SetTarget(nullptr);
    maskBlurEffect->SetInput(0, nullptr);
    if (SUCCEEDED(hr)) mask.blurred = blurred;
}

void Graphics::DrawGlow(GlowMask& mask, float x, float y, Color color, float blurRadius, float opacity) {
    const Color c = Filter(color);
    if (!mask.blurTried) BlurMask(mask, blurRadius);
    if (mask.blurred) {
        // FillOpacityMask: the blurred mask's alpha times the glow color, a single textured quad
        const auto size = mask.blurred->GetSize();
        const D2D1_RECT_F dest = D2D1::RectF(x - mask.pad, y - mask.pad, x - mask.pad + size.width, y - mask.pad + size.height);
        solid->SetColor(c.Fade(opacity).ToD2D());
        solid->SetOpacity(1.f);
        const D2D1_ANTIALIAS_MODE antialias = dc->GetAntialiasMode();
        dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);   // required by FillOpacityMask
        dc->FillOpacityMask(mask.blurred.Get(), solid.Get(), &dest, nullptr);
        dc->SetAntialiasMode(antialias);
        return;
    }
    // Fallback: blur every frame
    if (!shadowEffect && FAILED(dc->CreateEffect(CLSID_D2D1Shadow, &shadowEffect))) return;
    shadowEffect->SetInput(0, mask.bitmap.Get());
    shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(c.r, c.g, c.b, c.a * opacity));
    shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, std::max(0.1f, blurRadius / 3.f));
    dc->DrawImage(shadowEffect.Get(), D2D1::Point2F(x - mask.pad, y - mask.pad));
}

void Graphics::GlowRound(const Rect& r, float radius, Color color, float blurRadius, float opacity) {
    blurRadius = QuantizeBlur(blurRadius);
    if (r.Empty() || opacity <= 0.f) return;
    std::wstring key(1, L'r');
    KeyAppend(key, r.w, 10.f);
    KeyAppend(key, r.h, 10.f);
    KeyAppend(key, radius, 10.f);
    KeyAppend(key, blurRadius, 10.f);
    const float pad = blurRadius * 1.5f + 2.f;
    GlowMask* mask = Mask(key, r.w, r.h, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush* white) {
        rc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, r.w, r.h), radius, radius), white);
    });
    if (mask) DrawGlow(*mask, r.x, r.y, color, blurRadius, opacity);
}

void Graphics::GlowText(const std::wstring& s, const Rect& r, const TextStyle& style, Color color, float blurRadius, float opacity) {
    blurRadius = QuantizeBlur(blurRadius);
    if (s.empty() || r.Empty() || opacity <= 0.f) return;
    IDWriteTextLayout* layout = Layout(s, style, r.w, r.h);
    if (!layout) return;
    std::wstring header(1, L't');
    KeyAppend(header, r.w, 10.f);
    KeyAppend(header, r.h, 10.f);
    KeyAppend(header, style.size, 10.f);
    KeyAppend(header, blurRadius, 10.f);
    KeyAppend(header, static_cast<uint32_t>(style.weight));
    const float pad = blurRadius * 1.5f + 2.f;
    ComPtr<IDWriteTextLayout> keep = layout;
    GlowMask* mask = Mask(header + s, r.w, r.h, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush* white) {
        rc->DrawTextLayout(D2D1::Point2F(0, 0), keep.Get(), white);
    });
    if (mask) DrawGlow(*mask, r.x, r.y, color, blurRadius, opacity);
}

void Graphics::GlowImage(const Image& image, const Rect& r, Color color, float blurRadius, float opacity) {
    blurRadius = QuantizeBlur(blurRadius);
    if (!image.bitmap || r.Empty() || opacity <= 0.f) return;
    std::wstring key(1, L'i');
    const uint64_t bitmapId = reinterpret_cast<uint64_t>(image.bitmap.Get());
    KeyAppend(key, static_cast<uint32_t>(bitmapId));
    KeyAppend(key, static_cast<uint32_t>(bitmapId >> 32));
    KeyAppend(key, r.w, 10.f);
    KeyAppend(key, r.h, 10.f);
    KeyAppend(key, blurRadius, 10.f);
    const float pad = blurRadius * 1.5f + 2.f;
    GlowMask* mask = Mask(key, r.w, r.h, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush*) {
        // The shadow only uses the alpha channel, so the image itself is the mask
        rc->DrawBitmap(image.bitmap.Get(), D2D1::RectF(0, 0, r.w, r.h), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    });
    if (mask) DrawGlow(*mask, r.x, r.y, color, blurRadius, opacity);
}

void Graphics::GlowRing(float cx, float cy, float radius, float thickness, Color color, float blurRadius, float opacity) {
    blurRadius = QuantizeBlur(blurRadius);
    if (radius <= 0.f || opacity <= 0.f) return;
    std::wstring key(1, L'o');
    KeyAppend(key, radius, 10.f);
    KeyAppend(key, thickness, 10.f);
    KeyAppend(key, blurRadius, 10.f);
    const float pad = blurRadius * 1.5f + 2.f;
    const float d = radius * 2.f;
    GlowMask* mask = Mask(key, d, d, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush* white) {
        const float r = radius - thickness * 0.5f;
        rc->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(radius, radius), r, r), white, thickness);
    });
    if (mask) DrawGlow(*mask, cx - radius, cy - radius, color, blurRadius, opacity);
}

void Graphics::Shadow(const Rect& r, float radius, float blur, Color c) {
    // Stacked translucent rounded rects approximate a gaussian falloff
    const int steps = 6;
    for (int i = steps; i >= 1; --i) {
        const float grow = blur * i / steps;
        FillRound(r.Inset(-grow), radius + grow, c.Fade(1.f / (steps + 2)));
    }
}

} // namespace CustomCursor
