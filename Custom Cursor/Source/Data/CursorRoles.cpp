#include "Data/CursorRoles.h"

namespace CustomCursor::Data::CursorRoles {

namespace {

constexpr Role kRoles[] = {
    { L"pointer", L"Arrow", "cursor_label_pointer" },               // Normal
    { L"help", L"Help", "cursor_label_help" },                      // Help
    { L"work", L"AppStarting", "cursor_label_work" },               // Working in Background
    { L"busy", L"Wait", "cursor_label_busy" },                      // Busy
    { L"cross", L"Crosshair", "cursor_label_cross" },               // Precision Select
    { L"text", L"IBeam", "cursor_label_text" },                     // Text Select
    { L"hand", L"NWPen", "cursor_label_hand" },                     // Handwriting
    { L"unavailiable", L"No", "cursor_label_unavailiable" },        // Unavailable
    { L"vert", L"SizeNS", "cursor_label_vert" },                    // Vertical Resize
    { L"horz", L"SizeWE", "cursor_label_horz" },                    // Horizontal Resize
    { L"dgn1", L"SizeNWSE", "cursor_label_dgn1" },                  // Diagonal Resize 1
    { L"dgn2", L"SizeNESW", "cursor_label_dgn2" },                  // Diagonal Resize 2
    { L"move", L"SizeAll", "cursor_label_move" },                   // Move
    { L"alternate", L"UpArrow", "cursor_label_alternate" },         // Alternate Select
    { L"link", L"Hand", "cursor_label_link" },                      // Link
    { L"pin", L"Pin", "cursor_label_pin" },                         // Location Select
    { L"person", L"Person", "cursor_label_person" },                // Person Select
};

} // namespace

std::span<const Role> All() { return kRoles; }

std::string LabelKey(const std::wstring& infKey) {
    for (const Role& role : kRoles)
        if (infKey == role.infKey) return role.labelKey;
    return {};
}

} // namespace CustomCursor::Data::CursorRoles
