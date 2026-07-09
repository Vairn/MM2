#include "mm2/gfx/PartyStatusFormat.h"

#include <cstdio>
#include <cstring>

using mm2::gfx::formatPartyHpField;
using mm2::gfx::formatPartyStatusLine;
using mm2::gfx::PartyStatusPrefix;

static bool expect(const char *label, const char *got, const char *want)
{
    if (std::strcmp(got, want) != 0) {
        std::fprintf(stderr, "FAIL %s: got '%s' want '%s'\n", label, got, want);
        return false;
    }
    return true;
}

int main()
{
    char buf[32];
    bool ok = true;

    formatPartyHpField(0, buf, sizeof(buf));
    ok &= expect("hp zero", buf, "0  ");
    formatPartyHpField(7, buf, sizeof(buf));
    ok &= expect("hp 7", buf, "7  ");
    formatPartyHpField(16, buf, sizeof(buf));
    ok &= expect("hp 16", buf, "16 ");
    formatPartyHpField(42, buf, sizeof(buf));
    ok &= expect("hp 42", buf, "42 ");
    formatPartyHpField(999, buf, sizeof(buf));
    ok &= expect("hp 999", buf, "999");
    formatPartyHpField(1000, buf, sizeof(buf));
    ok &= expect("hp 1000", buf, "+++");
    formatPartyHpField(1234, buf, sizeof(buf));
    ok &= expect("hp 1234", buf, "+++");

    formatPartyStatusLine(buf, sizeof(buf), 0, "Crag Hack", 42, PartyStatusPrefix::Exploration);
    ok &= expect("explore line", buf, " 1) Crag Hack   /42 ");

    formatPartyStatusLine(buf, sizeof(buf), 0, "Crag Hack", 1000, PartyStatusPrefix::Exploration);
    ok &= expect("explore overflow", buf, " 1) Crag Hack   /+++");

    /* Combat front-rank prefix is glyph 0x17 (check), not ASCII '~'. */
    formatPartyStatusLine(buf, sizeof(buf), 0, "Crag Hack", 1000, PartyStatusPrefix::CombatFrontRank);
    {
        char want[32];
        std::snprintf(want, sizeof(want), "%c1) Crag Hack   /+++", static_cast<char>(0x17));
        ok &= expect("combat overflow", buf, want);
    }

    formatPartyStatusLine(buf, sizeof(buf), 0, "Al", 5, PartyStatusPrefix::Exploration);
    ok &= expect("short name align", buf, " 1) Al          /5  ");

    formatPartyStatusLine(buf, sizeof(buf), 4, "Cassandra", 7, PartyStatusPrefix::Exploration);
    ok &= expect("ones column align", buf, " 5) Cassandra   /7  ");

    mm2::gfx::formatSlashStatCurrent(0, buf, sizeof(buf), 5);
    ok &= expect("slash field zero", buf, "    0");
    mm2::gfx::formatSlashStatCurrent(1190, buf, sizeof(buf), 5);
    ok &= expect("slash field 1190", buf, " 1190");
    mm2::gfx::formatSlashStatPair(buf, sizeof(buf), 16, 16, 5);
    ok &= expect("slash pair", buf, "   16/16");
    mm2::gfx::formatSlashStatPair(buf, sizeof(buf), 1209, 1190, 5);
    ok &= expect("slash pair wide", buf, " 1209/1190");

    if (!ok) {
        return 1;
    }
    std::printf("OK: party_hp_format_test\n");
    return 0;
}
