// Offline decode + VM smoke test for Middlegate (loc 0) events:
//   All OP_04 door labels — scanner fires at correct (x,y,facing)
//   Event 20 — OP_01 + OP_09 city gates Y/N at (5,15)/ENTER + OP_0C exit to map 11
//   Shop tiles — OP_0B service sign survives OP_0E stub
//
// Usage: event_middlegate_test <data_dir>

#include <cstdio>
#include <cstring>

#include "mm2/GameState.h"
#include "mm2/events/EventRuntime.h"
#include "mm2/events/ServiceSignResolver.h"
#include "mm2/gfx/ViewportAnmOverlay.h"
#include "mm2/platform/Platform.h"
#include "mm2/world/MapWorld.h"

#include "mm2_items_codec.h"
#include "mm2_party_launch.h"
#include "mm2_gamestate.h"

namespace {

bool expect(bool cond, const char *msg, int &fails)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++fails;
        return false;
    }
    return true;
}

bool expectTableIdLoadsAnm(const char *data_dir, int table_id, int &fails)
{
    mm2::gfx::ViewportAnmOverlay overlay;
    char tag[96];
    std::snprintf(tag, sizeof(tag), "OP_0B table id %d loads %02d.anm", table_id, table_id);
    if (!expect(overlay.loadFromId(data_dir, table_id), tag, fails)) {
        return false;
    }
    return expect(overlay.loaded(), tag, fails);
}

struct DoorCase {
    int event_id;
    int x;
    int y;
    char facing;
    const char *label_snippet;
};

/* Cond bits match context_mask_tbl (W=0x10 S=0x20 E=0x40 N=0x80) per scanner @ 0x17684. */
constexpr DoorCase kOp04Doors[] = {
    {1, 7, 5, 'S', "Middlegate Inn"},
    {2, 5, 4, 'W', "Blacksmith"},
    {3, 5, 6, 'W', "Slaughtered Lamb"},
    {4, 7, 6, 'N', "Gateway Temple"},
    {5, 9, 7, 'E', "Turkov"},
    {6, 13, 6, 'S', "Arena"},
    {7, 1, 5, 'W', "Poorman"},
    {8, 7, 13, 'N', "Mage Guild"},
    {10, 14, 6, 'S', "Exit Only"},
    {11, 2, 8, 'W', "Lock and Key"},
    {12, 1, 15, 'W', "Otto Mapper"},
    {13, 2, 12, 'E', "Edmund"},
    {14, 2, 9, 'W', "Track and Trail"},
    {15, 12, 11, 'S', "Brain Detox"},
    {28, 13, 8, 'E', "Travel Moore"},
    {37, 10, 3, 'N', "Skeleton Closet"},
};

bool scanDoorCase(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                  mm2::world::MapWorld &world, const DoorCase &door, int &fails)
{
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(static_cast<uint8_t>(door.x));
    gs.setCoordY(static_cast<uint8_t>(door.y));
    gs.setFacingKey(door.facing);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    char tag[96];
    std::snprintf(tag, sizeof(tag), "event %02d OP_04 fires at (%d,%d) facing %c", door.event_id,
                  door.x, door.y, door.facing);
    if (!expect(runtime.scanAndRun(gs, world), tag, fails)) {
        return false;
    }
    expect(!runtime.blocksMovement(), tag, fails);
    expect(runtime.textView().layerCount() > 0, "OP_04 layer persists after script end", fails);
    return true;
}

bool scanDoorNoFire(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                    mm2::world::MapWorld &world, const DoorCase &door, int &fails)
{
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(static_cast<uint8_t>(door.x));
    gs.setCoordY(static_cast<uint8_t>(door.y));
    gs.setFacingKey(door.facing);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    char tag[96];
    std::snprintf(tag, sizeof(tag), "event %02d OP_04 does NOT fire at (%d,%d) facing %c",
                  door.event_id, door.x, door.y, door.facing);
    expect(!runtime.scanAndRun(gs, world), tag, fails);
    expect(runtime.textView().layerCount() == 0, "no OP_04 layer when cond mismatches", fails);
    return true;
}

void scanDoorWrongFacings(mm2::events::EventRuntime &runtime, mm2::GameStateView &gs,
                          mm2::world::MapWorld &world, const DoorCase &door, int &fails)
{
    const char facings[] = {'N', 'E', 'S', 'W'};
    for (char f : facings) {
        if (f == door.facing) {
            continue;
        }
        scanDoorNoFire(runtime, gs, world, DoorCase{door.event_id, door.x, door.y, f, door.label_snippet},
                       fails);
    }
}

}  // namespace

int main(int argc, char **argv)
{
    const char *data_dir = (argc > 1) ? argv[1] : "../..";
    int fails = 0;

    Mm2ItemsFile items{};
    char items_path[512];
    std::snprintf(items_path, sizeof(items_path), "%s/items.dat", data_dir);
    if (mm2_items_load_file(items_path, &items) == MM2_ITEMS_OK) {
        const Mm2ItemRecord *club = mm2_items_lookup(&items, 1);
        char name[32];
        if (club) {
            mm2_item_name_to_cstr(club, name, sizeof(name));
            expect(std::strcmp(name, "Small Club") == 0, "item #1 name is 'Small Club'", fails);
        } else {
            expect(false, "item #1 lookup", fails);
        }
    } else {
        expect(false, "load items.dat", fails);
    }

    mm2::events::EventRuntime runtime;
    if (!runtime.load(data_dir)) {
        std::fprintf(stderr, "FAIL: load event.dat\n");
        return 1;
    }

    mm2::world::MapWorld world;
    if (!world.load(data_dir) || !world.enterScreen(0)) {
        std::fprintf(stderr, "FAIL: load map/attrib\n");
        return 1;
    }

    uint8_t gs_image[static_cast<size_t>(MM2_A4_ANCHOR) + 0x8000u]{};
    mm2::GameStateView gs(mm2_gs_base_from_image(gs_image));
    gs.initCalendarDefaults();

    Mm2PartyLaunch launch{};
    mm2_party_launch_build(&launch, 1, nullptr, 0);
    mm2_party_launch_apply(gs.a4(), &launch);

    if (!runtime.enterLocation(0, gs, world)) {
        std::fprintf(stderr, "FAIL: enterLocation(0)\n");
        return 1;
    }

    for (const DoorCase &door : kOp04Doors) {
        scanDoorCase(runtime, gs, world, door, fails);
        scanDoorWrongFacings(runtime, gs, world, door, fails);
    }

    /* Event 01 @ (7,5) facing S — re-scan must not clear triplet permanently. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(5);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 01 re-scan at (7,5) S", fails);
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 01 fires again at (7,5) S", fails);

    /* Turn away from inn — stale OP_04 label must clear. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(5);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 01 at (7,5) S", fails);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(!runtime.scanAndRun(gs, world), "event 01 does not fire facing N", fails);
    expect(runtime.textView().layerCount() == 0, "inn label cleared after turn to N", fails);

    /* Temple shop @ (6,4) DIR_W — OP_0B sign persists through OP_0E stub. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(6);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 24 temple fires at (4,6) facing W", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B temple sign persists after OP_0E", fails);
    expect(runtime.textView().containsText("Temple services"),
           "OP_0E 0x03 temple shows unavailable stub", fails);
    expect(runtime.blocksMovement(), "temple OP_0E waits for SPACE", fails);

    /* Blacksmith shop @ (4,4) DIR_W — not on N/E/S. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(4);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 22 blacksmith fires at (4,4) facing W", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B blacksmith sign persists after OP_0E", fails);
    scanDoorNoFire(runtime, gs, world, DoorCase{22, 4, 4, 'N', "Blacksmith"}, fails);

    /* Mage guild spell shop @ (7,14) DIR_N — evt 27: OP_0B sign (str idx 0x14 → 37.anm)
     * + OP_0E 0x05 str.dat hall intro block + Y/N; must not show event str[20] farthing line. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(7);
    gs.setCoordY(14);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 27 mage guild fires at (7,14) facing N", fails);
    expect(!runtime.textView().containsText("farthing"),
           "mage guild must not show farthing error string", fails);
    expect(runtime.textView().containsText("archmage"),
           "mage guild shows spell-shop hall intro (str.dat)", fails);
    expect(runtime.textView().containsText("Interested"),
           "mage guild hall intro includes Y/N prompt line", fails);
    expect(runtime.blocksMovement(), "mage guild spell prompt waits for Y/N", fails);
    scanDoorNoFire(runtime, gs, world, DoorCase{27, 7, 14, 'W', "Mage Guild"}, fails);

    /* Event 20 @ (5,15) ENTER — OP_01 str[24] then OP_09 Y/N. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(5);
    gs.setCoordY(15);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);

    const bool fired_gates = runtime.scanAndRun(gs, world);
    expect(fired_gates, "event 20 fires at (5,15) ENTER facing N", fails);
    expect(runtime.blocksMovement(), "event 20 waits for Y/N", fails);
    expect(runtime.textView().exitBit0(), "OP_01 sets exit bit 0", fails);

    mm2::platform::KeyState keys{};
    keys.last_ascii = 'Y';
    const bool still_waiting_y = runtime.continueInput(gs, world, keys);
    expect(!still_waiting_y, "Y/N answered Y — script ended", fails);
    expect(!runtime.blocksMovement(), "movement unblocked after Y", fails);
    expect(gs.screenId() == 11, "OP_0C exits to overland map 11", fails);
    expect(gs.coordX() == 7 && gs.coordY() == 3, "OP_0C lands at tile 0x37 (7,3)", fails);
    expect(runtime.screenChanged(), "OP_0C sets screen-changed flag", fails);

    keys.last_ascii = 'N';
    runtime.enterLocation(0, gs, world);
    world.enterScreen(0);
    gs.setScreenId(0);
    gs.setCoordX(5);
    gs.setCoordY(15);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    runtime.scanAndRun(gs, world);
    const bool still_waiting_n = runtime.continueInput(gs, world, keys);
    expect(!still_waiting_n, "Y/N answered N — script ended", fails);
    expect(gs.screenId() == 0, "N stays in Middlegate", fails);

    /* C2 portcullis loc 11 evt 01: OP_0B str[24] → 29.anm (env 3 Vulcania @ 0x15756). */
    expect(mm2::events::ServiceSignResolver::envIdForScreen(11, nullptr) == 3,
           "C2 screen 11 area_env_lookup -> env 3 Vulcania", fails);
    world.enterScreen(11);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(11, &world.attribFile().records[11], 24) == 29,
           "C2 OP_0B str[24] resolves to 29.anm portcullis", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(11, &world.attribFile().records[11], 24) != 61,
           "C2 OP_0B str[24] must not use wrong env 61.anm warrior", fails);

    /* Town screens 0..4 share area_env_lookup range 0 → env 0 (Middlegate table @ 0x15756).
     * Blacksmith OP_0B str[2] → id 62; str[3] Slaughtered Lamb → 63; evt 26 str[6] → 68.
     * sign_sprite_load @ 0x316E/0x9A30: table byte maps directly to NN.anm (no extra −1). */
    expect(mm2::events::ServiceSignResolver::envIdForScreen(0, nullptr) == 0,
           "Middlegate screen 0 -> env 0", fails);
    expect(mm2::events::ServiceSignResolver::envIdForScreen(1, nullptr) == 0,
           "Atlantium town screen 1 -> env 0 (not env 1)", fails);
    expect(mm2::events::ServiceSignResolver::envIdForScreen(2, nullptr) == 0,
           "Tundara town screen 2 -> env 0", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 2) == 62,
           "Middlegate blacksmith OP_0B str[2] -> id 62", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 3) == 63,
           "Middlegate str[3] Slaughtered Lamb OP_0B -> id 63", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 6) == 68,
           "Middlegate evt 26 OP_0B str[6] -> id 68", fails);
    expectTableIdLoadsAnm(data_dir, 62, fails);
    expectTableIdLoadsAnm(data_dir, 63, fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 2) !=
               mm2::events::ServiceSignResolver::resolveForScreen(0, &world.attribFile().records[0], 6),
           "Middlegate blacksmith vs inn sign ids differ", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(1, &world.attribFile().records[1], 2) == 62,
           "Atlantium blacksmith OP_0B str[2] -> id 62", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(1, &world.attribFile().records[1], 2) != 33,
           "Atlantium blacksmith must not use Atlantium-table warrior id 33", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(2, &world.attribFile().records[2], 2) == 62,
           "Tundara blacksmith OP_0B str[2] -> id 62 via shared town table", fails);
    expect(mm2::events::ServiceSignResolver::envIdForScreen(55, nullptr) == 2,
           "Hillstone screen 55 -> env 2 Tundara", fails);

    /* C2 portcullis loc 11 evt 01 @ (3,7)/S: OP_0B loads sign overlay + OP_02 y/n text. */
    runtime.enterLocation(11, gs, world);
    world.enterScreen(11);
    gs.setScreenId(11);
    mm2::events::ServiceSignResolver::syncSignEnvId(gs.a4(), 11, &world.attrib());
    gs.setCoordX(7);
    gs.setCoordY(3);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "C2 portcullis evt 01 fires at (3,7) facing S", fails);
    expect(runtime.textView().hasServicePortrait(), "C2 portcullis OP_0B loads 29.anm overlay", fails);
    expect(runtime.textView().containsText("portcullis"), "C2 portcullis OP_02 text", fails);
    expect(runtime.blocksMovement(), "C2 portcullis waits for Y/N", fails);

    /* Hillstone Lord Slayer evt 15: OP_0B str[14] → 49.anm (env 2 Tundara table @ 0x15756). */
    expect(mm2::events::ServiceSignResolver::resolveForScreen(55, &world.attribFile().records[55], 14) == 49,
           "Hillstone OP_0B str[14] resolves to 49.anm", fails);
    expect(mm2::events::ServiceSignResolver::resolveForScreen(55, &world.attribFile().records[55], 14) != 53,
           "Hillstone OP_0B str[14] must not resolve to 53.anm (Middlegate str[13] mummy)", fails);
    world.enterScreen(55);
    gs.setScreenId(55);
    runtime.enterLocation(55, gs, world);
    gs.setCoordX(5);
    gs.setCoordY(2);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Hillstone evt 15 fires at tile (2,5) facing S", fails);
    expect(runtime.textView().hasServicePortrait(), "Lord Slayer OP_0B loads portrait overlay", fails);
    expect(runtime.textView().containsText("Lord Slayer"),
           "Lord Slayer Crusader rejection text", fails);

    /* Atlantium evt 17 blacksmith @ (6,13)/S: OP_0B str[2] + OP_0E 0x06 — sign id 62, not warrior 33. */
    world.enterScreen(1);
    gs.setScreenId(1);
    runtime.enterLocation(1, gs, world);
    gs.setCoordX(6);
    gs.setCoordY(13);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Atlantium evt 17 blacksmith fires at (6,13) facing S", fails);
    expect(runtime.textView().hasServicePortrait(), "Atlantium blacksmith OP_0B loads sign overlay", fails);
    expect(mm2::events::ServiceSignResolver::resolveForGameState(gs.a4(), 1, &world.attrib(), 2) == 62,
           "Atlantium blacksmith synced env resolves str[2] -> 62", fails);

    /* Middlegate evt 22 blacksmith vs evt 26 inn/arena — different str_idx, different sign ids. */
    world.enterScreen(0);
    gs.setScreenId(0);
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(4);
    gs.setFacingKey('W');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Middlegate evt 22 blacksmith at (4,4) W", fails);
    expect(runtime.textView().hasServicePortrait(), "Middlegate blacksmith OP_0B overlay", fails);
    {
        const int frame0 = runtime.textView().serviceSignFrame();
        bool saw_change = false;
        for (int t = 0; t < 50; ++t) {
            runtime.textView().tickAnimation();
            if (runtime.textView().serviceSignFrame() != frame0) {
                saw_change = true;
                break;
            }
        }
        expect(saw_change, "Middlegate blacksmith 62.anm sign animates across ticks", fails);
    }
    const int mg_blacksmith_id =
        mm2::events::ServiceSignResolver::resolveForGameState(gs.a4(), 0, &world.attrib(), 2);
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(10);
    gs.setCoordY(7);
    gs.setFacingKey('E');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "Middlegate evt 26 arena/inn at (7,10) E", fails);
    expect(runtime.textView().hasServicePortrait(), "Middlegate inn OP_0B overlay", fails);
    const int mg_inn_id =
        mm2::events::ServiceSignResolver::resolveForGameState(gs.a4(), 0, &world.attrib(), 6);
    expect(mg_blacksmith_id == 62, "Middlegate blacksmith str[2] -> 62", fails);
    expect(mg_inn_id == 68, "Middlegate evt 26 str[6] -> 68", fails);
    expectTableIdLoadsAnm(data_dir, mg_blacksmith_id, fails);
    expectTableIdLoadsAnm(data_dir, mg_inn_id, fails);
    expect(mg_blacksmith_id != mg_inn_id, "inn tile must not reuse blacksmith sign id", fails);

    /* C2 loc 11 evt 22 @ (14,8)/N: era 9 (Year 900 / 10th century) shows ruins, must not OP_0C to 59. */
    world.enterScreen(11);
    gs.setScreenId(11);
    runtime.enterLocation(11, gs, world);
    mm2_gs_set_u16(gs.a4(), MM2_GS_ERA, 9);
    mm2_gs_set_u8(gs.a4(), MM2_GS_ERA_LOW, 9);
    gs.setCoordX(14);
    gs.setCoordY(8);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    const uint8_t c2_screen_before = gs.screenId();
    expect(runtime.scanAndRun(gs, world), "C2 evt 22 Castle Xabran gate at (14,8) N", fails);
    expect(gs.screenId() == c2_screen_before, "era 9 must not teleport to Castle Xabran (59)", fails);
    expect(runtime.textView().containsText("Ruins"), "era 9 shows ruins text not enter prompt", fails);

    /* C2 loc 11 evt 04 @ (4,7)/N: Pegasus greeting via event VM (not scripted-scene hack). */
    gs.setPartyProgress(0);
    runtime.enterLocation(11, gs, world);
    mm2::events::ServiceSignResolver::syncSignEnvId(gs.a4(), 11, &world.attrib());
    gs.setCoordX(7);
    gs.setCoordY(4);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "C2 evt 04 Pegasus at (4,7) facing N", fails);
    expect(runtime.textView().containsText("Guardian Pegasus"),
           "Pegasus OP_03 shows greeting text", fails);
    expect(runtime.textView().hasPegasusIllustration(), "Pegasus OP_03 loads 21.anm overlay", fails);
    expect(runtime.blocksMovement(), "Pegasus evt waits for SPACE", fails);
    expect((gs.partyProgress() & 0x40) != 0, "Pegasus OP_18 sets seen bit 0x40", fails);

    if (fails == 0) {
        std::printf("OK: event_middlegate_test\n");
        return 0;
    }
    return 1;
}
