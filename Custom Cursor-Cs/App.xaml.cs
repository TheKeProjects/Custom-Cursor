using System;
using System.Threading.Tasks;
using System.Windows;
using CustomCursor.Services;
using CustomCursor.Utilities;
using CustomCursor.Views;

namespace CustomCursor
{
    public partial class App : Application
    {
        private LoadingWindow? _loadingWindow;

        protected override async void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // Check for command-line arguments first (skip loading screen for command-line operations)
            if (e.Args.Length > 0)
            {
                // Handle elevated installation/uninstallation
                int exitCode = ElevatedInstaller.HandleCommandLineInstall(e.Args);

                if (exitCode != 0 ||
                    Array.Exists(e.Args, arg => arg == "--install-cursors" || arg == "--uninstall-cursor"))
                {
                    // Exit if this was a command-line operation
                    Environment.Exit(exitCode);
                    return;
                }
            }

            // Normal startup - show loading screen
            _loadingWindow = new LoadingWindow();
            _loadingWindow.Show();

            // Perform initialization asynchronously
            await Task.Run(async () =>
            {
                // Initialize path system (detects exe mode and sets up temp folder if needed)
                await Dispatcher.InvokeAsync(() =>
                    _loadingWindow.UpdateStatus("Initializing path system...", 30));
                PathHelper.Initialize();
                await Task.Delay(100);

                // Extract resources to temp folder if running as single-file exe
                await Dispatcher.InvokeAsync(() =>
                    _loadingWindow.UpdateStatus("Extracting resources...", 60));
                ResourceExtractor.ExtractResourcesIfNeeded();
                await Task.Delay(100);

                // Ready to show main window
                await Dispatcher.InvokeAsync(() =>
                    _loadingWindow.UpdateStatus("Loading main window...", 90));
                await Task.Delay(200);
            });

            // Complete loading and show main window
            await _loadingWindow.CompleteLoading();

            var mainWindow = new MainWindow();
            mainWindow.Show();
        }
    }
}
