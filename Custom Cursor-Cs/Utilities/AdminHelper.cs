using System;
using System.Diagnostics;
using System.Security.Principal;

namespace CustomCursor.Utilities
{
    public static class AdminHelper
    {
        /// <summary>
        /// Checks if the application is running with administrator privileges
        /// </summary>
        public static bool IsAdministrator()
        {
            try
            {
                using WindowsIdentity identity = WindowsIdentity.GetCurrent();
                WindowsPrincipal principal = new WindowsPrincipal(identity);
                return principal.IsInRole(WindowsBuiltInRole.Administrator);
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Restarts the application with administrator privileges
        /// </summary>
        public static void RestartAsAdministrator()
        {
            try
            {
                ProcessStartInfo startInfo = new ProcessStartInfo
                {
                    FileName = Environment.ProcessPath ?? Process.GetCurrentProcess().MainModule?.FileName,
                    UseShellExecute = true,
                    Verb = "runas" // Request elevation
                };

                Process.Start(startInfo);
                Environment.Exit(0);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException("Failed to restart with administrator privileges", ex);
            }
        }
    }
}
