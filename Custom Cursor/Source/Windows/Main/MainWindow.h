#pragma once
#include <memory>
#include <vector>
#include "Models/CursorScheme.h"
#include "UI/Window.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/MainState.h"

namespace CustomCursor::Main {

class Overlay;
class LoadingOverlay;
class LanguagePopup;
class MessageOverlay;
class PreviewTransition;
class ThemePopup;
class ThemeTransition;
class ManualUpdateOverlay;
class UpdatesView;
class ChangelogView;
class ReplacementPickerOverlay;
class InstallProgressOverlay;
class SchemesView;
class PreviewView;
class TutorialView;

// The window (borderless, with rounded corners and its own title bar, resizable from every edge): the
// navigation rail and the pages as one content area that slides and fades between them, plus the
// in-window cards and popups drawn over them (LoadingWindow.xaml became the first of them). Bars live
// in MainScreen\, the pages in Schemes\, Preview\, Tutorial\, Updates\ and Changelog\, the cards
// and popups in Overlays\.
class MainWindow : public UI::Window {
public:
    MainWindow();
    ~MainWindow() override;
    bool Open();
    static MainWindow* Instance();

    MainState& State() { return *state; }
    const MainLayout& Layout() const { return layout; }
    LanguagePopup& Languages() { return *languages; }
    ThemePopup& Themes() { return *themes; }
    ManualUpdateOverlay& ManualUpdate() { return *manualUpdate; }
    MessageOverlay& Messages() { return *messages; }
    SchemesView& Schemes() { return *schemes; }
    PreviewTransition& Transition() { return *transition; }
    ReplacementPickerOverlay& Replacement() { return *replacement; }
    InstallProgressOverlay& InstallProgress() { return *installProgress; }

    // The loading screen faded out (LoadingWindow.CompleteLoading -> new MainWindow().Show())
    void LoadingFinished();
    // The background start-up work is done (resources unpacked, schemes scanned)
    bool StartupDone() const { return startupDone; }
    // When the main view first showed (the rail's entrance)
    double ShownAt() const { return shownAt; }

    // ── Pages ──
    void ShowSchemes();
    void ShowPreview(const Models::CursorScheme& scheme);
    void ShowTutorialView();
    void ShowUpdates();
    void ShowChangelog();
    void ShowTutorial();                // TutorialButton_Click (it can have no video to show)
    void OpenWindowsCursorSettings();   // WindowsSettingsButton_Click (WindowsCursorTool.OpenCursorSettings)
    // Saves the theme and changes to it with the sweep
    void ChangeTheme(const std::wstring& theme);
    void Quit() { Close(); }

protected:
    void OnFrame() override;
    bool IsCaption(float x, float y) const override;
    void OnDestroyed() override;
    bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void DrawFrame(const Rect& screen);
    // The theme's gradient with two soft glows, or the season's scenery over it
    void Backdrop(const Rect& screen);
    // The pages, sliding and fading; `modal` = a card or popup is over them
    void DrawViews(bool modal);
    void Navigate(MainState::View view);
    void ApplyWindowIcon();

    std::unique_ptr<MainState> state;
    MainLayout layout;
    std::unique_ptr<SchemesView> schemes;
    std::unique_ptr<PreviewView> preview;
    std::unique_ptr<TutorialView> tutorial;
    std::unique_ptr<LoadingOverlay> loading;
    std::unique_ptr<LanguagePopup> languages;
    std::unique_ptr<MessageOverlay> messages;
    std::unique_ptr<PreviewTransition> transition;
    std::unique_ptr<UpdatesView> updates;
    std::unique_ptr<ChangelogView> changelog;
    std::unique_ptr<ThemePopup> themes;
    std::unique_ptr<ThemeTransition> themeTransition;
    std::unique_ptr<ManualUpdateOverlay> manualUpdate;
    std::unique_ptr<ReplacementPickerOverlay> replacement;
    std::unique_ptr<InstallProgressOverlay> installProgress;
    std::vector<Overlay*> overlays;   // z-order (first = bottom)
    bool startupDone = false;
    bool modalShown = false;
    double shownAt = -1.0;
    double pageAt[5] = { -1.0, -1.0, -1.0, -1.0, -1.0 };   // when each page last came in
    int slide = 1;                                         // the side the page comes in from
#ifdef _DEBUG
    // Development aid: CC_SCREENSHOT=<file.png> renders a frame into that file after
    // CC_SCREENSHOT_DELAY ms (default 6000), then closes; the window is never activated.
    // CC_OPEN=preview|updates|manual|tutorial|changelog|theme|language|select|sweep|available opens that page / card
    // CC_OPEN_LEAD ms (default 2000) before the shot; CC_LANG=<lang> uses that language; CC_SMALL=1 the smallest window;
    // CC_THEME=<theme> uses that theme, without saving it
    std::wstring screenshotPath;
    bool screenshotDue = false;
    // CC_FRAME_LOG=1: the frames drawn and their average / worst time, every 2 s, in the log
    bool frameLog = false;
    double frameLogAt = 0.0, frameTotal = 0.0, frameWorst = 0.0;
    int frameCount = 0;
#endif
};

} // namespace CustomCursor::Main
