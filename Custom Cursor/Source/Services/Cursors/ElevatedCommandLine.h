#pragma once
#include <string>
#include <vector>

// ElevatedInstaller.HandleCommandLineInstall and App.OnStartup's check: the elevated copy of the exe
// does the work with no window and exits with its code.
namespace CustomCursor::Services::Cursors::ElevatedCommandLine {

// --install-cursors <json> / --uninstall-cursor <json>: 0 on success, 1 on failure (the reason in
// <json>.error when the work itself failed); 0 when the arguments hold neither command
int HandleCommandLineInstall(const std::vector<std::wstring>& args);

// OnStartup: exit right away (with `exitCode`) when there were arguments and they failed or named one
// of the two commands; otherwise the normal start-up goes on. nullopt-like: false = carry on.
bool ShouldExit(const std::vector<std::wstring>& args, int& exitCode);

} // namespace CustomCursor::Services::Cursors::ElevatedCommandLine
