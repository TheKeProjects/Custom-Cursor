#include "Services/App/ResourceExtractor.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Generated/AssetTable.h"

namespace CustomCursor::Services::ResourceExtractor {

namespace {

bool ResourceBytes(int id, const void*& data, DWORD& size) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return false;
    HGLOBAL handle = LoadResource(nullptr, res);
    if (!handle) return false;
    data = LockResource(handle);
    size = SizeofResource(nullptr, res);
    return data != nullptr;
}

// The schemes and the tutorial: almost every byte, unpacked behind the loading screen
bool IsHeavy(const EmbeddedAsset& asset) {
    return Text::Equals(asset.root, Config::CursorFolderName, true) || Text::Equals(asset.root, Config::TutorialFolderName, true);
}

// A stamp of the exe build + embedded content: when it matches the previous run, files whose size
// still matches are left alone instead of being rewritten on every start.
std::wstring CurrentStamp() {
    const std::wstring exe = Files::ExecutablePath();
    return std::wstring(kEmbeddedAssetsStamp) + L"|" + std::to_wstring(Files::Size(exe).value_or(0)) + L"|" +
           std::to_wstring(Files::LastWriteTime(exe).value_or(0));
}

std::wstring StampFile() { return Files::Combine(Paths::TempDir(), L".extracted"); }

// Whether the folder holds this build's files: decided once, before the first file is written (a
// different build empties the folder first, so no file of another version is ever used)
bool SameBuild() {
    static const bool same = [] {
        const bool match = Files::ReadText(StampFile()).value_or(L"") == CurrentStamp();
        if (!match) Files::DeleteDirectory(Paths::TempDir());
        Files::CreateDirectories(Paths::TempDir());
        return match;
    }();
    return same;
}

void ExtractOne(const EmbeddedAsset& asset, const std::wstring& folder, bool sameBuild) {
    const void* data = nullptr;
    DWORD size = 0;
    if (!ResourceBytes(asset.id, data, size)) return;
    const std::wstring dest = Files::Combine(folder, asset.root, asset.path);
    if (sameBuild && Files::Size(dest).value_or(~0ull) == size) return;
    Files::WriteBytes(dest, data, size);
}

std::atomic<bool> ready{ false };
std::atomic<bool> stopping{ false };
std::atomic<float> progress{ 0.f };
HANDLE finished = nullptr;   // set when the background part has returned

} // namespace

void ExtractLight() {
    const bool same = SameBuild();
    for (const auto& asset : kEmbeddedAssets)
        if (!IsHeavy(asset)) ExtractOne(asset, Paths::TempDir(), same);
}

void ExtractHeavyInBackground(std::function<void()> done) {
    finished = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const bool same = SameBuild();
    Async::Run([done = std::move(done), folder = Paths::TempDir(), same, finishedEvent = finished]() mutable {
        size_t total = 0, count = 0;
        for (const auto& asset : kEmbeddedAssets)
            if (IsHeavy(asset)) ++total;
        for (const auto& asset : kEmbeddedAssets) {
            if (stopping) break;
            if (!IsHeavy(asset)) continue;
            ExtractOne(asset, folder, same);
            progress = static_cast<float>(++count) / static_cast<float>(std::max<size_t>(total, 1));
        }
        const bool stopped = stopping;
        // Only a complete extraction is stamped: one cut short starts clean next time
        if (!stopped) Files::WriteText(StampFile(), CurrentStamp());
        SetEvent(finishedEvent);
        if (stopped) return;
        Async::Post([done = std::move(done)] {
            ready = true;
            progress = 1.f;
            if (done) done();
        });
    });
}

void StopBackground() {
    if (!finished) return;
    stopping = true;
    WaitForSingleObject(finished, 5000);
}

bool Ready() { return ready; }
float Progress() { return progress; }

std::optional<std::string> ReadEmbedded(const std::wstring& root, const std::wstring& path) {
    for (const auto& asset : kEmbeddedAssets) {
        if (!Text::Equals(asset.root, root, true) || !Text::Equals(asset.path, path, true)) continue;
        const void* data = nullptr;
        DWORD size = 0;
        if (!ResourceBytes(asset.id, data, size)) return std::nullopt;
        return std::string(static_cast<const char*>(data), size);
    }
    return std::nullopt;
}

} // namespace CustomCursor::Services::ResourceExtractor
