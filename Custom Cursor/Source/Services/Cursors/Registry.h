#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <vector>

// The bits of Microsoft.Win32.Registry the cursor services use, on HKEY_CURRENT_USER. The writes
// throw CursorError like RegistryKey.SetValue / DeleteValue throw.
namespace CustomCursor::Services::Cursors {

// An exception of the cursor services: the message and the inner exception's ("" for none), as the
// C#'s errors read them (ex.Message, ex.InnerException?.Message)
struct CursorError {
    std::wstring message;
    std::wstring inner;
};

// The system's text for a Win32 error code (Win32Exception.Message)
std::wstring ErrorText(DWORD code);

// The hive the keys are opened in: HKEY_CURRENT_USER, or HKEY_USERS\<sid> for the elevated copy
// working for another account than the one that approved the prompt (false when that hive isn't loaded)
bool UseUserHive(const std::wstring& sid);

class RegistryKey {
public:
    // Registry.CurrentUser.OpenSubKey(path, writable): an empty key (!Valid) when it doesn't exist
    // (in the hive UseUserHive chose)
    static RegistryKey Open(const std::wstring& path, bool writable);
    // Registry.CurrentUser.CreateSubKey(path)
    static RegistryKey Create(const std::wstring& path);
    RegistryKey() = default;
    RegistryKey(RegistryKey&& other) noexcept : key(other.key) { other.key = nullptr; }
    RegistryKey& operator=(RegistryKey&& other) noexcept;
    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;
    ~RegistryKey();

    bool Valid() const { return key != nullptr; }

    // GetValue(name) as string: REG_SZ, or REG_EXPAND_SZ with the variables expanded (as .NET
    // reads it); nullopt for a missing value or another type
    std::optional<std::wstring> GetString(const std::wstring& name) const;
    // GetValue(name) != null
    bool HasValue(const std::wstring& name) const;
    // GetValueNames()
    std::vector<std::wstring> ValueNames() const;

    void SetString(const std::wstring& name, const std::wstring& value, bool expand = false);   // RegistryValueKind.String / ExpandString
    void SetDword(const std::wstring& name, DWORD value);
    // DeleteValue(name, throwOnMissingValue: false)
    void DeleteValue(const std::wstring& name);

private:
    explicit RegistryKey(HKEY handle) : key(handle) {}
    HKEY key = nullptr;
};

} // namespace CustomCursor::Services::Cursors
