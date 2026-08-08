using System.Collections.Generic;

namespace CustomCursor.Models
{
    /// <summary>
    /// Represents a complete cursor scheme/set
    /// </summary>
    public class CursorScheme
    {
        public string Name { get; set; } = string.Empty;
        public string Path { get; set; } = string.Empty;
        public string RelativePath { get; set; } = string.Empty;
        public string? PreviewImagePath { get; set; }
        public Dictionary<string, string> CursorMap { get; set; } = new();
        public string Category { get; set; } = "General";
    }
}
