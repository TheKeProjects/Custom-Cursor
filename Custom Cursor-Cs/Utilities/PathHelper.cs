using System;
using System.IO;
using System.Reflection;
using System.Diagnostics;

namespace CustomCursor.Utilities
{
    public static class PathHelper
    {
        private static string? _workingDirectory = null;
        private static bool _isInitialized = false;

        /// <summary>
        /// Determines if the application is running as a single-file executable
        /// </summary>
        public static bool IsRunningAsSingleFileExe()
        {
            // Check if running from a temp extraction directory (single-file indicator)
            string? processPath = Environment.ProcessPath;
            if (string.IsNullOrEmpty(processPath))
                return false;

            // Single-file apps extract to a temp directory
            string tempPath = Path.GetTempPath();
            return processPath.StartsWith(tempPath, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Initializes the working directory (temp folder for exe mode, base directory for dev mode)
        /// Must be called at application startup
        /// </summary>
        public static void Initialize()
        {
            if (_isInitialized)
                return;

            if (IsRunningAsSingleFileExe())
            {
                // Create a persistent temp folder for this application
                string tempBase = Path.Combine(Path.GetTempPath(), "CustomCursor");
                _workingDirectory = tempBase;

                // Create directories if they don't exist
                if (!Directory.Exists(_workingDirectory))
                {
                    Directory.CreateDirectory(_workingDirectory);
                }
            }
            else
            {
                // Development mode - use the base directory
                _workingDirectory = AppDomain.CurrentDomain.BaseDirectory;
            }

            _isInitialized = true;
        }

        /// <summary>
        /// Gets the working directory (temp folder in exe mode, base directory in dev mode)
        /// </summary>
        public static string GetWorkingDirectory()
        {
            if (!_isInitialized)
                Initialize();

            return _workingDirectory ?? AppDomain.CurrentDomain.BaseDirectory;
        }

        /// <summary>
        /// Gets the base directory of the application (executable or script location)
        /// </summary>
        public static string GetBaseDirectory()
        {
            return GetWorkingDirectory();
        }

        /// <summary>
        /// Gets the path to the Cursors folder
        /// </summary>
        public static string GetCursorDirectory()
        {
            string baseDir = GetWorkingDirectory();
            string cursorDir = Path.Combine(baseDir, Constants.CursorFolderName);

            if (!Directory.Exists(cursorDir))
            {
                Directory.CreateDirectory(cursorDir);
            }

            return cursorDir;
        }

        /// <summary>
        /// Gets the path to the tutorial folder
        /// </summary>
        public static string GetTutorialDirectory()
        {
            string baseDir = GetWorkingDirectory();
            return Path.Combine(baseDir, Constants.TutorialFolderName);
        }

        /// <summary>
        /// Gets the path to the application icon
        /// </summary>
        public static string? GetIconPath()
        {
            string baseDir = GetWorkingDirectory();
            string iconPath = Path.Combine(baseDir, "icon.ico");
            return File.Exists(iconPath) ? iconPath : null;
        }

        /// <summary>
        /// Gets the Windows Cursors system directory
        /// </summary>
        public static string GetWindowsCursorsDirectory()
        {
            string winDir = Environment.GetEnvironmentVariable("WINDIR")
                ?? @"C:\Windows";
            return Path.Combine(winDir, "Cursors");
        }

        /// <summary>
        /// Creates a safe directory name from a scheme name
        /// </summary>
        public static string SanitizeSchemeName(string schemeName)
        {
            // Remove invalid characters for directory names
            char[] invalidChars = Path.GetInvalidFileNameChars();
            string safe = schemeName;

            foreach (char c in invalidChars)
            {
                safe = safe.Replace(c.ToString(), "");
            }

            // Remove other problematic characters
            safe = System.Text.RegularExpressions.Regex.Replace(safe, @"[^a-zA-Z0-9 _-]", "");

            return safe.Trim();
        }
    }
}
