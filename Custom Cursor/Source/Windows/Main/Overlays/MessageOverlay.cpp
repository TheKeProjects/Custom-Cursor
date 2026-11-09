#include "Windows/Main/Overlays/MessageOverlay.h"
#include <algorithm>
#include <cmath>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/CursorKit.h"

namespace CustomCursor::Main {

namespace {

namespace M = UI::Material;
namespace Kit = CursorKit;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;

constexpr float kBadge = 42.f, kButtonW = 104.f, kButtonH = 38.f;
constexpr float kMaxTextH = 360.f;   // a long text (the install summary) scrolls

// MessageBoxImage.Information / Warning / Error / Question
Color IconColor(MessageIcon icon) {
    switch (icon) {
        case MessageIcon::Warning: return Hex(0xF5B841);
        case MessageIcon::Error: return Kit::Palette::Danger;
        default: return Kit::Palette::AccentLight;
    }
}

M::Icon IconOf(MessageIcon icon) {
    switch (icon) {
        case MessageIcon::Warning: return M::Icon::AlertOutline;
        case MessageIcon::Error: return M::Icon::AlertCircle;
        default: return M::Icon::InformationOutline;
    }
}

TextStyle BodyStyle() { return Kit::Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true); }

} // namespace

void MessageOverlay::Show(const std::wstring& text, const std::wstring& title, MessageButtons buttons, MessageIcon icon,
                          std::function<void(MessageResult)> done) {
    queue.push_back({ text, title, buttons, icon, std::move(done) });
    if (!Visible()) Next();
}

void MessageOverlay::Next() {
    if (queue.empty()) return;
    current = std::move(queue.front());
    queue.pop_front();
    // Escape / a click outside: the last button (No, or OK)
    result = current.buttons == MessageButtons::YesNo         ? MessageResult::No
             : current.buttons == MessageButtons::YesNoCancel ? MessageResult::Cancel
                                                              : MessageResult::Ok;
    onClosed = [this] {
        auto done = std::move(current.done);
        const MessageResult answer = result;
        current = {};
        if (done) done(answer);
        Next();
    };
    Open();
}

void MessageOverlay::Answer(MessageResult answer) {
    if (!IsOpen()) return;   // already closing
    result = answer;
    Close();
}

Color MessageOverlay::Accent() const { return IconColor(current.icon); }

float MessageOverlay::CardHeight(UI::Ui& ui, float width) {
    const float textH = std::min(kMaxTextH, ui.g.Measure(current.text, BodyStyle(), width - kPad * 2.f).h);
    return kPad + kBadge + 16.f + textH + 24.f + kButtonH + kPad;
}

void MessageOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;

    // Icon badge + title
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        float x = inner.x;
        if (current.icon != MessageIcon::None) {
            const Color c = IconColor(current.icon);
            const Rect badge{ x, y, kBadge, kBadge };
            // A ring popping out of the badge as the card opens
            const float pulse = M::Motion::Stagger(ui, OpenedAt() + 0.2, 0, 0.f, 0.6f);
            if (pulse < 1.f) ui.g.StrokeRound(badge.Inset(-10.f * pulse), 12.f + 10.f * pulse, c.Fade(0.6f * (1.f - pulse)), 1.5f);
            ui.g.Border(badge, 12.f, c.Fade(0.16f), c.Fade(0.55f), 1.f);
            M::DrawIcon(ui.g, IconOf(current.icon), badge.CX(), badge.CY(), 24.f, Lerp(c, Colors::White, 0.2f));
            x = badge.R() + 14.f;
        }
        TextStyle title = Kit::Style(18.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        ui.g.Text(current.title, { x, y, inner.R() - x, kBadge }, Kit::Palette::TextPrimary, title);
    }
    y += kBadge + 16.f;

    // The message (scrolls when it's long)
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const float fullH = ui.g.Measure(current.text, BodyStyle(), inner.w - 10.f).h;
        const float textH = std::min(kMaxTextH, ui.g.Measure(current.text, BodyStyle(), inner.w).h);
        if (fullH > kMaxTextH) {
            UI::ScrollArea scroll(ui, ui.MakeId("msg-scroll"), { inner.x, y, inner.w, textH });
            ui.g.Text(current.text, { inner.x, y, inner.w - 10.f, fullH }, Kit::Palette::TextSecondary, BodyStyle());
            scroll.SetContentSize(fullH);
        } else {
            ui.g.Text(current.text, { inner.x, y, inner.w, textH }, Kit::Palette::TextSecondary, BodyStyle());
        }
        y += textH + 24.f;
    }

    // Buttons, right aligned: OK, Yes / No, or Yes / No / Cancel (Enter = the first, Escape = the last)
    M::Motion::Enter enter(ui, Reveal(ui, 2));
    const Rect last{ inner.R() - kButtonW, y, kButtonW, kButtonH };
    if (current.buttons == MessageButtons::YesNoCancel) {
        const Rect no{ last.x - 8.f - kButtonW, y, kButtonW, kButtonH };
        const Rect yes{ no.x - 8.f - kButtonW, y, kButtonW, kButtonH };
        if (Kit::ApplyButton(ui, ui.MakeId("msg-yes"), yes, Loc::Get("yes"))) Answer(MessageResult::Yes);
        if (Kit::GlassButton(ui, ui.MakeId("msg-no"), no, Loc::Get("no"))) Answer(MessageResult::No);
        if (Kit::GlassButton(ui, ui.MakeId("msg-cancel"), last, Loc::Get("cancel"))) Answer(MessageResult::Cancel);
        if (IsOpen() && ui.KeyPressed(VK_RETURN)) Answer(MessageResult::Yes);
    } else if (current.buttons == MessageButtons::YesNo) {
        const Rect yes{ last.x - 8.f - kButtonW, y, kButtonW, kButtonH };
        if (Kit::ApplyButton(ui, ui.MakeId("msg-yes"), yes, Loc::Get("yes"))) Answer(MessageResult::Yes);
        if (Kit::GlassButton(ui, ui.MakeId("msg-no"), last, Loc::Get("no"))) Answer(MessageResult::No);
        if (IsOpen() && ui.KeyPressed(VK_RETURN)) Answer(MessageResult::Yes);
    } else {
        if (Kit::ApplyButton(ui, ui.MakeId("msg-ok"), last, Loc::Get("ok"))) Answer(MessageResult::Ok);
        if (IsOpen() && ui.KeyPressed(VK_RETURN)) Answer(MessageResult::Ok);
    }
}

} // namespace CustomCursor::Main
