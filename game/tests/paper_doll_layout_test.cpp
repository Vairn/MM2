// Agui paper-doll slot mapping (visual only — roster 6+6 slots unchanged).

#include <cstdio>
#include <cstring>

#include "mm2/ui/AguiPaperDollLayout.h"

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

}  // namespace

int main()
{
    int fails = 0;
    using namespace mm2::ui::agui_doll;

    expect(bodyForItemId(0x9B) == Body::Head, "helm -> head", fails);
    expect(bodyForItemId(0x85) == Body::Torso, "plate -> torso", fails);
    expect(bodyForItemId(0xCC) == Body::Torso, "elven cloak -> torso", fails);
    expect(bodyForItemId(0x13) == Body::MainHand, "long sword -> main hand", fails);
    expect(bodyForItemId(0x4E) == Body::MainHand, "flamberge -> main hand", fails);
    expect(itemIsTwoHandedMelee(0x4E), "flamberge is 2H", fails);
    expect(bodyForItemId(0x73) == Body::OffHand, "small shield -> off hand", fails);
    expect(bodyForItemId(0x5E) == Body::Missile, "short bow -> missile", fails);
    expect(bodyForItemId(0x6F) == Body::Trinket, "green key is trinket (not missile)", fails);
    expect(bodyForItemId(0xA1) == Body::Trinket, "torch -> trinket", fails);
    expect(bodyForItemId(0xBD) == Body::Trinket, "defense ring -> trinket", fails);

    uint8_t eq[MM2_ROSTER_ITEM_SLOTS] = {0x13, 0x85, 0x73, 0x9B, 0x5E, 0xA1};
    EquipView v = assignEquip(eq);
    expect(v.body_id[static_cast<int>(Body::MainHand)] == 0x13, "sword on main hand", fails);
    expect(v.body_slot[static_cast<int>(Body::MainHand)] == 0, "sword is equip slot 1", fails);
    expect(v.body_id[static_cast<int>(Body::Torso)] == 0x85, "armor on torso", fails);
    expect(v.body_id[static_cast<int>(Body::OffHand)] == 0x73, "shield on off hand", fails);
    expect(v.body_id[static_cast<int>(Body::Head)] == 0x9B, "helm on head", fails);
    expect(v.body_id[static_cast<int>(Body::Missile)] == 0x5E, "bow on missile", fails);
    expect(v.trinket_count == 1 && v.trinket_id[0] == 0xA1, "torch overflow to trinket", fails);
    expect(v.trinket_slot[0] == 5, "torch is equip slot 6", fails);

    uint8_t two_h[MM2_ROSTER_ITEM_SLOTS] = {0x4E, 0, 0, 0, 0, 0};
    v = assignEquip(two_h);
    expect(v.body_id[static_cast<int>(Body::MainHand)] == 0x4E, "2H on main hand", fails);
    expect(v.body_id[static_cast<int>(Body::OffHand)] == 0x4E, "2H spans off hand", fails);
    expect(v.body_slot[static_cast<int>(Body::OffHand)] == 0, "2H both hands share slot 1", fails);

    uint8_t overflow[MM2_ROSTER_ITEM_SLOTS] = {0x9B, 0x9C, 0, 0, 0, 0};
    v = assignEquip(overflow);
    expect(v.body_id[static_cast<int>(Body::Head)] == 0x9B, "first helm on head", fails);
    expect(v.trinket_count == 1 && v.trinket_id[0] == 0x9C, "second helm -> trinket overflow", fails);

    const SlotPx a = packSlot(0);
    const SlotPx b = packSlot(1);
    const SlotPx c = packSlot(2);
    expect(a.x == kPackIconX0 && a.y == kPackY0, "pack A top-left", fails);
    expect(b.x == kPackIconX0 + kPackColW && b.y == kPackY0, "pack B top-right", fails);
    expect(c.x == kPackIconX0 && c.y == kPackY0 + kPackRowH, "pack C second row", fails);

    if (fails) {
        std::fprintf(stderr, "%d failure(s)\n", fails);
        return 1;
    }
    std::printf("OK: paper_doll_layout_test\n");
    return 0;
}
