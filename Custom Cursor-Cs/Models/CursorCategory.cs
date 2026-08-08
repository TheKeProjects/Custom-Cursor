using System.Collections.Generic;

namespace CustomCursor.Models
{
    /// <summary>
    /// Represents a category of cursor schemes
    /// </summary>
    public class CursorCategory
    {
        public string Name { get; set; } = string.Empty;
        public List<CursorScheme> Schemes { get; set; } = new();
    }
}
