#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Models/CursorScheme.h"
#include "Services/Cursors/SchemeInstaller.h"

// Services\ElevatedInstaller.cs: installs / uninstalls in this process when it runs as an
// administrator, otherwise in a second copy of the exe started with "runas" (one UAC prompt) and
// --install-cursors / --uninstall-cursor "<a JSON file in %TEMP%>", whose exit code says how it went
// (a failure's text in <json>.error). Every function blocks: call them from a worker thread.
namespace CustomCursor::Services::Cursors::ElevatedInstaller {

bool InstallCursorScheme(const Models::CursorScheme& scheme, std::wstring& errorMessage);

// As admin: one after the other with `progress`. Otherwise the whole list goes to one elevated
// process (one UAC prompt) that reports its progress and each scheme's own result
std::vector<SchemeInstaller::Result> InstallMultipleCursorSchemes(const std::vector<Models::CursorScheme>& schemes,
                                                                  const SchemeInstaller::Progress& progress = {});

bool UninstallCursorScheme(const Models::CursorScheme& scheme, const std::optional<std::wstring>& replacement, std::wstring& errorMessage);

} // namespace CustomCursor::Services::Cursors::ElevatedInstaller
