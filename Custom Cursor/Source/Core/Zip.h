#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Zip archives, read the way System.IO.Compression.ZipFile.OpenRead / ZipArchiveEntry.ExtractToFile
// do, without loading the archive into memory: only the central directory is read up front, and an
// entry is inflated straight from the file to its destination (mod .pak files are often hundreds
// of MB). Stored, Deflate and Deflate64 entries; ZIP64 sizes and offsets; names in UTF-8 (or CP437
// when they aren't valid UTF-8).
namespace CustomCursor::Zip {

struct Entry {
    std::wstring fullName;   // ZipArchiveEntry.FullName: "Paks/~mods/mod_P.pak" (as stored)
    std::wstring name;       // ZipArchiveEntry.Name: the last segment, "" for a folder entry
    uint64_t compressedSize = 0;
    uint64_t length = 0;     // uncompressed size
    uint16_t method = 0;     // 0 stored, 8 deflate, 9 deflate64
    uint16_t flags = 0;      // general purpose bit flag
    uint32_t crc32 = 0;
    uint16_t dosTime = 0, dosDate = 0;   // LastWriteTime (local)
    uint64_t localHeaderOffset = 0;
};

// Why something failed (the services turn it into the user's message)
enum class Error {
    None,
    OpenFailed,     // the archive can't be opened / read (win32 has the system error)
    NotAZip,        // no End Of Central Directory record
    Corrupt,        // a damaged header or compressed data that can't be decoded
    BadCrc,         // the data decodes but isn't what was stored (CRC-32 or size mismatch)
    Unsupported,    // a compression method other than stored / deflate / deflate64
    Encrypted,      // password-protected entry
    WriteFailed,    // the destination file can't be created / written (win32 has the system error)
};
struct Failure {
    Error error = Error::None;
    unsigned long win32 = 0;   // GetLastError() for OpenFailed / WriteFailed
};

class Reader {
public:
    ~Reader();
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    // ZipFile.OpenRead: reads the central directory; null (and `failure`) when it can't
    static std::unique_ptr<Reader> Open(const std::wstring& path, Failure* failure = nullptr);

    const std::vector<Entry>& Entries() const { return entries; }

    // ZipArchiveEntry.ExtractToFile(destination, overwrite: true): creates or replaces the file,
    // streams the entry into it and sets its last write time. `progress` gets the bytes written so
    // far (any thread). False (and `failure`) on error; a partly written file stays, as in .NET.
    bool ExtractToFile(const Entry& entry, const std::wstring& destination, Failure* failure = nullptr,
                       const std::function<void(uint64_t written)>& progress = {}) const;
    // The entry's data, decoded, handed to `sink` a piece at a time (any size). False on failure
    // (`failure`), or when `sink` returns false to stop early (`failure` then None). The CRC and
    // the size are checked once everything was read.
    bool ReadEntry(const Entry& entry, const std::function<bool(const uint8_t* data, size_t size)>& sink, Failure* failure = nullptr) const;

private:
    Reader() = default;
    bool ReadAt(uint64_t offset, void* buffer, size_t size) const;
    bool ReadDirectory(Failure& failure);

    void* file = nullptr;   // HANDLE
    uint64_t fileSize = 0;
    std::vector<Entry> entries;
};

// The CRC-32 of `data` continuing from `crc` (0 to start)
uint32_t Crc32(const uint8_t* data, size_t size, uint32_t crc = 0);

// Streaming raw DEFLATE (RFC 1951) / Deflate64 decoder. `read` fills a buffer with compressed bytes
// (returns how many; 0 at the end), `write` takes decompressed bytes (false stops). False when the
// data is corrupt or `write` refused.
bool Inflate(const std::function<size_t(uint8_t* buffer, size_t size)>& read,
             const std::function<bool(const uint8_t* data, size_t size)>& write, bool deflate64 = false);

} // namespace CustomCursor::Zip
