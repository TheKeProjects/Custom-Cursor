using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace CustomCursor.Controls
{
    /// <summary>
    /// A control that displays animated cursors (.ani files) by cycling through their frames
    /// </summary>
    public class AnimatedCursorControl : Image
    {
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr LoadImage(IntPtr hInst, string lpszName, uint uType, int cxDesired, int cyDesired, uint fuLoad);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyIcon(IntPtr hIcon);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr CreateIconFromResourceEx(byte[] pbIconBits, uint cbIconBits, bool fIcon, uint dwVersion, int cxDesired, int cyDesired, uint uFlags);

        private const uint IMAGE_CURSOR = 2;
        private const uint LR_LOADFROMFILE = 0x00000010;
        private const uint LR_DEFAULTCOLOR = 0x0000;

        // Shared timer for all animations to reduce overhead
        private static DispatcherTimer? _sharedTimer;
        private static List<AnimatedCursorControl> _activeAnimations = new List<AnimatedCursorControl>();
        private static readonly object _lock = new object();

        private List<BitmapSource> _frames = new List<BitmapSource>();
        private int _currentFrame = 0;
        private string? _cursorPath;
        private bool _isAnimating = false;

        public static readonly DependencyProperty CursorPathProperty =
            DependencyProperty.Register(
                nameof(CursorPath),
                typeof(string),
                typeof(AnimatedCursorControl),
                new PropertyMetadata(null, OnCursorPathChanged));

        public string? CursorPath
        {
            get => (string?)GetValue(CursorPathProperty);
            set => SetValue(CursorPathProperty, value);
        }

        public static readonly DependencyProperty FrameRateProperty =
            DependencyProperty.Register(
                nameof(FrameRate),
                typeof(int),
                typeof(AnimatedCursorControl),
                new PropertyMetadata(150)); // Default 150ms per frame for consistent speed and less CPU load

        public int FrameRate
        {
            get => (int)GetValue(FrameRateProperty);
            set => SetValue(FrameRateProperty, value);
        }

        // Property to force using the default frame rate instead of the cursor's native rate
        public static readonly DependencyProperty UseDefaultFrameRateProperty =
            DependencyProperty.Register(
                nameof(UseDefaultFrameRate),
                typeof(bool),
                typeof(AnimatedCursorControl),
                new PropertyMetadata(true)); // Default to true for consistent animation speed

        public bool UseDefaultFrameRate
        {
            get => (bool)GetValue(UseDefaultFrameRateProperty);
            set => SetValue(UseDefaultFrameRateProperty, value);
        }

        private static void OnCursorPathChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is AnimatedCursorControl control)
            {
                control.LoadCursor(e.NewValue as string);
            }
        }

        private void LoadCursor(string? path)
        {
            StopAnimation();
            _frames.Clear();
            _currentFrame = 0;
            _cursorPath = path;

            if (string.IsNullOrEmpty(path) || !File.Exists(path))
            {
                Source = null;
                return;
            }

            string ext = Path.GetExtension(path).ToLowerInvariant();

            if (ext == ".ani")
            {
                LoadAnimatedCursor(path);
            }
            else
            {
                // For static cursors, just load the single frame
                LoadStaticCursor(path);
            }
        }

        private void LoadStaticCursor(string path)
        {
            try
            {
                IntPtr hCursor = LoadImage(IntPtr.Zero, path, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);

                if (hCursor != IntPtr.Zero)
                {
                    try
                    {
                        var bitmap = BitmapFromHCursor(hCursor);
                        if (bitmap != null)
                        {
                            _frames.Add(bitmap);
                            Source = bitmap;
                        }
                    }
                    finally
                    {
                        DestroyIcon(hCursor);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading cursor: {ex.Message}");
            }
        }

        // Converts a loaded HCURSOR into a frozen, size-capped BitmapSource ready for the UI thread
        private static BitmapSource BitmapFromHCursor(IntPtr hCursor)
        {
            var bitmap = System.Windows.Interop.Imaging.CreateBitmapSourceFromHIcon(
                hCursor,
                Int32Rect.Empty,
                BitmapSizeOptions.FromEmptyOptions());

            // Scale down large frames for performance (max 32x32)
            int maxSize = 32;
            if (bitmap.PixelWidth > maxSize || bitmap.PixelHeight > maxSize)
            {
                double scale = Math.Min((double)maxSize / bitmap.PixelWidth, (double)maxSize / bitmap.PixelHeight);
                var scaled = new TransformedBitmap(bitmap, new ScaleTransform(scale, scale));
                scaled.Freeze();
                return scaled;
            }

            bitmap.Freeze();
            return bitmap;
        }

        private void LoadAnimatedCursor(string path)
        {
            try
            {
                // Read .ani file structure
                var aniData = ParseAniFile(path);

                if (aniData.FrameCount > 0 && aniData.FrameData.Count > 0)
                {
                    // Limit frames for performance (skip frames if too many)
                    int maxFrames = 10; // Limit to 10 frames max for performance
                    int frameStep = aniData.FrameData.Count > maxFrames
                        ? (int)Math.Ceiling((double)aniData.FrameData.Count / maxFrames)
                        : 1;

                    // Extract frames with optional skipping for heavy animations
                    for (int i = 0; i < aniData.FrameData.Count; i += frameStep)
                    {
                        var frameData = aniData.FrameData[i];
                        var frame = LoadFrameFromData(frameData);
                        if (frame != null)
                        {
                            _frames.Add(frame);
                        }
                    }

                    if (_frames.Count > 0)
                    {
                        Source = _frames[0];

                        if (_frames.Count > 1)
                        {
                            // Use default frame rate for consistency, or file's rate if UseDefaultFrameRate is false
                            int frameDelay;
                            if (UseDefaultFrameRate)
                            {
                                frameDelay = FrameRate;
                            }
                            else
                            {
                                frameDelay = aniData.DefaultRate > 0 ? aniData.DefaultRate : FrameRate;
                            }
                            StartAnimation(frameDelay);
                        }
                    }
                }
                else
                {
                    // Fallback: just load the first frame using Win32 API
                    LoadStaticCursor(path);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading animated cursor: {ex.Message}");
                // Fallback to static load
                LoadStaticCursor(path);
            }
        }

        private AniFileData ParseAniFile(string path)
        {
            var data = new AniFileData();

            using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read))
            using (var br = new BinaryReader(fs))
            {
                // Read RIFF header
                string riff = new string(br.ReadChars(4));
                if (riff != "RIFF")
                    return data;

                int fileSize = br.ReadInt32();
                string acon = new string(br.ReadChars(4));
                if (acon != "ACON")
                    return data;

                // Parse chunks
                while (fs.Position < fs.Length - 8)
                {
                    string chunkType = new string(br.ReadChars(4));
                    int chunkSize = br.ReadInt32();
                    long chunkEnd = fs.Position + chunkSize;

                    if (chunkType == "anih")
                    {
                        // Animation header
                        int headerSize = br.ReadInt32();
                        data.FrameCount = br.ReadInt32();
                        int numSteps = br.ReadInt32();
                        br.ReadInt32(); // width
                        br.ReadInt32(); // height
                        br.ReadInt32(); // bit count
                        br.ReadInt32(); // num planes
                        data.DefaultRate = br.ReadInt32(); // display rate in 1/60th second units
                        data.DefaultRate = data.DefaultRate * 1000 / 60; // Convert to milliseconds
                        br.ReadInt32(); // flags
                    }
                    else if (chunkType == "LIST")
                    {
                        string listType = new string(br.ReadChars(4));

                        if (listType == "fram")
                        {
                            // Frame list - contains icon chunks
                            while (fs.Position < chunkEnd - 8)
                            {
                                string iconType = new string(br.ReadChars(4));
                                int iconSize = br.ReadInt32();

                                if (iconType == "icon")
                                {
                                    byte[] iconData = br.ReadBytes(iconSize);
                                    data.FrameData.Add(iconData);
                                }
                                else
                                {
                                    // Skip unknown chunk
                                    fs.Seek(iconSize, SeekOrigin.Current);
                                }
                            }
                        }
                    }

                    // Move to next chunk
                    fs.Position = chunkEnd;

                    // Handle padding
                    if (chunkSize % 2 != 0 && fs.Position < fs.Length)
                    {
                        fs.Position++;
                    }
                }
            }

            return data;
        }

        private BitmapSource? LoadFrameFromData(byte[] iconData)
        {
            try
            {
                // Decode straight from the in-memory chunk bytes (each "icon" chunk in an
                // .ani is itself a complete .cur file). This avoids a disk write+read+delete
                // per animation frame, which is what made scheme cards/previews feel laggy
                // (especially with real-time antivirus scanning every temp file touch).
                IntPtr hCursor = CreateIconFromResourceEx(iconData, (uint)iconData.Length, false, 0x00030000, 0, 0, LR_DEFAULTCOLOR);

                if (hCursor != IntPtr.Zero)
                {
                    try
                    {
                        return BitmapFromHCursor(hCursor);
                    }
                    finally
                    {
                        DestroyIcon(hCursor);
                    }
                }

                // Fallback for the rare frame the in-memory decoder rejects
                return LoadFrameFromDataViaTempFile(iconData);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading frame: {ex.Message}");
                return null;
            }
        }

        private BitmapSource? LoadFrameFromDataViaTempFile(byte[] iconData)
        {
            string tempFile = Path.Combine(Path.GetTempPath(), $"cursor_frame_{Guid.NewGuid()}.cur");

            try
            {
                File.WriteAllBytes(tempFile, iconData);

                IntPtr hCursor = LoadImage(IntPtr.Zero, tempFile, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);

                if (hCursor != IntPtr.Zero)
                {
                    try
                    {
                        return BitmapFromHCursor(hCursor);
                    }
                    finally
                    {
                        DestroyIcon(hCursor);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading frame via temp file: {ex.Message}");
            }
            finally
            {
                try { File.Delete(tempFile); } catch { }
            }

            return null;
        }

        private void StartAnimation(int frameDelayMs)
        {
            lock (_lock)
            {
                if (_isAnimating || _frames.Count <= 1)
                    return;

                _isAnimating = true;
                _activeAnimations.Add(this);

                // Initialize shared timer if needed
                if (_sharedTimer == null)
                {
                    _sharedTimer = new DispatcherTimer(DispatcherPriority.Background)
                    {
                        Interval = TimeSpan.FromMilliseconds(150) // Use consistent interval
                    };
                    _sharedTimer.Tick += SharedTimerTick;
                    _sharedTimer.Start();
                }
            }
        }

        public void StopAnimation()
        {
            lock (_lock)
            {
                if (!_isAnimating)
                    return;

                _isAnimating = false;
                _activeAnimations.Remove(this);

                // Stop shared timer if no more animations
                if (_activeAnimations.Count == 0 && _sharedTimer != null)
                {
                    _sharedTimer.Stop();
                    _sharedTimer.Tick -= SharedTimerTick;
                    _sharedTimer = null;
                }
            }
        }

        public void Cleanup()
        {
            StopAnimation();
            _frames.Clear();
            _currentFrame = 0;
            Source = null;
            _cursorPath = null;
        }

        private static void SharedTimerTick(object? sender, EventArgs e)
        {
            lock (_lock)
            {
                // Update all active animations
                foreach (var control in _activeAnimations.ToList())
                {
                    if (control._frames.Count > 1)
                    {
                        control._currentFrame = (control._currentFrame + 1) % control._frames.Count;
                        control.Source = control._frames[control._currentFrame];
                    }
                }
            }
        }

        protected override void OnRenderSizeChanged(SizeChangedInfo sizeInfo)
        {
            base.OnRenderSizeChanged(sizeInfo);
        }

        private class AniFileData
        {
            public int FrameCount { get; set; }
            public int DefaultRate { get; set; } = 60; // Default 60ms
            public List<byte[]> FrameData { get; set; } = new List<byte[]>();
        }
    }
}
