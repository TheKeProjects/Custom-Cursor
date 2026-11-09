// The app's changelog. Add new versions at the TOP (newest first).
// Each entry: version, what's shown after it (a date: "| 21/09/26"), and its lines: "## " section
// headers (Additions, Redesigns, Seasonal Themes, Improvements, Performance, Bug Fixes, Other) and
// "- " changes. The published changelog.txt uses the same text (Tools\GenerateChangelog.ps1).
#include "Services/App/ChangelogData.h"

namespace CustomCursor::Services::Changelog {

const std::vector<Entry>& Entries() {
    static const std::vector<Entry> entries = {
        { L"3.0.0", L"| 09/11/26", {
            L"## Redesigns",
            L"- New look: a side bar with animated navigation, banners and redrawn menus",
            L"- Scheme cards: animated cursor on a glowing stage, active and installed badges",
            L"- Categories are now chips with their counts, the highlight sliding between them",
            L"- Preview: a big stage for any cursor of the scheme, click a card to see it there",
            L"- Selecting schemes brings up a bar to install them all or clear the selection",
            L"- Updates and Changelog are pages of their own",
            L"- New logo: Clicky",
            L"## Seasonal Themes",
            L"- Halloween, Summer, Christmas and LGBTQ+ themes: colours, scenery, particles and logo",
            L"- Default theme: follows the season of the current release",
            L"## Additions",
            L"- Languages: English, Spanish, French and Italian",
            L"- The search box finds schemes by name or folder",
            L"- Animated previews for the schemes with a GIF",
            L"- Installing several schemes shows real progress and each scheme's own result",
            L"## Performance",
            L"- Long operations no longer freeze the window",
            L"## Bug Fixes",
            L"- Offline, the updates page says it couldn't check instead of \"You're up to date\"",
            L"- The manual update no longer installs an older version over a newer one",
            L"- Schemes with accents or other alphabets in their name no longer overwrite each other",
            L"- Uninstalling can no longer remove Windows' own cursors",
            L"- Approving the admin prompt with another account changes your cursors, not that account's",
            L"- Cancelling the admin prompt no longer leaves a temporary file behind",
            L"- Animated cursors with an odd-sized frame no longer lose the following frames",
        } },
        { L"2.1.0", L"| 21/09/26", {
            L"## Additions",
            L"- Added a Wii Cursor",
            L"- Added a manual system update",
            L"## Bug Fixes",
            L"- Fixed an error in the installation of the cursors",
        } },
        { L"2.0.2", L"| 08/08/26", {
            L"## Additions",
            L"- Added a Sabrina Carpenter Cursor",
        } },
        { L"2.0.1", L"| 08/08/26", {
            L"## Additions",
            L"- Added more TADC Cursors",
            L"- Added more Utaloid Cursors",
            L"- Added more Vocaloid Cursors",
            L"- Added Umamusume Cursors",
            L"## Bug Fixes",
            L"- Fixed installation of the cursor: Simple",
            L"- Fixed the Hand cursor preview card incorrectly labeled \"Link Select\" instead of \"Handwriting\"",
        } },
        { L"2.0.0", L"| 08/08/26", {
            L"## Redesigns",
            L"- Complete UI redesign with a refined \"dark glass\" style",
            L"- The top action bar is now a segmented toolbar",
            L"- The Categories sidebar is now a flat list with an accent bar on the selected category",
            L"- \"Install Selected\" now uses the same highlighted style as \"Apply This Cursor Scheme\"",
            L"## Additions",
            L"- New preview button on every scheme card, showing every cursor in the set, animated or static",
            L"## Bug Fixes",
            L"- Fixed a severe UI freeze when opening the preview of schemes with several animated cursors",
        } },
        { L"1.0.7", L"| 25/09/25", {
            L"## Additions",
            L"- Added the Colores section",
            L"- Added to Extras: Clear 4.1 Mini, Clear 4.2 and Simple",
            L"- Added the Silksong section with the Hornet cursor",
            L"- Added support for installing several cursors at once",
        } },
        { L"1.0.6", L"| 04/09/25", {
            L"## Additions",
            L"- Added a basic tutorial",
        } },
        { L"1.0.5", L"| 30/07/25", {
            L"## Bug Fixes",
            L"- Fixed a problem with the links of the update system",
        } },
        { L"1.0.4", L"| 30/07/25", {
            L"## Improvements",
            L"- Completely new update system",
        } },
        { L"1.0.3", L"| 30/07/25", {
            L"## Bug Fixes",
            L"- Fixed problems with the updates",
        } },
        { L"1.0.2", L"| 30/07/25", {
            L"## Bug Fixes",
            L"- Fixed problems with the updates",
        } },
        { L"1.0.1", L"| 30/07/25", {
            L"## Bug Fixes",
            L"- Fixed problems with the updates",
        } },
        { L"1.0.0", L"| 30/07/25", {
            L"## Additions",
            L"- First version of Custom Cursor",
        } },
    };
    return entries;
}

} // namespace CustomCursor::Services::Changelog
