#include "core/PcDatLzw.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <vector>

#include "core/PcGfx.h"  // pcLzwDecompress

namespace mm2 {

namespace fs = std::filesystem;

namespace {

// Generous upper bound for a single decompressed blob (guards the "plausible
// header" heuristic against reading garbage as a huge allocation). Every
// known dat/gfx blob is well under this.
constexpr size_t kMaxPlausibleSize = 4u * 1024u * 1024u;

std::optional<Bytes> tryFlatLzw(const Bytes& data) {
    if (data.size() < 5) return std::nullopt;  // u32 header + at least one LZW-coded byte
    uint32_t destSize = readU32LE(data.data());
    if (destSize == 0 || destSize > kMaxPlausibleSize) return std::nullopt;
    Bytes source(data.begin() + 4, data.end());
    Bytes dec = pcLzwDecompress(source, destSize);
    if (dec.size() != destSize) return std::nullopt;
    return dec;
}

// Per-slot table blob: u32 LE decompressed_size @ offset, LZW stream after.
// offset == 0 (empty slot) yields an empty Bytes.
Bytes decompressTableBlob(const Bytes& data, size_t offset) {
    if (offset == 0 || offset + 4 > data.size()) return {};
    uint32_t size = readU32LE(data.data() + offset);
    if (size == 0 || size > kMaxPlausibleSize) return {};
    Bytes source(data.begin() + static_cast<std::ptrdiff_t>(offset + 4), data.end());
    Bytes dec = pcLzwDecompress(source, size);
    return dec;
}

// Self-describing LE offset table (entry[0] == table byte size), same
// convention as the MONSTERS.4/.16 combat-atlas header. `entrySize` is 2
// (MAP.DAT) or 4 (EVENTSI.DAT/EVENTSO.DAT).
//
// When `expectedCount` is given, the count is taken directly from it rather
// than derived from entry[0] -- needed for EVENTSI.DAT/EVENTSO.DAT, where
// slot 0 may legitimately be empty (offset 0), unlike MAP.DAT.
std::optional<std::vector<uint32_t>> readOffsetTable(const Bytes& data, int entrySize,
                                                      std::optional<int> expectedCount) {
    auto readEntry = [&](size_t i) -> uint32_t {
        const uint8_t* p = data.data() + i * static_cast<size_t>(entrySize);
        return entrySize == 2 ? static_cast<uint32_t>(readU16LE(p)) : readU32LE(p);
    };

    int count;
    if (expectedCount.has_value()) {
        count = *expectedCount;
        if (static_cast<size_t>(count) * static_cast<size_t>(entrySize) > data.size()) return std::nullopt;
    } else {
        if (data.size() < static_cast<size_t>(entrySize)) return std::nullopt;
        uint32_t header = readEntry(0);
        if (header == 0 || header % static_cast<uint32_t>(entrySize) != 0) return std::nullopt;
        count = static_cast<int>(header / static_cast<uint32_t>(entrySize));
        if (count <= 0 || static_cast<size_t>(count) * static_cast<size_t>(entrySize) > data.size())
            return std::nullopt;
    }

    std::vector<uint32_t> offsets(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        uint32_t o = readEntry(static_cast<size_t>(i));
        if (o > data.size()) return std::nullopt;
        offsets[static_cast<size_t>(i)] = o;
    }
    return offsets;
}

std::optional<Bytes> tryMapTable(const Bytes& data) {
    auto offsets = readOffsetTable(data, 2, kPcMapScreenCount);
    if (!offsets) return std::nullopt;
    Bytes out;
    out.reserve(static_cast<size_t>(kPcMapScreenCount) * static_cast<size_t>(kPcMapScreenSize));
    for (uint32_t off : *offsets) {
        Bytes blob = decompressTableBlob(data, off);
        if (blob.size() != static_cast<size_t>(kPcMapScreenSize)) return std::nullopt;
        out.insert(out.end(), blob.begin(), blob.end());
    }
    return out;
}

}  // namespace

bool pcDatIsFlatLzw(const Bytes& raw) { return tryFlatLzw(raw).has_value(); }

Bytes pcDatDecompressFlat(const Bytes& raw) {
    auto dec = tryFlatLzw(raw);
    return dec ? *dec : raw;
}

bool pcDatIsMapTable(const Bytes& raw) { return tryMapTable(raw).has_value(); }

Bytes pcDatDecompressMap(const Bytes& raw) {
    auto dec = tryMapTable(raw);
    return dec ? *dec : raw;
}

bool pcDatIsEventTableHalf(const Bytes& raw) {
    return readOffsetTable(raw, 4, kPcEventLocationCount).has_value();
}

Bytes pcDatMergeEvent(const Bytes& indoorRaw, const Bytes& outdoorRaw) {
    auto ti = readOffsetTable(indoorRaw, 4, kPcEventLocationCount);
    auto to = readOffsetTable(outdoorRaw, 4, kPcEventLocationCount);
    if (!ti || !to) return {};

    std::vector<Bytes> blobs(static_cast<size_t>(kPcEventLocationCount));
    for (int i = 0; i < kPcEventLocationCount; ++i) {
        uint32_t oi = (*ti)[static_cast<size_t>(i)];
        uint32_t oo = (*to)[static_cast<size_t>(i)];
        if (oi != 0 && oo != 0) return {};  // slot defined in both halves -- unexpected
        if (oi != 0) blobs[static_cast<size_t>(i)] = decompressTableBlob(indoorRaw, oi);
        else if (oo != 0) blobs[static_cast<size_t>(i)] = decompressTableBlob(outdoorRaw, oo);
    }

    constexpr size_t kEntrySize = 6;  // u32 BE offset + u16 BE length (Amiga event.dat)
    Bytes header(static_cast<size_t>(kPcEventLocationCount) * kEntrySize, 0);
    Bytes body;
    uint32_t cur = static_cast<uint32_t>(header.size());
    for (int i = 0; i < kPcEventLocationCount; ++i) {
        const Bytes& blob = blobs[static_cast<size_t>(i)];
        writeU32BE(header.data() + static_cast<size_t>(i) * kEntrySize, cur);
        writeU16BE(header.data() + static_cast<size_t>(i) * kEntrySize + 4,
                   static_cast<uint16_t>(blob.size()));
        body.insert(body.end(), blob.begin(), blob.end());
        cur += static_cast<uint32_t>(blob.size());
    }

    Bytes out;
    out.reserve(header.size() + body.size());
    out.insert(out.end(), header.begin(), header.end());
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

namespace {
std::string toUpperAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// Case-insensitive lookup of `name` within `dir`.
std::string findCi(const std::string& dir, const std::string& name) {
    std::error_code ec;
    fs::path direct = fs::path(dir) / name;
    if (fs::is_regular_file(direct, ec)) return direct.string();
    std::string wanted = toUpperAscii(name);
    if (!fs::is_directory(dir, ec)) return "";
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (toUpperAscii(entry.path().filename().string()) == wanted) return entry.path().string();
    }
    return "";
}
}  // namespace

bool pcDatLoadEventAuto(const std::string& dir, Bytes& out) {
    std::string plain = findCi(dir, "event.dat");
    if (!plain.empty() && readFile(plain, out)) return true;

    std::string indoorPath = findCi(dir, "EVENTSI.DAT");
    std::string outdoorPath = findCi(dir, "EVENTSO.DAT");
    if (indoorPath.empty() || outdoorPath.empty()) return false;

    Bytes indoor, outdoor;
    if (!readFile(indoorPath, indoor) || !readFile(outdoorPath, outdoor)) return false;
    Bytes merged = pcDatMergeEvent(indoor, outdoor);
    if (merged.empty()) return false;
    out = std::move(merged);
    return true;
}

bool pcDatReadFlexible(const std::string& path, Bytes& out) {
    if (readFile(path, out)) return true;
    fs::path p(path);
    std::string dir = p.parent_path().string();
    if (dir.empty()) dir = ".";
    std::string found = findCi(dir, p.filename().string());
    if (found.empty()) return false;
    return readFile(found, out);
}

}  // namespace mm2
