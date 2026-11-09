#pragma once
#include <span>
#include <string>

// Utilities\Constants.cs's RegistryNames and CursorLabels as one table.
namespace CustomCursor::Data::CursorRoles {

struct Role {
    const wchar_t* infKey;         // the scheme.inf key ("pointer"; "unavailiable" is spelt as the .inf files spell it)
    const wchar_t* registryName;   // the value under HKCU\Control Panel\Cursors ("Arrow")
    const char* labelKey;          // the translated label ("cursor_label_pointer" = "Normal")
};

// In RegistryNames' order, which is part of the format: the positional order of the comma-separated
// string kept in ...\Cursors\Schemes. Windows' names aren't the Mouse control panel's labels:
// "NWPen" is Handwriting and "Hand" is Link Select.
std::span<const Role> All();

// The label key of an .inf key, or "" when it isn't a role (CursorLabels.ContainsKey)
std::string LabelKey(const std::wstring& infKey);

} // namespace CustomCursor::Data::CursorRoles
