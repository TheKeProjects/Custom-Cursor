#pragma once
#include <string>
#include "Services/App/UpdateService.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace CustomCursor::Main {

// Views\ManualUpdateWindow.xaml(.cs) as a card over the Updates card: "MANUAL UPDATE", the intro,
// the status (checking / ready with the file and size / couldn't reach the server), the download's
// progress, "DOWNLOAD & REPLACE APP" and "OPEN DOWNLOAD PAGE IN BROWSER". Always fetches the latest
// published release, whatever the version comparison says.
class ManualUpdateOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show();

protected:
    float CardWidth() const override { return 500.f; }
    float CardHeight(UI::Ui&, float) override { return 420.f; }
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    bool Closable() const override { return !downloading; }
    Color Accent() const override;

private:
    enum class Status { Checking, Ready, Older, Failed };   // Older: the published version is older than this one
    void Check();      // CheckAsync
    void Download();   // DownloadBtn_Click

    Status status = Status::Checking;
    Services::Updates::UpdateInfo release;   // _releaseInfo
    bool downloading = false;
    int progress = 0;
};

} // namespace CustomCursor::Main
