using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using CustomCursor.Models;
using CustomCursor.Utilities;

namespace CustomCursor.Services
{
    /// <summary>
    /// Parses cursor scheme INF files and scans directories for cursor sets
    /// </summary>
    public class CursorParser
    {
        /// <summary>
        /// Parses a scheme.inf file to extract cursor configuration
        /// </summary>
        public (string SchemeName, Dictionary<string, string> CursorMap) ParseInfFile(string infPath)
        {
            try
            {
                string content = File.ReadAllText(infPath);

                // Extract cursor mappings (key = "filename")
                var cursorMap = new Dictionary<string, string>();
                var matches = Regex.Matches(content, @"(\w+)\s*=\s*""(.+?)""");

                foreach (Match match in matches)
                {
                    if (match.Groups.Count >= 3)
                    {
                        string key = match.Groups[1].Value;
                        string value = match.Groups[2].Value;
                        cursorMap[key] = value;
                    }
                }

                // Extract scheme name
                string schemeName = "Unknown Scheme";
                var schemeMatch = Regex.Match(content, @"SCHEME_NAME\s*=\s*""(.+?)""");
                if (schemeMatch.Success && schemeMatch.Groups.Count >= 2)
                {
                    schemeName = schemeMatch.Groups[1].Value;
                }

                return (schemeName, cursorMap);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"Error parsing INF file: {infPath}", ex);
            }
        }

        /// <summary>
        /// Finds a preview image in the cursor folder
        /// </summary>
        public string? FindPreviewImage(string folderPath)
        {
            string[] previewExtensions = { ".png", ".jpg", ".jpeg", ".gif" };

            foreach (string file in Directory.GetFiles(folderPath))
            {
                string ext = Path.GetExtension(file).ToLowerInvariant();
                if (Array.Exists(previewExtensions, e => e == ext))
                {
                    return file;
                }
            }

            return null;
        }

        /// <summary>
        /// Recursively scans the cursor directory for cursor sets
        /// </summary>
        public Dictionary<string, List<CursorScheme>> FindCursorSets(string cursorDirectory)
        {
            var cursorCategories = new Dictionary<string, List<CursorScheme>>();

            if (!Directory.Exists(cursorDirectory))
            {
                return cursorCategories;
            }

            // Recursively walk through directories
            ScanDirectory(cursorDirectory, cursorDirectory, cursorCategories);

            return cursorCategories;
        }

        private void ScanDirectory(string currentPath, string basePath, Dictionary<string, List<CursorScheme>> categories)
        {
            try
            {
                // Look for scheme.inf files
                string infPath = Path.Combine(currentPath, Constants.SchemeFileName);

                if (File.Exists(infPath))
                {
                    try
                    {
                        var (schemeName, cursorMap) = ParseInfFile(infPath);
                        string? previewPath = FindPreviewImage(currentPath);

                        // Get relative path for categorization
                        string relativePath = Path.GetRelativePath(basePath, currentPath);
                        string category = GetCategoryFromPath(relativePath);

                        if (!categories.ContainsKey(category))
                        {
                            categories[category] = new List<CursorScheme>();
                        }

                        categories[category].Add(new CursorScheme
                        {
                            Name = schemeName,
                            Path = currentPath,
                            RelativePath = relativePath,
                            PreviewImagePath = previewPath,
                            CursorMap = cursorMap,
                            Category = category
                        });
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Error processing {infPath}: {ex.Message}");
                    }
                }

                // Recursively scan subdirectories
                foreach (string subDir in Directory.GetDirectories(currentPath))
                {
                    // Skip hidden directories
                    DirectoryInfo dirInfo = new DirectoryInfo(subDir);
                    if (!dirInfo.Attributes.HasFlag(FileAttributes.Hidden))
                    {
                        ScanDirectory(subDir, basePath, categories);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error scanning directory {currentPath}: {ex.Message}");
            }
        }

        private string GetCategoryFromPath(string relativePath)
        {
            if (relativePath == ".")
            {
                return "General";
            }

            string[] parts = relativePath.Split(Path.DirectorySeparatorChar);
            return parts.Length > 0 ? parts[0] : "General";
        }
    }
}
