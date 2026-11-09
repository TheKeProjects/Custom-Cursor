#ifdef _DEBUG
#include "App/DevTools.h"
#include <windows.h>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Data/CursorRoles.h"
#include "Graphics/CursorImage.h"
#include "Models/SchemeJson.h"
#include "Services/App/UpdateService.h"
#include "Services/Cursors/AniParser.h"
#include "Services/Cursors/Registry.h"
#include "Services/Cursors/SchemeQueries.h"
#include "Services/Cursors/SchemeScanner.h"

namespace CustomCursor::DevTools {

namespace {

namespace Cursors = Services::Cursors;

std::wstring Variable(const wchar_t* name) {
    wchar_t value[2048]{};
    return GetEnvironmentVariableW(name, value, 2048) > 0 ? std::wstring(value) : std::wstring();
}

// "frames:<n> rate:<ms> first:<w>x<h>" for an .ani, "<w>x<h>" for a .cur / .ico, "fail" when it can't be read
std::wstring DescribeCursor(const std::wstring& path) {
    const auto bytes = Files::ReadBytes(path);
    if (!bytes) return L"missing";
    if (Text::EndsWith(path, L".ani", true)) {
        const auto ani = Cursors::AniParser::Parse(*bytes);
        if (!ani) return L"parse-error";
        std::wstring out = L"count:" + std::to_wstring(ani->frameCount) + L" frames:" + std::to_wstring(ani->frames.size()) + L" rate:" +
                           std::to_wstring(ani->defaultRateMs);
        int decoded = 0;
        for (const auto& frame : ani->frames)
            if (CursorImage::Decode(frame)) ++decoded;
        out += L" decoded:" + std::to_wstring(decoded);
        if (!ani->frames.empty())
            if (auto first = CursorImage::Decode(ani->frames.front())) out += L" first:" + std::to_wstring(first->width) + L"x" + std::to_wstring(first->height);
        return out;
    }
    const auto image = CursorImage::Decode(*bytes);
    return image ? std::to_wstring(image->width) + L"x" + std::to_wstring(image->height) : L"fail";
}

void DumpSchemes(const std::wstring& spec) {
    const auto parts = Text::Split(spec, L'|');
    if (parts.size() != 2) return;
    const auto categories = Cursors::SchemeScanner::FindCursorSets(parts[0]);
    Json::Value root = Json::Value::MakeArray();
    std::vector<Models::CursorScheme> all;
    for (const auto& category : categories) {
        Json::Value c = Json::Value::MakeObject();
        c.Set("Category", category.name);
        Json::Value schemes = Json::Value::MakeArray();
        for (const auto& scheme : category.schemes) {
            Json::Value s = Models::SchemeJson::ToJson(scheme);
            s.Set("Sanitized", Paths::SanitizeSchemeName(scheme.name));
            s.Set("Count", Cursors::SchemeScanner::CountChangedCursors(scheme));
            const auto preview = Cursors::SchemeScanner::PreviewPath(scheme);
            s.Set("PreviewPath", preview ? Json::Value(*preview) : Json::Value(nullptr));
            Json::Value infos = Json::Value::MakeArray();
            for (const auto& info : Cursors::SchemeScanner::LoadCursorInfos(scheme)) {
                Json::Value i = Json::Value::MakeObject();
                i.Set("Key", info.key);
                i.Set("FileName", info.fileName);
                i.Set("Decoded", DescribeCursor(Files::DotNetCombine(scheme.path, info.fileName)));
                infos.Push(std::move(i));
            }
            s.Set("Infos", std::move(infos));
            schemes.Push(std::move(s));
            all.push_back(scheme);
        }
        c.Set("Schemes", std::move(schemes));
        root.Push(std::move(c));
    }
    Files::WriteBytes(parts[1], root.Serialize(true));
    Files::WriteBytes(parts[1] + L".install.json", Models::SchemeJson::SerializeInstall(all));
}

void DumpRegistry(const std::wstring& out) {
    Json::Value root = Json::Value::MakeObject();
    for (const wchar_t* path : { Config::CursorsKey, Config::SchemesKey }) {
        const auto key = Cursors::RegistryKey::Open(path, false);
        Json::Value values = Json::Value::MakeObject();
        for (const auto& name : key.ValueNames()) {
            HKEY raw = nullptr;
            DWORD type = 0;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &raw) == ERROR_SUCCESS) {
                RegQueryValueExW(raw, name.c_str(), nullptr, &type, nullptr, nullptr);
                RegCloseKey(raw);
            }
            // The raw text (not expanded) with its type
            DWORD size = 0;
            std::wstring text;
            if (RegGetValueW(HKEY_CURRENT_USER, path, name.c_str(), RRF_RT_ANY | RRF_NOEXPAND, nullptr, nullptr, &size) == ERROR_SUCCESS && size) {
                std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
                if (RegGetValueW(HKEY_CURRENT_USER, path, name.c_str(), RRF_RT_ANY | RRF_NOEXPAND, nullptr, buffer.data(), &size) == ERROR_SUCCESS) {
                    if (type == REG_DWORD) text = std::to_wstring(*reinterpret_cast<const DWORD*>(buffer.data()));
                    else text = buffer.c_str();
                }
            }
            values.Set(Text::Narrow(name.empty() ? L"(Default)" : name), std::to_wstring(type) + L":" + text);
        }
        root.Set(Text::Narrow(path), std::move(values));
    }
    const auto active = Cursors::SchemeQueries::GetActiveScheme();
    root.Set("Active", active ? Json::Value(*active) : Json::Value(nullptr));
    Json::Value installed = Json::Value::MakeArray();
    for (const auto& name : Cursors::SchemeQueries::GetInstalledSchemes()) installed.Push(name);
    root.Set("Installed", std::move(installed));
    Files::WriteBytes(out, root.Serialize(true));
}

// Every frame of each listed cursor file (one path per line) as raw BGRA: per frame a little-endian
// width, height and the premultiplied pixels, frames of one file in a row, files one after the other
void DumpPixels(const std::wstring& spec) {
    const auto parts = Text::Split(spec, L'|');
    if (parts.size() != 2) return;
    std::string out;
    const auto put = [&out](uint32_t v) { out.append(reinterpret_cast<const char*>(&v), 4); };
    for (const auto& path : Files::ReadLines(parts[0])) {
        std::vector<CursorImage::Bitmap> frames;
        if (const auto bytes = Files::ReadBytes(path)) {
            if (Text::EndsWith(path, L".ani", true)) {
                if (const auto ani = Cursors::AniParser::Parse(*bytes))
                    for (const auto& frame : ani->frames)
                        if (auto image = CursorImage::Decode(frame)) frames.push_back(CursorImage::Downscale(*image, Config::CursorMaxSize));
            } else if (auto image = CursorImage::Decode(*bytes)) {
                frames.push_back(*image);
            }
        }
        put(static_cast<uint32_t>(frames.size()));
        for (const auto& f : frames) {
            put(f.width);
            put(f.height);
            out.append(reinterpret_cast<const char*>(f.pixels.data()), f.pixels.size() * 4);
        }
    }
    Files::WriteBytes(parts[1], out);
}

void Sanitize(const std::wstring& spec) {
    const auto parts = Text::Split(spec, L'|');
    if (parts.size() != 2) return;
    std::wstring out;
    for (const auto& line : Files::ReadLines(parts[0])) out += Paths::SanitizeSchemeName(line) + L"\n";
    Files::WriteText(parts[1], out);
}

} // namespace

bool Run(int& exitCode) {
    exitCode = 0;
    bool ran = false;
    if (const auto spec = Variable(L"CC_DUMP_SCHEMES"); !spec.empty()) {
        DumpSchemes(spec);
        ran = true;
    }
    if (const auto out = Variable(L"CC_DUMP_REGISTRY"); !out.empty()) {
        DumpRegistry(out);
        ran = true;
    }
    if (const auto spec = Variable(L"CC_DUMP_PIXELS"); !spec.empty()) {
        DumpPixels(spec);
        ran = true;
    }
    if (const auto url = Variable(L"CC_TEST_UPDATE"); !url.empty()) {
        // The automatic update from that URL (a local server's zip): exits on success
        Services::Updates::UpdateInfo info;
        info.downloadUrl = url;
        info.fileName = Files::FileName(url);
        info.latestVersion = L"test";
        exitCode = Services::Updates::DownloadAndInstall(info, {}) ? 0 : 3;
        return true;
    }
    if (const auto spec = Variable(L"CC_SANITIZE"); !spec.empty()) {
        Sanitize(spec);
        ran = true;
    }
    return ran;
}

} // namespace CustomCursor::DevTools
#endif
