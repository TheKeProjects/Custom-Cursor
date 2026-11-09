#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Models/CursorCategory.h"
#include "Models/CursorInfo.h"
#include "Models/CursorScheme.h"

// Services\CursorParser.cs (the folder scan) with the small helpers around a scheme the C# kept in
// CursorInfoLoader.cs, CursorImageExtractor.GetSchemePreviewPath and MainWindow.CountChangedCursors.
namespace CustomCursor::Services::Cursors::SchemeScanner {

// FindCursorSets: every folder under `cursorDirectory` with a scheme.inf (hidden folders skipped),
// grouped by the first folder of its relative path, the categories in the order they were found.
// A scheme.inf that can't be read is skipped; empty when the folder doesn't exist.
std::vector<Models::CursorCategory> FindCursorSets(const std::wstring& cursorDirectory);

// FindPreviewImage: the first .png / .jpg / .jpeg / .gif of the folder, in Directory.GetFiles' order
std::optional<std::wstring> FindPreviewImage(const std::wstring& folderPath);

// The cursor files ApplyCursorScheme copies: not blank, no '$', no "..", no invalid file name characters
bool IsCopyableFileName(const std::wstring& fileName);

// CountChangedCursors: how many roles installing the scheme replaces (a copyable file that exists)
int CountChangedCursors(const Models::CursorScheme& scheme);

// CursorInfoLoader.LoadAllCursorImages: the roles, in order, whose file exists in the scheme's folder
std::vector<Models::CursorInfo> LoadCursorInfos(const Models::CursorScheme& scheme);

// GetSchemePreviewPath: the preview image when it's there, else the folder's first .gif, then .ani,
// then .png / .jpg / .jpeg; nullopt for the placeholder
std::optional<std::wstring> PreviewPath(const Models::CursorScheme& scheme);

} // namespace CustomCursor::Services::Cursors::SchemeScanner
