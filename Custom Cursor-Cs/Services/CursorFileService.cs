using System;
using System.IO;

namespace CustomCursor.Services
{
    /// <summary>
    /// Handles file operations for cursor management
    /// </summary>
    public class CursorFileService
    {
        /// <summary>
        /// Copies cursor files to the Windows Cursors directory
        /// </summary>
        public void CopyCursorFiles(string sourcePath, string destinationPath)
        {
            if (!Directory.Exists(destinationPath))
            {
                Directory.CreateDirectory(destinationPath);
            }

            foreach (string file in Directory.GetFiles(sourcePath))
            {
                string fileName = Path.GetFileName(file);
                string destFile = Path.Combine(destinationPath, fileName);

                try
                {
                    File.Copy(file, destFile, true);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error copying {fileName}: {ex.Message}");
                }
            }
        }

        /// <summary>
        /// Copies a single cursor file
        /// </summary>
        public void CopyCursorFile(string sourceFile, string destinationFile)
        {
            try
            {
                string? destDir = Path.GetDirectoryName(destinationFile);
                if (destDir != null && !Directory.Exists(destDir))
                {
                    Directory.CreateDirectory(destDir);
                }

                File.Copy(sourceFile, destinationFile, true);
            }
            catch (Exception ex)
            {
                throw new IOException($"Failed to copy cursor file from {sourceFile} to {destinationFile}", ex);
            }
        }
    }
}
