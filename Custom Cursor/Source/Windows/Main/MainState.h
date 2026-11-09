#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Models/CursorCategory.h"
#include "Services/App/UpdateService.h"

namespace CustomCursor::Main {

// MainWindow.xaml.cs's state outside the controls: the schemes by category (_cursorCategories), the
// category shown (_currentCategory), the selection (_selectedSchemes), the scheme in the preview
// (_currentPreviewScheme), what the content shows and the update information.
class MainState {
public:
    explicit MainState(HWND owner);

    // The page shown
    enum class View { Schemes, Preview, Tutorial, Updates, Changelog };
    View view = View::Schemes;

    // ── Schemes (LoadCursorSets) ──
    std::vector<Models::CursorCategory> categories;
    // Scans Cursores on a worker thread (CursorParser.FindCursorSets), with what each card shows
    // (CountChangedCursors, GetSchemePreviewPath), then runs `done` on the UI thread
    void LoadCursorSets(std::function<void()> done);
    bool Loaded() const { return loaded; }
    // A card's extras, found with the scan
    struct CardInfo {
        int cursorCount = 0;
        std::optional<std::wstring> previewPath;
    };
    const CardInfo& Card(const Models::CursorScheme& scheme) const;
    const Models::CursorScheme* FindScheme(const std::wstring& path) const;

    // ── Categories (RefreshCategories / CreateCategoryButton) ──
    std::wstring currentCategory;           // Config::AllCategory = every scheme
    // "All" first, then the categories in alphabetical order
    std::vector<std::wstring> ShownCategories() const;
    // How many schemes it holds (Config::AllCategory: all of them)
    int SchemeCount(const std::wstring& category) const;
    // A category button: DisplaySchemes(category)
    void SelectCategory(const std::wstring& category);

    // ── Schemes of the category (DisplaySchemes) ──
    std::wstring schemeSearch;              // SchemeSearchBox: name or relative path
    std::vector<const Models::CursorScheme*> ShownSchemes() const;
    // DisplaySchemes(_currentCategory): the selection cleared, the installed state read again, the
    // cards coming in again
    void RefreshSchemes();
    bool IsInstalled(const Models::CursorScheme& scheme) const;
    // The scheme Windows is using right now (read by RefreshSchemes)
    bool IsActive(const Models::CursorScheme& scheme) const;
    unsigned revision = 0;                  // bumped when the cards should play their entrance

    // ── Selection (_selectedSchemes, by folder, in the order checked) ──
    bool IsSelected(const Models::CursorScheme& scheme) const;
    void SetSelected(const Models::CursorScheme& scheme, bool selected);
    std::vector<Models::CursorScheme> SelectedSchemes() const;
    size_t SelectedCount() const { return selected.size(); }
    void ClearSelection() { selected.clear(); }

    // ── Preview (_currentPreviewScheme) ──
    std::optional<Models::CursorScheme> previewScheme;

    // ── Updates (_updateInfo / _gbUpdateInfo) ──
    // Null until the first check finishes (the Updates card then shows only the version banner)
    std::optional<Services::Updates::UpdateInfo> updateInfo;
    std::optional<Services::Updates::GbUpdateInfo> gbUpdateInfo;
    bool checkingUpdates = false;
    // UpdateAvailableBadge.Visibility
    bool UpdateAvailable() const {
        return (updateInfo && updateInfo->isUpdateAvailable) || (gbUpdateInfo && gbUpdateInfo->isUpdateAvailable);
    }
    // CheckForUpdatesAsync then CheckGameBananaUpdateAsync on a worker; `done` on the UI thread
    void CheckForUpdates(std::function<void()> done = {});

    HWND Owner() const { return owner; }

private:
    HWND owner;
    bool loaded = false;
    std::map<std::wstring, CardInfo> cards;       // by scheme folder
    std::map<std::wstring, bool> installed;       // by scheme folder, read by RefreshSchemes
    std::wstring activeScheme;                    // read by RefreshSchemes
    std::vector<std::wstring> selected;           // scheme folders
};

} // namespace CustomCursor::Main
