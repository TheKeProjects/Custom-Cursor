#include "Core/Zip.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace CustomCursor::Zip {

uint32_t Crc32(const uint8_t* data, size_t size, uint32_t crc) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

namespace {

constexpr uint32_t kLocalHeader = 0x04034b50, kCentralHeader = 0x02014b50, kEndOfDirectory = 0x06054b50;
constexpr uint32_t kZip64Locator = 0x07064b50, kZip64EndOfDirectory = 0x06064b50;

uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t U32(const uint8_t* p) { return static_cast<uint32_t>(U16(p)) | static_cast<uint32_t>(U16(p + 2)) << 16; }
uint64_t U64(const uint8_t* p) { return static_cast<uint64_t>(U32(p)) | static_cast<uint64_t>(U32(p + 4)) << 32; }

std::wstring Decode(const char* raw, int size, UINT codePage, DWORD flags) {
    if (size == 0) return {};
    const int n = MultiByteToWideChar(codePage, flags, raw, size, nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(codePage, flags, raw, size, out.data(), n);
    return out;
}

// UTF-8 when the language encoding flag says so, or when the bytes are valid UTF-8 anyway (.NET
// reads unflagged names as UTF-8); otherwise the original IBM PC code page 437
std::wstring DecodeName(const char* raw, int size, bool utf8Flag) {
    if (utf8Flag) return Decode(raw, size, CP_UTF8, 0);
    std::wstring name = Decode(raw, size, CP_UTF8, MB_ERR_INVALID_CHARS);
    if (name.empty() && size > 0) name = Decode(raw, size, 437, 0);
    return name;
}

} // namespace

Reader::~Reader() {
    if (file) CloseHandle(static_cast<HANDLE>(file));
}

std::unique_ptr<Reader> Reader::Open(const std::wstring& path, Failure* failure) {
    Failure local;
    Failure& f = failure ? *failure : local;
    f = {};
    std::unique_ptr<Reader> reader(new Reader());
    // FileMode.Open, FileAccess.Read, FileShare.Read (ZipFile.OpenRead)
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        f = { Error::OpenFailed, GetLastError() };
        return nullptr;
    }
    reader->file = h;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        f = { Error::OpenFailed, GetLastError() };
        return nullptr;
    }
    reader->fileSize = static_cast<uint64_t>(size.QuadPart);
    if (!reader->ReadDirectory(f)) return nullptr;
    return reader;
}

bool Reader::ReadAt(uint64_t offset, void* buffer, size_t size) const {
    if (offset + size > fileSize) return false;
    auto* out = static_cast<uint8_t*>(buffer);
    while (size > 0) {
        OVERLAPPED at{};
        at.Offset = static_cast<DWORD>(offset);
        at.OffsetHigh = static_cast<DWORD>(offset >> 32);
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size, 1u << 24));
        DWORD got = 0;
        if (!ReadFile(static_cast<HANDLE>(file), out, chunk, &got, &at) || got == 0) return false;
        out += got;
        offset += got;
        size -= got;
    }
    return true;
}

bool Reader::ReadDirectory(Failure& f) {
    // End Of Central Directory: searched backwards over the last 64 KB (the comment can be that long)
    const uint64_t tail = std::min<uint64_t>(fileSize, 22 + 65535);
    std::vector<uint8_t> end(static_cast<size_t>(tail));
    if (tail < 22 || !ReadAt(fileSize - tail, end.data(), end.size())) {
        f = { Error::NotAZip, 0 };
        return false;
    }
    size_t eocd = std::string::npos;
    for (size_t i = end.size() - 22 + 1; i-- > 0;) {
        if (U32(&end[i]) == kEndOfDirectory) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) {
        f = { Error::NotAZip, 0 };
        return false;
    }
    uint64_t count = U16(&end[eocd + 10]);
    uint64_t directorySize = U32(&end[eocd + 12]);
    uint64_t directoryOffset = U32(&end[eocd + 16]);
    // ZIP64: the locator just before the record points at the 64-bit End Of Central Directory
    const uint64_t eocdOffset = fileSize - tail + eocd;
    uint8_t locator[20];
    if (eocdOffset >= 20 && ReadAt(eocdOffset - 20, locator, 20) && U32(locator) == kZip64Locator) {
        uint8_t record[56];
        if (!ReadAt(U64(locator + 8), record, 56) || U32(record) != kZip64EndOfDirectory) {
            f = { Error::Corrupt, 0 };
            return false;
        }
        count = U64(record + 32);
        directorySize = U64(record + 40);
        directoryOffset = U64(record + 48);
    }
    if (directorySize > fileSize || directoryOffset > fileSize - directorySize) {
        f = { Error::Corrupt, 0 };
        return false;
    }
    std::vector<uint8_t> dir(static_cast<size_t>(directorySize));
    if (!dir.empty() && !ReadAt(directoryOffset, dir.data(), dir.size())) {
        f = { Error::Corrupt, 0 };
        return false;
    }

    size_t pos = 0;
    for (uint64_t i = 0; i < count; ++i) {
        if (pos + 46 > dir.size() || U32(&dir[pos]) != kCentralHeader) {
            f = { Error::Corrupt, 0 };
            return false;
        }
        const uint8_t* h = &dir[pos];
        Entry e;
        e.flags = U16(h + 8);
        e.method = U16(h + 10);
        e.dosTime = U16(h + 12);
        e.dosDate = U16(h + 14);
        e.crc32 = U32(h + 16);
        e.compressedSize = U32(h + 20);
        e.length = U32(h + 24);
        const uint16_t nameLen = U16(h + 28), extraLen = U16(h + 30), commentLen = U16(h + 32);
        e.localHeaderOffset = U32(h + 42);
        if (pos + 46 + nameLen + extraLen + commentLen > dir.size()) {
            f = { Error::Corrupt, 0 };
            return false;
        }
        e.fullName = DecodeName(reinterpret_cast<const char*>(h + 46), nameLen, (e.flags & 0x800) != 0);
        // ZIP64 extra field (0x0001): the 64-bit values of the fields saturated at 0xFFFFFFFF, in order
        for (size_t x = 0; x + 4 <= extraLen;) {
            const uint8_t* field = h + 46 + nameLen + x;
            const uint16_t id = U16(field), size = U16(field + 2);
            if (x + 4 + size > extraLen) break;
            if (id == 0x0001) {
                const uint8_t* v = field + 4;
                const uint8_t* stop = v + size;
                if (e.length == 0xFFFFFFFF && v + 8 <= stop) { e.length = U64(v); v += 8; }
                if (e.compressedSize == 0xFFFFFFFF && v + 8 <= stop) { e.compressedSize = U64(v); v += 8; }
                if (e.localHeaderOffset == 0xFFFFFFFF && v + 8 <= stop) e.localHeaderOffset = U64(v);
            }
            x += 4u + size;
        }
        // ZipArchiveEntry.Name: Path.GetFileName of the full name (either separator)
        const size_t slash = e.fullName.find_last_of(L"/\\");
        e.name = slash == std::wstring::npos ? e.fullName : e.fullName.substr(slash + 1);
        entries.push_back(std::move(e));
        pos += 46u + nameLen + extraLen + commentLen;
    }
    return true;
}

bool Reader::ReadEntry(const Entry& entry, const std::function<bool(const uint8_t*, size_t)>& sink, Failure* failure) const {
    Failure local;
    Failure& f = failure ? *failure : local;
    f = {};
    if (entry.flags & 1) {
        f = { Error::Encrypted, 0 };
        return false;
    }
    if (entry.method != 0 && entry.method != 8 && entry.method != 9) {
        f = { Error::Unsupported, 0 };
        return false;
    }
    // The data starts after the local header (its name / extra lengths can differ from the central one)
    uint8_t header[30];
    if (!ReadAt(entry.localHeaderOffset, header, 30) || U32(header) != kLocalHeader) {
        f = { Error::Corrupt, 0 };
        return false;
    }
    const uint64_t start = entry.localHeaderOffset + 30 + U16(header + 26) + U16(header + 28);
    if (start > fileSize || entry.compressedSize > fileSize - start) {
        f = { Error::Corrupt, 0 };
        return false;
    }

    uint64_t remaining = entry.compressedSize, readAt = start, written = 0;
    uint32_t crc = 0;
    bool stopped = false;
    const auto read = [&](uint8_t* buffer, size_t size) -> size_t {
        const size_t n = static_cast<size_t>(std::min<uint64_t>(remaining, size));
        if (n == 0 || !ReadAt(readAt, buffer, n)) return 0;
        readAt += n;
        remaining -= n;
        return n;
    };
    const auto write = [&](const uint8_t* data, size_t size) -> bool {
        crc = Crc32(data, size, crc);
        written += size;
        if (!sink(data, size)) {
            stopped = true;
            return false;
        }
        return true;
    };

    bool ok = false;
    if (entry.method == 0) {
        // Up to 1 MB at a time (not zero-filled: archives can hold thousands of tiny entries)
        const size_t size = static_cast<size_t>(std::min<uint64_t>(remaining, 1u << 20));
        const auto buffer = std::make_unique_for_overwrite<uint8_t[]>(std::max<size_t>(size, 1));
        ok = true;
        while (ok && remaining > 0) {
            const size_t n = read(buffer.get(), size);
            ok = n > 0 && write(buffer.get(), n);
        }
    } else {
        ok = Inflate(read, write, entry.method == 9);
    }
    if (stopped) return false;   // the sink had enough (or failed): its call
    if (!ok) {
        f = { Error::Corrupt, 0 };
        return false;
    }
    // .NET didn't check these: a damaged .pak went into Paks without a word
    if (written != entry.length || crc != entry.crc32) {
        f = { Error::BadCrc, 0 };
        return false;
    }
    return true;
}

bool Reader::ExtractToFile(const Entry& entry, const std::wstring& destination, Failure* failure,
                           const std::function<void(uint64_t)>& progress) const {
    Failure local;
    Failure& f = failure ? *failure : local;
    f = {};
    if (entry.flags & 1) {
        f = { Error::Encrypted, 0 };
        return false;
    }
    if (entry.method != 0 && entry.method != 8 && entry.method != 9) {
        f = { Error::Unsupported, 0 };
        return false;
    }

    // FileMode.Create, FileShare.None: replaces an existing file (a read-only one fails, as in .NET)
    HANDLE out = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        f = { Error::WriteFailed, GetLastError() };
        return false;
    }

    uint64_t written = 0;
    DWORD writeError = 0;
    const bool ok = ReadEntry(entry, [&](const uint8_t* data, size_t size) {
        while (size > 0) {
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size, 1u << 24));
            DWORD done = 0;
            if (!WriteFile(out, data, chunk, &done, nullptr) || done == 0) {
                writeError = GetLastError();
                return false;
            }
            written += done;
            data += done;
            size -= done;
        }
        if (progress) progress(written);
        return true;
    }, &f);
    if (!ok && writeError) f = { Error::WriteFailed, writeError };

    if (ok) {
        // File.SetLastWriteTime(destination, entry.LastWriteTime): the DOS time is local, converted
        // with the offset of that date (not today's daylight saving bias)
        FILETIME localTime{}, utc{};
        SYSTEMTIME localParts{}, universal{};
        if (DosDateTimeToFileTime(entry.dosDate, entry.dosTime, &localTime) && FileTimeToSystemTime(&localTime, &localParts) &&
            TzSpecificLocalTimeToSystemTime(nullptr, &localParts, &universal) && SystemTimeToFileTime(&universal, &utc))
            SetFileTime(out, nullptr, nullptr, &utc);
    }
    CloseHandle(out);
    return ok;
}

} // namespace CustomCursor::Zip
