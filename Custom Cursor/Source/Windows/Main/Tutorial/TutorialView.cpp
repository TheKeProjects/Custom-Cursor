#include "Windows/Main/Tutorial/TutorialView.h"
#include <cmath>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/App/TutorialService.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/CursorKit.h"
#include "Windows/Main/MainScreen/PageBanner.h"
#include "Windows/Main/MainWindow.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
} // namespace

void TutorialView::Open() {
    namespace Tutorial = Services::TutorialService;
    if (!Tutorial::HasTutorialVideos()) {
        Dialogs::Message(Loc::Get("tutorial_none_message"), Loc::Get("tutorial_none_title"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Information);
        return;
    }
    // ShowTutorialVideo
    const auto video = Tutorial::GetFirstTutorialVideo();
    if (!video) {
        Dialogs::Message(Loc::Get("tutorial_no_file"), Loc::Get("dialog_error"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        return;
    }
    if (!player.Created()) {
        player.onError = [](const std::wstring& what) {
            Log::Error(L"Tutorial video: " + what);
            // MediaFailed: "Error playing video: ..." (WebView2 missing: the same card)
            Dialogs::Message(Loc::Fmt("tutorial_play_error", what == L"WebView2" ? Loc::Get("tutorial_webview2_missing") : Loc::Get("tutorial_video_failed")),
                             Loc::Get("tutorial_error_title"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        };
        player.Create(main.Hwnd(), Paths::TutorialDirectory());
    }
    const std::wstring relative = video->substr(Paths::TutorialDirectory().size() + 1);
    player.Load(Controls::MoviePlayer::MediaUrl(relative), L"#88DDFF", Loc::Get("tutorial_loading"));
    shownAt = -1.0;
    main.ShowTutorialView();
}

void TutorialView::Stop() {
    player.Stop();
    player.SetVisible(false);
}

void TutorialView::Draw(UI::Ui& ui, const Rect& content, bool videoShown) {
    if (shownAt < 0.0) shownAt = ui.Time();
    const Rect banner{ content.x, content.y, content.w, kPageBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, 0, 0.f, 0.45f), 14.f);
        PageBannerSpec spec;
        spec.icon = M::Icon::PlayCircle;
        spec.motion = PageBannerSpec::Motion::Float;
        spec.title = Loc::Get("tutorial_title");
        spec.subtitle = Loc::Get("tutorial_subtitle");
        PageBanner(ui, banner, spec, shownAt);
    }
    if (!ui.InputMuted() && ui.KeyPressed(VK_ESCAPE)) {
        main.ShowSchemes();
        return;
    }
    // The frame; the video's popup follows the area inside it (physical pixels of the client area)
    const Rect frame{ content.x, banner.B() + 12.f, content.w, content.B() - banner.B() - 12.f };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt + 0.08, 0, 0.f, 0.4f), 16.f);
        Kit::Panel(ui, frame);
        ui.g.FillRound(frame.Inset(8.f), Kit::Palette::ControlRadius, Colors::Black);
        M::DrawIcon(ui.g, M::Icon::Video, frame.CX(), frame.CY(), 44.f, Kit::Palette::TextMuted);
    }
    const Rect video = frame.Inset(10.f);
    const float scale = main.DpiScale();
    const RECT physical{ static_cast<LONG>(std::lround(video.x * scale)), static_cast<LONG>(std::lround(video.y * scale)),
                         static_cast<LONG>(std::lround(video.R() * scale)), static_cast<LONG>(std::lround(video.B() * scale)) };
    if (player.Created()) {
        player.SetBounds(physical);
        player.SetVisible(videoShown);
    }
}

} // namespace CustomCursor::Main
