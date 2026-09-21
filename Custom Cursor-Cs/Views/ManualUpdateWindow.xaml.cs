using System;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using CustomCursor.Services;

namespace CustomCursor.Views
{
    // Fallback for when the automatic updater (see MainWindow's Updates overlay) doesn't
    // detect or install a new version. Always fetches the latest published release —
    // regardless of what the local version comparison thinks — so the user can force a
    // reinstall, or fall back to downloading it manually in a browser.
    public partial class ManualUpdateWindow : Window
    {
        private static readonly Color AccentColor = Color.FromRgb(0x00, 0xCC, 0xAA);
        private static readonly Color ErrorColor  = Color.FromRgb(0xE6, 0x51, 0x00);

        private readonly UpdateService _updateService;
        private UpdateInfo? _releaseInfo;
        private bool _isDownloading;

        public ManualUpdateWindow(UpdateService updateService)
        {
            InitializeComponent();
            _updateService = updateService;

            HeaderText.Text      = "MANUAL UPDATE";
            IntroText.Text       = "If the app isn't detecting or installing updates on its own, you can force a manual download and reinstall of the latest version here.";
            DownloadBtnText.Text = "DOWNLOAD & REPLACE APP";
            BrowserBtnText.Text  = "OPEN DOWNLOAD PAGE IN BROWSER";

            DownloadBtn.IsEnabled = false;
            SetStatus("Checking for the latest release...", "", AccentColor);

            Loaded += async (_, _) => await CheckAsync();
        }

        private async Task CheckAsync()
        {
            _releaseInfo = await _updateService.GetLatestReleaseInfoAsync();

            if (!string.IsNullOrWhiteSpace(_releaseInfo.DownloadUrl))
            {
                var version = string.IsNullOrWhiteSpace(_releaseInfo.LatestVersion)
                    ? "The latest version"
                    : $"Version v{_releaseInfo.LatestVersion}";

                SetStatus(
                    $"{version} is ready to download and install.",
                    $"{_releaseInfo.FileName}  |  {UpdateService.FormatFileSize(_releaseInfo.FileSize)}",
                    AccentColor);
                DownloadBtn.IsEnabled = true;
            }
            else
            {
                SetStatus(
                    "Couldn't reach the update server. Try again later, or open the download page in your browser instead.",
                    "",
                    ErrorColor);
            }
        }

        private void SetStatus(string title, string message, Color color)
        {
            StatusTitleText.Text       = title;
            StatusMessageText.Text     = message;
            var brush = new SolidColorBrush(color);
            StatusIcon.Foreground      = brush;
            StatusTitleText.Foreground = brush;
        }

        private async void DownloadBtn_Click(object sender, RoutedEventArgs e)
        {
            if (_isDownloading || _releaseInfo == null) return;
            _isDownloading = true;

            DownloadBtn.IsEnabled = false;
            BrowserBtn.IsEnabled  = false;
            DownloadProgressPanel.Visibility = Visibility.Visible;

            // Created on the UI thread, so Progress<T> marshals its callback back here.
            var progress = new Progress<int>(pct =>
            {
                DownloadProgressBar.Value = pct;
                DownloadProgressText.Text = $"Downloading...  {pct}%";
            });

            var ok = await _updateService.DownloadAndInstallLatestExeAsync(progress);

            // On success the app restarts itself and this process exits before reaching here.
            if (!ok)
            {
                _isDownloading = false;
                DownloadProgressPanel.Visibility = Visibility.Collapsed;
                DownloadBtn.IsEnabled = true;
                BrowserBtn.IsEnabled  = true;
                SetStatus(
                    "Couldn't reach the update server. Try again later, or open the download page in your browser instead.",
                    "",
                    ErrorColor);
            }
        }

        private void BrowserBtn_Click(object sender, RoutedEventArgs e)
        {
            var url = string.IsNullOrWhiteSpace(_releaseInfo?.ReleaseUrl)
                ? $"{AppVersion.GetGitHubRepoUrl()}/releases/latest"
                : _releaseInfo.ReleaseUrl;

            try
            {
                Process.Start(new ProcessStartInfo { FileName = url, UseShellExecute = true });
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[ManualUpdateWindow] Failed to open browser: {ex.Message}");
            }
        }

        private void Close_Click(object sender, RoutedEventArgs e) => Close();
    }
}
