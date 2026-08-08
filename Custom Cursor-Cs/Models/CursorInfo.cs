namespace CustomCursor.Models
{
    /// <summary>
    /// Represents information about an individual cursor file
    /// </summary>
    public class CursorInfo
    {
        public string Name { get; set; } = string.Empty;
        public string FileName { get; set; } = string.Empty;
        public string Key { get; set; } = string.Empty;
        public object? Image { get; set; }  // Not used in WPF version
    }
}
