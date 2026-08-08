using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using CustomCursor.Models;

namespace CustomCursor.Services
{
    /// <summary>
    /// Handles elevated cursor installation via subprocess
    /// </summary>
    public class ElevatedInstaller
    {
        private readonly CursorRegistry _cursorRegistry;

        public ElevatedInstaller()
        {
            _cursorRegistry = new CursorRegistry();
        }

        /// <summary>
        /// Install cursor scheme with elevation if needed
        /// </summary>
        public bool InstallCursorScheme(CursorScheme scheme, out string errorMessage)
        {
            if (IsRunningAsAdmin())
            {
                // Already admin, install directly
                return _cursorRegistry.ApplyCursorScheme(scheme, out errorMessage);
            }
            else
            {
                // Not admin, spawn elevated subprocess
                return InstallWithElevation(new List<CursorScheme> { scheme }, out errorMessage);
            }
        }

        /// <summary>
        /// Install multiple cursor schemes with elevation if needed
        /// </summary>
        public List<(string SchemeName, bool Success, string ErrorMessage)> InstallMultipleCursorSchemes(
            List<CursorScheme> schemes,
            Action<int, int, string>? progressCallback = null)
        {
            if (IsRunningAsAdmin())
            {
                // Already admin, install directly
                return _cursorRegistry.ApplyMultipleCursorSchemes(schemes, progressCallback);
            }
            else
            {
                // Not admin, spawn elevated subprocess
                string errorMessage;
                bool success = InstallWithElevation(schemes, out errorMessage);

                if (success)
                {
                    // Return success for all schemes
                    var results = new List<(string, bool, string)>();
                    foreach (var scheme in schemes)
                    {
                        results.Add((scheme.Name, true, string.Empty));
                    }
                    return results;
                }
                else
                {
                    // Return failure
                    var results = new List<(string, bool, string)>();
                    foreach (var scheme in schemes)
                    {
                        results.Add((scheme.Name, false, errorMessage));
                    }
                    return results;
                }
            }
        }

        /// <summary>
        /// Spawn elevated subprocess to install cursors
        /// </summary>
        private bool InstallWithElevation(List<CursorScheme> schemes, out string errorMessage)
        {
            errorMessage = string.Empty;

            try
            {
                // Serialize cursor schemes to temp file
                string tempFile = Path.Combine(Path.GetTempPath(), $"cursor_install_{Guid.NewGuid()}.json");
                var json = JsonSerializer.Serialize(schemes, new JsonSerializerOptions
                {
                    WriteIndented = false
                });
                File.WriteAllText(tempFile, json);

                // Get current executable path
                string exePath = Process.GetCurrentProcess().MainModule?.FileName
                    ?? Environment.ProcessPath
                    ?? string.Empty;

                // If it's a .dll, we need to use dotnet
                if (exePath.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                {
                    exePath = "dotnet";
                }

                // Start elevated process
                var startInfo = new ProcessStartInfo
                {
                    FileName = exePath,
                    Arguments = $"--install-cursors \"{tempFile}\"",
                    Verb = "runas", // Request elevation
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Hidden
                };

                using (var process = Process.Start(startInfo))
                {
                    if (process == null)
                    {
                        errorMessage = "Failed to start elevated process";
                        return false;
                    }

                    process.WaitForExit();

                    // Check exit code
                    if (process.ExitCode == 0)
                    {
                        // Success - cleanup temp file
                        try { File.Delete(tempFile); } catch { }
                        return true;
                    }
                    else
                    {
                        // Failure - read error from temp file
                        string errorFile = tempFile + ".error";
                        if (File.Exists(errorFile))
                        {
                            errorMessage = File.ReadAllText(errorFile);
                            try { File.Delete(errorFile); } catch { }
                        }
                        else
                        {
                            errorMessage = $"Installation failed with exit code: {process.ExitCode}";
                        }

                        try { File.Delete(tempFile); } catch { }
                        return false;
                    }
                }
            }
            catch (System.ComponentModel.Win32Exception ex)
            {
                // User cancelled UAC prompt
                if (ex.NativeErrorCode == 1223) // ERROR_CANCELLED
                {
                    errorMessage = "User cancelled elevation request";
                }
                else
                {
                    errorMessage = $"Failed to elevate: {ex.Message}";
                }
                return false;
            }
            catch (Exception ex)
            {
                errorMessage = $"Elevation error: {ex.Message}";
                return false;
            }
        }

        /// <summary>
        /// Handle command-line cursor installation/uninstallation (called by elevated subprocess)
        /// </summary>
        public static int HandleCommandLineInstall(string[] args)
        {
            try
            {
                var registry = new CursorRegistry();

                // Check for --install-cursors argument
                for (int i = 0; i < args.Length; i++)
                {
                    if (args[i] == "--install-cursors" && i + 1 < args.Length)
                    {
                        string tempFile = args[i + 1];

                        if (!File.Exists(tempFile))
                        {
                            Console.Error.WriteLine($"Temp file not found: {tempFile}");
                            return 1;
                        }

                        // Read cursor schemes from temp file
                        string json = File.ReadAllText(tempFile);
                        var schemes = JsonSerializer.Deserialize<List<CursorScheme>>(json);

                        if (schemes == null || schemes.Count == 0)
                        {
                            Console.Error.WriteLine("No cursor schemes to install");
                            return 1;
                        }

                        // Install cursors
                        bool allSuccess = true;
                        string errorMessage = string.Empty;

                        foreach (var scheme in schemes)
                        {
                            bool success = registry.ApplyCursorScheme(scheme, out string error);
                            if (!success)
                            {
                                allSuccess = false;
                                errorMessage += $"{scheme.Name}: {error}\n";
                            }
                        }

                        if (!allSuccess)
                        {
                            // Write error to temp file
                            string errorFile = tempFile + ".error";
                            File.WriteAllText(errorFile, errorMessage);
                            return 1;
                        }

                        return 0; // Success
                    }

                    // Check for --uninstall-cursor argument
                    if (args[i] == "--uninstall-cursor" && i + 1 < args.Length)
                    {
                        string tempFile = args[i + 1];

                        if (!File.Exists(tempFile))
                        {
                            Console.Error.WriteLine($"Temp file not found: {tempFile}");
                            return 1;
                        }

                        // Read cursor scheme and replacement from temp file
                        string json = File.ReadAllText(tempFile);
                        var data = JsonSerializer.Deserialize<JsonElement>(json);

                        var scheme = data.GetProperty("Scheme").Deserialize<CursorScheme>();
                        string? replacementScheme = null;

                        if (data.TryGetProperty("ReplacementScheme", out JsonElement replacementElement) &&
                            replacementElement.ValueKind != JsonValueKind.Null)
                        {
                            replacementScheme = replacementElement.GetString();
                        }

                        if (scheme == null)
                        {
                            Console.Error.WriteLine("No cursor scheme to uninstall");
                            return 1;
                        }

                        // Uninstall cursor with replacement
                        bool success = registry.UninstallCursorScheme(scheme, replacementScheme, out string errorMessage);

                        if (!success)
                        {
                            // Write error to temp file
                            string errorFile = tempFile + ".error";
                            File.WriteAllText(errorFile, errorMessage);
                            return 1;
                        }

                        return 0; // Success
                    }
                }

                return 0; // No command found, continue normal execution
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Command-line error: {ex.Message}");
                return 1;
            }
        }

        /// <summary>
        /// Check if a cursor scheme is installed
        /// </summary>
        public bool IsSchemeInstalled(string schemeName)
        {
            return _cursorRegistry.IsSchemeInstalled(schemeName);
        }

        /// <summary>
        /// Get list of all installed cursor schemes
        /// </summary>
        public List<string> GetInstalledSchemes()
        {
            return _cursorRegistry.GetInstalledSchemes();
        }

        /// <summary>
        /// Get the currently active cursor scheme name
        /// </summary>
        public string? GetActiveScheme()
        {
            return _cursorRegistry.GetActiveScheme();
        }

        /// <summary>
        /// Uninstall a cursor scheme with elevation if needed
        /// </summary>
        public bool UninstallCursorScheme(CursorScheme scheme, string? replacementScheme, out string errorMessage)
        {
            if (IsRunningAsAdmin())
            {
                // Already admin, uninstall directly
                return _cursorRegistry.UninstallCursorScheme(scheme, replacementScheme, out errorMessage);
            }
            else
            {
                // Not admin, spawn elevated subprocess
                return UninstallWithElevation(scheme, replacementScheme, out errorMessage);
            }
        }

        /// <summary>
        /// Uninstall a cursor scheme (backward compatibility)
        /// </summary>
        public bool UninstallCursorScheme(CursorScheme scheme, out string errorMessage)
        {
            return UninstallCursorScheme(scheme, null, out errorMessage);
        }

        /// <summary>
        /// Spawn elevated subprocess to uninstall cursor
        /// </summary>
        private bool UninstallWithElevation(CursorScheme scheme, string? replacementScheme, out string errorMessage)
        {
            errorMessage = string.Empty;

            try
            {
                // Serialize cursor scheme and replacement to temp file
                string tempFile = Path.Combine(Path.GetTempPath(), $"cursor_uninstall_{Guid.NewGuid()}.json");
                var data = new
                {
                    Scheme = scheme,
                    ReplacementScheme = replacementScheme
                };
                var json = JsonSerializer.Serialize(data, new JsonSerializerOptions
                {
                    WriteIndented = false
                });
                File.WriteAllText(tempFile, json);

                // Get current executable path
                string exePath = Process.GetCurrentProcess().MainModule?.FileName
                    ?? Environment.ProcessPath
                    ?? string.Empty;

                if (exePath.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                {
                    exePath = "dotnet";
                }

                // Start elevated process
                var startInfo = new ProcessStartInfo
                {
                    FileName = exePath,
                    Arguments = $"--uninstall-cursor \"{tempFile}\"",
                    Verb = "runas",
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Hidden
                };

                using (var process = Process.Start(startInfo))
                {
                    if (process == null)
                    {
                        errorMessage = "Failed to start elevated process";
                        return false;
                    }

                    process.WaitForExit();

                    if (process.ExitCode == 0)
                    {
                        try { File.Delete(tempFile); } catch { }
                        return true;
                    }
                    else
                    {
                        string errorFile = tempFile + ".error";
                        if (File.Exists(errorFile))
                        {
                            errorMessage = File.ReadAllText(errorFile);
                            try { File.Delete(errorFile); } catch { }
                        }
                        else
                        {
                            errorMessage = $"Uninstall failed with exit code: {process.ExitCode}";
                        }

                        try { File.Delete(tempFile); } catch { }
                        return false;
                    }
                }
            }
            catch (System.ComponentModel.Win32Exception ex)
            {
                if (ex.NativeErrorCode == 1223)
                {
                    errorMessage = "User cancelled elevation request";
                }
                else
                {
                    errorMessage = $"Failed to elevate: {ex.Message}";
                }
                return false;
            }
            catch (Exception ex)
            {
                errorMessage = $"Elevation error: {ex.Message}";
                return false;
            }
        }

        /// <summary>
        /// Check if running as administrator
        /// </summary>
        private bool IsRunningAsAdmin()
        {
            try
            {
                var identity = System.Security.Principal.WindowsIdentity.GetCurrent();
                var principal = new System.Security.Principal.WindowsPrincipal(identity);
                return principal.IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
            }
            catch
            {
                return false;
            }
        }
    }
}
