#include "Windows/Main/MainWindow.h"
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/Log.h"
#include "Seasonal/SeasonalBackdrop.h"
#include "Seasonal/SeasonalOverlay.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/ResourceExtractor.h"
#include "Settings/ConfigFile.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Widgets/AnimatedCursor.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/Changelog/ChangelogView.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/Overlays/HeaderPopups.h"
#include "Windows/Main/Overlays/InstallProgressOverlay.h"
#include "Windows/Main/Overlays/MessageOverlay.h"
#include "Windows/Main/Overlays/ReplacementPickerOverlay.h"
#include "Windows/Main/Overlays/ThemeTransition.h"
#include "Windows/Main/Preview/PreviewTransition.h"
#include "Windows/Main/Preview/PreviewView.h"
#include "Windows/Main/Schemes/SchemesView.h"
#include "Windows/Main/Tutorial/TutorialView.h"
#include "Windows/Main/Updates/ManualUpdateOverlay.h"
#include "Windows/Main/Updates/UpdatesView.h"
#include "Windows/Splash/LoadingScreen.h"

namespace CustomCursor::Main {

namespace {
MainWindow* instance = nullptr;
namespace Kit = CursorKit;
} // namespace

MainWindow* MainWindow::Instance() { return instance; }

MainWindow::MainWindow() {
    instance = this;
    loading = std::make_unique<LoadingOverlay>(*this);
    languages = std::make_unique<LanguagePopup>(*this);
    messages = std::make_unique<MessageOverlay>(*this);
    transition = std::make_unique<PreviewTransition>(*this);
    manualUpdate = std::make_unique<ManualUpdateOverlay>(*this);
    themes = std::make_unique<ThemePopup>(*this);
    themeTransition = std::make_unique<ThemeTransition>();
    replacement = std::make_unique<ReplacementPickerOverlay>(*this);
    installProgress = std::make_unique<InstallProgressOverlay>(*this);
    // The message card goes on top: the errors of a card's action show over it; the loading screen
    // covers everything until it has faded out
    overlays = { transition.get(), languages.get(), themes.get(), manualUpdate.get(), replacement.get(), installProgress.get(), messages.get(),
                 loading.get() };
}

MainWindow::~MainWindow() {
    if (instance == this) instance = nullptr;
}

bool MainWindow::Open() {
    UI::Window::Options o;
    o.title = L"Custom Cursor Manager";   // Title (taskbar / Alt+Tab)
    // No Windows frame: the rounded window is drawn on a transparent one, resizable from every edge
    o.style = WS_OVERLAPPEDWINDOW;
    o.transparent = true;
    o.customFrame = true;
    // Height="846" Width="1285", kept inside the work area (94% of it at most) on smaller screens
    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    const float dips = 96.f / static_cast<float>(GetDpiForSystem());
    const float maxW = (area.right - area.left) * dips * 0.94f, maxH = (area.bottom - area.top) * dips * 0.94f;
    o.width = static_cast<int>(std::max(MainLayout::MinWidth, std::min(MainLayout::WindowWidth, maxW)));
    o.height = static_cast<int>(std::max(MainLayout::MinHeight, std::min(MainLayout::WindowHeight, maxH)));
    o.minWidth = static_cast<int>(MainLayout::MinWidth);
    o.minHeight = static_cast<int>(MainLayout::MinHeight);
    o.showCommand = SW_HIDE;
#ifdef _DEBUG
    if (GetEnvironmentVariableW(L"CC_SMALL", nullptr, 0) > 0) {
        o.width = static_cast<int>(MainLayout::MinWidth);
        o.height = static_cast<int>(MainLayout::MinHeight);
    }
#endif
    if (!Create(o)) return false;
#ifdef _DEBUG
    wchar_t themeOverride[32]{};
    if (GetEnvironmentVariableW(L"CC_THEME", themeOverride, 32) > 0) Settings::IconTheme::Current().theme = themeOverride;
#endif
    UI::Material::RefreshSeason();
    ApplyWindowIcon();
    state = std::make_unique<MainState>(hwnd);
    schemes = std::make_unique<SchemesView>(*this);
    preview = std::make_unique<PreviewView>(*this);
    tutorial = std::make_unique<TutorialView>(*this);
    updates = std::make_unique<UpdatesView>(*this);
    changelog = std::make_unique<ChangelogView>(*this);
    ui.tooltipRenderer = [](Graphics& gr, const std::wstring& text, float mx, float my, float age, float left) {
        Kit::DrawTooltip(gr, text, mx, my, age, left);
    };
    // The animated cursors' shared timer redraws the window
    UI::AnimatedCursor::SetTickHandler([this] { Invalidate(); });

    // WindowStartupLocation="CenterScreen"
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT r{};
    GetWindowRect(hwnd, &r);
    MoveTo(work.left + (work.right - work.left - (r.right - r.left)) / 2, work.top + (work.bottom - work.top - (r.bottom - r.top)) / 2);

    // App.OnStartup's Task.Run: the schemes and the tutorial are unpacked behind the loading screen,
    // then the schemes are scanned (LoadCursorSets) and their previews decoded
    Services::ResourceExtractor::ExtractHeavyInBackground([this] {
        state->LoadCursorSets([this] {
            // The cards' previews decoded behind the loading screen, so the schemes show at once
            std::vector<std::wstring> previews;
            for (const auto& category : state->categories)
                for (const auto& scheme : category.schemes)
                    if (const auto& path = state->Card(scheme).previewPath) previews.push_back(*path);
            UI::AnimatedCursor::Preload(previews, "cards", [this] {
                startupDone = true;
                Invalidate();
            });
        });
    });

#ifdef _DEBUG
    frameLog = GetEnvironmentVariableW(L"CC_FRAME_LOG", nullptr, 0) > 0;
    wchar_t shot[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"CC_SCREENSHOT", shot, MAX_PATH) > 0) {
        screenshotPath = shot;
        wchar_t open[32]{}, delay[16]{}, lead[16]{}, lang[8]{};
        GetEnvironmentVariableW(L"CC_OPEN", open, 32);
        if (GetEnvironmentVariableW(L"CC_LANG", lang, 8) > 0) {
            const std::wstring saved = Loc::CurrentLang();
            Loc::Load(lang);
            Config::WriteText(Config::LanguageFileName, saved);
        }
        const unsigned leadMs = GetEnvironmentVariableW(L"CC_OPEN_LEAD", lead, 16) > 0 ? static_cast<unsigned>(_wtoi(lead)) : 2000u;
        const unsigned ms = GetEnvironmentVariableW(L"CC_SCREENSHOT_DELAY", delay, 16) > 0 ? static_cast<unsigned>(_wtoi(delay)) : 6000u;
        Show(SW_SHOWNOACTIVATE);
        Async::After(ms - std::min(ms, leadMs), [this, view = std::wstring(open)] {
            if (view == L"preview" && !state->categories.empty()) ShowPreview(state->categories.front().schemes.front());
            else if (view == L"updates") ShowUpdates();
            else if (view == L"changelog") ShowChangelog();
            else if (view == L"tutorial") ShowTutorial();
            else if (view == L"manual") {
                ShowUpdates();
                manualUpdate->Show();
            }
            else if (view == L"select" && !state->categories.empty()) {
                state->SetSelected(state->categories.front().schemes.front(), true);
                state->SetSelected(state->categories.back().schemes.front(), true);
            }
            else if (view == L"sweep") ChangeTheme(L"summer");
            else if (view == L"available") {
                Services::Updates::UpdateInfo info;
                info.isUpdateAvailable = true;
                info.latestVersion = L"3.1.0";
                info.fileName = L"Custom Cursor.exe";
                info.fileSize = 12345678;
                info.changelog = Services::Changelog::EmbeddedText();
                state->updateInfo = info;
                ShowUpdates();
            }
            else if (view == L"theme") themes->Toggle({ 8.f, g.Height() - 110.f, 80.f, 40.f });
            else if (view == L"language") languages->Toggle({ 8.f, g.Height() - 66.f, 80.f, 40.f });
        });
        Async::After(ms, [this] {
            screenshotDue = true;
            Invalidate();
        });
        return true;
    }
#endif
    Show(SW_SHOW);
    Activate();
    return true;
}

void MainWindow::LoadingFinished() {
    Log::Info(L"Loading finished");
    // LoadCursorSets: "No cursor sets found" when Cursores is empty
    if (state->categories.empty())
        Dialogs::Message(Loc::Fmt("no_cursors_message", Paths::CursorDirectory()), Loc::Get("no_cursors_title"), Dialogs::MessageButtons::Ok,
                         Dialogs::MessageIcon::Warning);
    // CheckForUpdatesOnStartupAsync
    state->CheckForUpdates();
}

void MainWindow::Navigate(MainState::View view) {
    const MainState::View from = state->view;
    if (from == view) return;
    if (from == MainState::View::Tutorial) tutorial->Stop();
    state->view = view;
    slide = static_cast<int>(view) > static_cast<int>(from) ? 1 : -1;
    pageAt[static_cast<int>(view)] = -1.0;
    ui.ClearFocus();
}

void MainWindow::ShowSchemes() { Navigate(MainState::View::Schemes); }

void MainWindow::ShowPreview(const Models::CursorScheme& scheme) {
    state->previewScheme = scheme;
    preview->Show(scheme);
    Navigate(MainState::View::Preview);
}

void MainWindow::ShowTutorialView() { Navigate(MainState::View::Tutorial); }

void MainWindow::ShowUpdates() { Navigate(MainState::View::Updates); }

void MainWindow::ShowChangelog() { Navigate(MainState::View::Changelog); }

void MainWindow::ShowTutorial() { tutorial->Open(); }

void MainWindow::ChangeTheme(const std::wstring& theme) { themeTransition->Request(theme); }

void MainWindow::ApplyWindowIcon() {
    const HINSTANCE module = GetModuleHandleW(nullptr);
    const LPCWSTR id = MAKEINTRESOURCEW(Settings::IconTheme::Current().IconResourceId());
    // (LR_SHARED: Windows owns them, nothing to free when the theme changes again)
    const auto load = [&](int metric) { return static_cast<HICON>(LoadImageW(module, id, IMAGE_ICON, GetSystemMetrics(metric), GetSystemMetrics(metric + 1), LR_SHARED)); };
    SetIcon(load(SM_CXICON), load(SM_CXSMICON));
}

void MainWindow::OpenWindowsCursorSettings() {
    // Mouse Properties on the Pointers tab (index 1)
    Process::Open(L"control", L"main.cpl,,1");
}

bool MainWindow::IsCaption(float x, float y) const { return layout.Caption().Contains(x, y); }

bool MainWindow::OnMessage(UINT msg, WPARAM, LPARAM, LRESULT&) {
    // The video's popup follows the window
    if (msg == WM_MOVE && state && state->view == MainState::View::Tutorial) Invalidate();
    // The decoration starts moving again
    if (msg == WM_ACTIVATE) Invalidate();
    return false;
}

void MainWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
#ifdef _DEBUG
    if (screenshotDue) {
        screenshotDue = false;
        auto bitmap = g.CreateWindowBitmap();
        g.BeginOffscreen(bitmap.Get());
        g.Clear(Colors::Transparent);
        DrawFrame(screen);
        themeTransition->Draw(g, screen, ui.Time());
        g.EndOffscreen();
        g.SavePng(bitmap.Get(), screenshotPath);
        Close();
        return;
    }
    const double frameStart = frameLog ? Async::Now() : 0.0;
#endif
    if (themeTransition->Pending()) {
        const std::wstring theme = themeTransition->PendingTheme();
        auto snapshot = g.CreateWindowBitmap();
        if (snapshot) {
            g.BeginOffscreen(snapshot.Get());
            g.Clear(Colors::Transparent);
            DrawFrame(screen);
            g.EndOffscreen();
        }
        themeTransition->Begin(snapshot, ui.Time());
        Settings::IconTheme::Current().theme = theme;
        Settings::IconTheme::Current().Save();
        UI::Material::RefreshSeason();
        ApplyWindowIcon();
    }
    DrawFrame(screen);
    themeTransition->Draw(g, screen, ui.Time());
    if (themeTransition->Animating()) ui.KeepAnimating();
#ifdef _DEBUG
    if (frameLog) {
        const double now = Async::Now(), took = (now - frameStart) * 1000.0;
        frameTotal += took;
        frameWorst = std::max(frameWorst, took);
        ++frameCount;
        if (now - frameLogAt >= 2.0) {
            Log::Info(L"Frames: " + std::to_wstring(frameCount) + L" in 2 s, avg " + std::to_wstring(frameTotal / std::max(1, frameCount)) + L" ms, worst " +
                      std::to_wstring(frameWorst) + L" ms");
            frameLogAt = now;
            frameTotal = frameWorst = 0.0;
            frameCount = 0;
        }
    }
#endif
}

void MainWindow::Backdrop(const Rect& screen) {
    ui.g.FillLinear(screen, 0.f, { { 0.f, Kit::Palette::BgTop }, { 1.f, Kit::Palette::BgBottom } }, { 0.f, 0.f }, { 1.f, 1.f });
    if (Seasonal::IsSeasonal()) {
        UI::AnimatedCursor::RequestAmbientTicks();
        const double season = Seasonal::AnimationTime(ui.Time(), false);
        Seasonal::Backdrop::Draw(ui.g, screen, MainLayout::TitleBarHeight, season);
        // (behind the pages: nothing crosses the text)
        Seasonal::Ambient::Draw(ui.g, screen, season);
        return;
    }
    const float x1 = screen.x + screen.w * 0.22f, y1 = screen.y + screen.h * 0.18f;
    ui.g.FillRadial({ x1 - 520.f, y1 - 360.f, 1040.f, 720.f }, { { 0.f, Kit::Palette::AccentLight.Fade(0.07f) }, { 1.f, Kit::Palette::AccentLight.Fade(0.f) } });
    const float x2 = screen.x + screen.w * 0.8f, y2 = screen.y + screen.h * 0.85f;
    ui.g.FillRadial({ x2 - 480.f, y2 - 340.f, 960.f, 680.f }, { { 0.f, Kit::Palette::AccentDark.Fade(0.08f) }, { 1.f, Kit::Palette::AccentDark.Fade(0.f) } });
}

void MainWindow::DrawViews(bool modal) {
    if (shownAt < 0.0) shownAt = ui.Time();
    const int shown = static_cast<int>(state->view);
    const Rect content = layout.content;
    // (the outgoing page must not spill over the rail or the title bar)
    ui.PushClip({ layout.rail.R(), layout.body.y, layout.body.R() - layout.rail.R(), layout.body.h });
    for (int i = 0; i < 5; ++i) {
        const float raw = ui.Tween(ui.MakeId("view", i), shown == i ? 1.f : 0.f, 0.22f);
        if (raw > 0.f && raw < 1.f) ui.KeepAnimating();
        if (raw <= 0.001f) {
            if (static_cast<MainState::View>(i) == MainState::View::Preview) preview->Hidden();
            continue;
        }
        const float k = shown == i ? Anim::EaseOutCubic(Anim::Clamp01((raw - 0.3f) / 0.7f)) : raw * raw;
        if (pageAt[i] < 0.0) pageAt[i] = ui.Time();
        ui.SetInputMuted(modal || shown != i);
        // (a layer only while it fades)
        const bool fading = raw < 1.f;
        if (fading) {
            ui.g.PushOpacity(k);
            ui.PushTranslate(0.f, static_cast<float>(shown == i ? slide : -slide) * 22.f * (1.f - k));
        }
        switch (static_cast<MainState::View>(i)) {
            case MainState::View::Schemes: schemes->Draw(ui, content, pageAt[i]); break;
            case MainState::View::Preview: preview->Draw(ui, content); break;
            case MainState::View::Tutorial:
                // The video's popup sits over the window: only while nothing is drawn over it
                tutorial->Draw(ui, content, shown == i && raw >= 1.f && !modal && !themeTransition->Animating());
                break;
            case MainState::View::Updates: updates->Draw(ui, content, pageAt[i]); break;
            case MainState::View::Changelog: changelog->Draw(ui, content, pageAt[i]); break;
        }
        if (fading) {
            ui.PopTransform();
            ui.g.PopLayer();
        }
    }
    ui.PopClip();
    ui.SetInputMuted(false);
}

void MainWindow::DrawFrame(const Rect& screen) {
    layout = MainLayout::Compute(screen);
#ifdef _DEBUG
    UI::AnimatedCursor::SetAmbientPaused(screenshotPath.empty() && GetForegroundWindow() != hwnd);
#else
    UI::AnimatedCursor::SetAmbientPaused(GetForegroundWindow() != hwnd);
#endif
    UI::ScrollArea::SetMaterialColors(UI::Material::P().scrollBarForeground, UI::Material::P().foreground);

    // An open card or popup is modal: what's behind it doesn't see the mouse, and a text box behind
    // it loses the keyboard when it opens
    const bool modal = std::any_of(overlays.begin(), overlays.end(), [](const Overlay* o) { return o->Visible(); });
    if (modal && !modalShown) ui.ClearFocus();
    modalShown = modal;

    // Rounded corners (0 while maximized) and a 1 px border
    const float radius = IsMaximized() ? 0.f : MainLayout::Radius;
    if (radius > 0.f) g.PushRoundClip(screen, radius);
    Backdrop(screen);

    ui.SetInputMuted(modal);
    TitleBar::Draw(ui, *this, layout.titleBar);
    if (!loading->Visible() || loading->Revealing()) {
        DrawViews(modal);
        ui.SetInputMuted(modal);
        NavRail::Draw(ui, *this, layout.rail);
    }
    ui.SetInputMuted(false);
    // The loading screen covers the whole window, title bar included; the rest sit under the title bar
    for (Overlay* o : overlays)
        if (o->Visible()) o->Draw(ui, o == loading.get() ? screen : layout.body);

    if (radius > 0.f) {
        g.PopLayer();
        g.StrokeRound(screen.Inset(0.5f), radius - 0.5f, Kit::Palette::CardBorder, 1.f);
    } else {
        g.StrokeRect(screen.Inset(0.5f), Kit::Palette::CardBorder, 1.f);
    }
}

void MainWindow::OnDestroyed() {
    Services::ResourceExtractor::StopBackground();
    PostQuitMessage(0);
}

} // namespace CustomCursor::Main
