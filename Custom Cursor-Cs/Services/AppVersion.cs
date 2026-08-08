namespace CustomCursor.Services
{
    public static class AppVersion
    {
        public const string CURRENT_VERSION = "2.0.2";

        public const string GITHUB_OWNER  = "TheKeProjects";
        public const string GITHUB_REPO   = "Custom-Cursor";
        public const string GITHUB_BRANCH = "main";

        // GameBanana tool ID for this app (0 = disabled)
        public static readonly int GB_TOOL_ID = 0;

        public static string GetDisplayVersion() => $"v{CURRENT_VERSION}";
        public static string GetGitHubRepoUrl()  => $"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}";
    }
}
