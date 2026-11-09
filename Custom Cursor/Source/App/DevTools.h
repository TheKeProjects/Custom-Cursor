#pragma once

// Development aids of the Debug build (environment variables, no window), to compare the services
// with the C# app's:
//   CC_DUMP_SCHEMES=<Cursores folder>|<out.json>  scans the folder as the app does and writes what it
//       found (schemes, sanitized names, counts, preview paths, cursor infos, decoded frame sizes);
//       <out.json>.install.json gets the --install-cursors JSON of every scheme
//   CC_DUMP_REGISTRY=<out.json>   HKCU\Control Panel\Cursors and its Schemes key, with the active
//       scheme and the installed list as the app reads them
//   CC_SANITIZE=<in.txt>|<out.txt>  SanitizeSchemeName on every line (UTF-8)
//   CC_NO_ELEVATION=1   ElevatedInstaller works in this process (see there)
//   CC_TEST_UPDATE=<zip url>  runs the automatic update from that zip (a local server)
//   CC_DUMP_PIXELS=<list.txt>|<out.bin>  the decoded frames of each cursor file listed (raw BGRA)
// (--install-cursors / --uninstall-cursor already run the installer in this process.)
namespace CustomCursor::DevTools {

// True when one of them ran (the process exits with `exitCode`)
bool Run(int& exitCode);

} // namespace CustomCursor::DevTools
