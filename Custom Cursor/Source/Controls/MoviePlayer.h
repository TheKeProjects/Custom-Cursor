#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>

namespace CustomCursor::Controls {

// The tutorial's MediaElement: a plain <video> (a local mp4, with its own controls: a click pauses /
// plays it, it stops at the end) hosted in WebView2, in a borderless popup over our Direct2D window
// (only shown while nothing is drawn over it). The page comes from a virtual host
// (a data: page loads but never shows the video) and the video skips the hardware overlay plane,
// which our DirectComposition windows don't show.
class MoviePlayer {
public:
    MoviePlayer();
    ~MoviePlayer();
    MoviePlayer(const MoviePlayer&) = delete;
    MoviePlayer& operator=(const MoviePlayer&) = delete;

    std::function<void()> onReady;    // the page is there (it draws its own loading animation from then on)
    std::function<void()> onLoaded;   // the video faded in (after each Load)
    std::function<void(const std::wstring&)> onError;

    // Starts WebView2 asynchronously over `parent`; the files of `mediaFolder` are served as
    // MediaUrl(<file>). Errors come through onError.
    void Create(HWND parent, const std::wstring& mediaFolder = L"");
    static std::wstring MediaUrl(const std::wstring& fileName);
    void Destroy();
    bool Created() const;
    // Client rect of the parent, in pixels; also follows the parent when it moves
    void SetBounds(const RECT& physical);
    void SetVisible(bool visible);

    // `cssColor` ("#RRGGBB") tints the page's searching animation, `loadingText` is shown under it
    void Load(const std::wstring& mp4Url, const std::wstring& cssColor, const std::wstring& loadingText);
    bool Ready() const { return pageReady; }
    void Stop();   // pauses and drops the video

private:
    struct Impl;
    void Run(const std::wstring& script);

    std::unique_ptr<Impl> impl;
    bool pageReady = false;
    std::wstring pending;   // the load() call waiting for the page
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

} // namespace CustomCursor::Controls
