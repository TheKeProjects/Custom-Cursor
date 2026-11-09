#include <algorithm>
#include <cstring>
#include <vector>
#include "Core/Zip.h"

// Streaming DEFLATE / Deflate64 decoder: compressed bytes come in through a 64 KB buffer, output
// goes through the 64 KB history window (flushed every time it wraps), so an entry of any size
// takes a fixed amount of memory. Huffman codes decode through a 10-bit lookup table, the longer
// ones bit by bit.
namespace CustomCursor::Zip {

namespace {

constexpr int kFastBits = 10;
constexpr size_t kWindow = 1u << 16;   // Deflate64's window (Deflate only needs 32 KB)

// Compressed input, read least significant bit first
class BitInput {
public:
    explicit BitInput(const std::function<size_t(uint8_t*, size_t)>& source) : read(source), buffer(1u << 16) {}

    // Past the end of the data zeros are fed in; Overrun() tells whether any of them were used
    void Need(int n) {
        while (count < n) {
            if (pos == end) Fill();
            uint64_t byte = 0;
            if (pos < end) byte = buffer[pos++];
            else fake += 8;
            bits |= byte << count;
            count += 8;
        }
    }
    uint32_t Peek(int n) {
        Need(n);
        return static_cast<uint32_t>(bits & ((1ull << n) - 1));
    }
    void Drop(int n) {
        bits >>= n;
        count -= n;
    }
    uint32_t Take(int n) {
        if (n == 0) return 0;
        const uint32_t v = Peek(n);
        Drop(n);
        return v;
    }
    void AlignByte() { Drop(count & 7); }
    bool Overrun() const { return fake > count; }

private:
    void Fill() {
        pos = 0;
        end = finished ? 0 : read(buffer.data(), buffer.size());
        if (end == 0) finished = true;
    }

    const std::function<size_t(uint8_t*, size_t)>& read;
    std::vector<uint8_t> buffer;
    size_t pos = 0, end = 0;
    bool finished = false;
    uint64_t bits = 0;
    int count = 0;
    int fake = 0;   // zero bits fed in after the end of the data
};

// Decompressed output: the history window, handed to `write` whenever it fills up
class Output {
public:
    explicit Output(const std::function<bool(const uint8_t*, size_t)>& sink) : write(sink), window(kWindow) {}

    void Put(uint8_t b) {
        window[pos++] = b;
        ++total;
        if (pos == kWindow) Flush();
    }
    // A back-reference `distance` bytes behind the end
    bool Copy(size_t distance, size_t length) {
        if (distance == 0 || distance > kWindow || distance > total) return false;
        for (size_t i = 0; i < length; ++i) Put(window[(pos + kWindow - distance) & (kWindow - 1)]);
        return ok;
    }
    bool Flush() {
        if (ok && pos > flushed) ok = write(window.data() + flushed, pos - flushed);
        if (pos == kWindow) pos = 0;
        flushed = pos;
        return ok;
    }
    bool Ok() const { return ok; }

private:
    const std::function<bool(const uint8_t*, size_t)>& write;
    std::vector<uint8_t> window;
    size_t pos = 0, flushed = 0;
    uint64_t total = 0;
    bool ok = true;
};

struct Huffman {
    uint16_t fast[1u << kFastBits]{};   // symbol << 4 | code length (0 = longer than kFastBits)
    uint16_t counts[16]{};
    uint16_t symbols[320]{};

    bool Build(const uint8_t* lengths, int n) {
        std::memset(fast, 0, sizeof(fast));
        std::memset(counts, 0, sizeof(counts));
        for (int i = 0; i < n; ++i) counts[lengths[i]]++;
        counts[0] = 0;
        // Over-subscribed code sets are corrupt (incomplete ones are allowed, e.g. a single distance)
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left = (left << 1) - counts[len];
            if (left < 0) return false;
        }
        uint16_t offsets[16]{};
        uint32_t next[16]{};
        uint32_t code = 0;
        for (int len = 1; len < 16; ++len) {
            offsets[len] = static_cast<uint16_t>(offsets[len - 1] + counts[len - 1]);
            code = (code + counts[len - 1]) << 1;
            next[len] = code;
        }
        for (int sym = 0; sym < n; ++sym) {
            const int len = lengths[sym];
            if (!len) continue;
            symbols[offsets[len]++] = static_cast<uint16_t>(sym);
            const uint32_t c = next[len]++;
            if (len > kFastBits) continue;
            // Codes are sent most significant bit first: the table is indexed by the reversed code
            uint32_t reversed = 0;
            for (int b = 0; b < len; ++b) reversed |= ((c >> b) & 1u) << (len - 1 - b);
            for (uint32_t k = reversed; k < (1u << kFastBits); k += 1u << len) fast[k] = static_cast<uint16_t>(sym << 4 | len);
        }
        return true;
    }

    int Decode(BitInput& in) const {
        if (const uint16_t e = fast[in.Peek(kFastBits)]) {
            in.Drop(e & 15);
            return e >> 4;
        }
        // Longer code: canonical decoding bit by bit
        const uint32_t bits = in.Peek(15);
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= static_cast<int>((bits >> (len - 1)) & 1u);
            const int count = counts[len];
            if (code - count < first) {
                in.Drop(len);
                return symbols[index + (code - first)];
            }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return -1;
    }
};

const uint16_t kLengthBase[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const uint8_t kLengthExtra[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
// 30 codes for Deflate; Deflate64 adds codes 30 and 31 (its 64 KB window)
const uint32_t kDistBase[] = { 1,    2,    3,    4,    5,    7,     9,     13,    17,    25,    33,   49,   65,   97,    129,  193,
                               257,  385,  513,  769,  1025, 1537,  2049,  3073,  4097,  6145,  8193, 12289, 16385, 24577, 32769, 49153 };
const uint8_t kDistExtra[] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14 };

bool Block(BitInput& in, Output& out, const Huffman& lit, const Huffman& dist, bool deflate64) {
    while (true) {
        const int sym = lit.Decode(in);
        if (sym < 0 || in.Overrun()) return false;
        if (sym < 256) {
            out.Put(static_cast<uint8_t>(sym));
            continue;
        }
        if (sym == 256) return out.Ok();
        const int li = sym - 257;
        if (li >= 29) return false;
        size_t length = 0;
        if (li == 28 && deflate64) length = 3 + in.Take(16);   // Deflate64: code 285 is 3 + 16 extra bits
        else length = kLengthBase[li] + in.Take(kLengthExtra[li]);
        const int ds = dist.Decode(in);
        if (ds < 0 || ds >= (deflate64 ? 32 : 30)) return false;
        const size_t distance = kDistBase[ds] + in.Take(kDistExtra[ds]);
        if (in.Overrun() || !out.Copy(distance, length)) return false;
    }
}

bool DynamicTables(BitInput& in, Huffman& lit, Huffman& dist) {
    const uint32_t hlit = in.Take(5) + 257, hdist = in.Take(5) + 1, hclen = in.Take(4) + 4;
    static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint8_t codeLengths[19]{};
    for (uint32_t i = 0; i < hclen; ++i) codeLengths[order[i]] = static_cast<uint8_t>(in.Take(3));
    Huffman lengthCode;
    if (!lengthCode.Build(codeLengths, 19)) return false;
    uint8_t lengths[320]{};
    uint32_t n = 0;
    while (n < hlit + hdist) {
        const int sym = lengthCode.Decode(in);
        if (sym < 0 || in.Overrun()) return false;
        if (sym < 16) {
            lengths[n++] = static_cast<uint8_t>(sym);
            continue;
        }
        uint32_t repeat = 0;
        uint8_t value = 0;
        if (sym == 16) {
            if (n == 0) return false;
            value = lengths[n - 1];
            repeat = 3 + in.Take(2);
        } else if (sym == 17) {
            repeat = 3 + in.Take(3);
        } else {
            repeat = 11 + in.Take(7);
        }
        if (n + repeat > hlit + hdist) return false;
        while (repeat--) lengths[n++] = value;
    }
    return lit.Build(lengths, static_cast<int>(hlit)) && dist.Build(lengths + hlit, static_cast<int>(hdist));
}

} // namespace

bool Inflate(const std::function<size_t(uint8_t*, size_t)>& read, const std::function<bool(const uint8_t*, size_t)>& write, bool deflate64) {
    BitInput in(read);
    Output out(write);

    static const auto fixed = [] {
        std::pair<Huffman, Huffman> tables;
        uint8_t lengths[288];
        std::fill(lengths, lengths + 144, static_cast<uint8_t>(8));
        std::fill(lengths + 144, lengths + 256, static_cast<uint8_t>(9));
        std::fill(lengths + 256, lengths + 280, static_cast<uint8_t>(7));
        std::fill(lengths + 280, lengths + 288, static_cast<uint8_t>(8));
        tables.first.Build(lengths, 288);
        uint8_t d[32];
        std::fill(std::begin(d), std::end(d), static_cast<uint8_t>(5));
        tables.second.Build(d, 32);
        return tables;
    }();

    bool last = false;
    do {
        last = in.Take(1) != 0;
        const uint32_t type = in.Take(2);
        if (in.Overrun()) return false;
        if (type == 0) {
            // Stored block: LEN, NLEN, then the bytes as they are
            in.AlignByte();
            const uint32_t len = in.Take(16), nlen = in.Take(16);
            if ((len ^ 0xFFFF) != nlen) return false;
            for (uint32_t i = 0; i < len; ++i) out.Put(static_cast<uint8_t>(in.Take(8)));
            if (in.Overrun() || !out.Ok()) return false;
        } else if (type == 1) {
            if (!Block(in, out, fixed.first, fixed.second, deflate64)) return false;
        } else if (type == 2) {
            Huffman lit, dist;
            if (!DynamicTables(in, lit, dist) || !Block(in, out, lit, dist, deflate64)) return false;
        } else {
            return false;
        }
    } while (!last);
    return out.Flush();
}

} // namespace CustomCursor::Zip
