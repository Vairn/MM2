#pragma once
// Generic raw-bytes container for formats not yet structurally decoded
// (spells.dat = 256 bytes, unknown layout; event.dat = container decoded by
// tools/decode_event.py but edited here as raw for now).

#include <string>

#include "core/ByteIO.h"

namespace mm2 {

struct RawFile {
    Bytes data;

    bool load(const std::string& path) { return readFile(path, data); }
    bool save(const std::string& path) const { return writeFile(path, data); }
};

}  // namespace mm2
