using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using CustomCursor.Utilities;

namespace CustomCursor.Services
{
    /// <summary>
    /// Manages tutorial videos
    /// </summary>
    public class TutorialManager
    {
        private static readonly string[] VideoExtensions = { ".mp4", ".webm", ".avi", ".mov", ".mkv", ".wmv" };

        /// <summary>
        /// Gets all video files from the tutorial directory
        /// </summary>
        public List<string> GetTutorialVideos()
        {
            string tutorialDir = PathHelper.GetTutorialDirectory();

            if (!Directory.Exists(tutorialDir))
            {
                return new List<string>();
            }

            try
            {
                var videoFiles = Directory.GetFiles(tutorialDir, "*.*", SearchOption.AllDirectories)
                    .Where(file => VideoExtensions.Contains(Path.GetExtension(file).ToLowerInvariant()))
                    .OrderBy(file => Path.GetFileName(file))
                    .ToList();

                return videoFiles;
            }
            catch (Exception)
            {
                return new List<string>();
            }
        }

        /// <summary>
        /// Gets the first available tutorial video
        /// </summary>
        public string? GetFirstTutorialVideo()
        {
            var videos = GetTutorialVideos();
            return videos.FirstOrDefault();
        }

        /// <summary>
        /// Checks if any tutorial videos exist
        /// </summary>
        public bool HasTutorialVideos()
        {
            return GetTutorialVideos().Count > 0;
        }

        /// <summary>
        /// Gets tutorial videos grouped by subdirectory
        /// </summary>
        public Dictionary<string, List<string>> GetTutorialVideosByCategory()
        {
            string tutorialDir = PathHelper.GetTutorialDirectory();
            var videos = GetTutorialVideos();
            var categorized = new Dictionary<string, List<string>>();

            foreach (var video in videos)
            {
                string relativePath = Path.GetRelativePath(tutorialDir, video);
                string category = Path.GetDirectoryName(relativePath) ?? "Root";

                if (category == ".")
                    category = "Root";

                if (!categorized.ContainsKey(category))
                {
                    categorized[category] = new List<string>();
                }

                categorized[category].Add(video);
            }

            return categorized;
        }
    }
}
