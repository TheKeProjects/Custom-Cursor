using System;
using System.Diagnostics;

namespace CustomCursor.Services
{
    /// <summary>
    /// Provides methods to interact with Windows cursor settings
    /// </summary>
    public static class WindowsCursorTool
    {
        /// <summary>
        /// Opens the Windows Mouse Properties dialog to the Pointers tab
        /// </summary>
        public static void OpenCursorSettings()
        {
            try
            {
                // Open Mouse Properties, specifically the Pointers tab (index 1)
                Process.Start(new ProcessStartInfo
                {
                    FileName = "control",
                    Arguments = "main.cpl,,1",
                    UseShellExecute = true
                });
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException("Failed to open Windows cursor settings", ex);
            }
        }
    }
}
