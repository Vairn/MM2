#include "core/ByteIO.h"

#include <fstream>

namespace mm2 {

bool readFile(const std::string& path, Bytes& out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    std::streamsize size = in.tellg();
    if (size < 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(out.data()), size)) return false;
    return true;
}

bool writeFile(const std::string& path, const Bytes& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!data.empty())
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

}  // namespace mm2
