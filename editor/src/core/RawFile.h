#pragma once
// Generic raw-bytes container.

#include <string>

#include "core/ByteIO.h"

namespace mm2 {

struct RawFile {
    Bytes data;

    bool load(const std::string& path) { return readFile(path, data); }
    bool save(const std::string& path) const { return writeFile(path, data); }
};

}  // namespace mm2
