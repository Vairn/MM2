#include "core/StrFile.h"

namespace mm2 {

bool StrFile::decode(const Bytes& bytes) {
    rawSize = bytes.size();
    text.clear();
    text.reserve(bytes.size());
    for (uint8_t b : bytes) text.push_back(static_cast<char>(strDecodeByte(b)));
    return true;
}

Bytes StrFile::encode() const {
    Bytes out;
    out.reserve(text.size());
    for (char c : text) out.push_back(strEncodeByte(static_cast<uint8_t>(c)));
    return out;
}

bool StrFile::load(const std::string& path) {
    Bytes b;
    if (!readFile(path, b)) return false;
    return decode(b);
}

bool StrFile::save(const std::string& path) const {
    return writeFile(path, encode());
}

}  // namespace mm2
