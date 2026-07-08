// Offline validator: loads Amiga-plain .dat files (repo root) and GOG/PC
// .DAT files (auto-decompressed) via the real Section-level File classes,
// and checks the results are byte-identical (or documented-close, for
// str.dat/event.dat where GOG's content genuinely differs slightly).
//
// Compile: g++ -std=c++17 -I../src pc_dat_check.cpp \
//   ../src/core/ByteIO.cpp ../src/core/AttribFile.cpp ../src/core/MonstersFile.cpp \
//   ../src/core/MapFile.cpp ../src/core/StrFile.cpp ../src/core/EventFile.cpp \
//   ../src/core/ItemsFile.cpp ../src/core/PcDatLzw.cpp ../src/core/PcGfx.cpp \
//   ../src/core/RosterFile.cpp -o pc_dat_check
#include <cstdio>
#include <string>
#include <algorithm>

#include "core/AttribFile.h"
#include "core/EventFile.h"
#include "core/ItemsFile.h"
#include "core/MapFile.h"
#include "core/MonstersFile.h"
#include "core/RosterFile.h"
#include "core/StrFile.h"

using namespace mm2;

static int gFail = 0;

static void expectTrue(bool cond, const char* what) {
    printf("  %-45s %s\n", what, cond ? "OK" : "FAIL");
    if (!cond) ++gFail;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: pc_dat_check <amiga_root_dir> <gog_dir>\n");
        return 1;
    }
    std::string amigaDir = argv[1];
    std::string gogDir = argv[2];

    printf("== Amiga plain files (%s) ==\n", amigaDir.c_str());
    {
        AttribFile a;
        expectTrue(a.load(amigaDir + "/attrib.dat"), "AttribFile::load(attrib.dat)");
        MonstersFile m;
        expectTrue(m.load(amigaDir + "/monsters.dat"), "MonstersFile::load(monsters.dat)");
        MapFile mp;
        expectTrue(mp.load(amigaDir + "/map.dat"), "MapFile::load(map.dat)");
        StrFile s;
        expectTrue(s.load(amigaDir + "/str.dat"), "StrFile::load(str.dat)");
        expectTrue(s.rawSize == 7808, "  str.dat rawSize == 7808 (amiga)");
        EventFile e;
        expectTrue(e.load(amigaDir + "/event.dat"), "EventFile::load(event.dat)");
        expectTrue(e.raw.size() == 95687, "  event.dat size == 95687 (amiga)");
        ItemsFile it;
        expectTrue(it.load(amigaDir + "/items.dat"), "ItemsFile::load(items.dat)");
        RosterFile r;
        expectTrue(r.load(amigaDir + "/roster.dat"), "RosterFile::load(roster.dat)");
    }

    printf("\n== GOG/PC files (%s) ==\n", gogDir.c_str());
    AttribFile a, aAmiga;
    expectTrue(a.load(gogDir + "/attrib.dat"), "AttribFile::load(GOG ATTRIB.DAT)");
    aAmiga.load(amigaDir + "/attrib.dat");
    expectTrue(a.encode() == aAmiga.encode(), "  ATTRIB.DAT decompresses byte-identical to attrib.dat");

    MonstersFile m, mAmiga;
    expectTrue(m.load(gogDir + "/monsters.dat"), "MonstersFile::load(GOG MONSTERS.DAT)");
    mAmiga.load(amigaDir + "/monsters.dat");
    expectTrue(m.encode() == mAmiga.encode(), "  MONSTERS.DAT decompresses byte-identical to monsters.dat");

    MapFile mp, mpAmiga;
    expectTrue(mp.load(gogDir + "/map.dat"), "MapFile::load(GOG MAP.DAT)");
    mpAmiga.load(amigaDir + "/map.dat");
    expectTrue(mp.encode() == mpAmiga.encode(), "  MAP.DAT reassembles byte-identical to map.dat");

    StrFile s;
    expectTrue(s.load(gogDir + "/str.dat"), "StrFile::load(GOG STR.DAT)");
    expectTrue(s.rawSize == 7707, "  STR.DAT decompresses to 7707 bytes (platform-specific length)");

    EventFile e, eAmiga;
    expectTrue(e.load(gogDir + "/event.dat"), "EventFile::load(GOG EVENTSI.DAT+EVENTSO.DAT auto-merge)");
    eAmiga.load(amigaDir + "/event.dat");
    expectTrue(e.raw.size() == 95687, "  merged event.dat size == 95687");
    if (e.raw.size() == eAmiga.raw.size()) {
        size_t diffs = 0;
        for (size_t i = 0; i < e.raw.size(); ++i)
            if (e.raw[i] != eAmiga.raw[i]) ++diffs;
        printf("  %-45s %zu / %zu bytes (expect 18, platform content diff)\n",
               "byte diffs vs amiga event.dat:", diffs, e.raw.size());
        expectTrue(diffs == 18, "  event.dat diff count matches expected platform delta");
    }

    ItemsFile it, itAmiga;
    expectTrue(it.load(gogDir + "/items.dat"), "ItemsFile::load(GOG ITEMS.DAT)");
    itAmiga.load(amigaDir + "/items.dat");
    expectTrue(it.encode() == itAmiga.encode(), "  ITEMS.DAT already byte-identical to items.dat");

    RosterFile r, rAmiga;
    expectTrue(r.load(gogDir + "/roster.dat"), "RosterFile::load(GOG ROSTER.DAT)");
    rAmiga.load(amigaDir + "/roster.dat");
    {
        Bytes enc = r.encode();
        expectTrue(enc.size() == static_cast<size_t>(kRosterFileSize), "  ROSTER.DAT encodes to 8320 bytes");
        bool padZero = true;
        for (size_t i = static_cast<size_t>(kRosterPcFileSize); i < enc.size(); ++i)
            if (enc[i] != 0) padZero = false;
        expectTrue(padZero, "  ROSTER.DAT pad bytes 8292..8319 are zero");
        Bytes amEnc = rAmiga.encode();
        expectTrue(std::equal(enc.begin() + kRosterCharSectionSize,
                              enc.begin() + kRosterPcFileSize,
                              amEnc.begin() + kRosterCharSectionSize),
                   "  ROSTER.DAT global stream matches Amiga roster.dat");
    }

    printf("\n%s (%d failure%s)\n", gFail == 0 ? "ALL PASS" : "SOME FAILED", gFail, gFail == 1 ? "" : "s");
    return gFail == 0 ? 0 : 1;
}
