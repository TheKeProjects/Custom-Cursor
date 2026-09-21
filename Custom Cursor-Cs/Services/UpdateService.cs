using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

namespace CustomCursor.Services
{
    public class UpdateInfo
    {
        public string LatestVersion      { get; set; } = string.Empty;
        public string Changelog          { get; set; } = string.Empty;
        public string DownloadUrl        { get; set; } = string.Empty;
        public string FileName           { get; set; } = string.Empty;
        public long   FileSize           { get; set; }
        public bool   IsUpdateAvailable  { get; set; }
        public string ReleaseUrl         { get; set; } = string.Empty;
    }

    public class GbUpdateInfo
    {
        public string LatestVersion     { get; set; } = string.Empty;
        public string DownloadUrl       { get; set; } = string.Empty;
        public string FileName          { get; set; } = string.Empty;
        public long   FileSize          { get; set; }
        public bool   IsUpdateAvailable { get; set; }
        public string Changelog         { get; set; } = string.Empty;
    }

    public class UpdateService : IDisposable
    {
        private readonly HttpClient _httpClient;
        private bool _disposed = false;

        public UpdateService()
        {
            _httpClient = new HttpClient();
            _httpClient.DefaultRequestHeaders.Add("User-Agent", "CustomCursorManager");
            _httpClient.Timeout = TimeSpan.FromSeconds(30);
        }

        public async Task<UpdateInfo> CheckForUpdatesAsync()
        {
            var updateInfo = new UpdateInfo
            {
                LatestVersion     = AppVersion.CURRENT_VERSION,
                IsUpdateAvailable = false
            };

            try
            {
                var versionUrl    = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/{AppVersion.GITHUB_BRANCH}/version.txt";
                var latestVersion = (await _httpClient.GetStringAsync(versionUrl))
                                        .Replace("\r", "").Replace("\n", "").Replace("\t", "").Trim();

                if (string.IsNullOrWhiteSpace(latestVersion))
                    throw new Exception("version.txt is empty or invalid");

                var cleanParts = new string(latestVersion.Where(c => char.IsDigit(c) || c == '.').ToArray())
                                    .Split('.').Where(p => !string.IsNullOrEmpty(p)).ToArray();
                if (cleanParts.Length < 2 || !cleanParts.All(p => int.TryParse(p, out _)))
                    throw new Exception($"GitHub returned an invalid version: '{latestVersion}'");

                updateInfo.LatestVersion     = latestVersion;
                updateInfo.IsUpdateAvailable = IsNewerVersion(AppVersion.CURRENT_VERSION, latestVersion);

                try
                {
                    var changelogUrl    = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/{AppVersion.GITHUB_BRANCH}/changelog.txt";
                    updateInfo.Changelog = (await _httpClient.GetStringAsync(changelogUrl)).Trim();
                }
                catch { }

                if (updateInfo.IsUpdateAvailable)
                {
                    var fileName    = $"CustomCursorv{latestVersion.Replace(".", "-")}.zip";
                    var downloadUrl = $"https://github.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/releases/latest/download/{fileName}";

                    updateInfo.DownloadUrl = downloadUrl;
                    updateInfo.FileName    = fileName;
                    updateInfo.ReleaseUrl  = $"https://github.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/releases/latest";

                    try
                    {
                        using var head = new HttpRequestMessage(HttpMethod.Head, downloadUrl);
                        using var res  = await _httpClient.SendAsync(head);
                        if (res.IsSuccessStatusCode && res.Content.Headers.ContentLength.HasValue)
                            updateInfo.FileSize = res.Content.Headers.ContentLength.Value;
                    }
                    catch { }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] GitHub check failed: {ex.Message}");
            }

            return updateInfo;
        }

        // Always points at whatever release GitHub currently flags as "latest", so this
        // never needs a version number baked into it.
        private const string LatestExeFileName = "CustomCursor.exe";
        private static string LatestExeUrl
            => $"https://github.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/releases/latest/download/{LatestExeFileName}";

        // Manual fallback for when the automatic update path fails or misreports the
        // installed version as current — always returns the download info for the
        // latest published release, regardless of the local version comparison.
        public async Task<UpdateInfo> GetLatestReleaseInfoAsync()
        {
            var info = new UpdateInfo();

            // The version label is cosmetic (best effort): the download itself doesn't depend on it.
            try
            {
                var versionUrl    = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/{AppVersion.GITHUB_BRANCH}/version.txt";
                var latestVersion = (await _httpClient.GetStringAsync(versionUrl))
                                        .Replace("\r", "").Replace("\n", "").Replace("\t", "").Trim();
                if (!string.IsNullOrWhiteSpace(latestVersion))
                    info.LatestVersion = latestVersion;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] Manual version lookup failed: {ex.Message}");
            }

            // Only report a download once the release asset is confirmed reachable.
            try
            {
                using var request  = new HttpRequestMessage(HttpMethod.Get, LatestExeUrl);
                using var response = await _httpClient.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
                response.EnsureSuccessStatusCode();

                info.DownloadUrl       = LatestExeUrl;
                info.FileName          = LatestExeFileName;
                info.FileSize          = response.Content.Headers.ContentLength ?? 0;
                info.ReleaseUrl        = $"{AppVersion.GetGitHubRepoUrl()}/releases/latest";
                info.IsUpdateAvailable = true;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] Manual release lookup failed: {ex.Message}");
            }

            return info;
        }

        // Used only by the manual-update fallback window: downloads the latest .exe straight
        // from the GitHub release into the folder the app is currently running from, then
        // closes the app, replaces the old exe with the new one, and relaunches.
        public async Task<bool> DownloadAndInstallLatestExeAsync(IProgress<int>? progress = null)
        {
            string? tempDir     = null;
            string? stagingPath = null;

            try
            {
                // ProcessPath is the real single-file exe. It resolves to dotnet.exe only when
                // running without an apphost (dev), so fall back to the app folder in that case.
                var processPath = Environment.ProcessPath;
                var currentExePath =
                    !string.IsNullOrEmpty(processPath) &&
                    !Path.GetFileName(processPath).Equals("dotnet.exe", StringComparison.OrdinalIgnoreCase)
                        ? processPath
                        : Path.Combine(AppContext.BaseDirectory, LatestExeFileName);

                var currentDir = Path.GetDirectoryName(currentExePath) ?? AppContext.BaseDirectory;
                stagingPath    = Path.Combine(currentDir, Path.GetFileNameWithoutExtension(currentExePath) + ".update.exe");

                using (var response = await _httpClient.GetAsync(LatestExeUrl, HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();
                    var totalBytes = response.Content.Headers.ContentLength ?? 0;
                    var buffer     = new byte[8192];
                    var bytesRead  = 0L;

                    using var contentStream = await response.Content.ReadAsStreamAsync();
                    using var fileStream    = new FileStream(stagingPath, FileMode.Create, FileAccess.Write, FileShare.None, 8192, true);

                    int read;
                    while ((read = await contentStream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                    {
                        await fileStream.WriteAsync(buffer, 0, read);
                        bytesRead += read;
                        if (totalBytes > 0)
                            progress?.Report((int)((bytesRead * 100) / totalBytes));
                    }

                    // Never swap in a truncated download over a working exe.
                    if (totalBytes > 0 && bytesRead != totalBytes)
                        throw new IOException($"Download incomplete ({bytesRead}/{totalBytes} bytes)");
                }

                tempDir = Path.Combine(Path.GetTempPath(), "CustomCursorManager_Update");
                if (Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
                Directory.CreateDirectory(tempDir);

                var batchPath    = Path.Combine(tempDir, "update.bat");
                var batchContent = $@"@echo off
chcp 65001 >nul
timeout /t 2 /nobreak >nul
set RETRIES=10
:retry
move /y ""{stagingPath}"" ""{currentExePath}"" >nul 2>&1
if not exist ""{stagingPath}"" goto done
set /a RETRIES-=1
if %RETRIES% GTR 0 (
    timeout /t 1 /nobreak >nul
    goto retry
)
del /f /q ""{stagingPath}"" >nul 2>&1
:done
start """" ""{currentExePath}""
rd /s /q ""{tempDir}""
exit";

                await File.WriteAllTextAsync(batchPath, batchContent);
                Process.Start(new ProcessStartInfo
                {
                    FileName        = "cmd.exe",
                    Arguments       = $"/c \"{batchPath}\"",
                    UseShellExecute = true,
                    WindowStyle     = ProcessWindowStyle.Hidden
                });

                await Task.Delay(1000);
                Environment.Exit(0);
                return true;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] Manual exe install failed: {ex.Message}");
                try
                {
                    if (stagingPath != null && File.Exists(stagingPath)) File.Delete(stagingPath);
                    if (tempDir != null && Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
                }
                catch { }
                return false;
            }
        }

        public async Task<GbUpdateInfo> CheckGameBananaUpdateAsync()
        {
            var info = new GbUpdateInfo();
            if (AppVersion.GB_TOOL_ID <= 0) return info;

            try
            {
                var url  = $"https://api.gamebanana.com/Core/Item/Data?itemtype=Tool&itemid={AppVersion.GB_TOOL_ID}&fields=name,Files().aFiles()";
                var json = await _httpClient.GetStringAsync(url);

                using var doc  = JsonDocument.Parse(json);
                var root = doc.RootElement;

                if (root.ValueKind != JsonValueKind.Array || root.GetArrayLength() < 2) return info;
                var filesElement = root[1];
                if (filesElement.ValueKind != JsonValueKind.Object) return info;

                foreach (var fileProp in filesElement.EnumerateObject())
                {
                    var f         = fileProp.Value;
                    var gbVersion = GbGetString(f, "_sVersion")?.Trim() ?? string.Empty;
                    info.LatestVersion     = gbVersion;
                    info.IsUpdateAvailable = IsNewerVersion(AppVersion.CURRENT_VERSION, gbVersion);

                    if (info.IsUpdateAvailable)
                    {
                        info.DownloadUrl = GbGetString(f, "_sDownloadUrl") ?? string.Empty;
                        info.FileName    = GbGetString(f, "_sFile")        ?? string.Empty;
                        info.FileSize    = GbGetLong(f,   "_nFilesize")    ?? 0;

                        try
                        {
                            var changelogUrl = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{AppVersion.GITHUB_REPO}/{AppVersion.GITHUB_BRANCH}/changelog.txt";
                            info.Changelog   = (await _httpClient.GetStringAsync(changelogUrl)).Trim();
                        }
                        catch { }
                    }
                    break;
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] GameBanana check failed: {ex.Message}");
            }

            return info;
        }

        public async Task<bool> DownloadAndInstallUpdateAsync(UpdateInfo updateInfo, IProgress<int>? progress = null)
        {
            if (string.IsNullOrWhiteSpace(updateInfo.DownloadUrl)) return false;

            string? tempDir = null;
            string? zipPath = null;

            try
            {
                tempDir = Path.Combine(Path.GetTempPath(), "CustomCursorManager_Update");
                if (Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
                Directory.CreateDirectory(tempDir);

                zipPath = Path.Combine(tempDir, updateInfo.FileName);

                using (var response = await _httpClient.GetAsync(updateInfo.DownloadUrl, HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();
                    var totalBytes = response.Content.Headers.ContentLength ?? 0;
                    var buffer     = new byte[8192];
                    var bytesRead  = 0L;

                    using var contentStream = await response.Content.ReadAsStreamAsync();
                    using var fileStream    = new FileStream(zipPath, FileMode.Create, FileAccess.Write, FileShare.None, 8192, true);

                    int read;
                    while ((read = await contentStream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                    {
                        await fileStream.WriteAsync(buffer, 0, read);
                        bytesRead += read;
                        if (totalBytes > 0)
                            progress?.Report((int)((bytesRead * 100) / totalBytes));
                    }
                }

                var extractDir = Path.Combine(tempDir, "extracted");
                Directory.CreateDirectory(extractDir);
                ZipFile.ExtractToDirectory(zipPath, extractDir);

                var newExePath = FindExecutable(extractDir);
                if (string.IsNullOrEmpty(newExePath)) return false;

                var currentExePath  = Environment.ProcessPath ?? Path.Combine(AppContext.BaseDirectory, "CustomCursor.exe");
                var currentDir      = Path.GetDirectoryName(currentExePath) ?? AppContext.BaseDirectory;
                var extractedExeDir = Path.GetDirectoryName(newExePath) ?? extractDir;

                var batchPath    = Path.Combine(tempDir, "update.bat");
                var batchContent = $@"@echo off
chcp 65001 >nul
timeout /t 2 /nobreak >nul
xcopy ""{extractedExeDir}\*"" ""{currentDir}"" /E /Y /I /Q
if %ERRORLEVEL% NEQ 0 (pause & exit /b 1)
start """" ""{currentExePath}""
rd /s /q ""{tempDir}""
exit";

                await File.WriteAllTextAsync(batchPath, batchContent);
                Process.Start(new ProcessStartInfo
                {
                    FileName        = "cmd.exe",
                    Arguments       = $"/c \"{batchPath}\"",
                    UseShellExecute = true,
                    WindowStyle     = ProcessWindowStyle.Hidden
                });

                await Task.Delay(1000);
                Environment.Exit(0);
                return true;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] Install failed: {ex.Message}");
                try
                {
                    if (zipPath != null && File.Exists(zipPath)) File.Delete(zipPath);
                    if (tempDir != null && Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
                }
                catch { }
                return false;
            }
        }

        public async Task<bool> DownloadAndInstallGbUpdateAsync(GbUpdateInfo gbInfo, IProgress<int>? progress = null)
        {
            var mapped = new UpdateInfo
            {
                DownloadUrl       = gbInfo.DownloadUrl,
                FileName          = string.IsNullOrWhiteSpace(gbInfo.FileName) ? "gamebanana_update.zip" : gbInfo.FileName,
                IsUpdateAvailable = true,
                LatestVersion     = gbInfo.LatestVersion
            };
            return await DownloadAndInstallUpdateAsync(mapped, progress);
        }

        private string FindExecutable(string directory)
        {
            var names = new List<string> { "CustomCursor.exe" };
            var processName = Path.GetFileName(Environment.ProcessPath);
            if (!string.IsNullOrEmpty(processName)) names.Insert(0, processName);

            foreach (var name in names)
            {
                var found = Directory.GetFiles(directory, name, SearchOption.AllDirectories).FirstOrDefault();
                if (found != null) return found;
            }

            return Directory.GetFiles(directory, "*.exe", SearchOption.AllDirectories).FirstOrDefault() ?? "";
        }

        private bool IsNewerVersion(string currentVersion, string newVersion)
        {
            try
            {
                var cur = ParseVersion(currentVersion);
                var lat = ParseVersion(newVersion);
                if (lat.major != cur.major) return lat.major > cur.major;
                if (lat.minor != cur.minor) return lat.minor > cur.minor;
                return lat.patch > cur.patch;
            }
            catch { return false; }
        }

        private static (int major, int minor, int patch) ParseVersion(string version)
        {
            var clean = new string(version.Where(c => char.IsDigit(c) || c == '.').ToArray());
            var parts = clean.Split('.');
            return (
                parts.Length > 0 && int.TryParse(parts[0], out int m) ? m : 0,
                parts.Length > 1 && int.TryParse(parts[1], out int n) ? n : 0,
                parts.Length > 2 && int.TryParse(parts[2], out int p) ? p : 0
            );
        }

        private static string? GbGetString(JsonElement el, string key)
            => el.TryGetProperty(key, out var p) && p.ValueKind == JsonValueKind.String ? p.GetString() : null;

        private static long? GbGetLong(JsonElement el, string key)
            => el.TryGetProperty(key, out var p) && p.TryGetInt64(out var v) ? v : null;

        public static string FormatFileSize(long bytes)
        {
            if (bytes <= 0)   return "—";
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            return $"{bytes / (1024.0 * 1024):F2} MB";
        }

        public void Dispose()
        {
            if (!_disposed) { _httpClient?.Dispose(); _disposed = true; }
        }
    }
}
