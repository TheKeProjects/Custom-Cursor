#pragma once
#include <string>
#include "Graphics/Images.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace CustomCursor::Main {

// <Popup Placement="Bottom" StaysOpen="False"> / a ContextMenu under a button, redesigned: grows out
// of the button with a little bounce (an arrow pointing at it) while it unfolds, its rows coming in
// one by one; the first click anywhere outside (or Escape) closes it with a quick fade. WPF popups
// are separate windows that can spill past the main window; here they're kept inside it.
class AnchoredPopup : public Overlay {
public:
    using Overlay::Overlay;
    void Toggle(const Rect& anchor);
    void Close();
    bool IsOpen() const { return open; }
    bool Visible() const override;
    void Draw(UI::Ui& ui, const Rect& screen) final;

protected:
    // Content size and drawing (inside the 2 px primary border, radius 5, paper background)
    virtual float Width(UI::Ui& ui) = 0;
    virtual float Height(UI::Ui& ui, float width) = 0;
    virtual void DrawContent(UI::Ui& ui, const Rect& box) = 0;
    virtual void OnOpened() {}
    // Look: fill, border (width) and corner radius; the gap between the button and the arrow
    virtual Color Fill() const;
    virtual Color Edge() const;
    virtual float EdgeWidth() const { return 2.f; }
    virtual float Radius() const { return 10.f; }
    virtual float Gap() const { return 4.f; }
    // Entrance (0..1) of content row `index` while the popup opens
    float RowReveal(UI::Ui& ui, int index) const;

private:
    bool open = false;
    double changedAt = -10.0;
    Rect anchor;
};

// LanguagePopup: one banner per language showing its whole flag rolled on a cylinder; a click
// switches the interface language (saved in language.cfg).
class LanguagePopup : public AnchoredPopup {
public:
    using AnchoredPopup::AnchoredPopup;

    struct Language {
        const wchar_t* code;   // Translations\<code>.txt
        const wchar_t* flag;   // Assets\Flags\<flag>
        const wchar_t* name;   // in its own language (tooltip)
    };
    static const Language kLanguages[4];
    static ImagePtr Flag(const std::wstring& lang);
    static void Banner(UI::Ui& ui, const Rect& r, const ImagePtr& flag, float hover, float glint);

protected:
    float Width(UI::Ui& ui) override;
    float Height(UI::Ui& ui, float width) override;
    void DrawContent(UI::Ui& ui, const Rect& box) override;
    // The cards' dark fill and an icy edge
    Color Fill() const override;
    Color Edge() const override;
    float EdgeWidth() const override { return 1.5f; }
    float Radius() const override { return 12.f; }
};

// The theme: one row per look with its logo; a click changes it with the sweep (ThemeTransition)
class ThemePopup : public AnchoredPopup {
public:
    using AnchoredPopup::AnchoredPopup;

protected:
    float Width(UI::Ui& ui) override;
    float Height(UI::Ui& ui, float width) override;
    void DrawContent(UI::Ui& ui, const Rect& box) override;
    Color Fill() const override;
    Color Edge() const override;
    float EdgeWidth() const override { return 1.5f; }
    float Radius() const override { return 12.f; }
};

} // namespace CustomCursor::Main
