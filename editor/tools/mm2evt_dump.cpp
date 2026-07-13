// Dump one location (or all) from event.dat as .mm2evt text.

#include "eventlang/Decompile.h"
#include "eventlang/DslEmit.h"
#include "eventlang/DslParse.h"
#include "eventlang/Encode.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: mm2evt_dump <event.dat> [locId] [--roundtrip]\n";
        return 1;
    }
    const std::string path = argv[1];
    int locId = -1;
    bool roundtrip = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--roundtrip")
            roundtrip = true;
        else
            locId = std::stoi(a);
    }

    auto data = readFile(path);
    if (data.empty()) {
        std::cerr << "cannot read " << path << "\n";
        return 1;
    }

    auto file = mm2::eventlang::decompileEventDat(data.data(), data.size());
    if (file.locations.empty()) {
        std::cerr << "decompile failed\n";
        return 1;
    }

    if (roundtrip) {
        int ok = 0;
        for (int i = 0; i < static_cast<int>(file.locations.size()); ++i) {
            auto loc = file.locations[i];
            loc.modified = true;  // force encode path (rawSegment fallback still applies)
            auto enc = mm2::eventlang::encodeLocation(loc);
            const auto& raw = file.rawRecords[i];
            if (enc == raw) {
                ++ok;
            } else {
                std::cout << "loc " << i << " MISMATCH raw=" << raw.size() << " enc=" << enc.size();
                size_t n = raw.size() < enc.size() ? raw.size() : enc.size();
                for (size_t b = 0; b < n; ++b) {
                    if (raw[b] != enc[b]) {
                        std::cout << " first@" << b << " raw=" << std::hex << int(raw[b])
                                  << " enc=" << int(enc[b]) << std::dec;
                        break;
                    }
                }
                if (enc.size() != raw.size() &&
                    std::equal(raw.begin(), raw.end(), enc.begin(),
                               enc.begin() + std::min(raw.size(), enc.size()))) {
                    std::cout << " (prefix equal; len delta="
                              << int(enc.size()) - int(raw.size()) << ")";
                }
                std::cout << " scripts=" << loc.scripts.size()
                          << " strings=" << loc.strings.size() << "\n";
                if (i == 0) {
                    // dump last 16 of each
                    std::cout << "  raw tail:";
                    for (size_t b = raw.size() > 16 ? raw.size() - 16 : 0; b < raw.size(); ++b)
                        std::cout << " " << std::hex << int(raw[b]);
                    std::cout << "\n  enc tail:";
                    for (size_t b = enc.size() > 16 ? enc.size() - 16 : 0; b < enc.size(); ++b)
                        std::cout << " " << std::hex << int(enc[b]);
                    std::cout << std::dec << "\n";
                }
            }
        }
        std::cout << "roundtrip: " << ok << "/" << file.locations.size() << "\n";
        return ok == static_cast<int>(file.locations.size()) ? 0 : 2;
    }

    if (locId < 0) {
        std::cout << mm2::eventlang::emitFile(file);
        return 0;
    }
    if (locId >= static_cast<int>(file.locations.size())) {
        std::cerr << "bad locId\n";
        return 1;
    }
    std::cout << mm2::eventlang::emitLocation(file.locations[locId], {}, &file);
    return 0;
}
