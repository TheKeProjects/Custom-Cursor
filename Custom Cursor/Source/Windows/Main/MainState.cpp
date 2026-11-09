#include "Windows/Main/MainState.h"
#include <algorithm>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Cursors/SchemeQueries.h"
#include "Services/Cursors/SchemeScanner.h"

namespace CustomCursor::Main {

namespace {

namespace Cursors = Services::Cursors;

// OrderBy(k => k): the current culture's order
bool CultureLess(const std::wstring& a, const std::wstring& b) {
    return CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0, a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), nullptr, nullptr, 0) ==
           CSTR_LESS_THAN;
}

} // namespace

MainState::MainState(HWND ownerWindow) : owner(ownerWindow), currentCategory(Config::AllCategory) {}

void MainState::LoadCursorSets(std::function<void()> done) {
    struct Result {
        std::vector<Models::CursorCategory> categories;
        std::map<std::wstring, CardInfo> cards;
    };
    Async::Run(
        [] {
            Result r;
            r.categories = Cursors::SchemeScanner::FindCursorSets(Paths::CursorDirectory());
            for (const auto& category : r.categories)
                for (const auto& scheme : category.schemes)
                    r.cards[scheme.path] = { Cursors::SchemeScanner::CountChangedCursors(scheme), Cursors::SchemeScanner::PreviewPath(scheme) };
            return r;
        },
        [this, done = std::move(done)](Result r) {
            categories = std::move(r.categories);
            cards = std::move(r.cards);
            loaded = true;
            size_t count = 0;
            for (const auto& c : categories) count += c.schemes.size();
            Log::Info(L"Cursor sets: " + std::to_wstring(count) + L" in " + std::to_wstring(categories.size()) + L" categories");
            // RefreshCategories: "All" selected
            currentCategory = Config::AllCategory;
            RefreshSchemes();
            if (done) done();
        });
}

const MainState::CardInfo& MainState::Card(const Models::CursorScheme& scheme) const {
    static const CardInfo none;
    const auto it = cards.find(scheme.path);
    return it == cards.end() ? none : it->second;
}

const Models::CursorScheme* MainState::FindScheme(const std::wstring& path) const {
    for (const auto& category : categories)
        for (const auto& scheme : category.schemes)
            if (scheme.path == path) return &scheme;
    return nullptr;
}

std::vector<std::wstring> MainState::ShownCategories() const {
    std::vector<std::wstring> names;
    for (const auto& category : categories) names.push_back(category.name);
    std::stable_sort(names.begin(), names.end(), CultureLess);
    std::vector<std::wstring> shown;
    if (categories.empty()) return shown;
    shown.push_back(Config::AllCategory);
    shown.insert(shown.end(), names.begin(), names.end());
    return shown;
}

int MainState::SchemeCount(const std::wstring& category) const {
    size_t count = 0;
    for (const auto& c : categories)
        if (category == Config::AllCategory || c.name == category) count += c.schemes.size();
    return static_cast<int>(count);
}

void MainState::SelectCategory(const std::wstring& category) {
    currentCategory = category;
    RefreshSchemes();
}

std::vector<const Models::CursorScheme*> MainState::ShownSchemes() const {
    std::vector<const Models::CursorScheme*> shown;
    const std::wstring search = Text::Trim(schemeSearch);
    const auto matches = [&](const Models::CursorScheme& s) {
        return search.empty() || Text::Contains(s.name, search, true) || Text::Contains(s.relativePath, search, true);
    };
    for (const auto& category : categories) {
        if (currentCategory != Config::AllCategory && category.name != currentCategory) continue;
        for (const auto& scheme : category.schemes)
            if (matches(scheme)) shown.push_back(&scheme);
    }
    return shown;
}

void MainState::RefreshSchemes() {
    selected.clear();
    installed.clear();
    for (const auto& category : categories)
        for (const auto& scheme : category.schemes) installed[scheme.path] = Cursors::SchemeQueries::IsSchemeInstalled(scheme.name);
    activeScheme = Cursors::SchemeQueries::GetActiveScheme().value_or(L"");
    ++revision;
}

bool MainState::IsActive(const Models::CursorScheme& scheme) const {
    if (activeScheme.empty() || !IsInstalled(scheme)) return false;
    return Text::Equals(activeScheme, Cursors::SchemeQueries::InstalledName(scheme.name), true);
}

bool MainState::IsInstalled(const Models::CursorScheme& scheme) const {
    const auto it = installed.find(scheme.path);
    return it != installed.end() && it->second;
}

bool MainState::IsSelected(const Models::CursorScheme& scheme) const { return std::find(selected.begin(), selected.end(), scheme.path) != selected.end(); }

void MainState::SetSelected(const Models::CursorScheme& scheme, bool on) {
    const auto it = std::find(selected.begin(), selected.end(), scheme.path);
    if (on && it == selected.end()) selected.push_back(scheme.path);
    else if (!on && it != selected.end()) selected.erase(it);
}

std::vector<Models::CursorScheme> MainState::SelectedSchemes() const {
    std::vector<Models::CursorScheme> schemes;
    for (const auto& path : selected)
        if (const auto* scheme = FindScheme(path)) schemes.push_back(*scheme);
    return schemes;
}

void MainState::CheckForUpdates(std::function<void()> done) {
    if (checkingUpdates) return;
    checkingUpdates = true;
    struct Result {
        Services::Updates::UpdateInfo info;
        Services::Updates::GbUpdateInfo gb;
        bool reachable = false;
    };
    Async::Run(
        [] {
            Result r;
            r.info = Services::Updates::CheckForUpdates(&r.reachable);
            r.gb = Services::Updates::CheckGameBanana();
            return r;
        },
        [this, done = std::move(done)](Result r) {
            checkingUpdates = false;
            if (!r.reachable) Log::Warning(L"Update check: GitHub could not be reached");
            else Log::Info(L"Update check: latest " + r.info.latestVersion + (r.info.isUpdateAvailable ? L" (newer)" : L""));
            updateInfo = std::move(r.info);
            gbUpdateInfo = std::move(r.gb);
            if (done) done();
        });
}

} // namespace CustomCursor::Main
