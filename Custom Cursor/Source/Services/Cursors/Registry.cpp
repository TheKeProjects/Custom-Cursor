#include "Services/Cursors/Registry.h"
#include "Core/Text.h"

namespace CustomCursor::Services::Cursors {

std::wstring ErrorText(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                                   reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring text = n && buffer ? Text::Trim(std::wstring(buffer, n)) : L"Unknown error (" + std::to_wstring(code) + L")";
    if (buffer) LocalFree(buffer);
    return text;
}

namespace {

[[noreturn]] void Fail(LSTATUS status) { throw CursorError{ ErrorText(static_cast<DWORD>(status)), L"" }; }

HKEY root = HKEY_CURRENT_USER;

} // namespace

bool UseUserHive(const std::wstring& sid) {
    HKEY hive = nullptr;
    if (RegOpenKeyExW(HKEY_USERS, sid.c_str(), 0, KEY_READ | KEY_WRITE, &hive) != ERROR_SUCCESS) return false;
    root = hive;   // kept open for the rest of the (short-lived) elevated process
    return true;
}

RegistryKey RegistryKey::Open(const std::wstring& path, bool writable) {
    HKEY handle = nullptr;
    const LSTATUS status = RegOpenKeyExW(root, path.c_str(), 0, writable ? KEY_READ | KEY_WRITE : KEY_READ, &handle);
    if (status == ERROR_SUCCESS) return RegistryKey(handle);
    if (status == ERROR_FILE_NOT_FOUND) return {};
    Fail(status);   // OpenSubKey throws on access denied
}

RegistryKey RegistryKey::Create(const std::wstring& path) {
    HKEY handle = nullptr;
    const LSTATUS status = RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS) Fail(status);
    return RegistryKey(handle);
}

RegistryKey& RegistryKey::operator=(RegistryKey&& other) noexcept {
    if (this != &other) {
        if (key) RegCloseKey(key);
        key = other.key;
        other.key = nullptr;
    }
    return *this;
}

RegistryKey::~RegistryKey() {
    if (key) RegCloseKey(key);
}

std::optional<std::wstring> RegistryKey::GetString(const std::wstring& name) const {
    if (!key) return std::nullopt;
    // RRF_RT_REG_SZ takes REG_EXPAND_SZ too, expanded
    DWORD size = 0;
    if (RegGetValueW(key, nullptr, name.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return std::nullopt;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::wstring value(size / sizeof(wchar_t) + 1, L'\0');
        DWORD bytes = static_cast<DWORD>(value.size() * sizeof(wchar_t));
        const LSTATUS status = RegGetValueW(key, nullptr, name.c_str(), RRF_RT_REG_SZ, nullptr, value.data(), &bytes);
        if (status == ERROR_MORE_DATA) {
            size = bytes;
            continue;
        }
        if (status != ERROR_SUCCESS) return std::nullopt;
        value.resize(wcsnlen(value.c_str(), value.size()));
        return value;
    }
    return std::nullopt;
}

bool RegistryKey::HasValue(const std::wstring& name) const {
    return key && RegQueryValueExW(key, name.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

std::vector<std::wstring> RegistryKey::ValueNames() const {
    std::vector<std::wstring> names;
    if (!key) return names;
    DWORD count = 0, longest = 0;
    if (RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &count, &longest, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
        return names;
    std::wstring buffer(longest + 2, L'\0');
    for (DWORD i = 0; i < count; ++i) {
        DWORD length = static_cast<DWORD>(buffer.size());
        if (RegEnumValueW(key, i, buffer.data(), &length, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) names.emplace_back(buffer.c_str(), length);
    }
    return names;
}

void RegistryKey::SetString(const std::wstring& name, const std::wstring& value, bool expand) {
    const LSTATUS status = RegSetValueExW(key, name.c_str(), 0, expand ? REG_EXPAND_SZ : REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    if (status != ERROR_SUCCESS) Fail(status);
}

void RegistryKey::SetDword(const std::wstring& name, DWORD value) {
    const LSTATUS status = RegSetValueExW(key, name.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    if (status != ERROR_SUCCESS) Fail(status);
}

void RegistryKey::DeleteValue(const std::wstring& name) {
    const LSTATUS status = RegDeleteValueW(key, name.c_str());
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) Fail(status);
}

} // namespace CustomCursor::Services::Cursors
