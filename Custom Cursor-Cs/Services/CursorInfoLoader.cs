using System;
using System.Collections.Generic;
using System.IO;
using CustomCursor.Models;
using CustomCursor.Utilities;

namespace CustomCursor.Services
{
    /// <summary>
    /// Loads detailed cursor information for preview
    /// </summary>
    public class CursorInfoLoader
    {
        public CursorInfoLoader()
        {
        }

        /// <summary>
        /// Loads all cursor information from a scheme (WPF version - no image loading)
        /// </summary>
        public List<CursorInfo> LoadAllCursorImages(CursorScheme scheme)
        {
            var cursorInfos = new List<CursorInfo>();

            // Iterate through cursor types in order
            foreach (var kvp in Constants.RegistryNames)
            {
                string cursorKey = kvp.Key;

                if (scheme.CursorMap.ContainsKey(cursorKey))
                {
                    string fileName = scheme.CursorMap[cursorKey];
                    string filePath = Path.Combine(scheme.Path, fileName);

                    if (File.Exists(filePath))
                    {
                        try
                        {
                            cursorInfos.Add(new CursorInfo
                            {
                                Name = Constants.CursorLabels.ContainsKey(cursorKey)
                                    ? Constants.CursorLabels[cursorKey]
                                    : cursorKey,
                                FileName = fileName,
                                Key = cursorKey,
                                Image = null  // WPF doesn't use System.Drawing.Image
                            });
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"Error loading cursor {fileName}: {ex.Message}");
                        }
                    }
                }
            }

            return cursorInfos;
        }
    }
}
