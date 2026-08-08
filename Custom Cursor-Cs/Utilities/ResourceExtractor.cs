using System;
using System.IO;

namespace CustomCursor.Utilities
{
    public static class ResourceExtractor
    {
        /// <summary>
        /// Extracts embedded resources to the working directory when running as exe
        /// </summary>
        public static void ExtractResourcesIfNeeded()
        {
            if (!PathHelper.IsRunningAsSingleFileExe())
            {
                // In development mode, files are already in the correct location
                return;
            }

            string workingDir = PathHelper.GetWorkingDirectory();

            // Extract Cursores folder
            ExtractFolderFromAssembly("Cursores", workingDir);

            // Extract Tutorial folder
            ExtractFolderFromAssembly("Tutorial", workingDir);

            // Extract icon
            ExtractFileFromAssembly("icon.ico", workingDir);
        }

        /// <summary>
        /// Extracts a folder from embedded resources to a target directory
        /// </summary>
        private static void ExtractFolderFromAssembly(string folderName, string targetBaseDir)
        {
            string targetFolder = Path.Combine(targetBaseDir, folderName);

            // Check if already extracted (has files)
            if (Directory.Exists(targetFolder) && Directory.GetFileSystemEntries(targetFolder).Length > 0)
            {
                // Already extracted
                return;
            }

            // This is only ever called when running as a single-file exe (see
            // ExtractResourcesIfNeeded), so the app's own base directory is where
            // the bundled folder lives - no need for Assembly.Location, which is
            // always empty for single-file apps anyway.
            string sourceFolder = Path.Combine(AppContext.BaseDirectory, folderName);
            if (Directory.Exists(sourceFolder))
            {
                Directory.CreateDirectory(targetFolder);
                CopyDirectory(sourceFolder, targetFolder);
            }
        }

        /// <summary>
        /// Extracts a single file from embedded resources
        /// </summary>
        private static void ExtractFileFromAssembly(string fileName, string targetDir)
        {
            string targetPath = Path.Combine(targetDir, fileName);

            // Check if already extracted
            if (File.Exists(targetPath))
            {
                return;
            }

            // Only ever called when running as a single-file exe, so look for the
            // bundled file next to the app's own base directory.
            string sourceFile = Path.Combine(AppContext.BaseDirectory, fileName);
            if (File.Exists(sourceFile))
            {
                File.Copy(sourceFile, targetPath, true);
            }
        }

        /// <summary>
        /// Recursively copies a directory and its contents
        /// </summary>
        private static void CopyDirectory(string sourceDir, string targetDir)
        {
            // Create target directory
            Directory.CreateDirectory(targetDir);

            // Copy files
            foreach (string file in Directory.GetFiles(sourceDir))
            {
                string fileName = Path.GetFileName(file);
                string targetFile = Path.Combine(targetDir, fileName);
                File.Copy(file, targetFile, true);
            }

            // Recursively copy subdirectories
            foreach (string subDir in Directory.GetDirectories(sourceDir))
            {
                string dirName = Path.GetFileName(subDir);
                string targetSubDir = Path.Combine(targetDir, dirName);
                CopyDirectory(subDir, targetSubDir);
            }
        }
    }
}
