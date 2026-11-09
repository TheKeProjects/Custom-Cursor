#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "App/MessageLoop.h"
#include "Core/Async.h"
#include "Graphics/Device.h"
#include "Graphics/Fonts.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/App/ResourceExtractor.h"
#include "Services/Cursors/ElevatedCommandLine.h"
#include "Windows/Main/MainWindow.h"
#ifdef _DEBUG
#include "App/DevTools.h"
#endif

using namespace CustomCursor;

namespace {

// e.Args: the command line without the exe
std::vector<std::wstring> Arguments() {
    int count = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::wstring> args;
    for (int i = 1; argv && i < count; ++i) args.emplace_back(argv[i]);
    if (argv) LocalFree(argv);
    return args;
}

} // namespace

// App.xaml.cs (OnStartup): the elevated installation commands first (no window), then the loading
// screen, the resources and the main window
int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    int exitCode = 0;
    if (Services::Cursors::ElevatedCommandLine::ShouldExit(Arguments(), exitCode)) return exitCode;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Async::Initialize();
    Device::Get().Init();
#ifdef _DEBUG
    if (DevTools::Run(exitCode)) return exitCode;
#endif

    int code = 0;
    Services::ResourceExtractor::ExtractLight();   // flags and translations to %TEMP%\CustomCursor
    Loc::LoadSaved();
    Fonts::Initialize();
    Log::Start();
    {
        Main::MainWindow window;
        if (window.Open()) code = App::RunMessageLoop();
        else Log::Error(L"The main window could not be opened");
    }
    Log::Info(L"Custom Cursor closed");
    Async::Shutdown();
    Device::Get().Shutdown();
    CoUninitialize();
    return code;
}
