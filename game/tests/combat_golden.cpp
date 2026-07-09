// Temporary golden harness: drive GameSession into combat and dump PPM frames.
// Usage: combat_golden [--gfx=auto|amiga|cga|ega] <data_dir>

#include "mm2/GameSession.h"
#include "mm2/gfx/GfxBackend.h"

#include <cstdio>
#include <cstring>

namespace {

const char *parseDataDir(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--gfx=", 6) == 0) {
            continue;
        }
        return arg;
    }
    return ".";
}

void parseGfxOptions(int argc, char **argv)
{
    auto &settings = mm2::gfx::gfxSettings();
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (std::strncmp(arg, "--gfx=", 6) == 0) {
            settings.backend = mm2::gfx::gfxBackendFromString(arg + 6);
        }
    }
}

bool dumpPpm(const mm2::gfx::ScreenCompositor &c, const char *name)
{
    std::FILE *f = std::fopen(name, "wb");
    if (!f) {
        return false;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", mm2::gfx::ScreenCompositor::kWidth,
                 mm2::gfx::ScreenCompositor::kHeight);
    const uint8_t *rgba = c.pixels();
    for (int i = 0; i < mm2::gfx::ScreenCompositor::kWidth * mm2::gfx::ScreenCompositor::kHeight;
         ++i) {
        std::fputc(rgba[i * 4 + 0], f);
        std::fputc(rgba[i * 4 + 1], f);
        std::fputc(rgba[i * 4 + 2], f);
    }
    std::fclose(f);
    return true;
}

void tickKey(mm2::GameSession &session, char ascii, bool up = false, bool space = false)
{
    mm2::platform::KeyState keys{};
    keys.last_ascii = ascii;
    keys.up = up;
    keys.space = space;
    keys.any_key = ascii != 0 || up || space;
    session.tick(keys);
    session.render();
    const auto &cs = session.combatSession();
    const auto view = cs.panelView();
    std::printf("tick(ascii=%d up=%d space=%d): pos=(%d,%d,%c) combat=%d partyopts=%d cmd=%d "
                "monsters=%d msg='%s'\n",
                ascii, up ? 1 : 0, space ? 1 : 0, session.gameState().coordX(),
                session.gameState().coordY(), session.gameState().facingKey(),
                cs.active() ? 1 : 0, cs.awaitingPartyOptions() ? 1 : 0,
                cs.awaitingCommand() ? 1 : 0, view.monster_line_count, view.message);
}

}  // namespace

int main(int argc, char **argv)
{
    parseGfxOptions(argc, argv);
    const char *data_dir = parseDataDir(argc, argv);
    mm2::gfx::initPcGfxFallbackDir(data_dir, nullptr);

    char path[512];
    std::snprintf(path, sizeof(path), "%s/roster.dat", data_dir);
    Mm2RosterFile roster{};
    if (mm2_roster_load_file(path, &roster) != MM2_ROSTER_OK) {
        std::fprintf(stderr, "FAIL: cannot load %s\n", path);
        return 1;
    }

    int members[MM2_PARTY_LAUNCH_SLOTS];
    int count = 0;
    for (int i = 0; i < MM2_ROSTER_RECORD_COUNT && count < 6; ++i) {
        const Mm2RosterRecord &rec = roster.records[i];
        if (rec.name[0] != 0 && (rec.town_flags & 0x7F) == 1) {
            members[count++] = i;
        }
    }
    if (count == 0) {
        std::fprintf(stderr, "FAIL: no Middlegate characters in roster.dat\n");
        return 1;
    }

    Mm2PartyLaunch launch{};
    mm2_party_launch_build(&launch, 1, members, count);
    /* Start just east of Middlegate's ambient-fight tile (6,11) (collision 0xC5
     * has the 0x80 event flag, no event.dat triplet); (7,11) facing W is open. */
    launch.coord_x = 7;
    launch.coord_y = 11;
    launch.facing_key = 'W';

    mm2::GameSession session;
    if (!session.start(data_dir, roster, launch, mm2::GameSession::kStartSkipScriptedIntros)) {
        std::fprintf(stderr, "FAIL: GameSession::start\n");
        return 1;
    }
    session.render();

    /* Step W onto (6,11) -> map-flag ambient fight (0x176F2). */
    tickKey(session, 0, /*up=*/true);
    dumpPpm(session.compositor(), "combat_golden_1_encounter.ppm");

    /* Advance (space acks banners/messages, 'A' picks Attack at party options)
     * until the first member's command grid is up. */
    bool dumped_options = false;
    for (int i = 0; i < 20 && session.combatSession().active(); ++i) {
        if (session.combatSession().awaitingCommand()) {
            break;
        }
        if (session.combatSession().awaitingPartyOptions()) {
            if (!dumped_options) {
                dumpPpm(session.compositor(), "combat_golden_2_options.ppm");
                dumped_options = true;
            }
            tickKey(session, 'A');
        } else {
            tickKey(session, ' ', false, /*space=*/true);
        }
    }
    dumpPpm(session.compositor(), "combat_golden_3_round.ppm");

    session.shutdown();
    std::printf("OK: combat_golden PPMs written\n");
    return 0;
}
