#include "Windows/Main/Overlays/ReplacementPickerOverlay.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/CursorKit.h"

namespace CustomCursor::Main {

namespace {
namespace M = UI::Material;
namespace Kit = CursorKit;
constexpr float kRowH = 30.f, kListH = 170.f, kButtonW = 100.f, kButtonH = 35.f;
} // namespace

void ReplacementPickerOverlay::Show(const std::wstring& schemeName, std::vector<std::wstring> installed,
                                    std::function<void(std::optional<std::wstring>)> ok) {
    scheme = schemeName;
    items = std::move(installed);
    selected = 0;   // listBox.SelectedIndex = 0
    accepted = false;
    onOk = std::move(ok);
    onClosed = [this] {
        if (!accepted || !onOk) return;
        // "Windows Default" -> null
        auto done = std::move(onOk);
        done(selected == 0 ? std::nullopt : std::optional<std::wstring>(items[static_cast<size_t>(selected - 1)]));
    };
    Open();
}

void ReplacementPickerOverlay::Accept() {
    if (!IsOpen()) return;
    accepted = true;
    Close();
}

float ReplacementPickerOverlay::CardHeight(UI::Ui& ui, float width) {
    const TextStyle header = Kit::Style(16.f, DWRITE_FONT_WEIGHT_BOLD, true);
    return kPad + Kit::TextH(ui, Loc::Fmt("replace_active", scheme), header, width - kPad * 2.f) + 10.f + 18.f + 15.f + kListH + 15.f + kButtonH + kPad;
}

void ReplacementPickerOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const Rect inner = card.Inset(kPad);
    float y = inner.y;
    {
        // headerText (16 Bold) and instructionText (12, secondary)
        M::Motion::Enter enter(ui, Reveal(ui, 0));
        const TextStyle header = Kit::Style(16.f, DWRITE_FONT_WEIGHT_BOLD, true);
        const std::wstring title = Loc::Fmt("replace_active", scheme);
        const float h = Kit::TextH(ui, title, header, inner.w);
        ui.g.Text(title, { inner.x, y, inner.w, h }, Kit::Palette::TextPrimary, header);
        y += h + 10.f;
        ui.g.Text(Loc::Get("replace_instruction"), { inner.x, y, inner.w, 18.f }, Kit::Palette::TextSecondary, Kit::Style(12.f));
        y += 18.f + 15.f;
    }
    {
        // The ListBox: CardBrush, a #3A404C border, 13 px rows
        M::Motion::Enter enter(ui, Reveal(ui, 1));
        const Rect list{ inner.x, y, inner.w, kListH };
        ui.g.Border(list, Kit::Palette::ControlRadius, Kit::Palette::Card, Hex(0x3A404C), 1.f);
        const Rect rows = list.Inset(5.f);
        UI::ScrollArea scroll(ui, ui.MakeId("replace-scroll"), rows);
        const int count = static_cast<int>(items.size()) + 1;
        for (int i = 0; i < count; ++i) {
            const Rect row{ rows.x, rows.y + i * kRowH, rows.w - 8.f, kRowH };
            const UI::Id id = ui.MakeId("replace-row", i);
            const UI::Interaction it = ui.Interact(id, row);
            const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            const bool on = selected == i;
            if (on) Kit::FillAccent(ui, row, 6.f);
            else if (hover > 0.f) ui.g.FillRound(row, 6.f, Argb(0x14FFFFFF).Fade(hover));
            TextStyle t = Kit::Style(13.f, on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
            t.valign = VAlign::Center;
            t.ellipsis = true;
            ui.g.Text(i == 0 ? Loc::Get("replace_windows_default") : items[static_cast<size_t>(i - 1)], row.Inset(Edges(10.f, 0.f)),
                      on ? Colors::White : Kit::Palette::TextPrimary, t);
            if (it.clicked) {
                // listBox.MouseDoubleClick: the second click on the selected row accepts it
                double& lastClick = ui.Data<double>(UI::HashId(id, 0xD0C));
                if (on && ui.Time() - lastClick < GetDoubleClickTime() / 1000.0) Accept();
                lastClick = ui.Time();
                selected = i;
            }
        }
        scroll.SetContentSize(count * kRowH);
        if (IsOpen() && ui.KeyPressed(VK_DOWN)) selected = std::min(selected + 1, count - 1);
        if (IsOpen() && ui.KeyPressed(VK_UP)) selected = std::max(selected - 1, 0);
        y += kListH + 15.f;
    }
    // Cancel (secondary) and OK (accent), right aligned
    M::Motion::Enter enter(ui, Reveal(ui, 2));
    const Rect ok{ inner.R() - kButtonW, y, kButtonW, kButtonH };
    const Rect cancel{ ok.x - 10.f - kButtonW, y, kButtonW, kButtonH };
    if (Kit::GlassButton(ui, ui.MakeId("replace-cancel"), cancel, Loc::Get("cancel"))) Close();
    if (Kit::ApplyButton(ui, ui.MakeId("replace-ok"), ok, Loc::Get("ok")) || (IsOpen() && ui.KeyPressed(VK_RETURN))) Accept();
}

} // namespace CustomCursor::Main
