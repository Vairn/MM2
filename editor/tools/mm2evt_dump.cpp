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

static void reportMismatch(int locId, const std::vector<uint8_t>& raw,
                           const std::vector<uint8_t>& enc, const char* mode) {
    std::cout << "loc " << locId << " MISMATCH [" << mode << "] raw=" << raw.size()
              << " enc=" << enc.size();
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
        std::cout << " (prefix equal; len delta=" << int(enc.size()) - int(raw.size()) << ")";
    }
    std::cout << "\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: mm2evt_dump <event.dat> [locId] [--roundtrip|--roundtrip-raw]\n";
        std::cerr << "  --roundtrip      strict: emit→parse→encode (no rawSegment cheat)\n";
        std::cerr << "  --roundtrip-raw  AST encode with rawSegment fallback (legacy)\n";
        return 1;
    }
    const std::string path = argv[1];
    int locId = -1;
    bool roundtrip = false;
    bool roundtripRaw = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--roundtrip")
            roundtrip = true;
        else if (a == "--roundtrip-raw")
            roundtripRaw = true;
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

    if (roundtrip || roundtripRaw) {
        int ok = 0;
        for (int i = 0; i < static_cast<int>(file.locations.size()); ++i) {
            const auto& raw = file.rawRecords[i];
            std::vector<uint8_t> enc;
            const char* mode = roundtrip ? "text" : "ast+raw";

            if (roundtrip) {
                // Strict text roundtrip: emit → parse → encode (no rawSegment/rawBlob).
                std::string text = mm2::eventlang::emitLocation(file.locations[i], {}, &file);
                auto parsed = mm2::eventlang::parseLocationText(text);
                if (!parsed.ok) {
                    std::cout << "loc " << i << " PARSE FAIL: " << parsed.error << "\n";
                    continue;
                }
                parsed.loc.modified = true;
                parsed.loc.rawBlob.clear();
                for (auto& sc : parsed.loc.scripts) sc.rawSegment.clear();
                // Keep record kind / terminated from decompile when parse defaults differ.
                parsed.loc.recordKind = file.locations[i].recordKind;
                parsed.loc.terminated = file.locations[i].terminated;
                enc = mm2::eventlang::encodeLocation(parsed.loc);
            } else {
                // Legacy: AST encode; Encode may still use empty-body rawSegment only.
                auto loc = file.locations[i];
                loc.modified = true;
                enc = mm2::eventlang::encodeLocation(loc);
            }

            if (enc == raw) {
                ++ok;
            } else {
                reportMismatch(i, raw, enc, mode);
            }
        }
        std::cout << (roundtrip ? "strict text roundtrip: " : "raw-fallback roundtrip: ") << ok
                  << "/" << file.locations.size() << "\n";

        // Also report AST-without-raw (clear rawSegment) when doing --roundtrip.
        if (roundtrip) {
            int okAst = 0;
            for (int i = 0; i < static_cast<int>(file.locations.size()); ++i) {
                auto loc = file.locations[i];
                loc.modified = true;
                loc.rawBlob.clear();
                for (auto& sc : loc.scripts) sc.rawSegment.clear();
                auto enc = mm2::eventlang::encodeLocation(loc);
                if (enc == file.rawRecords[i]) {
                    ++okAst;
                } else {
                    reportMismatch(i, file.rawRecords[i], enc, "ast-noraw");
                }
            }
            std::cout << "strict AST (no rawSegment): " << okAst << "/" << file.locations.size()
                      << "\n";
            return (ok == static_cast<int>(file.locations.size()) &&
                    okAst == static_cast<int>(file.locations.size()))
                       ? 0
                       : 2;
        }
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
