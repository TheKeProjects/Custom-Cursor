using System.Collections.Generic;

namespace CustomCursor.Utilities
{
    public static class Constants
    {
        // Scheme.inf key -> value name under HKCU\Control Panel\Cursors.
        // Order matters: it is the positional order of the string stored in ...\Schemes.
        // Windows' own names are not the same as the labels in the Mouse control panel:
        // "NWPen" is Handwriting and "Hand" is Link Select.
        public static readonly Dictionary<string, string> RegistryNames = new()
        {
            { "pointer", "Arrow" },
            { "help", "Help" },
            { "work", "AppStarting" },
            { "busy", "Wait" },
            { "cross", "Crosshair" },
            { "text", "IBeam" },
            { "hand", "NWPen" },
            { "unavailiable", "No" },
            { "vert", "SizeNS" },
            { "horz", "SizeWE" },
            { "dgn1", "SizeNWSE" },
            { "dgn2", "SizeNESW" },
            { "move", "SizeAll" },
            { "alternate", "UpArrow" },
            { "link", "Hand" },
            { "pin", "Pin" },
            { "person", "Person" }
        };

        // User-friendly labels for cursor types
        public static readonly Dictionary<string, string> CursorLabels = new()
        {
            { "pointer", "Normal" },
            { "help", "Help" },
            { "work", "Working in Background" },
            { "busy", "Busy" },
            { "cross", "Precision Select" },
            { "text", "Text Select" },
            { "hand", "Handwriting" },
            { "unavailiable", "Unavailable" },
            { "vert", "Vertical Resize" },
            { "horz", "Horizontal Resize" },
            { "dgn1", "Diagonal Resize 1" },
            { "dgn2", "Diagonal Resize 2" },
            { "move", "Move" },
            { "alternate", "Alternate Select" },
            { "link", "Link" },
            { "pin", "Location Select" },
            { "person", "Person Select" }
        };

        // Registry paths
        public const string CursorsRegistryPath = @"Control Panel\Cursors";
        public const string CursorSchemesRegistryPath = @"Control Panel\Cursors\Schemes";

        // SystemParametersInfo constants
        public const uint SPI_SETCURSORS = 0x0057;
        public const uint SPIF_UPDATEINIFILE = 0x01;
        public const uint SPIF_SENDCHANGE = 0x02;

        // Cursor folder name
        public const string CursorFolderName = "Cursores";
        public const string SchemeFileName = "scheme.inf";

        // Tutorial folder name
        public const string TutorialFolderName = "Tutorial";

        // Preview image settings
        public const int PreviewImageSize = 64;
        public const int DetailedCursorImageSize = 48;
    }
}
