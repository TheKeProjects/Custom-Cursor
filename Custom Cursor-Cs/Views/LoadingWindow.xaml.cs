using System;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media.Animation;
using System.Windows.Threading;

namespace CustomCursor.Views
{
    public partial class LoadingWindow : Window
    {
        private readonly string[] _loadingIcons = { "⌛", "⏳", "🔄", "⚙️", "💫" };
        private int _currentIconIndex = 0;
        private DispatcherTimer? _iconTimer;
        private DispatcherTimer? _progressTimer;
        private double _currentProgress = 0;
        private double _targetProgress = 0;
        private bool _isCompleting = false;

        public LoadingWindow()
        {
            InitializeComponent();
            Loaded += LoadingWindow_Loaded;
        }

        private void LoadingWindow_Loaded(object sender, RoutedEventArgs e)
        {
            StartAnimations();
        }

        private void StartAnimations()
        {
            // Start icon changing animation (sequential)
            _iconTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(1000)
            };
            _iconTimer.Tick += IconTimer_Tick;
            _iconTimer.Start();

            // Start pulsing animation for icon
            var pulseAnimation = new DoubleAnimationUsingKeyFrames
            {
                RepeatBehavior = RepeatBehavior.Forever,
                Duration = TimeSpan.FromMilliseconds(1200)
            };
            pulseAnimation.KeyFrames.Add(new EasingDoubleKeyFrame(1.0, KeyTime.FromPercent(0)));
            pulseAnimation.KeyFrames.Add(new EasingDoubleKeyFrame(1.2, KeyTime.FromPercent(0.5)));
            pulseAnimation.KeyFrames.Add(new EasingDoubleKeyFrame(1.0, KeyTime.FromPercent(1.0)));

            IconScaleTransform.BeginAnimation(System.Windows.Media.ScaleTransform.ScaleXProperty, pulseAnimation);
            IconScaleTransform.BeginAnimation(System.Windows.Media.ScaleTransform.ScaleYProperty, pulseAnimation);

            // Gentle rotation animation for icon
            var rotateAnimation = new DoubleAnimation
            {
                From = 0,
                To = 360,
                Duration = TimeSpan.FromSeconds(4),
                RepeatBehavior = RepeatBehavior.Forever,
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut }
            };
            IconRotateTransform.BeginAnimation(System.Windows.Media.RotateTransform.AngleProperty, rotateAnimation);

            // Start glow pulsing animation
            var glowAnimation = new DoubleAnimationUsingKeyFrames
            {
                RepeatBehavior = RepeatBehavior.Forever,
                Duration = TimeSpan.FromMilliseconds(1500)
            };
            glowAnimation.KeyFrames.Add(new EasingDoubleKeyFrame(25, KeyTime.FromPercent(0)));
            glowAnimation.KeyFrames.Add(new EasingDoubleKeyFrame(45, KeyTime.FromPercent(0.5)));
            glowAnimation.KeyFrames.Add(new EasingDoubleKeyFrame(25, KeyTime.FromPercent(1.0)));

            IconGlowEffect.BeginAnimation(System.Windows.Media.Effects.DropShadowEffect.BlurRadiusProperty, glowAnimation);

            // Start smooth progress bar animation
            _progressTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(16) // ~60 FPS
            };
            _progressTimer.Tick += ProgressTimer_Tick;
            _progressTimer.Start();

            // Simulate loading progress
            SimulateProgress();
        }

        private void IconTimer_Tick(object? sender, EventArgs e)
        {
            if (!_isCompleting)
            {
                _currentIconIndex = (_currentIconIndex + 1) % _loadingIcons.Length;
                AnimatedIcon.Text = _loadingIcons[_currentIconIndex];
            }
        }

        private void ProgressTimer_Tick(object? sender, EventArgs e)
        {
            // Smooth progress bar animation
            if (_currentProgress < _targetProgress)
            {
                _currentProgress += (_targetProgress - _currentProgress) * 0.1;
                if (_targetProgress - _currentProgress < 0.5)
                    _currentProgress = _targetProgress;

                UpdateProgressBar(_currentProgress);
            }
        }

        private async void SimulateProgress()
        {
            await SetProgress(20, "Loading resources...");
            await Task.Delay(300);

            await SetProgress(40, "Initializing path system...");
            await Task.Delay(300);

            await SetProgress(60, "Extracting resources...");
            await Task.Delay(300);

            await SetProgress(80, "Loading cursor sets...");
            await Task.Delay(300);

            await SetProgress(100, "Ready!");
            await Task.Delay(400);
        }

        private Task SetProgress(double progress, string status)
        {
            _targetProgress = progress;
            StatusText.Text = status;
            return Task.CompletedTask;
        }

        private void UpdateProgressBar(double progress)
        {
            double maxWidth = 620; // Container width minus margins (700 - 80)
            ProgressBarFill.Width = (progress / 100.0) * maxWidth;
        }

        public void UpdateStatus(string status, double progress)
        {
            Dispatcher.Invoke(() =>
            {
                StatusText.Text = status;
                _targetProgress = progress;
            });
        }

        public async Task CompleteLoading()
        {
            _isCompleting = true;
            await SetProgress(100, "Complete!");
            await Task.Delay(200);

            // Stop the cycling and start final animation sequence
            _iconTimer?.Stop();

            // Stop all current animations
            IconScaleTransform.BeginAnimation(System.Windows.Media.ScaleTransform.ScaleXProperty, null);
            IconScaleTransform.BeginAnimation(System.Windows.Media.ScaleTransform.ScaleYProperty, null);
            IconRotateTransform.BeginAnimation(System.Windows.Media.RotateTransform.AngleProperty, null);
            IconGlowEffect.BeginAnimation(System.Windows.Media.Effects.DropShadowEffect.BlurRadiusProperty, null);

            // Final icon sequence
            AnimatedIcon.Text = "✨";
            await Task.Delay(300);
            AnimatedIcon.Text = "⚡";
            await Task.Delay(300);
            AnimatedIcon.Text = "🎯";
            await Task.Delay(300);
            AnimatedIcon.Text = "✅";

            // Dramatic final animation: Scale up with intense glow then fade
            var finalScale = new DoubleAnimation
            {
                From = 1,
                To = 1.5,
                Duration = TimeSpan.FromMilliseconds(400),
                EasingFunction = new BackEase { EasingMode = EasingMode.EaseOut, Amplitude = 0.5 }
            };

            var finalGlow = new DoubleAnimation
            {
                From = 25,
                To = 80,
                Duration = TimeSpan.FromMilliseconds(400)
            };

            IconScaleTransform.BeginAnimation(System.Windows.Media.ScaleTransform.ScaleXProperty, finalScale);
            IconScaleTransform.BeginAnimation(System.Windows.Media.ScaleTransform.ScaleYProperty, finalScale);
            IconGlowEffect.BeginAnimation(System.Windows.Media.Effects.DropShadowEffect.BlurRadiusProperty, finalGlow);

            await Task.Delay(500);

            // Fade out animation
            var fadeOut = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(400));
            fadeOut.Completed += (s, e) =>
            {
                _iconTimer?.Stop();
                _progressTimer?.Stop();
                Close();
            };
            BeginAnimation(OpacityProperty, fadeOut);
        }

        protected override void OnClosed(EventArgs e)
        {
            _iconTimer?.Stop();
            _progressTimer?.Stop();
            base.OnClosed(e);
        }
    }
}
