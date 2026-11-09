#include "Controls/MoviePlayer.h"
#include <cstring>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace CustomCursor::Controls {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace {

constexpr const wchar_t* kVirtualHost = L"ccplayer.local";
// The tutorial folder (Create's mediaFolder): the video is https://ccmedia.local/<file>
constexpr const wchar_t* kMediaHost = L"ccmedia.local";

// load(src, color, text) swaps the video; "ready", "loaded" (the video faded in) and "error" go back
// through postMessage. Chromium paints the first frames of a video unscaled (small, in the corner)
// for a moment, so the video first plays muted and transparent behind a searching animation in the
// element's colour, then pauses, goes back to the start and fades in playing. No poster either.
constexpr const char* kPlayerHtml = R"html(<!DOCTYPE html>
<html><head><meta charset="utf-8"><style>
html,body{margin:0;padding:0;background:#000;overflow:hidden;height:100%;font-family:"Segoe UI",sans-serif}
video{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;background:#000;outline:none;opacity:0;transition:opacity .25s ease}
video.on{opacity:1}
#load{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:22px;color:#fff;transition:opacity .2s ease;
  background:radial-gradient(circle at 50% 46%,color-mix(in srgb,var(--c) 32%,transparent) 0,transparent 170px)}
#load.off{opacity:0;pointer-events:none}
.spin{position:relative;width:84px;height:84px}
.spin i{position:absolute;left:50%;top:50%;width:9px;height:9px;margin:-4.5px;border-radius:50%;
  background:color-mix(in srgb,var(--c) 65%,#fff);animation:chase 1s linear infinite}
@keyframes chase{0%{opacity:1;scale:1}100%{opacity:.2;scale:.45}}
.mag{position:absolute;left:50%;top:50%;width:28px;height:28px;margin:-14px;animation:sweep 2.1s linear infinite}
@keyframes sweep{0%{transform:rotate(0deg) translateX(6px) rotate(0deg) rotate(-12deg)}50%{transform:rotate(180deg) translateX(6px) rotate(-180deg) rotate(12deg)}
  100%{transform:rotate(360deg) translateX(6px) rotate(-360deg) rotate(-12deg)}}
#txt{font-size:15px;font-weight:600;opacity:.88}
#txt::after{content:"";display:inline-block;width:1.2em;text-align:left;animation:dots 1.6s steps(4) infinite}
@keyframes dots{0%{content:""}25%{content:"."}50%{content:".."}75%{content:"..."}}
</style></head>
<body>
<video id="v" playsinline controls controlslist="nodownload noplaybackrate" disablepictureinpicture></video>
<div id="load"><div class="spin" id="spin"><svg class="mag" viewBox="0 0 24 24" fill="#fff"><path d="M9.5,3A6.5,6.5 0 0,1 16,9.5C16,11.11 15.41,12.59 14.44,13.73L14.71,14H15.5L20.5,19L19,20.5L14,15.5V14.71L13.73,14.44C12.59,15.41 11.11,16 9.5,16A6.5,6.5 0 0,1 3,9.5A6.5,6.5 0 0,1 9.5,3M9.5,5C7,5 5,7 5,9.5C5,12 7,14 9.5,14C12,14 14,12 14,9.5C14,7 12,5 9.5,5Z"/></svg></div><div id="txt"></div></div>
<script>
  var v = document.getElementById('v'), load_ = document.getElementById('load'), spin = document.getElementById('spin');
  v.volume = 0.5;   // MediaElement's default volume
  // MediaEnded: Stop() (back to the first frame, paused)
  v.addEventListener('ended', function () { v.pause(); v.currentTime = 0; });
  // The ring: 12 dots, each one starting its fade a step later (a bright head chasing round)
  for (var i = 0; i < 12; ++i) {
    var d = document.createElement('i'), a = i / 12 * 2 * Math.PI - Math.PI / 2;
    d.style.transform = 'translate(' + (34 * Math.cos(a)) + 'px,' + (34 * Math.sin(a)) + 'px)';
    d.style.animationDelay = (i / 12 - 1) + 's';
    spin.insertBefore(d, spin.firstChild);
  }
  function post(s) { window.chrome.webview.postMessage(s); }
  // Each video goes through: "warm" (playing muted and transparent while Chromium paints its first,
  // unscaled frames) -> "rewind" (paused, back to 0:00) -> "show" (the searching screen goes, the
  // video plays from the start with its sound)
  var phase = 'show', timer = 0;
  function rewind() {
    if (phase !== 'warm') return;
    phase = 'rewind';
    clearTimeout(timer);
    v.pause();
    if (v.currentTime === 0) show(); else v.currentTime = 0;
  }
  function show() {
    if (phase === 'show') return;
    phase = 'show';
    clearTimeout(timer);
    v.muted = false;
    v.classList.add('on');
    load_.classList.add('off');
    v.play().catch(function () {});
    post('loaded');
  }
  v.addEventListener('timeupdate', function () { if (phase === 'warm' && v.currentTime >= 0.35) rewind(); });
  v.addEventListener('seeked', function () { if (phase === 'rewind') show(); });
  // Slow or stuck start: rewind anyway once it can play
  v.addEventListener('canplay', function () { if (phase === 'warm') { clearTimeout(timer); timer = setTimeout(rewind, 2500); } });
  v.addEventListener('error', function () { if (v.getAttribute('src')) post('error'); });
  document.addEventListener('contextmenu', function (e) { e.preventDefault(); });
  function load(src, color, text) {
    phase = 'warm';
    clearTimeout(timer);
    document.body.style.setProperty('--c', color);
    document.getElementById('txt').textContent = text;
    v.classList.remove('on');
    load_.classList.remove('off');
    v.muted = true;
    v.src = src;
    v.play().catch(function () {});
  }
  function stop() { phase = 'show'; clearTimeout(timer); v.pause(); v.removeAttribute('src'); v.load(); }
  post('ready');
</script>
</body></html>
)html";

std::wstring JsString(const std::wstring& s) {
    std::wstring out = L"'";
    for (wchar_t c : s) {
        if (c == L'\\' || c == L'\'') out += L'\\';
        out += c;
    }
    return out + L"'";
}

// A child HWND isn't composited inside our DirectComposition windows, so the WebView lives in a
// borderless popup owned by the window it decorates
LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_SIZE) {
        auto* controller = reinterpret_cast<ICoreWebView2Controller*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (controller) {
            RECT r{ 0, 0, LOWORD(lp), HIWORD(lp) };
            controller->put_Bounds(r);
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND CreateHost(HWND owner) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = HostProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CustomCursor.MovieHost";
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        RegisterClassW(&wc);
        registered = true;
    }
    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"CustomCursor.MovieHost", L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
}

} // namespace

struct MoviePlayer::Impl {
    HWND parent = nullptr;
    HWND host = nullptr;
    RECT bounds{};   // client coordinates of the parent
    POINT origin{ LONG_MIN, LONG_MIN };
    bool visible = false;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
};

MoviePlayer::MoviePlayer() : impl(std::make_unique<Impl>()) {}

MoviePlayer::~MoviePlayer() { Destroy(); }

bool MoviePlayer::Created() const { return impl->host != nullptr; }

void MoviePlayer::Create(HWND parent, const std::wstring& mediaFolder) {
    if (impl->host) return;
    *alive = true;
    impl->parent = parent;
    impl->host = CreateHost(parent);
    const std::wstring appData = Paths::DataDir();
    // A user data folder of its own, next to language.cfg
    const std::wstring userData = Files::Combine(appData, L"WebView2Movies");
    const std::wstring site = Files::Combine(appData, L"WebView2Movie");
    Files::CreateDirectories(site);
    Files::WriteBytes(Files::Combine(site, L"player.html"), std::string(kPlayerHtml));

    auto options = Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(L"--disable-features=DirectCompositionVideoOverlays --autoplay-policy=no-user-gesture-required");

    auto guard = alive;
    const auto fail = [this, guard] {
        if (*guard && onError) onError(L"WebView2");
    };
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([this, guard, site, mediaFolder, fail](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (!*guard) return S_OK;
            if (FAILED(result) || !env) {
                fail();
                return S_OK;
            }
            return env->CreateCoreWebView2Controller(
                impl->host,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([this, guard, site, mediaFolder, fail](HRESULT r, ICoreWebView2Controller* controller) -> HRESULT {
                    if (!*guard) {
                        if (controller) controller->Close();
                        return S_OK;
                    }
                    if (FAILED(r) || !controller) {
                        fail();
                        return S_OK;
                    }
                    impl->controller = controller;
                    SetWindowLongPtrW(impl->host, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
                    controller->get_CoreWebView2(&impl->webview);
                    ComPtr<ICoreWebView2Controller2> c2;
                    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&c2)))) c2->put_DefaultBackgroundColor({ 255, 0, 0, 0 });
                    RECT client{};
                    GetClientRect(impl->host, &client);
                    controller->put_Bounds(client);
                    controller->put_IsVisible(TRUE);

                    ComPtr<ICoreWebView2Settings> settings;
                    if (SUCCEEDED(impl->webview->get_Settings(&settings))) {
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_AreDevToolsEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                    }
                    ComPtr<ICoreWebView2_3> wv3;
                    if (SUCCEEDED(impl->webview->QueryInterface(IID_PPV_ARGS(&wv3))))
                        {
                        wv3->SetVirtualHostNameToFolderMapping(kVirtualHost, site.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                        if (!mediaFolder.empty())
                            wv3->SetVirtualHostNameToFolderMapping(kMediaHost, mediaFolder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                    }

                    EventRegistrationToken token{};
                    impl->webview->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>([this, guard](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            if (!*guard) return S_OK;
                            LPWSTR message = nullptr;
                            if (FAILED(args->TryGetWebMessageAsString(&message)) || !message) return S_OK;
                            const std::wstring text = message;
                            CoTaskMemFree(message);
                            if (text == L"ready") {
                                pageReady = true;
                                if (onReady) onReady();
                                if (!pending.empty()) {
                                    Run(pending);
                                    pending.clear();
                                }
                            } else if (text == L"loaded") {
                                if (onLoaded) onLoaded();
                            } else if (text == L"error") {
                                if (onError) onError(L"video");
                            }
                            return S_OK;
                        }).Get(),
                        &token);
                    impl->webview->Navigate((std::wstring(L"https://") + kVirtualHost + L"/player.html").c_str());
                    return S_OK;
                }).Get());
        }).Get());
    if (FAILED(hr)) fail();
}

std::wstring MoviePlayer::MediaUrl(const std::wstring& fileName) {
    // Each folder of a relative path encoded on its own, joined with '/'
    std::wstring url = std::wstring(L"https://") + kMediaHost;
    for (const auto& part : Text::Split(fileName, L'\\', true)) url += L"/" + Text::UrlEncode(part);
    return url;
}

void MoviePlayer::Destroy() {
    *alive = false;
    pageReady = false;
    pending.clear();
    if (impl && impl->controller) {
        SetWindowLongPtrW(impl->host, GWLP_USERDATA, 0);
        impl->controller->Close();
        impl->controller.Reset();
        impl->webview.Reset();
    }
    if (impl && impl->host) {
        DestroyWindow(impl->host);
        impl->host = nullptr;
    }
}

void MoviePlayer::SetBounds(const RECT& physical) {
    if (!impl->host) return;
    POINT origin{ physical.left, physical.top };
    ClientToScreen(impl->parent, &origin);
    if (std::memcmp(&physical, &impl->bounds, sizeof(RECT)) == 0 && origin.x == impl->origin.x && origin.y == impl->origin.y) return;
    impl->bounds = physical;
    impl->origin = origin;
    SetWindowPos(impl->host, nullptr, origin.x, origin.y, physical.right - physical.left, physical.bottom - physical.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

void MoviePlayer::SetVisible(bool on) {
    if (!impl->host || on == impl->visible) return;
    impl->visible = on;
    ShowWindow(impl->host, on ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void MoviePlayer::Run(const std::wstring& script) {
    if (impl->webview) impl->webview->ExecuteScript(script.c_str(), nullptr);
}

void MoviePlayer::Load(const std::wstring& mp4Url, const std::wstring& cssColor, const std::wstring& loadingText) {
    const std::wstring call = L"load(" + JsString(mp4Url) + L"," + JsString(cssColor) + L"," + JsString(loadingText) + L")";
    if (pageReady) Run(call);
    else pending = call;
}

void MoviePlayer::Stop() {
    pending.clear();
    if (pageReady) Run(L"stop()");
}

} // namespace CustomCursor::Controls
