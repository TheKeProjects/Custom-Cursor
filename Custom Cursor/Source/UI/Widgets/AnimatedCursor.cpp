#include "UI/Widgets/AnimatedCursor.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include "App/Config.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Graphics/CursorImage.h"
#include "Graphics/Images.h"
#include "Services/Cursors/AniParser.h"

namespace CustomCursor::UI::AnimatedCursor {

namespace {

namespace Cursors = Services::Cursors;

// What a worker decodes: cursor frames or a picture's frames, as pixels
struct Decoded {
    std::vector<CursorImage::Bitmap> frames;
    Images::AnimationPixels picture;
    double frameSeconds = Config::CursorFrameMs / 1000.0;
};

struct Entry {
    enum class State { Loading, Ready, Failed } state = State::Loading;
    std::vector<ImagePtr> frames;   // cursors
    AnimationPtr picture;           // preview pictures (GIF frames included)
    double frameSeconds = Config::CursorFrameMs / 1000.0;
    double startedAt = -1.0;        // first drawn (the animation starts at frame 0 then)
};

std::map<std::string, std::map<std::wstring, std::shared_ptr<Entry>>> groups;
std::map<std::string, unsigned> generations;   // bumped by Release: late decodes of a released group are dropped
std::function<void()> tickHandler;
Async::TimerId timer = 0;
unsigned timerMs = 0;
double lastAnimated = -10.0, lastPicture = -10.0, lastAmbient = -10.0;
bool ambientPaused = false;

constexpr unsigned kWorkers = 4;
constexpr unsigned kPictureSide = 160;   // the cards show pictures about 75 px tall (DecodePixelWidth="100" in the C#)

bool IsPicture(const std::wstring& ext) { return ext == L".gif" || ext == L".png" || ext == L".jpg" || ext == L".jpeg"; }

// LoadAnimatedCursor: the frames with skipping for heavy animations, or the first frame alone
void DecodeAni(const std::string& bytes, Decoded& out) {
    const auto ani = Cursors::AniParser::Parse(bytes);
    if (ani && ani->frameCount > 0 && !ani->frames.empty()) {
        const size_t count = ani->frames.size();
        const size_t maxFrames = Config::CursorFullAnimation ? count : static_cast<size_t>(Config::CursorMaxFrames);
        const size_t step = count > maxFrames ? (count + maxFrames - 1) / maxFrames : 1;
        for (size_t i = 0; i < count; i += step)
            if (auto bitmap = CursorImage::Decode(ani->frames[i])) out.frames.push_back(CursorImage::Downscale(*bitmap, Config::CursorMaxSize));
        if (Config::CursorFullAnimation && ani->defaultRateMs > 0) out.frameSeconds = ani->defaultRateMs / 1000.0;
    }
    // Fallback: the first frame (LoadStaticCursor)
    if (out.frames.empty() && ani)
        for (const auto& frame : ani->frames)
            if (auto bitmap = CursorImage::Decode(frame)) {
                out.frames.push_back(CursorImage::Downscale(*bitmap, Config::CursorMaxSize));
                break;
            }
}

// Worker thread: the file's pixels
Decoded Decode(const std::wstring& path) {
    Decoded out;
    const std::wstring ext = Text::Lower(Files::Extension(path));
    if (IsPicture(ext)) {
        Images::DecodeAnimationPixels(path, kPictureSide, out.picture);
        return out;
    }
    const auto bytes = Files::ReadBytes(path);
    if (!bytes) return out;
    if (ext == L".ani") DecodeAni(*bytes, out);
    else if (ext == L".cur" || ext == L".ico")
        if (auto bitmap = CursorImage::Decode(*bytes)) out.frames.push_back(std::move(*bitmap));
    return out;
}

// UI thread: the pixels made into bitmaps
void Finish(Entry& entry, const Decoded& decoded) {
    for (const auto& frame : decoded.frames)
        if (auto image = Images::FromPixels(frame.width, frame.height, frame.pixels.data())) entry.frames.push_back(image);
    if (!decoded.picture.frames.empty()) entry.picture = Images::FromAnimationPixels(decoded.picture);
    entry.frameSeconds = decoded.frameSeconds;
    entry.state = entry.frames.empty() && !entry.picture ? Entry::State::Failed : Entry::State::Ready;
}

// Decodes `paths` on up to kWorkers threads; each result is posted back to its entry, then `done`
void Load(std::vector<std::wstring> paths, const std::string& group, std::function<void()> done) {
    if (paths.empty()) {
        if (done) Async::Post(std::move(done));
        return;
    }
    struct Job {
        std::vector<std::wstring> paths;
        std::atomic<size_t> next{ 0 };
        std::atomic<size_t> left;
        std::string group;
        unsigned generation = 0;
        std::function<void()> done;
    };
    auto job = std::make_shared<Job>();
    job->left = paths.size();
    job->paths = std::move(paths);
    job->group = group;
    job->generation = generations[group];
    job->done = std::move(done);
    const unsigned workers = std::min<unsigned>(kWorkers, static_cast<unsigned>(job->paths.size()));
    for (unsigned w = 0; w < workers; ++w)
        Async::Run([job] {
            for (size_t i = job->next++; i < job->paths.size(); i = job->next++) {
                auto decoded = std::make_shared<Decoded>(Decode(job->paths[i]));
                Async::Post([job, i, decoded] {
                    if (generations[job->group] == job->generation) {
                        auto& slot = groups[job->group][job->paths[i]];
                        if (slot && slot->state == Entry::State::Loading) Finish(*slot, *decoded);
                        if (tickHandler) tickHandler();
                    }
                    if (--job->left == 0 && job->done) job->done();
                });
            }
        });
}

// The shared timer: ticks while something animated was drawn in the last half second, every 150 ms
// for cursors alone, every 50 ms while a GIF is showing, every 33 ms for the ambient decoration
void KeepTicking(double now, bool picture, bool ambient = false) {
    lastAnimated = now;
    if (picture) lastPicture = now;
    if (ambient) lastAmbient = now;
    const unsigned wanted = now - lastAmbient < 0.5 ? 33u : now - lastPicture < 0.5 ? 50u : static_cast<unsigned>(Config::CursorFrameMs);
    if (timer && timerMs == wanted) return;
    if (timer) Async::CancelTimer(timer);
    timerMs = wanted;
    timer = Async::Every(wanted, [] {
        if (Async::Now() - lastAnimated > 0.5) {
            Async::CancelTimer(timer);
            timer = 0;
            return;
        }
        if (tickHandler) tickHandler();
    });
}

} // namespace

void Preload(const std::vector<std::wstring>& paths, const char* group, std::function<void()> done) {
    std::vector<std::wstring> missing;
    auto& entries = groups[group];
    for (const auto& path : paths) {
        auto& slot = entries[path];
        if (slot) continue;
        slot = std::make_shared<Entry>();
        missing.push_back(path);
    }
    Load(std::move(missing), group, std::move(done));
}

bool Draw(Ui& ui, const std::wstring& path, const Rect& r, float opacity, const char* group) {
    auto& slot = groups[group][path];
    if (!slot) {
        slot = std::make_shared<Entry>();
        Load({ path }, group, {});
    }
    Entry& entry = *slot;
    if (entry.state == Entry::State::Failed) return false;
    if (entry.state == Entry::State::Loading) return true;
    const double now = Async::Now();
    if (entry.startedAt < 0.0) entry.startedAt = now;
    const double elapsed = now - entry.startedAt;
    ImagePtr image;
    if (entry.picture) {
        image = entry.picture->FrameAt(elapsed);
        if (entry.picture->Animated()) KeepTicking(now, true);
    } else {
        image = entry.frames[static_cast<size_t>(elapsed / entry.frameSeconds) % entry.frames.size()];
        if (entry.frames.size() > 1) KeepTicking(now, false);
    }
    if (image) ui.g.DrawImage(*image, r, opacity, Stretch::Uniform);
    return true;
}

void Release(const char* group) {
    groups.erase(group);
    ++generations[group];
}

void RequestAmbientTicks() {
    if (!ambientPaused) KeepTicking(Async::Now(), false, true);
}

void SetAmbientPaused(bool paused) { ambientPaused = paused; }

void SetTickHandler(std::function<void()> onTick) { tickHandler = std::move(onTick); }

} // namespace CustomCursor::UI::AnimatedCursor
