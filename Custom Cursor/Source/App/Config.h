#pragma once

// Every fixed name and path the app uses, in one place (the C# kept them in Utilities\Constants.cs,
// PathHelper.cs, ElevatedInstaller.cs, TutorialManager.cs and Services\AppVersion.cs). The services
// reference these instead of keeping their own copies.
namespace CustomCursor::Config {

// ── Folders ─────────────────────────────────────────────────────────────────
// %TEMP%\<WorkFolder>: the embedded Cursores\, Tutorial\, Assets\ and Translations\ are unpacked
// here on startup (PathHelper.Initialize's "CustomCursor" temp folder).
inline constexpr const wchar_t* WorkFolder = L"CustomCursor";
// %LOCALAPPDATA%\<DataFolder>: the C++ port's own files (language.cfg, logs\); the C# kept nothing
inline constexpr const wchar_t* DataFolder = L"CustomCursor";
inline constexpr const wchar_t* LanguageFileName = L"language.cfg";

// ── Cursor schemes (Constants.cs) ───────────────────────────────────────────
inline constexpr const wchar_t* CursorFolderName = L"Cursores";   // CursorFolderName
inline constexpr const wchar_t* SchemeFileName = L"scheme.inf";   // SchemeFileName
inline constexpr const wchar_t* DefaultCategory = L"General";     // a scheme.inf right in Cursores\ (GetCategoryFromPath)
inline constexpr const wchar_t* AllCategory = L"All";             // _currentCategory's "everything" key (shown translated)
inline constexpr const wchar_t* UnknownSchemeName = L"Unknown Scheme";   // ParseInfFile without SCHEME_NAME
// FindPreviewImage: the first file with one of these (lower-case) extensions
inline constexpr const wchar_t* PreviewExtensions[] = { L".png", L".jpg", L".jpeg", L".gif" };

// ── Windows (PathHelper.GetWindowsCursorsDirectory, CursorRegistry) ─────────
inline constexpr const wchar_t* WindowsCursorsFolder = L"Cursors";   // %WINDIR%\Cursors
inline constexpr const wchar_t* DefaultWindowsDir = L"C:\\Windows";  // when %WINDIR% isn't set
// What the Schemes string points at: "%SystemRoot%\Cursors\<scheme>\<file>"
inline constexpr const wchar_t* SchemeFilePrefix = L"%SystemRoot%\\Cursors\\";
// HKCU keys (CursorsRegistryPath / CursorSchemesRegistryPath)
inline constexpr const wchar_t* CursorsKey = L"Control Panel\\Cursors";
inline constexpr const wchar_t* SchemesKey = L"Control Panel\\Cursors\\Schemes";
inline constexpr const wchar_t* SchemeSourceValue = L"Scheme Source";   // DWORD: 1 user scheme, 0 Windows default
inline constexpr const wchar_t* LegacySchemeNameValue = L"Scheme Name"; // what older versions of the app wrote

// ── Elevation (ElevatedInstaller.cs) ────────────────────────────────────────
// The elevated copy of the exe is started with one of these and the path of a JSON file in %TEMP%;
// a failure is written to <json>.error
inline constexpr const wchar_t* InstallArgument = L"--install-cursors";
inline constexpr const wchar_t* UninstallArgument = L"--uninstall-cursor";
inline constexpr const wchar_t* InstallJsonPrefix = L"cursor_install_";
inline constexpr const wchar_t* UninstallJsonPrefix = L"cursor_uninstall_";
inline constexpr const wchar_t* ErrorFileSuffix = L".error";
// New in the C++ port (a C# copy ignores them): the user whose settings to change ("--user <SID>",
// when another administrator's account approved the prompt), and two files next to the JSON the
// elevated copy writes for the waiting one: the progress ("<current>\t<total>\t<name>") and each
// scheme's result ([{"Name","Success","Error"}])
inline constexpr const wchar_t* UserArgument = L"--user";
inline constexpr const wchar_t* ProgressFileSuffix = L".progress";
inline constexpr const wchar_t* ResultsFileSuffix = L".results";

// ── Tutorial (TutorialManager.cs) ───────────────────────────────────────────
inline constexpr const wchar_t* TutorialFolderName = L"Tutorial";
inline constexpr const wchar_t* VideoExtensions[] = { L".mp4", L".webm", L".avi", L".mov", L".mkv", L".wmv" };

// ── Animated cursors (AnimatedCursorControl.cs) ─────────────────────────────
inline constexpr int CursorFrameMs = 150;      // FrameRate: one shared 150 ms tick for every animation
inline constexpr int CursorMaxFrames = 10;     // more frames are skipped evenly
inline constexpr int CursorMaxSize = 32;       // frames bigger than 32 x 32 are scaled down
// false = the C#'s behaviour; true would play every frame at the .ani's own rate (pending decision)
inline constexpr bool CursorFullAnimation = false;

// ── Updates (Services\UpdateService.cs, AppVersion.cs) ──────────────────────
// version.txt and changelog.txt at the root of `Branch` (raw.githubusercontent.com). Each release
// carries the zip CustomCursorv<x-y-z>.zip (ZipPrefix + the version with dashes, the exe inside:
// the automatic update) and the bare exe (LatestExeUrl: the manual update)
namespace GitHub {
inline constexpr const wchar_t* Owner = L"TheKeProjects";
inline constexpr const wchar_t* Repo = L"Custom-Cursor";
inline constexpr const wchar_t* Branch = L"main";
inline constexpr const wchar_t* ExeAsset = L"CustomCursor.exe";
inline constexpr const wchar_t* ZipPrefix = L"CustomCursorv";
inline constexpr const wchar_t* LatestExeUrl = L"https://github.com/TheKeProjects/Custom-Cursor/releases/latest/download/CustomCursor.exe";
} // namespace GitHub
namespace GameBanana {
inline constexpr int ToolId = 0;   // GB_TOOL_ID: 0 = the GameBanana check is off
inline constexpr const wchar_t* ApiItemUrl = L"https://api.gamebanana.com/Core/Item/Data?itemtype=Tool&itemid=";
inline constexpr const wchar_t* ApiFields = L"&fields=name,Files().aFiles()";
inline constexpr const wchar_t* FallbackZipName = L"gamebanana_update.zip";
} // namespace GameBanana
// Where an update is staged (under %TEMP%) before the helper batch swaps it in
inline constexpr const wchar_t* UpdateFolder = L"CustomCursorManager_Update";

} // namespace CustomCursor::Config
