using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using CustomCursor.Models;
using CustomCursor.Utilities;
using Microsoft.Win32;

namespace CustomCursor.Services
{
    /// <summary>
    /// Manages Windows Registry operations for cursor schemes
    /// </summary>
    public class CursorRegistry
    {
        private readonly CursorFileService _fileService;

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool SystemParametersInfo(uint uiAction, uint uiParam, IntPtr pvParam, uint fWinIni);

        public CursorRegistry()
        {
            _fileService = new CursorFileService();
        }

        /// <summary>
        /// Applies a cursor scheme to the system
        /// </summary>
        public bool ApplyCursorScheme(CursorScheme scheme, out string errorMessage)
        {
            errorMessage = string.Empty;

            try
            {
                string safeSchemeName = PathHelper.SanitizeSchemeName(scheme.Name);
                string systemCursorPath = Path.Combine(
                    PathHelper.GetWindowsCursorsDirectory(),
                    safeSchemeName
                );

                // Create directory if it doesn't exist
                if (!Directory.Exists(systemCursorPath))
                {
                    Directory.CreateDirectory(systemCursorPath);
                }

                // Copy cursor files to system directory
                foreach (var kvp in scheme.CursorMap)
                {
                    string filename = kvp.Value;

                    // Skip special tokens and invalid filenames
                    if (string.IsNullOrWhiteSpace(filename) ||
                        filename.Contains("$") ||
                        filename.Contains("..") ||
                        Path.GetInvalidFileNameChars().Any(c => filename.Contains(c)))
                    {
                        Console.WriteLine($"Skipping invalid filename: {filename}");
                        continue;
                    }

                    string sourceFile = Path.Combine(scheme.Path, filename);
                    string destFile = Path.Combine(systemCursorPath, filename);

                    if (File.Exists(sourceFile))
                    {
                        try
                        {
                            _fileService.CopyCursorFile(sourceFile, destFile);
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"Error copying {filename}: {ex.Message}");
                            // Continue with other files
                        }
                    }
                    else
                    {
                        Console.WriteLine($"Source file not found (skipping): {sourceFile}");
                        // Continue with other files instead of failing
                    }
                }

                // Build scheme string for registry
                List<string> schemeValues = new List<string>();
                foreach (string regName in Constants.RegistryNames.Keys)
                {
                    if (scheme.CursorMap.ContainsKey(regName))
                    {
                        string filename = scheme.CursorMap[regName];

                        // Skip special tokens and invalid filenames
                        if (!string.IsNullOrWhiteSpace(filename) &&
                            !filename.Contains("$") &&
                            !filename.Contains("..") &&
                            !Path.GetInvalidFileNameChars().Any(c => filename.Contains(c)))
                        {
                            string cursorPath = $"%SystemRoot%\\Cursors\\{safeSchemeName}\\{filename}";
                            schemeValues.Add(cursorPath);
                        }
                        else
                        {
                            schemeValues.Add("");
                        }
                    }
                    else
                    {
                        schemeValues.Add("");
                    }
                }

                string fullSchemeString = string.Join(",", schemeValues);

                // Register the scheme in the registry
                RegisterScheme(safeSchemeName, fullSchemeString);

                // Apply the scheme
                ApplyScheme(safeSchemeName, scheme.CursorMap, systemCursorPath);

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = $"Error: {ex.Message}\n\nDetails: {ex.InnerException?.Message ?? "N/A"}\n\nStack: {ex.StackTrace}";
                Console.WriteLine($"Error applying cursor scheme: {errorMessage}");
                return false;
            }
        }

        /// <summary>
        /// Applies a cursor scheme to the system (backward compatibility)
        /// </summary>
        public bool ApplyCursorScheme(CursorScheme scheme)
        {
            return ApplyCursorScheme(scheme, out _);
        }

        /// <summary>
        /// Registers a cursor scheme in the Windows Registry
        /// </summary>
        private void RegisterScheme(string schemeName, string schemeString)
        {
            try
            {
                using RegistryKey? key = Registry.CurrentUser.OpenSubKey(
                    Constants.CursorSchemesRegistryPath,
                    writable: true
                );

                if (key == null)
                {
                    // Create the key if it doesn't exist
                    using RegistryKey? newKey = Registry.CurrentUser.CreateSubKey(
                        Constants.CursorSchemesRegistryPath
                    );
                    newKey?.SetValue(schemeName, schemeString, RegistryValueKind.String);
                }
                else
                {
                    key.SetValue(schemeName, schemeString, RegistryValueKind.String);
                }
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"Failed to register cursor scheme: {schemeName}", ex);
            }
        }

        /// <summary>
        /// Applies a registered cursor scheme by reading from Schemes and setting individual values
        /// </summary>
        private void ApplyScheme(string schemeName, Dictionary<string, string> cursorMap, string systemCursorPath)
        {
            try
            {
                // Read the scheme from the Schemes registry to get the full cursor list
                string? schemeString = null;
                using (RegistryKey? schemesKey = Registry.CurrentUser.OpenSubKey(Constants.CursorSchemesRegistryPath))
                {
                    schemeString = schemesKey?.GetValue(schemeName) as string;
                }

                if (string.IsNullOrEmpty(schemeString))
                {
                    throw new InvalidOperationException($"Scheme '{schemeName}' not found in registry");
                }

                // Parse the scheme string to get cursor paths
                string[] cursorPaths = schemeString.Split(',');

                // Set the scheme name and individual cursor values
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(
                    Constants.CursorsRegistryPath,
                    writable: true
                ))
                {
                    if (key != null)
                    {
                        // Set the scheme name FIRST, exactly as Control Panel does:
                        // (Default) holds the name and Source 1 means a user scheme (HKCU\...\Schemes)
                        key.SetValue("Scheme Source", 1, RegistryValueKind.DWord);
                        key.SetValue("", schemeName, RegistryValueKind.String);

                        // Set individual cursor paths from the scheme string
                        int index = 0;
                        foreach (var kvp in Constants.RegistryNames)
                        {
                            string regName = kvp.Value; // e.g., "Arrow"

                            if (index < cursorPaths.Length)
                            {
                                string cursorPath = cursorPaths[index].Trim();

                                if (!string.IsNullOrEmpty(cursorPath))
                                {
                                    // Expand the path if it contains environment variables
                                    key.SetValue(regName, cursorPath, RegistryValueKind.ExpandString);
                                }
                                else
                                {
                                    key.SetValue(regName, "", RegistryValueKind.String);
                                }
                            }
                            else
                            {
                                key.SetValue(regName, "", RegistryValueKind.String);
                            }

                            index++;
                        }
                    }
                }

                // Refresh cursors using SystemParametersInfo
                SystemParametersInfo(
                    Constants.SPI_SETCURSORS,
                    0,
                    IntPtr.Zero,
                    Constants.SPIF_UPDATEINIFILE | Constants.SPIF_SENDCHANGE
                );
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"Failed to apply cursor scheme: {schemeName}", ex);
            }
        }

        /// <summary>
        /// Applies multiple cursor schemes with progress callback
        /// </summary>
        public List<(string SchemeName, bool Success, string ErrorMessage)> ApplyMultipleCursorSchemes(
            List<CursorScheme> schemes,
            Action<int, int, string>? progressCallback = null)
        {
            var results = new List<(string, bool, string)>();
            int total = schemes.Count;

            for (int i = 0; i < total; i++)
            {
                var scheme = schemes[i];
                progressCallback?.Invoke(i + 1, total, scheme.Name);

                bool success = ApplyCursorScheme(scheme, out string errorMessage);
                results.Add((scheme.Name, success, errorMessage));

                // Small delay for UI update
                System.Threading.Thread.Sleep(100);
            }

            return results;
        }

        /// <summary>
        /// Check if a cursor scheme is installed
        /// </summary>
        public bool IsSchemeInstalled(string schemeName)
        {
            try
            {
                string safeSchemeName = PathHelper.SanitizeSchemeName(schemeName);

                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(Constants.CursorSchemesRegistryPath))
                {
                    if (key == null) return false;

                    var schemeValue = key.GetValue(safeSchemeName);
                    return schemeValue != null;
                }
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Get the currently active cursor scheme name
        /// </summary>
        public string? GetActiveScheme()
        {
            try
            {
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(Constants.CursorsRegistryPath))
                {
                    // (Default) is where Windows keeps the active scheme name; "Scheme Name" is
                    // only what older versions of this app wrote, so use it just when (Default) is unset.
                    return key?.GetValue("") as string ?? key?.GetValue("Scheme Name") as string;
                }
            }
            catch
            {
                return null;
            }
        }

        /// <summary>
        /// Apply the Windows default cursor scheme
        /// </summary>
        public bool ApplyDefaultCursorScheme(out string errorMessage)
        {
            errorMessage = string.Empty;

            try
            {
                // Set to Windows default (empty values)
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(
                    Constants.CursorsRegistryPath,
                    writable: true))
                {
                    if (key != null)
                    {
                        key.SetValue("Scheme Source", 0, RegistryValueKind.DWord);
                        key.SetValue("", "", RegistryValueKind.String);

                        // Set all cursor values to empty (uses Windows defaults)
                        foreach (var kvp in Constants.RegistryNames)
                        {
                            key.SetValue(kvp.Value, "", RegistryValueKind.String);
                        }
                    }
                }

                // Refresh cursors
                SystemParametersInfo(
                    Constants.SPI_SETCURSORS,
                    0,
                    IntPtr.Zero,
                    Constants.SPIF_UPDATEINIFILE | Constants.SPIF_SENDCHANGE
                );

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = $"Error applying default cursor: {ex.Message}";
                return false;
            }
        }

        /// <summary>
        /// Get list of all installed cursor schemes
        /// </summary>
        public List<string> GetInstalledSchemes()
        {
            var schemes = new List<string>();

            try
            {
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(Constants.CursorSchemesRegistryPath))
                {
                    if (key != null)
                    {
                        foreach (string valueName in key.GetValueNames())
                        {
                            if (!string.IsNullOrWhiteSpace(valueName))
                            {
                                schemes.Add(valueName);
                            }
                        }
                    }
                }
            }
            catch
            {
                // Ignore errors
            }

            return schemes;
        }

        /// <summary>
        /// Apply a specific cursor scheme by name
        /// </summary>
        public bool ApplySchemeByName(string schemeName, out string errorMessage)
        {
            errorMessage = string.Empty;

            try
            {
                // Read the scheme from the Schemes registry
                string? schemeString = null;
                using (RegistryKey? schemesKey = Registry.CurrentUser.OpenSubKey(Constants.CursorSchemesRegistryPath))
                {
                    schemeString = schemesKey?.GetValue(schemeName) as string;
                }

                if (string.IsNullOrEmpty(schemeString))
                {
                    errorMessage = $"Scheme '{schemeName}' not found in registry";
                    return false;
                }

                // Parse and apply
                string[] cursorPaths = schemeString.Split(',');

                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(
                    Constants.CursorsRegistryPath,
                    writable: true))
                {
                    if (key != null)
                    {
                        key.SetValue("Scheme Source", 1, RegistryValueKind.DWord);
                        key.SetValue("", schemeName, RegistryValueKind.String);

                        int index = 0;
                        foreach (var kvp in Constants.RegistryNames)
                        {
                            if (index < cursorPaths.Length)
                            {
                                string cursorPath = cursorPaths[index].Trim();
                                key.SetValue(kvp.Value, cursorPath,
                                    string.IsNullOrEmpty(cursorPath) ? RegistryValueKind.String : RegistryValueKind.ExpandString);
                            }
                            else
                            {
                                key.SetValue(kvp.Value, "", RegistryValueKind.String);
                            }
                            index++;
                        }
                    }
                }

                SystemParametersInfo(
                    Constants.SPI_SETCURSORS,
                    0,
                    IntPtr.Zero,
                    Constants.SPIF_UPDATEINIFILE | Constants.SPIF_SENDCHANGE
                );

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = $"Error applying scheme: {ex.Message}";
                return false;
            }
        }

        /// <summary>
        /// Uninstall a cursor scheme (with optional replacement scheme)
        /// </summary>
        public bool UninstallCursorScheme(CursorScheme scheme, string? replacementScheme, out string errorMessage)
        {
            errorMessage = string.Empty;

            try
            {
                string safeSchemeName = PathHelper.SanitizeSchemeName(scheme.Name);
                string systemCursorPath = Path.Combine(
                    PathHelper.GetWindowsCursorsDirectory(),
                    safeSchemeName
                );

                // Check if this is the currently active scheme
                string? activeScheme = GetActiveScheme();
                if (!string.IsNullOrEmpty(activeScheme) &&
                    activeScheme.Equals(safeSchemeName, StringComparison.OrdinalIgnoreCase))
                {
                    // Switch to replacement scheme or default
                    if (!string.IsNullOrEmpty(replacementScheme))
                    {
                        if (!ApplySchemeByName(replacementScheme, out string switchError))
                        {
                            errorMessage = $"Failed to switch to replacement cursor: {switchError}";
                            return false;
                        }
                    }
                    else
                    {
                        if (!ApplyDefaultCursorScheme(out string switchError))
                        {
                            errorMessage = $"Failed to switch to default cursor: {switchError}";
                            return false;
                        }
                    }
                }

                // Remove from registry
                using (RegistryKey? key = Registry.CurrentUser.OpenSubKey(
                    Constants.CursorSchemesRegistryPath,
                    writable: true))
                {
                    if (key != null)
                    {
                        key.DeleteValue(safeSchemeName, false);
                    }
                }

                // Delete files from system directory
                if (Directory.Exists(systemCursorPath))
                {
                    Directory.Delete(systemCursorPath, true);
                }

                return true;
            }
            catch (Exception ex)
            {
                errorMessage = $"Error uninstalling cursor: {ex.Message}";
                return false;
            }
        }

        /// <summary>
        /// Uninstall a cursor scheme (backward compatibility)
        /// </summary>
        public bool UninstallCursorScheme(CursorScheme scheme, out string errorMessage)
        {
            return UninstallCursorScheme(scheme, null, out errorMessage);
        }
    }
}
