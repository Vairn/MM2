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

struct DoorCase {
    int event_id;
    int x;
    int y;
    char facing;
    const char *label_snippet;
};

constexpr DoorCase kOp04Doors[] = {
    {1, 7, 5, 'S', "Middlegate Inn"},
    {2, 5, 4, 'S', "Blacksmith"},
    {3, 5, 6, 'N', "Slaughtered Lamb"},
    {4, 7, 6, 'N', "Gateway Temple"},
    {5, 9, 7, 'E', "Turkov"},
    {6, 13, 6, 'S', "Arena"},
    {7, 1, 5, 'W', "Poorman"},
    {8, 7, 13, 'N', "Mage Guild"},
    {10, 14, 6, 'S', "Exit Only"},
    {11, 2, 8, 'N', "Lock and Key"},
    {12, 1, 15, 'E', "Otto Mapper"},
    {13, 2, 12, 'E', "Edmund"},
    {14, 2, 9, 'S', "Track and Trail"},
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

    /* Temple shop @ (6,4) ALWAYS — OP_0B sign persists through OP_0E stub. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(6);
    gs.setFacingKey('N');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 24 temple fires at (4,6)", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B temple sign persists after OP_0E", fails);

    /* Blacksmith shop @ (4,4) ALWAYS. */
    runtime.enterLocation(0, gs, world);
    gs.setCoordX(4);
    gs.setCoordY(4);
    gs.setFacingKey('S');
    mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 1);
    expect(runtime.scanAndRun(gs, world), "event 22 blacksmith fires at (4,4)", fails);
    expect(runtime.textView().layerCount() > 0, "OP_0B blacksmith sign persists after OP_0E", fails);

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

    if (fails == 0) {
        std::printf("OK: event_middlegate_test (%d checks)\n", 14 + static_cast<int>(sizeof(kOp04Doors) / sizeof(kOp04Doors[0])) * 2 + 6);
        return 0;
    }
    return 1;
}
