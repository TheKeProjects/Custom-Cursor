#include "Services/Cursors/AniParser.h"
#include <cstdint>
#include <cstring>

namespace CustomCursor::Services::Cursors::AniParser {

namespace {

// A FileStream + BinaryReader over the bytes: an int read past the end fails (EndOfStreamException),
// the position may be set past the end (reads then fail), a negative one fails
struct Reader {
    const std::string& data;
    long long pos = 0;
    bool failed = false;

    long long Length() const { return static_cast<long long>(data.size()); }
    // ReadChars(4): fewer characters at the end of the stream (no exception)
    std::string Chars4() {
        if (pos >= Length()) return {};
        std::string s = data.substr(static_cast<size_t>(pos), 4);
        pos += static_cast<long long>(s.size());
        return s;
    }
    int32_t Int() {
        if (pos < 0 || pos + 4 > Length()) {
            failed = true;
            return 0;
        }
        int32_t v = 0;
        std::memcpy(&v, data.data() + pos, 4);
        pos += 4;
        return v;
    }
    // ReadBytes(count): what's left when there are fewer (no exception), a negative count throws
    std::string Bytes(int32_t count) {
        if (count < 0) {
            failed = true;
            return {};
        }
        const long long available = pos >= Length() ? 0 : Length() - pos;
        const long long n = count < available ? count : available;
        std::string s = n > 0 ? data.substr(static_cast<size_t>(pos), static_cast<size_t>(n)) : std::string();
        pos += n;
        return s;
    }
    void Seek(long long to) {
        if (to < 0) failed = true;
        else pos = to;
    }
};

} // namespace

std::optional<AniData> Parse(const std::string& bytes) {
    AniData data;
    Reader br{ bytes };
    if (br.Chars4() != "RIFF") return data;
    br.Int();   // file size
    if (br.failed) return std::nullopt;
    if (br.Chars4() != "ACON") return data;

    while (br.pos < br.Length() - 8) {
        const std::string chunkType = br.Chars4();
        const int32_t chunkSize = br.Int();
        if (br.failed) return std::nullopt;
        const long long chunkEnd = br.pos + chunkSize;

        if (chunkType == "anih") {
            br.Int();   // header size
            data.frameCount = br.Int();
            br.Int();   // steps
            br.Int();   // width
            br.Int();   // height
            br.Int();   // bit count
            br.Int();   // planes
            const int32_t rate = br.Int();   // display rate in 1/60th second units
            data.defaultRateMs = static_cast<int32_t>(static_cast<uint32_t>(rate) * 1000u) / 60;
            br.Int();   // flags
        } else if (chunkType == "LIST") {
            if (br.Chars4() == "fram") {
                // Frame list: the icon chunks
                while (!br.failed && br.pos < chunkEnd - 8) {
                    const std::string iconType = br.Chars4();
                    const int32_t iconSize = br.Int();
                    if (br.failed) break;
                    if (iconType == "icon") data.frames.push_back(br.Bytes(iconSize));
                    else br.Seek(br.pos + iconSize);
                    // RIFF pads a chunk of odd size with one zero byte (the C# didn't skip it and
                    // misread every frame after such a chunk)
                    if (iconSize % 2 != 0 && br.pos < br.Length() && bytes[static_cast<size_t>(br.pos)] == 0) ++br.pos;
                }
            }
        }
        if (br.failed) return std::nullopt;

        // Move to the next chunk, past the pad byte of an odd size
        br.Seek(chunkEnd);
        if (br.failed) return std::nullopt;
        if (chunkSize % 2 != 0 && br.pos < br.Length()) ++br.pos;
    }
    return data;
}

} // namespace CustomCursor::Services::Cursors::AniParser
