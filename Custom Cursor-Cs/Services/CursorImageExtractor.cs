using System;
using System.IO;
using System.Linq;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Runtime.InteropServices;

namespace CustomCursor.Services
{
    /// <summary>
    /// Extracts preview images from cursor files for WPF
    /// </summary>
    public class CursorImageExtractor
    {
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr LoadImage(IntPtr hInst, string lpszName, uint uType, int cxDesired, int cyDesired, uint fuLoad);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyIcon(IntPtr hIcon);

        private const uint IMAGE_CURSOR = 2;
        private const uint LR_LOADFROMFILE = 0x00000010;

        /// <summary>
        /// Attempts to extract a preview image from a cursor file
        /// </summary>
        public BitmapSource? ExtractCursorPreview(string cursorPath)
        {
            if (!File.Exists(cursorPath))
                return null;

            try
            {
                string ext = Path.GetExtension(cursorPath).ToLowerInvariant();

                if (ext == ".cur")
                {
                    return LoadCursorFile(cursorPath);
                }
                else if (ext == ".ani")
                {
                    // .ani files are more complex - try to extract first frame
                    return LoadAnimatedCursorFirstFrame(cursorPath);
                }
                else if (ext == ".ico")
                {
                    return LoadIconFile(cursorPath);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error extracting cursor preview: {ex.Message}");
            }

            return null;
        }

        private BitmapSource? LoadCursorFile(string cursorPath)
        {
            try
            {
                // Load cursor using Win32 API
                IntPtr hCursor = LoadImage(IntPtr.Zero, cursorPath, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);

                if (hCursor != IntPtr.Zero)
                {
                    try
                    {
                        // Convert to BitmapSource
                        var bitmap = System.Windows.Interop.Imaging.CreateBitmapSourceFromHIcon(
                            hCursor,
                            System.Windows.Int32Rect.Empty,
                            BitmapSizeOptions.FromEmptyOptions());

                        return bitmap;
                    }
                    finally
                    {
                        DestroyIcon(hCursor);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading cursor file: {ex.Message}");
            }

            return null;
        }

        private BitmapSource? LoadAnimatedCursorFirstFrame(string aniPath)
        {
            try
            {
                // Try to load as cursor first
                IntPtr hCursor = LoadImage(IntPtr.Zero, aniPath, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);

                if (hCursor != IntPtr.Zero)
                {
                    try
                    {
                        var bitmap = System.Windows.Interop.Imaging.CreateBitmapSourceFromHIcon(
                            hCursor,
                            System.Windows.Int32Rect.Empty,
                            BitmapSizeOptions.FromEmptyOptions());

                        return bitmap;
                    }
                    finally
                    {
                        DestroyIcon(hCursor);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading animated cursor: {ex.Message}");
            }

            return null;
        }

        private BitmapSource? LoadIconFile(string iconPath)
        {
            try
            {
                var bitmap = new BitmapImage();
                bitmap.BeginInit();
                bitmap.UriSource = new Uri(iconPath, UriKind.Absolute);
                bitmap.CacheOption = BitmapCacheOption.OnLoad;
                bitmap.EndInit();
                bitmap.Freeze();

                return bitmap;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading icon file: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// Gets the preview image path for a cursor scheme (returns path for GIF and .ani animation support)
        /// </summary>
        public string? GetSchemePreviewPath(string schemePath, string? previewImagePath)
        {
            // Try preview.png/gif/ani first
            if (!string.IsNullOrEmpty(previewImagePath) && File.Exists(previewImagePath))
            {
                return previewImagePath;
            }

            // Look for any image or cursor file in the scheme folder (prioritize animated formats)
            try
            {
                // Try GIF first for potential animation
                var gifFiles = Directory.GetFiles(schemePath, "*.gif");
                if (gifFiles.Length > 0)
                    return gifFiles[0];

                // Try .ani files for animated cursor preview
                var aniFiles = Directory.GetFiles(schemePath, "*.ani");
                if (aniFiles.Length > 0)
                    return aniFiles[0];

                // Then try other formats
                var imageFiles = Directory.GetFiles(schemePath, "*.png")
                    .Concat(Directory.GetFiles(schemePath, "*.jpg"))
                    .Concat(Directory.GetFiles(schemePath, "*.jpeg"))
                    .ToArray();

                if (imageFiles.Length > 0)
                    return imageFiles[0];
            }
            catch { }

            return null;
        }

        /// <summary>
        /// Loads an animated or static image from path
        /// </summary>
        public BitmapImage? LoadAnimatedImage(string imagePath)
        {
            try
            {
                var bitmap = new BitmapImage();
                bitmap.BeginInit();
                bitmap.UriSource = new Uri(Path.GetFullPath(imagePath));
                bitmap.CacheOption = BitmapCacheOption.OnLoad;
                bitmap.DecodePixelWidth = 100;
                bitmap.EndInit();
                // Don't freeze animated GIFs!
                if (!imagePath.EndsWith(".gif", StringComparison.OrdinalIgnoreCase))
                {
                    bitmap.Freeze();
                }
                return bitmap;
            }
            catch
            {
                return null;
            }
        }
    }
}
