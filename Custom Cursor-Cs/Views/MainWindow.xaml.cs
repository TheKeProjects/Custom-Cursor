using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using CustomCursor.Controls;
using CustomCursor.Models;
using CustomCursor.Services;
using CustomCursor.Utilities;

namespace CustomCursor.Views
{
    public partial class MainWindow : Window
    {
        private readonly CursorParser _cursorParser;
        private readonly ElevatedInstaller _elevatedInstaller;
        private readonly CursorInfoLoader _cursorInfoLoader;
        private readonly TutorialManager _tutorialManager;
        private readonly CursorImageExtractor _imageExtractor;
        private readonly UpdateService _updateService;

        private Dictionary<string, List<CursorScheme>> _cursorCategories;
        private List<CursorScheme> _selectedSchemes;
        private string _currentCategory = "All";
        private CursorScheme? _currentPreviewScheme;
        private List<CursorInfo>? _currentCursorInfos;
        private bool _tutorialPlaying = true;
        private Button? _selectedCategoryButton;

        private UpdateInfo? _updateInfo;
        private GbUpdateInfo? _gbUpdateInfo;
        private bool _showingInstallView;
        private bool _isGbInstall;
        private bool _isDownloading;

        // Shared design tokens for elements built in code-behind (kept in sync with App.xaml)
        private static readonly SolidColorBrush CardBrush = Freeze(new SolidColorBrush(Color.FromArgb(0xF5, 0x1E, 0x23, 0x30)));
        private static readonly SolidColorBrush CardBorderBrush = Freeze(new SolidColorBrush(Color.FromArgb(0x1F, 0xFF, 0xFF, 0xFF)));
        private static readonly SolidColorBrush TextPrimaryBrush = Freeze(new SolidColorBrush(Color.FromRgb(0xF5, 0xF7, 0xFA)));
        private static readonly SolidColorBrush TextSecondaryBrush = Freeze(new SolidColorBrush(Color.FromRgb(0x9A, 0xA5, 0xB1)));
        private static readonly SolidColorBrush DangerBrush = Freeze(new SolidColorBrush(Color.FromRgb(0xE5, 0x48, 0x4D)));
        private static readonly SolidColorBrush NeutralDisabledBrush = Freeze(new SolidColorBrush(Color.FromRgb(0x4B, 0x55, 0x63)));
        private static readonly LinearGradientBrush AccentGradientBrush = Freeze(new LinearGradientBrush
        {
            StartPoint = new Point(0, 0),
            EndPoint = new Point(1, 1),
            GradientStops = new GradientStopCollection
            {
                new GradientStop(Color.FromRgb(0x6F, 0xC6, 0xF2), 0),
                new GradientStop(Color.FromRgb(0x3D, 0x7D, 0xD8), 1)
            }
        });

        private static T Freeze<T>(T brush) where T : Freezable
        {
            brush.Freeze();
            return brush;
        }

        public MainWindow()
        {
            InitializeComponent();

            _cursorParser = new CursorParser();
            _elevatedInstaller = new ElevatedInstaller();
            _cursorInfoLoader = new CursorInfoLoader();
            _tutorialManager = new TutorialManager();
            _imageExtractor = new CursorImageExtractor();
            _updateService = new UpdateService();
            _selectedSchemes = new List<CursorScheme>();
            _cursorCategories = new Dictionary<string, List<CursorScheme>>();

            VersionText.Text = AppVersion.GetDisplayVersion();

            // Add window size changed handler for responsive preview cards
            this.SizeChanged += MainWindow_SizeChanged;

            LoadCursorSets();

            this.Closed += (s, e) => _updateService.Dispose();
            _ = CheckForUpdatesOnStartupAsync();
        }

        #region Updates

        private async System.Threading.Tasks.Task CheckForUpdatesOnStartupAsync()
        {
            try
            {
                _updateInfo = await _updateService.CheckForUpdatesAsync();
                _gbUpdateInfo = await _updateService.CheckGameBananaUpdateAsync();

                if (_updateInfo.IsUpdateAvailable || _gbUpdateInfo.IsUpdateAvailable)
                    UpdateAvailableBadge.Visibility = Visibility.Visible;

                if (UpdatesOverlay.Visibility == Visibility.Visible)
                    RefreshUpdateCheckView();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Startup update check failed: {ex.Message}");
            }
        }

        private void CheckUpdatesButton_Click(object sender, RoutedEventArgs e)
        {
            ShowUpdateCheckView();
            UpdatesOverlay.Visibility = Visibility.Visible;
        }

        private void ShowUpdateCheckView()
        {
            _showingInstallView = false;
            UpdateCheckView.Visibility = Visibility.Visible;
            UpdateInstallView.Visibility = Visibility.Collapsed;
            InstallButtonsPanel.Visibility = Visibility.Collapsed;
            DownloadProgressPanel.Visibility = Visibility.Collapsed;
            RefreshUpdateCheckView();
        }

        private void RefreshUpdateCheckView()
        {
            UpdateCurrentVersionText.Text = AppVersion.GetDisplayVersion();

            UpdateStatusBtn.Visibility = Visibility.Collapsed;
            GbUpdateBtn.Visibility = Visibility.Collapsed;
            UpdateDetailsBorder.Visibility = Visibility.Collapsed;

            if (_updateInfo == null)
                return;

            UpdateStatusBtn.Visibility = Visibility.Visible;

            if (_updateInfo.IsUpdateAvailable)
            {
                UpdateStatusBtn.Background = AccentGradientBrush;
                UpdateStatusIcon.Text = "⬆️";
                UpdateStatusTitle.Text = "Update available";
                UpdateStatusMessage.Text = $"Version {_updateInfo.LatestVersion} is ready to install";
                UpdateStatusBtn.IsEnabled = true;

                UpdateDetailsBorder.Visibility = Visibility.Visible;
                UpdateLatestVersionText.Text = $"v{_updateInfo.LatestVersion}";
                UpdateFileNameText.Text = _updateInfo.FileName;
                UpdateFileSizeText.Text = UpdateService.FormatFileSize(_updateInfo.FileSize);
            }
            else if (!string.IsNullOrEmpty(_updateInfo.LatestVersion))
            {
                UpdateStatusBtn.Background = CardBrush;
                UpdateStatusIcon.Text = "✅";
                UpdateStatusTitle.Text = "You're up to date";
                UpdateStatusMessage.Text = $"Version {AppVersion.GetDisplayVersion()} is the latest";
                UpdateStatusBtn.IsEnabled = false;
            }
            else
            {
                UpdateStatusBtn.Background = DangerBrush;
                UpdateStatusIcon.Text = "⚠️";
                UpdateStatusTitle.Text = "Couldn't check for updates";
                UpdateStatusMessage.Text = "Check your internet connection and try again";
                UpdateStatusBtn.IsEnabled = false;
            }

            if (!_updateInfo.IsUpdateAvailable && (_gbUpdateInfo?.IsUpdateAvailable ?? false))
            {
                GbUpdateBtn.Visibility = Visibility.Visible;
                GbUpdateTitle.Text = "Update available on GameBanana";
                GbUpdateMessage.Text = $"Version {_gbUpdateInfo.LatestVersion} is ready to install";
            }

            UpdateAvailableBadge.Visibility = (_updateInfo.IsUpdateAvailable || (_gbUpdateInfo?.IsUpdateAvailable ?? false))
                ? Visibility.Visible
                : Visibility.Collapsed;
        }

        private void ShowInstallView(bool isGb)
        {
            _showingInstallView = true;
            _isGbInstall = isGb;

            UpdateCheckView.Visibility = Visibility.Collapsed;
            UpdateInstallView.Visibility = Visibility.Visible;
            InstallButtonsPanel.Visibility = Visibility.Visible;
            DownloadProgressPanel.Visibility = Visibility.Collapsed;

            string latestVersion = isGb ? (_gbUpdateInfo?.LatestVersion ?? "") : (_updateInfo?.LatestVersion ?? "");
            string fileName = isGb ? (_gbUpdateInfo?.FileName ?? "") : (_updateInfo?.FileName ?? "");
            long fileSize = isGb ? (_gbUpdateInfo?.FileSize ?? 0) : (_updateInfo?.FileSize ?? 0);
            string changelog = isGb ? (_gbUpdateInfo?.Changelog ?? "") : (_updateInfo?.Changelog ?? "");

            InstallTitleText.Text = $"Update ready — v{latestVersion}";
            InstallSubtitleText.Text = fileSize > 0
                ? $"{fileName} • {UpdateService.FormatFileSize(fileSize)}"
                : fileName;
            ChangelogText.Text = string.IsNullOrWhiteSpace(changelog) ? "No changelog available." : changelog;
        }

        private void CheckUpdatesBanner_Click(object sender, RoutedEventArgs e)
        {
            _ = RecheckForUpdatesAsync();
        }

        private async System.Threading.Tasks.Task RecheckForUpdatesAsync()
        {
            CheckUpdatesBannerBtn.IsEnabled = false;
            UpdateCheckHint.Text = "Checking...";
            try
            {
                _updateInfo = await _updateService.CheckForUpdatesAsync();
                _gbUpdateInfo = await _updateService.CheckGameBananaUpdateAsync();
            }
            finally
            {
                UpdateCheckHint.Text = "Click to check again";
                CheckUpdatesBannerBtn.IsEnabled = true;
                RefreshUpdateCheckView();
            }
        }

        private void UpdateStatusBtn_Click(object sender, RoutedEventArgs e)
        {
            if (_updateInfo?.IsUpdateAvailable ?? false)
                ShowInstallView(isGb: false);
        }

        private void GbUpdateBtn_Click(object sender, RoutedEventArgs e)
        {
            if (_gbUpdateInfo?.IsUpdateAvailable ?? false)
                ShowInstallView(isGb: true);
        }

        private async void AcceptInstallBtn_Click(object sender, RoutedEventArgs e)
        {
            if (_isDownloading)
                return;

            _isDownloading = true;
            InstallButtonsPanel.Visibility = Visibility.Collapsed;
            DownloadProgressPanel.Visibility = Visibility.Visible;

            var progress = new Progress<int>(percent =>
            {
                UpdateDownloadProgressBar.Value = percent;
                UpdateDownloadProgressText.Text = $"Downloading... {percent}%";
            });

            bool success = _isGbInstall && _gbUpdateInfo != null
                ? await _updateService.DownloadAndInstallGbUpdateAsync(_gbUpdateInfo, progress)
                : _updateInfo != null && await _updateService.DownloadAndInstallUpdateAsync(_updateInfo, progress);

            // On success the app restarts itself and this process exits before reaching here.
            if (!success)
            {
                _isDownloading = false;
                MessageBox.Show(
                    "The update could not be downloaded or installed. Please try again later or download it manually from GitHub.",
                    "Update Failed",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                ShowUpdateCheckView();
            }
        }

        private void CancelInstallBtn_Click(object sender, RoutedEventArgs e) => ShowUpdateCheckView();

        private void CloseUpdatesBtn_Click(object sender, RoutedEventArgs e)
        {
            UpdatesOverlay.Visibility = Visibility.Collapsed;
            if (_showingInstallView)
                ShowUpdateCheckView();
        }

        private void ManualUpdateLink_Click(object sender, RoutedEventArgs e)
        {
            var window = new ManualUpdateWindow(_updateService) { Owner = this };
            window.ShowDialog();
        }

        #endregion

        private void MainWindow_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            // Refresh preview cards when window is resized
            if (PreviewPanel.Visibility == Visibility.Visible && _currentCursorInfos != null)
            {
                RefreshCursorPreviewCards(_currentCursorInfos);
            }
        }

        #region Loading

        private void LoadCursorSets()
        {
            try
            {
                string cursorDir = PathHelper.GetCursorDirectory();
                _cursorCategories = _cursorParser.FindCursorSets(cursorDir);

                if (_cursorCategories.Count == 0)
                {
                    MessageBox.Show(
                        $"No cursor sets found.\n\nPlease place cursor sets in:\n{cursorDir}\n\nEach set must have a scheme.inf file.",
                        "No Cursors Found",
                        MessageBoxButton.OK,
                        MessageBoxImage.Warning
                    );
                    return;
                }

                RefreshCategories();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error loading cursor sets: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void RefreshCategories()
        {
            CategoriesListPanel.Children.Clear();

            // Add "All" category
            _selectedCategoryButton = null;
            var allButton = CreateCategoryButton("All");
            CategoriesListPanel.Children.Add(allButton);

            // Add each category
            foreach (string category in _cursorCategories.Keys.OrderBy(k => k))
            {
                var button = CreateCategoryButton(category);
                CategoriesListPanel.Children.Add(button);
            }

            // Select "All" by default
            SelectCategoryButton(allButton);
            DisplaySchemes("All");
        }

        private sealed class NavItemParts
        {
            public required Border Indicator { get; init; }
            public required TextBlock Label { get; init; }
        }

        private Button CreateCategoryButton(string category)
        {
            var indicator = new Border
            {
                Width = 3,
                Margin = new Thickness(0, 3, 10, 3),
                CornerRadius = new CornerRadius(0, 2, 2, 0),
                Background = Brushes.Transparent
            };

            var label = new TextBlock
            {
                Text = $"📁 {category}",
                FontSize = 13,
                FontFamily = new System.Windows.Media.FontFamily("Segoe UI Emoji"),
                Foreground = TextSecondaryBrush,
                VerticalAlignment = VerticalAlignment.Center,
                TextWrapping = TextWrapping.NoWrap,
                TextTrimming = TextTrimming.CharacterEllipsis
            };

            var content = new Grid();
            content.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            content.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            Grid.SetColumn(indicator, 0);
            Grid.SetColumn(label, 1);
            content.Children.Add(indicator);
            content.Children.Add(label);

            var button = new Button
            {
                Content = content,
                Style = FindResource("NavListButton") as Style,
                Tag = new NavItemParts { Indicator = indicator, Label = label }
            };

            button.Click += (s, e) =>
            {
                SelectCategoryButton(button);
                _currentCategory = category;
                DisplaySchemes(category);
            };

            return button;
        }

        private void SelectCategoryButton(Button button)
        {
            if (_selectedCategoryButton?.Tag is NavItemParts previousParts)
            {
                previousParts.Indicator.Background = Brushes.Transparent;
                previousParts.Label.Foreground = TextSecondaryBrush;
                previousParts.Label.FontWeight = FontWeights.Normal;
            }

            if (button.Tag is NavItemParts parts)
            {
                parts.Indicator.Background = AccentGradientBrush;
                parts.Label.Foreground = TextPrimaryBrush;
                parts.Label.FontWeight = FontWeights.Bold;
            }

            _selectedCategoryButton = button;
        }

        private void DisplaySchemes(string category)
        {
            SchemesWrapPanel.Children.Clear();
            _selectedSchemes.Clear();
            UpdateInstallButtonState();

            List<CursorScheme> schemes = new List<CursorScheme>();

            if (category == "All")
            {
                foreach (var categorySchemes in _cursorCategories.Values)
                    schemes.AddRange(categorySchemes);
            }
            else if (_cursorCategories.ContainsKey(category))
            {
                schemes = _cursorCategories[category];
            }

            if (schemes.Count == 0)
            {
                var noSchemesText = new TextBlock
                {
                    Text = "No cursor schemes found in this category",
                    Foreground = TextSecondaryBrush,
                    FontSize = 14,
                    Margin = new Thickness(20)
                };
                SchemesWrapPanel.Children.Add(noSchemesText);
                return;
            }

            foreach (var scheme in schemes)
            {
                var schemeCard = CreateSchemeCard(scheme);
                SchemesWrapPanel.Children.Add(schemeCard);
            }
        }

        // How many cursor roles installing this scheme will actually replace. Uses the same
        // filename rules as CursorRegistry.ApplyCursorScheme, and requires the file to exist
        // so a role that would point at a missing file isn't counted.
        private static int CountChangedCursors(CursorScheme scheme)
        {
            int count = 0;

            foreach (string role in Constants.RegistryNames.Keys)
            {
                if (!scheme.CursorMap.TryGetValue(role, out string? filename))
                    continue;

                if (string.IsNullOrWhiteSpace(filename) ||
                    filename.Contains('$') ||
                    filename.Contains("..") ||
                    filename.IndexOfAny(System.IO.Path.GetInvalidFileNameChars()) >= 0)
                    continue;

                if (File.Exists(System.IO.Path.Combine(scheme.Path, filename)))
                    count++;
            }

            return count;
        }

        private UIElement CreateSchemeCard(CursorScheme scheme)
        {
            // Container grid with fixed size
            var containerGrid = new Grid
            {
                Width = 220,
                Height = 280,
                Margin = new Thickness(8),
                Cursor = System.Windows.Input.Cursors.Hand
            };

            var gradientBrush = new LinearGradientBrush
            {
                StartPoint = new Point(0, 0),
                EndPoint = new Point(1, 1)
            };
            gradientBrush.GradientStops.Add(new GradientStop(Color.FromArgb(255, 255, 255, 255), 0));
            gradientBrush.GradientStops.Add(new GradientStop(Color.FromArgb(255, 136, 221, 255), 0.25));
            gradientBrush.GradientStops.Add(new GradientStop(Color.FromArgb(255, 68, 153, 255), 0.5));
            gradientBrush.GradientStops.Add(new GradientStop(Color.FromArgb(255, 136, 221, 255), 0.75));
            gradientBrush.GradientStops.Add(new GradientStop(Color.FromArgb(255, 255, 255, 255), 1));

            // Hover border overlay (initially hidden)
            var hoverBorder = new Border
            {
                BorderBrush = gradientBrush,
                BorderThickness = new Thickness(0),
                CornerRadius = new CornerRadius(12),
                IsHitTestVisible = false  // Don't block mouse events
            };

            // Main content border
            var border = new Border
            {
                Background = CardBrush,
                BorderBrush = CardBorderBrush,
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(12),
                Padding = new Thickness(10),
                Margin = new Thickness(8)
            };

            var grid = new Grid();
            grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(85) });
            grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            // Try to load actual cursor preview (animated GIF and .ani support)
            var previewPath = _imageExtractor.GetSchemePreviewPath(scheme.Path, scheme.PreviewImagePath);

            Border imagePreview;
            if (previewPath != null)
            {
                string ext = System.IO.Path.GetExtension(previewPath).ToLowerInvariant();

                if (ext == ".ani")
                {
                    // Use AnimatedCursorControl for .ani files
                    var animatedCursor = new AnimatedCursorControl
                    {
                        CursorPath = previewPath,
                        Stretch = Stretch.Uniform,
                        Margin = new Thickness(5)
                    };

                    imagePreview = new Border
                    {
                        Background = Brushes.Transparent,
                        CornerRadius = new CornerRadius(5),
                        Child = animatedCursor
                    };
                }
                else
                {
                    var previewImage = _imageExtractor.LoadAnimatedImage(previewPath);

                    if (previewImage != null)
                    {
                        // Show actual cursor image (transparent background for cursors)
                        imagePreview = new Border
                        {
                            Background = Brushes.Transparent,
                            CornerRadius = new CornerRadius(5),
                            Child = new System.Windows.Controls.Image
                            {
                                Source = previewImage,
                                Stretch = Stretch.Uniform,
                                Margin = new Thickness(5)
                            }
                        };
                    }
                    else
                    {
                        // Fallback to gradient placeholder
                        imagePreview = CreatePlaceholderPreview();
                    }
                }
            }
            else
            {
                // Fallback to gradient placeholder
                imagePreview = CreatePlaceholderPreview();
            }

            Grid.SetRow(imagePreview, 0);
            grid.Children.Add(imagePreview);

            // Scheme info
            var infoPanel = new StackPanel { Margin = new Thickness(0, 10, 0, 0) };

            var nameText = new TextBlock
            {
                Text = scheme.Name,
                FontSize = 14,
                FontWeight = FontWeights.Bold,
                Foreground = TextPrimaryBrush,
                TextWrapping = TextWrapping.Wrap
            };
            infoPanel.Children.Add(nameText);

            var pathText = new TextBlock
            {
                Text = scheme.RelativePath,
                FontSize = 10,
                Foreground = TextSecondaryBrush,
                TextWrapping = TextWrapping.Wrap,
                Margin = new Thickness(0, 5, 0, 0)
            };
            infoPanel.Children.Add(pathText);

            int cursorCount = CountChangedCursors(scheme);
            var countText = new TextBlock
            {
                Text = cursorCount == 1 ? "1 cursor" : $"{cursorCount} cursors",
                FontSize = 11,
                FontWeight = FontWeights.SemiBold,
                Foreground = (Brush)FindResource("AccentLightBrush"),
                ToolTip = "Number of Windows cursors this scheme will change when installed",
                Margin = new Thickness(0, 4, 0, 0)
            };
            infoPanel.Children.Add(countText);

            Grid.SetRow(infoPanel, 1);
            grid.Children.Add(infoPanel);

            // Buttons (without checkbox)
            var buttonPanel = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Center,
                Margin = new Thickness(0, 5, 0, 0)
            };

            // Check if cursor is installed
            bool isInstalled = _elevatedInstaller.IsSchemeInstalled(scheme.Name);

            var statusBtn = new Button
            {
                Style = FindResource("ModernButton") as Style,
                Width = 52,
                Height = 35,
                Margin = new Thickness(0, 0, 4, 0),
                ToolTip = isInstalled ? "Uninstall" : "Not Installed",
                FontSize = 15,
                Padding = new Thickness(5),
                Background = isInstalled ? DangerBrush : NeutralDisabledBrush
            };
            statusBtn.Content = new TextBlock
            {
                Text = isInstalled ? "🗑️" : "🚫",
                FontFamily = new System.Windows.Media.FontFamily("Segoe UI Emoji"),
                TextAlignment = TextAlignment.Center
            };

            if (isInstalled)
            {
                statusBtn.Click += (s, e) => UninstallCursorScheme(scheme);
            }
            else
            {
                statusBtn.IsEnabled = false;
            }

            buttonPanel.Children.Add(statusBtn);

            // Preview button - opens the full-scheme preview showing every mapped cursor
            // (animated .ani roles via AnimatedCursorControl, static .cur roles as images)
            var previewBtn = new Button
            {
                Style = FindResource("ModernButton") as Style,
                Width = 52,
                Height = 35,
                Margin = new Thickness(4, 0, 4, 0),
                ToolTip = "Preview all cursors in this scheme",
                FontSize = 15,
                Padding = new Thickness(5)
            };
            previewBtn.Content = new TextBlock
            {
                Text = "👁️",
                FontFamily = new System.Windows.Media.FontFamily("Segoe UI Emoji"),
                TextAlignment = TextAlignment.Center
            };
            previewBtn.Click += (s, e) => AnimatedTransitionToPreview(scheme, border);
            buttonPanel.Children.Add(previewBtn);

            var applyBtn = new Button
            {
                Style = FindResource("ModernButton") as Style,
                Width = 52,
                Height = 35,
                Margin = new Thickness(4, 0, 0, 0),
                ToolTip = "Install",
                FontSize = 15,
                Padding = new Thickness(5),
                Background = AccentGradientBrush,
                BorderBrush = Brushes.Transparent
            };
            applyBtn.Content = new TextBlock
            {
                Text = "✨",
                FontFamily = new System.Windows.Media.FontFamily("Segoe UI Emoji"),
                TextAlignment = TextAlignment.Center
            };
            applyBtn.Click += (s, e) => ApplyCursorScheme(scheme);
            buttonPanel.Children.Add(applyBtn);

            Grid.SetRow(buttonPanel, 2);
            grid.Children.Add(buttonPanel);

            // Select checkbox on its own row below
            var selectCheckBox = new CheckBox
            {
                Content = "Select",
                Foreground = TextPrimaryBrush,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(0, 5, 0, 5),
                FontSize = 12
            };
            selectCheckBox.Checked += (s, e) =>
            {
                if (!_selectedSchemes.Contains(scheme))
                    _selectedSchemes.Add(scheme);
                UpdateInstallButtonState();
            };
            selectCheckBox.Unchecked += (s, e) =>
            {
                _selectedSchemes.Remove(scheme);
                UpdateInstallButtonState();
            };

            Grid.SetRow(selectCheckBox, 3);
            grid.Children.Add(selectCheckBox);

            border.Child = grid;

            // Add both border and hover border to container
            containerGrid.Children.Add(border);
            containerGrid.Children.Add(hoverBorder);

            // Add hover effect to show/hide gradient border
            containerGrid.MouseEnter += (s, e) =>
            {
                hoverBorder.BorderThickness = new Thickness(3);
            };

            containerGrid.MouseLeave += (s, e) =>
            {
                hoverBorder.BorderThickness = new Thickness(0);
            };

            // Click animation and navigation to preview
            containerGrid.MouseLeftButtonDown += (s, e) =>
            {
                AnimatedTransitionToPreview(scheme, border);
            };

            return containerGrid;
        }

        private void AnimatedTransitionToPreview(CursorScheme scheme, Border cardBorder)
        {
            // Get the position of the card relative to the window
            var cardPosition = cardBorder.TransformToAncestor(this).Transform(new Point(0, 0));

            // Create overlay with full dark background
            var overlay = new Grid
            {
                Background = new SolidColorBrush(Colors.Black) { Opacity = 0 }
            };

            // Capture current page as background
            var backgroundImage = new System.Windows.Controls.Image
            {
                Stretch = Stretch.Fill,
                Opacity = 1
            };

            var renderBitmap = new System.Windows.Media.Imaging.RenderTargetBitmap(
                (int)MainViewPanel.ActualWidth,
                (int)MainViewPanel.ActualHeight,
                96, 96, PixelFormats.Pbgra32);
            renderBitmap.Render(MainViewPanel);
            backgroundImage.Source = renderBitmap;

            // Extract only the image preview from the card (first child in grid)
            var cardGrid = cardBorder.Child as Grid;
            UIElement? imagePreview = null;
            if (cardGrid != null && cardGrid.Children.Count > 0)
            {
                imagePreview = cardGrid.Children[0]; // First element is the image preview
            }

            // Create animated card with only the image
            var animatedCard = new Border
            {
                Width = 200,
                Height = 200,
                Background = CardBrush,
                CornerRadius = new CornerRadius(12),
                Padding = new Thickness(10)
            };

            if (imagePreview != null)
            {
                var visualBrush = new VisualBrush(imagePreview)
                {
                    Stretch = Stretch.Uniform
                };
                animatedCard.Child = new Border
                {
                    Background = visualBrush
                };
            }

            // Add intense divine glow effect
            animatedCard.Effect = new System.Windows.Media.Effects.DropShadowEffect
            {
                Color = Color.FromRgb(136, 221, 255),
                BlurRadius = 0,
                ShadowDepth = 0,
                Opacity = 1
            };

            // Position elements
            var canvas = new Canvas();
            Canvas.SetLeft(animatedCard, cardPosition.X);
            Canvas.SetTop(animatedCard, cardPosition.Y + 10); // Offset to align with image part
            animatedCard.RenderTransformOrigin = new Point(0.5, 0.5);

            overlay.Children.Add(backgroundImage);
            canvas.Children.Add(animatedCard);
            overlay.Children.Add(canvas);

            // Hide preview panel initially
            PreviewPanel.Opacity = 0;

            MainGrid.Children.Add(overlay);

            // Animate card to center and scale up
            var centerX = (this.ActualWidth - 200) / 2;
            var centerY = (this.ActualHeight - 200) / 2;

            var transformGroup = new TransformGroup();
            var scaleTransform = new ScaleTransform(1, 1);
            var translateTransform = new TranslateTransform(0, 0);
            transformGroup.Children.Add(scaleTransform);
            transformGroup.Children.Add(translateTransform);
            animatedCard.RenderTransform = transformGroup;

            // Phase 1: Move to center and grow with glow (500ms)
            var moveX = new System.Windows.Media.Animation.DoubleAnimation(
                0, centerX - cardPosition.X, TimeSpan.FromMilliseconds(500))
            {
                EasingFunction = new System.Windows.Media.Animation.CubicEase { EasingMode = System.Windows.Media.Animation.EasingMode.EaseInOut }
            };

            var moveY = new System.Windows.Media.Animation.DoubleAnimation(
                0, centerY - cardPosition.Y - 10, TimeSpan.FromMilliseconds(500))
            {
                EasingFunction = new System.Windows.Media.Animation.CubicEase { EasingMode = System.Windows.Media.Animation.EasingMode.EaseInOut }
            };

            var scale = new System.Windows.Media.Animation.DoubleAnimation(
                1, 2.0, TimeSpan.FromMilliseconds(500))
            {
                EasingFunction = new System.Windows.Media.Animation.CubicEase { EasingMode = System.Windows.Media.Animation.EasingMode.EaseInOut }
            };

            var fadeBackgroundDark = new System.Windows.Media.Animation.DoubleAnimation(
                0, 1.0, TimeSpan.FromMilliseconds(500)); // Full dark

            var bgFade = new System.Windows.Media.Animation.DoubleAnimation(
                1, 0, TimeSpan.FromMilliseconds(500)); // Background completely fades

            var glowAnimation = new System.Windows.Media.Animation.DoubleAnimation(
                0, 100, TimeSpan.FromMilliseconds(500));

            scale.Completed += (s, e) =>
            {
                // Phase 2: Hold for 0.3 seconds at peak
                var timer = new System.Windows.Threading.DispatcherTimer
                {
                    Interval = TimeSpan.FromMilliseconds(300)
                };
                timer.Tick += (sender, args) =>
                {
                    timer.Stop();

                    // Phase 3: Show preview panel
                    ShowDetailedPreview(scheme);

                    // Fade in preview
                    var fadeIn = new System.Windows.Media.Animation.DoubleAnimation(
                        0, 1, TimeSpan.FromMilliseconds(300));
                    fadeIn.Completed += (sender2, args2) =>
                    {
                        MainGrid.Children.Remove(overlay);
                    };
                    PreviewPanel.BeginAnimation(OpacityProperty, fadeIn);
                };
                timer.Start();
            };

            translateTransform.BeginAnimation(TranslateTransform.XProperty, moveX);
            translateTransform.BeginAnimation(TranslateTransform.YProperty, moveY);
            scaleTransform.BeginAnimation(ScaleTransform.ScaleXProperty, scale);
            scaleTransform.BeginAnimation(ScaleTransform.ScaleYProperty, scale);
            overlay.Background.BeginAnimation(SolidColorBrush.OpacityProperty, fadeBackgroundDark);
            backgroundImage.BeginAnimation(OpacityProperty, bgFade);
            ((System.Windows.Media.Effects.DropShadowEffect)animatedCard.Effect).BeginAnimation(
                System.Windows.Media.Effects.DropShadowEffect.BlurRadiusProperty, glowAnimation);
        }

        private Border CreatePlaceholderPreview()
        {
            return new Border
            {
                Background = AccentGradientBrush,
                CornerRadius = new CornerRadius(5),
                Child = new TextBlock
                {
                    Text = "🖱️",
                    FontSize = 48,
                    FontFamily = new System.Windows.Media.FontFamily("Segoe UI Emoji"),
                    HorizontalAlignment = HorizontalAlignment.Center,
                    VerticalAlignment = VerticalAlignment.Center,
                    Foreground = Brushes.White
                }
            };
        }

        #endregion

        #region Navigation

        private void ShowMainView()
        {
            MainViewPanel.Visibility = Visibility.Visible;
            PreviewPanel.Visibility = Visibility.Collapsed;
            TutorialPanel.Visibility = Visibility.Collapsed;

            TutorialMediaPlayer.Stop();
            TutorialMediaPlayer.Source = null;
        }

        private void ShowDetailedPreview(CursorScheme scheme)
        {
            // Clean up any existing preview animations before showing new preview
            foreach (var child in PreviewCursorsPanel.Children)
            {
                if (child is DependencyObject depObj)
                {
                    StopAnimatedCursorsInElement(depObj);
                }
            }
            PreviewCursorsPanel.Children.Clear();

            _currentPreviewScheme = scheme;
            PreviewTitleText.Text = scheme.Name;
            PreviewPathText.Text = scheme.RelativePath;

            var cursorInfos = _cursorInfoLoader.LoadAllCursorImages(scheme);
            _currentCursorInfos = cursorInfos;

            if (cursorInfos.Count == 0)
            {
                var noDataText = new TextBlock
                {
                    Text = "No cursor data available for preview",
                    Foreground = Brushes.White,
                    FontSize = 14,
                    Margin = new Thickness(20)
                };
                PreviewCursorsPanel.Children.Add(noDataText);
            }
            else
            {
                RefreshCursorPreviewCards(cursorInfos);
            }

            MainViewPanel.Visibility = Visibility.Collapsed;
            PreviewPanel.Visibility = Visibility.Visible;
            TutorialPanel.Visibility = Visibility.Collapsed;
        }

        private void RefreshCursorPreviewCards(List<CursorInfo> cursorInfos)
        {
            PreviewCursorsPanel.Children.Clear();

            // Calculate optimal card size based on window dimensions
            var scrollViewer = (ScrollViewer)PreviewCursorsPanel.Parent;
            double availableWidth = scrollViewer.ActualWidth > 0 ? scrollViewer.ActualWidth - 60 : this.ActualWidth - 120;
            double availableHeight = scrollViewer.ActualHeight > 0 ? scrollViewer.ActualHeight - 60 : this.ActualHeight - 250;

            // Calculate how many cards per row and how many rows needed
            int minCardsPerRow = 4;
            int maxCardsPerRow = 6;
            int cardsPerRow = Math.Min(maxCardsPerRow, Math.Max(minCardsPerRow, cursorInfos.Count));
            int rows = (int)Math.Ceiling((double)cursorInfos.Count / cardsPerRow);

            // Calculate card dimensions to fit all cards without scrolling
            double cardMargin = 10;
            double cardWidth = (availableWidth / cardsPerRow) - (cardMargin * 2);
            cardWidth = Math.Max(100, Math.Min(cardWidth, 220)); // Clamp between 100-220px

            // Calculate height based on available vertical space and number of rows
            double cardHeight = (availableHeight / rows) - (cardMargin * 2);
            cardHeight = Math.Max(120, Math.Min(cardHeight, cardWidth * 1.3)); // Max 1.3 ratio, min 120px

            foreach (var cursorInfo in cursorInfos)
            {
                var cursorCard = CreateCursorPreviewCard(cursorInfo, cardWidth, cardHeight);
                PreviewCursorsPanel.Children.Add(cursorCard);
            }
        }

        private Border CreateCursorPreviewCard(CursorInfo cursorInfo, double width = 180, double height = 150)
        {
            var border = new Border
            {
                Background = CardBrush,
                BorderBrush = CardBorderBrush,
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(12),
                Padding = new Thickness(8),
                Margin = new Thickness(10),
                Width = width,
                Height = height
            };

            var stack = new StackPanel
            {
                VerticalAlignment = VerticalAlignment.Top,
                HorizontalAlignment = HorizontalAlignment.Center
            };

            // Calculate sizes based on card height (40% for image, rest for text)
            double imageSize = Math.Max(32, Math.Min(height * 0.40, width * 0.7));
            double fontSize = Math.Max(9, height * 0.10);

            // Try to load actual cursor image with transparent background
            if (_currentPreviewScheme != null)
            {
                var cursorPath = System.IO.Path.Combine(_currentPreviewScheme.Path, cursorInfo.FileName);
                var ext = System.IO.Path.GetExtension(cursorPath).ToLowerInvariant();

                // For .ani files, show animated cursor
                if (ext == ".ani")
                {
                    var animatedCursor = new AnimatedCursorControl
                    {
                        CursorPath = cursorPath,
                        MaxWidth = imageSize,
                        MaxHeight = imageSize,
                        Stretch = Stretch.Uniform,
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment = VerticalAlignment.Center
                    };

                    var imgBorder = new Border
                    {
                        Background = Brushes.Transparent,
                        MaxWidth = imageSize,
                        MaxHeight = imageSize,
                        Margin = new Thickness(0, 5, 0, 8),
                        HorizontalAlignment = HorizontalAlignment.Center,
                        Child = animatedCursor
                    };
                    stack.Children.Add(imgBorder);
                }
                else
                {
                    var cursorImage = _imageExtractor.ExtractCursorPreview(cursorPath);

                    if (cursorImage != null)
                    {
                        // Transparent background for cursor preview
                        var img = new System.Windows.Controls.Image
                        {
                            Source = cursorImage,
                            Stretch = Stretch.Uniform,
                            MaxWidth = imageSize,
                            MaxHeight = imageSize,
                            HorizontalAlignment = HorizontalAlignment.Center,
                            VerticalAlignment = VerticalAlignment.Center
                        };

                        var imgBorder = new Border
                        {
                            Background = Brushes.Transparent,
                            MaxWidth = imageSize,
                            MaxHeight = imageSize,
                            Margin = new Thickness(0, 5, 0, 8),
                            HorizontalAlignment = HorizontalAlignment.Center,
                            Child = img
                        };
                        stack.Children.Add(imgBorder);
                    }
                    else
                    {
                        // Fallback emoji
                        var emojiText = new TextBlock
                        {
                            Text = "🖱️",
                            FontSize = imageSize * 0.6,
                            FontFamily = new System.Windows.Media.FontFamily("Segoe UI Emoji"),
                            TextAlignment = TextAlignment.Center,
                            Margin = new Thickness(0, 5, 0, 8),
                            HorizontalAlignment = HorizontalAlignment.Center
                        };
                        stack.Children.Add(emojiText);
                    }
                }
            }

            var nameText = new TextBlock
            {
                Text = cursorInfo.Name,
                FontSize = Math.Max(10, fontSize * 0.90),
                FontWeight = FontWeights.Bold,
                Foreground = TextPrimaryBrush,
                TextAlignment = TextAlignment.Center,
                TextWrapping = TextWrapping.Wrap,
                MaxWidth = width - 20,
                Margin = new Thickness(2, 0, 2, 3),
                HorizontalAlignment = HorizontalAlignment.Center
            };
            stack.Children.Add(nameText);

            var fileText = new TextBlock
            {
                Text = cursorInfo.FileName,
                FontSize = Math.Max(8, fontSize * 0.75),
                Foreground = TextSecondaryBrush,
                TextAlignment = TextAlignment.Center,
                Margin = new Thickness(2, 0, 2, 2),
                TextWrapping = TextWrapping.Wrap,
                MaxWidth = width - 20,
                HorizontalAlignment = HorizontalAlignment.Center
            };
            stack.Children.Add(fileText);

            border.Child = stack;
            return border;
        }

        private void ShowTutorialVideo()
        {
            string? videoPath = _tutorialManager.GetFirstTutorialVideo();
            if (videoPath == null)
            {
                MessageBox.Show("No video file found!", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

            try
            {
                TutorialMediaPlayer.Source = new Uri(videoPath, UriKind.Absolute);
                TutorialMediaPlayer.Position = TimeSpan.Zero;
                TutorialMediaPlayer.Play();
                _tutorialPlaying = true;

                // Switch to tutorial panel
                MainViewPanel.Visibility = Visibility.Collapsed;
                PreviewPanel.Visibility = Visibility.Collapsed;
                TutorialPanel.Visibility = Visibility.Visible;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error loading video: {ex.Message}\n\nPath: {videoPath}", "Tutorial Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void TutorialMediaPlayer_MediaEnded(object sender, RoutedEventArgs e)
        {
            TutorialMediaPlayer.Stop();
            _tutorialPlaying = false;
        }

        private void TutorialMediaPlayer_MediaFailed(object sender, ExceptionRoutedEventArgs e)
        {
            MessageBox.Show($"Error playing video: {e.ErrorException.Message}", "Tutorial Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }

        private void TutorialMediaPlayer_MouseLeftButtonDown(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (TutorialMediaPlayer.CanPause)
            {
                if (_tutorialPlaying)
                    TutorialMediaPlayer.Pause();
                else
                    TutorialMediaPlayer.Play();

                _tutorialPlaying = !_tutorialPlaying;
            }
        }

        #endregion

        #region Actions

        private void ApplyCursorScheme(CursorScheme scheme)
        {
            try
            {
                bool success = _elevatedInstaller.InstallCursorScheme(scheme, out string errorMessage);

                if (success)
                {
                    var result = MessageBox.Show(
                        $"Cursor '{scheme.Name}' applied successfully!\n\nDo you want to open Windows cursor settings?",
                        "Success",
                        MessageBoxButton.YesNo,
                        MessageBoxImage.Information);

                    // Refresh the display to update button states
                    DisplaySchemes(_currentCategory);

                    if (result == MessageBoxResult.Yes)
                    {
                        WindowsCursorTool.OpenCursorSettings();
                    }
                }
                else
                {
                    MessageBox.Show($"Failed to apply cursor scheme '{scheme.Name}'\n\n{errorMessage}", "Error",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error applying cursor: {ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void UninstallCursorScheme(CursorScheme scheme)
        {
            try
            {
                var confirmResult = MessageBox.Show(
                    $"Are you sure you want to uninstall '{scheme.Name}'?\n\nThis will remove the cursor files from your system.",
                    "Confirm Uninstall",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);

                if (confirmResult != MessageBoxResult.Yes)
                    return;

                // Check if this is the active cursor
                string safeSchemeName = PathHelper.SanitizeSchemeName(scheme.Name);
                string? activeScheme = _elevatedInstaller.GetActiveScheme();

                string? replacementScheme = null;

                if (!string.IsNullOrEmpty(activeScheme) &&
                    activeScheme.Equals(safeSchemeName, StringComparison.OrdinalIgnoreCase))
                {
                    // Get list of other installed cursors
                    var installedSchemes = _elevatedInstaller.GetInstalledSchemes();
                    installedSchemes.Remove(safeSchemeName); // Remove the one being uninstalled

                    if (installedSchemes.Count > 0)
                    {
                        // Ask user which cursor to switch to
                        var selectionWindow = new Window
                        {
                            Title = "Select Replacement Cursor",
                            Width = 450,
                            Height = 350,
                            WindowStartupLocation = WindowStartupLocation.CenterOwner,
                            Owner = this,
                            ResizeMode = ResizeMode.NoResize,
                            Background = new SolidColorBrush(Color.FromRgb(0x1B, 0x1F, 0x27))
                        };

                        var mainGrid = new Grid { Margin = new Thickness(20) };
                        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
                        mainGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
                        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

                        // Header panel with text
                        var instructionPanel = new StackPanel();

                        var headerText = new TextBlock
                        {
                            Text = $"'{scheme.Name}' is currently active.",
                            FontSize = 16,
                            FontWeight = FontWeights.Bold,
                            Foreground = TextPrimaryBrush,
                            Margin = new Thickness(0, 0, 0, 10)
                        };
                        instructionPanel.Children.Add(headerText);

                        var instructionText = new TextBlock
                        {
                            Text = "Select a cursor to switch to before uninstalling:",
                            FontSize = 12,
                            Foreground = TextSecondaryBrush,
                            Margin = new Thickness(0, 0, 0, 15)
                        };
                        instructionPanel.Children.Add(instructionText);

                        Grid.SetRow(instructionPanel, 0);
                        mainGrid.Children.Add(instructionPanel);

                        // ListBox with custom styling
                        var listBox = new System.Windows.Controls.ListBox
                        {
                            Margin = new Thickness(0, 10, 0, 15),
                            Background = CardBrush,
                            Foreground = TextPrimaryBrush,
                            BorderBrush = new SolidColorBrush(Color.FromRgb(0x3A, 0x40, 0x4C)),
                            BorderThickness = new Thickness(1),
                            FontSize = 13,
                            Padding = new Thickness(5)
                        };

                        // Add "Windows Default" option
                        listBox.Items.Add("Windows Default");

                        // Add other installed cursors
                        foreach (var schemeName in installedSchemes)
                        {
                            listBox.Items.Add(schemeName);
                        }

                        listBox.SelectedIndex = 0;

                        // Double-click to select
                        listBox.MouseDoubleClick += (s, e) =>
                        {
                            if (listBox.SelectedItem != null)
                            {
                                selectionWindow.DialogResult = true;
                            }
                        };

                        Grid.SetRow(listBox, 1);
                        mainGrid.Children.Add(listBox);

                        // Button panel
                        var buttonPanel = new StackPanel
                        {
                            Orientation = Orientation.Horizontal,
                            HorizontalAlignment = HorizontalAlignment.Right,
                            Margin = new Thickness(0, 10, 0, 0)
                        };

                        var cancelButton = new Button
                        {
                            Content = "Cancel",
                            Width = 100,
                            Height = 35,
                            Margin = new Thickness(0, 0, 10, 0),
                            Background = new SolidColorBrush(Color.FromRgb(0x28, 0x2D, 0x36)),
                            Foreground = TextPrimaryBrush,
                            BorderBrush = new SolidColorBrush(Color.FromRgb(0x3A, 0x40, 0x4C)),
                            BorderThickness = new Thickness(1),
                            FontSize = 13,
                            Cursor = System.Windows.Input.Cursors.Hand
                        };
                        cancelButton.Click += (s, e) => selectionWindow.Close();
                        buttonPanel.Children.Add(cancelButton);

                        var okButton = new Button
                        {
                            Content = "OK",
                            Width = 100,
                            Height = 35,
                            Background = AccentGradientBrush,
                            Foreground = Brushes.White,
                            BorderBrush = Brushes.Transparent,
                            BorderThickness = new Thickness(1),
                            FontSize = 13,
                            FontWeight = FontWeights.Bold,
                            Cursor = System.Windows.Input.Cursors.Hand
                        };
                        okButton.Click += (s, e) => selectionWindow.DialogResult = true;
                        buttonPanel.Children.Add(okButton);

                        Grid.SetRow(buttonPanel, 2);
                        mainGrid.Children.Add(buttonPanel);

                        selectionWindow.Content = mainGrid;

                        if (selectionWindow.ShowDialog() == true && listBox.SelectedItem != null)
                        {
                            // Get the selected cursor
                            string selected = listBox.SelectedItem.ToString()!;
                            replacementScheme = selected == "Windows Default" ? null : selected;
                        }
                        else
                        {
                            return; // User cancelled
                        }
                    }
                    // If no other cursors, will use default automatically (replacementScheme stays null)
                }

                bool success = _elevatedInstaller.UninstallCursorScheme(scheme, replacementScheme, out string errorMessage);

                if (success)
                {
                    MessageBox.Show(
                        $"Cursor '{scheme.Name}' uninstalled successfully!",
                        "Success",
                        MessageBoxButton.OK,
                        MessageBoxImage.Information);

                    // Refresh the display to update button states
                    DisplaySchemes(_currentCategory);
                }
                else
                {
                    MessageBox.Show(
                        $"Failed to uninstall cursor scheme '{scheme.Name}'\n\n{errorMessage}",
                        "Error",
                        MessageBoxButton.OK,
                        MessageBoxImage.Error);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Error uninstalling cursor: {ex.Message}",
                    "Error",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }
        }

        private void UpdateInstallButtonState()
        {
            InstallSelectedButton.IsEnabled = _selectedSchemes.Count > 0;
            InstallSelectedButton.Content = _selectedSchemes.Count > 0
                ? $"⚡ Install Selected ({_selectedSchemes.Count})"
                : "⚡ Install Selected";
        }

        #endregion

        #region Event Handlers

        private void CategorySearchBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            // TODO: Implement category search
        }

        private void SchemeSearchBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            // TODO: Implement scheme search
        }

        private void TutorialButton_Click(object sender, RoutedEventArgs e)
        {
            if (!_tutorialManager.HasTutorialVideos())
            {
                MessageBox.Show(
                    "No tutorial videos found.\n\nPlace video files (.mp4, .webm, .avi, etc.) in the 'tutorial' folder.",
                    "No Tutorials",
                    MessageBoxButton.OK,
                    MessageBoxImage.Information
                );
                return;
            }

            ShowTutorialVideo();
        }

        private void WindowsSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            WindowsCursorTool.OpenCursorSettings();
        }

        private void BackFromPreviewButton_Click(object sender, RoutedEventArgs e)
        {
            // Stop any running animations in preview panel
            foreach (var child in PreviewCursorsPanel.Children)
            {
                if (child is Border border)
                {
                    border.BeginAnimation(OpacityProperty, null);

                    // Stop all animated cursor controls to prevent lag
                    StopAnimatedCursorsInElement(border);
                }
            }

            // Clear preview panel to release resources
            PreviewCursorsPanel.Children.Clear();

            // Force garbage collection to free memory from animations
            GC.Collect();
            GC.WaitForPendingFinalizers();

            ShowMainView();
        }

        private void StopAnimatedCursorsInElement(DependencyObject element)
        {
            if (element is AnimatedCursorControl animatedCursor)
            {
                // Fully cleanup the animated cursor to release all resources
                animatedCursor.Cleanup();
                return;
            }

            // Recursively check children
            int childrenCount = VisualTreeHelper.GetChildrenCount(element);
            for (int i = 0; i < childrenCount; i++)
            {
                var child = VisualTreeHelper.GetChild(element, i);
                StopAnimatedCursorsInElement(child);
            }
        }

        private void BackFromTutorialButton_Click(object sender, RoutedEventArgs e)
        {
            ShowMainView();
        }

        private void ApplyPreviewButton_Click(object sender, RoutedEventArgs e)
        {
            if (_currentPreviewScheme != null)
            {
                ApplyCursorScheme(_currentPreviewScheme);
            }
        }

        private void InstallSelectedButton_Click(object sender, RoutedEventArgs e)
        {
            if (_selectedSchemes.Count == 0) return;

            string cursorNames = string.Join("\n", _selectedSchemes.Select(s => $"• {s.Name}"));
            var result = MessageBox.Show(
                $"Install {_selectedSchemes.Count} cursor schemes?\n\n{cursorNames}\n\nThis may take a few moments.",
                "Confirm Installation",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question
            );

            if (result != MessageBoxResult.Yes) return;

            // Create progress window
            var progressWindow = new Window
            {
                Title = "Installing Cursors",
                Width = 350,
                Height = 150,
                WindowStartupLocation = WindowStartupLocation.CenterOwner,
                Owner = this,
                ResizeMode = ResizeMode.NoResize,
                WindowStyle = WindowStyle.ToolWindow,
                Background = new SolidColorBrush(Color.FromRgb(0x1B, 0x1F, 0x27))
            };

            var progressPanel = new StackPanel
            {
                Margin = new Thickness(20)
            };

            var progressLabel = new TextBlock
            {
                Text = $"Installing cursors... (0/{_selectedSchemes.Count})",
                FontSize = 14,
                Foreground = TextPrimaryBrush,
                Margin = new Thickness(0, 0, 0, 10),
                TextAlignment = TextAlignment.Center
            };

            var progressBar = new System.Windows.Controls.Primitives.StatusBar
            {
                Height = 30
            };

            var progressBarItem = new System.Windows.Controls.Primitives.StatusBarItem();
            var progressBarControl = new System.Windows.Controls.ProgressBar
            {
                Minimum = 0,
                Maximum = _selectedSchemes.Count,
                Height = 20
            };
            progressBarItem.Content = progressBarControl;
            progressBar.Items.Add(progressBarItem);

            var currentLabel = new TextBlock
            {
                Text = "",
                FontSize = 12,
                Foreground = TextSecondaryBrush,
                Margin = new Thickness(0, 10, 0, 0),
                TextAlignment = TextAlignment.Center,
                TextWrapping = TextWrapping.Wrap
            };

            progressPanel.Children.Add(progressLabel);
            progressPanel.Children.Add(progressBar);
            progressPanel.Children.Add(currentLabel);
            progressWindow.Content = progressPanel;

            // Show progress window
            progressWindow.Show();

            // Install cursors with progress callback
            var results = _elevatedInstaller.InstallMultipleCursorSchemes(
                _selectedSchemes,
                (current, total, schemeName) =>
                {
                    Dispatcher.Invoke(() =>
                    {
                        progressLabel.Text = $"Installing cursors... ({current}/{total})";
                        progressBarControl.Value = current;
                        currentLabel.Text = schemeName;
                    });
                }
            );

            // Close progress window
            progressWindow.Close();

            // Show results
            int successCount = results.Count(r => r.Success);
            int failedCount = results.Count - successCount;

            MessageBoxResult openSettingsResult;

            if (failedCount == 0)
            {
                openSettingsResult = MessageBox.Show(
                    $"✅ All {successCount} cursor schemes installed successfully!\n\n" +
                    "Do you want to open Windows cursor settings?",
                    "Installation Complete",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Information
                );
            }
            else
            {
                var failedItems = results.Where(r => !r.Success).ToList();
                var failedDetails = string.Join("\n\n", failedItems.Select(r =>
                    $"• {r.SchemeName}\n  Error: {r.ErrorMessage}"));

                openSettingsResult = MessageBox.Show(
                    $"✅ {successCount} cursor schemes installed successfully.\n\n" +
                    $"❌ {failedCount} failed:\n\n{failedDetails}\n\n" +
                    "Do you want to open Windows cursor settings?",
                    "Installation Partial",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning
                );
            }

            // Clear selection and refresh UI
            _selectedSchemes.Clear();
            DisplaySchemes(_currentCategory);

            // Open Windows cursor settings if user chose Yes
            if (openSettingsResult == MessageBoxResult.Yes)
            {
                WindowsCursorTool.OpenCursorSettings();
            }
        }

        #endregion
    }
}
