#include "mm2/gameplay/DevMenu.h"

#include "mm2/gfx/AmigaPlayScreenLayout.h"
#include "mm2/ui/AmigaCharacterUiLayout.h"

#include "mm2_attrib_codec.h"
#include "mm2_gamestate.h"
#include "mm2_map_codec.h"
#include "mm2_monsters_codec.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace mm2::gameplay {

namespace {

using namespace mm2::ui::amiga_layout;
using namespace mm2::gfx::play_layout;

constexpr const char *kCheatLabels[] = {
    "Add XP",       "Add gold",     "Add gems",     "Temp stats",  "Heal / revive", "Fill food",
    "Max spells",   "Party buffs",  "Reveal map",   "Unlock hirelings",
};

constexpr const char *kMonsterActionLabels[] = {
    "Instant win", "Instant flee", "Sleep all", "Heal party", "Kill selected", "Invulnerable",
};

constexpr const char *kMoveLabels[] = {
    "No clip walls",
    "No random fights",
    "Skip tile events",
};

const char *devMapName(int screen)
{
    switch (screen) {
    case 0:
        return "Middlegate";
    case 1:
        return "Atlantium";
    case 2:
        return "Tundara";
    case 3:
        return "Vulcania";
    case 4:
        return "Sandsobar";
    case 5:
        return "A1";
    case 6:
        return "B1";
    case 7:
        return "C1";
    case 8:
        return "D1";
    case 9:
        return "A2";
    case 10:
        return "B2";
    case 11:
        return "C2";
    case 12:
        return "A3";
    case 13:
        return "B3";
    case 14:
        return "C3";
    case 15:
        return "A4";
    case 16:
        return "B4";
    case 17:
        return "Middlegate Cavern";
    case 18:
        return "Atlantium Cavern";
    case 19:
        return "Tundara Cavern";
    case 20:
        return "Vulcania Cavern";
    case 21:
        return "Sandsobar Cavern";
    case 22:
        return "Corak's Cave";
    case 23:
        return "Square Lake Cave";
    case 24:
        return "Ice Cavern";
    case 25:
        return "Sarakin's Mine";
    case 26:
        return "Murray's Cave";
    case 27:
        return "Druid's Cave";
    case 28:
        return "Forbidden Forest Cav";
    case 29:
        return "Dragon's Dominion";
    case 30:
        return "Dawn's Mist Bog";
    case 31:
        return "Gemmaker's Cave";
    case 32:
        return "Nomadic Rift";
    case 33:
        return "E1";
    case 34:
        return "D2";
    case 35:
        return "E2";
    case 36:
        return "D3";
    case 37:
        return "E3";
    case 38:
        return "C4";
    case 39:
        return "D4";
    case 40:
        return "E4";
    case 41:
        return "Plane of Air";
    case 42:
        return "Plane of Fire";
    case 43:
        return "Plane of Earth";
    case 44:
        return "Plane of Water";
    case 45:
        return "Hillstone Dun L1";
    case 46:
        return "Hillstone Dun L2";
    case 47:
        return "Woodhaven Dun L1";
    case 48:
        return "Woodhaven Dun L2";
    case 49:
        return "Pinehurst Dun L1";
    case 50:
        return "Pinehurst Dun L2";
    case 51:
        return "Luxus Dun L1";
    case 52:
        return "Luxus Dun L2";
    case 53:
        return "Ancients (Evil)";
    case 54:
        return "Ancients (Good)";
    case 55:
        return "Hillstone";
    case 56:
        return "Woodhaven";
    case 57:
        return "Pinehurst";
    case 58:
        return "Luxus Palace";
    case 59:
        return "Castle Xabran";
    default:
        return nullptr;
    }
}

void formatMonsterType(char *out, size_t cap, int type, const Mm2MonstersFile *monsters)
{
    if (!out || cap == 0) {
        return;
    }
    type = clampDevMonsterType(type);
    if (monsters) {
        const Mm2MonsterRecord &rec = monsters->records[type];
        if (!mm2_monster_slot_is_empty(&rec)) {
            char name[16];
            mm2_monster_name_to_cstr(&rec, name, sizeof(name));
            std::snprintf(out, cap, "%03d %s", type, name);
            return;
        }
    }
    std::snprintf(out, cap, "%03d (empty)", type);
}

void applyTeleportSpawnPreview(DevMenuState &st, const world::MapWorld &world)
{
    st.teleport_screen = clampDevScreenId(st.teleport_screen);
    if (!world.loaded()) {
        return;
    }
    const uint8_t packed =
        mm2_attrib_entry_coord(&world.attribFile().records[st.teleport_screen]);
    st.teleport_x = packed & 0x0F;
    st.teleport_y = (packed >> 4) & 0x0F;
}

void drawText(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r = kUiYellowTextR,
              uint8_t g = kUiYellowTextG, uint8_t b = kUiYellowTextB)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

void drawTextRgb(gfx::ScreenCompositor &c, int row, int col, const char *text, uint8_t r, uint8_t g,
                 uint8_t b)
{
    c.drawText(cellX(col), cellY(row), text, r, g, b, 255);
}

void fillBlueWindow(gfx::ScreenCompositor &c)
{
    constexpr int kWinX1 = 1;
    constexpr int kWinY1 = 1;
    constexpr int kWinX2 = 38;
    constexpr int kWinY2 = 22;
    constexpr int kWinW = kWinX2 - kWinX1 + 1;
    constexpr int kWinH = kWinY2 - kWinY1 + 1;
    c.fillRect(kWinX1 * 8, kWinY1 * 8, kWinW * 8, kWinH * 8, kUiBlueFillR, kUiBlueFillG, kUiBlueFillB,
               255);
    c.drawConsoleBox(kWinY1, kWinX1, kWinW, kWinH, kUiYellowTextR, kUiYellowTextG, kUiYellowTextB);
}

void setStatus(DevMenuState &st, const char *msg)
{
    std::snprintf(st.status_line, sizeof(st.status_line), "%s", msg ? msg : "");
}

const char *waitKindName(events::EventVmWait w)
{
    switch (w) {
    case events::EventVmWait::None:
        return "None";
    case events::EventVmWait::Space:
        return "Space";
    case events::EventVmWait::YesNo:
        return "YesNo";
    case events::EventVmWait::MemberSelect:
        return "Member";
    case events::EventVmWait::Answer:
        return "Answer";
    case events::EventVmWait::HexDigit:
        return "Hex";
    case events::EventVmWait::LetterSelect:
        return "Letter";
    case events::EventVmWait::Delay:
        return "Delay";
    default:
        return "?";
    }
}

const char *combatStateName(combat::CombatState s)
{
    switch (s) {
    case combat::CombatState::Inactive:
        return "Inactive";
    case combat::CombatState::AwaitingSurpriseDismiss:
        return "Surprise";
    case combat::CombatState::AwaitingPartyOptions:
        return "PartyOpts";
    case combat::CombatState::AwaitingBribeKind:
        return "BribeKind";
    case combat::CombatState::AwaitingBribeAmount:
        return "BribeAmt";
    case combat::CombatState::AwaitingCommand:
        return "Command";
    case combat::CombatState::AwaitingCastLevel:
        return "CastLvl";
    case combat::CombatState::AwaitingCastNumber:
        return "CastNum";
    case combat::CombatState::AwaitingCastTarget:
        return "CastTgt";
    case combat::CombatState::AwaitingAttackTarget:
        return "AtkTgt";
    case combat::CombatState::AwaitingPartyPick:
        return "PartyPick";
    case combat::CombatState::AwaitingItemPick:
        return "ItemPick";
    case combat::CombatState::AwaitingExchangeWith:
        return "Exchange";
    case combat::CombatState::AwaitingActionAck:
        return "Ack";
    case combat::CombatState::AwaitingVictoryDismiss:
        return "Victory";
    default:
        return "?";
    }
}

void formatCond(uint8_t cond, char *buf, size_t cap)
{
    char tmp[8]{};
    int n = 0;
    if (cond & 0x80) {
        tmp[n++] = 'N';
    }
    if (cond & 0x40) {
        tmp[n++] = 'E';
    }
    if (cond & 0x20) {
        tmp[n++] = 'S';
    }
    if (cond & 0x10) {
        tmp[n++] = 'W';
    }
    if (n == 0) {
        std::snprintf(buf, cap, "%02X", cond);
    } else {
        tmp[n] = '\0';
        std::snprintf(buf, cap, "%s", tmp);
    }
}

void seedMapCursor(DevMenuState &st, const GameStateView &gs)
{
    if (!st.map_cursor_seeded && gs.valid()) {
        st.map_cursor_x = static_cast<int>(gs.coordX()) & 0x0F;
        st.map_cursor_y = static_cast<int>(gs.coordY()) & 0x0F;
        st.map_cursor_seeded = true;
        st.map_triplet_scroll = 0;
    }
}

int countTripletsAt(const Mm2EventLocation *loc, int x, int y)
{
    if (!loc) {
        return 0;
    }
    const uint8_t pos = static_cast<uint8_t>(((y & 0x0F) << 4) | (x & 0x0F));
    int n = 0;
    for (int i = 0; i < loc->triplet_count; ++i) {
        if (loc->triplets[i].pos == pos) {
            ++n;
        }
    }
    return n;
}

const Mm2EventTriplet *nthTripletAt(const Mm2EventLocation *loc, int x, int y, int n)
{
    if (!loc || n < 0) {
        return nullptr;
    }
    const uint8_t pos = static_cast<uint8_t>(((y & 0x0F) << 4) | (x & 0x0F));
    int seen = 0;
    for (int i = 0; i < loc->triplet_count; ++i) {
        if (loc->triplets[i].pos == pos) {
            if (seen == n) {
                return &loc->triplets[i];
            }
            ++seen;
        }
    }
    return nullptr;
}

bool tileHasTriplet(const Mm2EventLocation *loc, int x, int y)
{
    return countTripletsAt(loc, x, y) > 0;
}

void drawMarker(gfx::ScreenCompositor &c, int px, int py, uint8_t r, uint8_t g, uint8_t b)
{
    c.fillRect(px + 4, py + 3, 6, 5, r, g, b, 220);
}

void renderCheats(gfx::ScreenCompositor &c, const DevMenuState &st)
{
    fillBlueWindow(c);
    drawText(c, 2, 12, "DEV MENU");
    drawText(c, 3, 2, "Cheats  [Tab: pages]");

    const int visible = 12;
    const int count = static_cast<int>(DevCheatRow::Count);
    int top = st.cheat_row - (visible / 2);
    if (top < 0) {
        top = 0;
    }
    if (top > count - visible) {
        top = std::max(0, count - visible);
    }

    for (int i = 0; i < visible && top + i < count; ++i) {
        const int row_i = top + i;
        const int screen_row = 5 + i;
        char line[40];
        const bool sel = (row_i == st.cheat_row);
        const DevCheatRow kind = static_cast<DevCheatRow>(row_i);
        if (kind == DevCheatRow::AddXp) {
            std::snprintf(line, sizeof(line), "%c %-14s %u", sel ? '>' : ' ', kCheatLabels[row_i],
                          grantAmountPreset(st.amount_idx_xp));
        } else if (kind == DevCheatRow::AddGold) {
            std::snprintf(line, sizeof(line), "%c %-14s %u", sel ? '>' : ' ', kCheatLabels[row_i],
                          grantAmountPreset(st.amount_idx_gold));
        } else if (kind == DevCheatRow::AddGems) {
            std::snprintf(line, sizeof(line), "%c %-14s %u", sel ? '>' : ' ', kCheatLabels[row_i],
                          grantAmountPreset(st.amount_idx_gems));
        } else if (kind == DevCheatRow::BoostStats) {
            std::snprintf(line, sizeof(line), "%c %-14s +%u", sel ? '>' : ' ', kCheatLabels[row_i],
                          statBoostPreset(st.amount_idx_stats));
        } else {
            std::snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', kCheatLabels[row_i]);
        }
        if (sel) {
            drawTextRgb(c, screen_row, 2, line, 255, 255, 100);
        } else {
            drawText(c, screen_row, 2, line);
        }
    }

    drawText(c, 18, 2, "Up/Dn  L/R amount  Enter");
    drawText(c, 19, 2, "ESC/F11/Help close");
    if (st.status_line[0]) {
        drawTextRgb(c, 21, 2, st.status_line, 180, 255, 180);
    }
}

void renderStatusVm(gfx::ScreenCompositor &c, const DevMenuState &st, const GameStateView &gs,
                    const Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                    const events::EventRuntime &events, const char *screen_label)
{
    fillBlueWindow(c);
    drawText(c, 2, 10, "STATUS / VM");

    char line[48];
    const char *label = (screen_label && screen_label[0]) ? screen_label : "?";
    if (gs.valid()) {
        std::snprintf(line, sizeof(line), "Scr %u %s  (%u,%u) %c",
                      static_cast<unsigned>(gs.screenId()), label,
                      static_cast<unsigned>(gs.coordX()), static_cast<unsigned>(gs.coordY()),
                      gs.facingKey() ? gs.facingKey() : '?');
        drawText(c, 4, 2, line);
        std::snprintf(line, sizeof(line), "Day %u Yr %u Era %u",
                      static_cast<unsigned>(gs.day()), static_cast<unsigned>(gs.year()),
                      static_cast<unsigned>(gs.era()));
        drawText(c, 5, 2, line);
    }

    std::snprintf(line, sizeof(line), "Party %d", launch.party_count);
    drawText(c, 7, 2, line);
    int row = 8;
    for (int i = 0; i < launch.party_count && i < MM2_PARTY_LAUNCH_SLOTS && row <= 14; ++i) {
        const int idx = launch.roster_slots[i];
        if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT) {
            continue;
        }
        const Mm2RosterRecord &rec = roster.records[idx];
        char name[12];
        std::memcpy(name, rec.name, 10);
        name[10] = '\0';
        for (int k = 9; k >= 0 && name[k] == ' '; --k) {
            name[k] = '\0';
        }
        std::snprintf(line, sizeof(line), "%d %-8s L%u XP%u G%u g%u", i + 1, name,
                      static_cast<unsigned>(rec.level), static_cast<unsigned>(rec.experience),
                      static_cast<unsigned>(rec.gold), static_cast<unsigned>(rec.gems));
        drawText(c, row++, 2, line);
    }

    std::snprintf(line, sizeof(line), "VM loc=%d act=%d wait=%s", events.locationId(),
                  events.isActive() ? 1 : 0, waitKindName(events.waitKind()));
    drawText(c, 16, 2, line);
    if (gs.valid()) {
        std::snprintf(line, sizeof(line), "parse=%04X anch=%04X",
                      mm2_gs_u16(gs.a4(), MM2_GS_EVENT_PARSE_POS),
                      mm2_gs_u16(gs.a4(), MM2_GS_EVENT_SCRIPT_ANCHOR));
        drawText(c, 17, 2, line);
        char bank[40];
        int n = 0;
        n += std::snprintf(bank + n, sizeof(bank) - static_cast<size_t>(n), "bank ");
        for (int i = 0; i < 8 && n < static_cast<int>(sizeof(bank)) - 4; ++i) {
            n += std::snprintf(bank + n, sizeof(bank) - static_cast<size_t>(n), "%02X",
                               mm2_gs_u8(gs.a4(), MM2_GS_EVENT_VAR_BANK + i));
        }
        drawText(c, 18, 2, bank);
    }
    drawText(c, 21, 2, "Tab: pages  ESC: close");
    (void)st;
}

const char *castWhenTag(SpellCastWhen w)
{
    switch (w) {
    case SpellCastWhen::Combat:
        return "Cmb";
    case SpellCastWhen::Explore:
        return "Exp";
    default:
        return "Any";
    }
}

void copyName10(const Mm2RosterRecord &rec, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return;
    }
    char name[12];
    std::memcpy(name, rec.name, 10);
    name[10] = '\0';
    for (int k = 9; k >= 0 && name[k] == ' '; --k) {
        name[k] = '\0';
    }
    std::snprintf(out, cap, "%s", name);
}

void clampSpellPartySlot(DevMenuState &st, const Mm2PartyLaunch &launch)
{
    if (launch.party_count <= 0) {
        st.spell_party_slot = 0;
        return;
    }
    if (st.spell_party_slot < 0) {
        st.spell_party_slot = 0;
    }
    if (st.spell_party_slot >= launch.party_count) {
        st.spell_party_slot = launch.party_count - 1;
    }
}

void renderSpells(gfx::ScreenCompositor &c, DevMenuState &st, const Mm2RosterFile &roster,
                  const Mm2PartyLaunch &launch)
{
    fillBlueWindow(c);
    drawText(c, 2, 11, "SPELL LIST");

    clampSpellPartySlot(st, launch);
    const Mm2RosterRecord *rec = partyRecord(roster, launch, st.spell_party_slot);
    char line[48];
    if (!rec) {
        drawText(c, 4, 2, "no character");
        drawText(c, 20, 2, "Tab: pages  ESC/F11 close");
        return;
    }

    char name[12];
    copyName10(*rec, name, sizeof(name));
    const SpellSchool school = spellSchoolForClass(rec->class_id);
    const int known = knownSpellCount(*rec, school);
    std::snprintf(line, sizeof(line), "%d) %-10s %-8s SL%u", st.spell_party_slot + 1, name,
                  schoolName(school), static_cast<unsigned>(rec->spell_level));
    drawText(c, 3, 2, line);
    std::snprintf(line, sizeof(line), "SP %u/%u  Gems %u  %d known",
                  static_cast<unsigned>(rec->sp_current), static_cast<unsigned>(rec->sp_max),
                  static_cast<unsigned>(rec->gems), known);
    drawText(c, 4, 2, line);

    if (school == SpellSchool::None) {
        drawText(c, 7, 2, "No spell book.");
        drawText(c, 19, 2, "1-8 / L/R character");
        drawText(c, 20, 2, "Tab: pages  ESC/F11 close");
        return;
    }

    const SpellMeta *table = schoolSpellTable(school);
    drawTextRgb(c, 5, 2, "  L-N Name               Cost   When", 180, 180, 180);

    constexpr int kVisible = 12;
    if (st.spell_row < 0) {
        st.spell_row = 0;
    }
    if (st.spell_row >= kSpellsPerSchool) {
        st.spell_row = kSpellsPerSchool - 1;
    }
    int top = st.spell_row - (kVisible / 2);
    if (top < 0) {
        top = 0;
    }
    if (top > kSpellsPerSchool - kVisible) {
        top = std::max(0, kSpellsPerSchool - kVisible);
    }

    for (int i = 0; i < kVisible; ++i) {
        const int flat = top + i;
        if (flat >= kSpellsPerSchool || !table) {
            break;
        }
        const SpellMeta &meta = table[flat];
        const bool known_bit = spellKnownInBook(*rec, flat);
        const bool sel = (flat == st.spell_row);
        char nam[19];
        const char *src = meta.name ? meta.name : "";
        int nlen = 0;
        while (nlen < 18 && src[nlen]) {
            nam[nlen] = src[nlen];
            ++nlen;
        }
        nam[nlen] = '\0';
        char cost[12];
        if (meta.per_level) {
            if (meta.gems) {
                std::snprintf(cost, sizeof(cost), "%u/L %u", meta.sp, meta.gems);
            } else {
                std::snprintf(cost, sizeof(cost), "%u/L", meta.sp);
            }
        } else if (meta.gems) {
            std::snprintf(cost, sizeof(cost), "%u %u", meta.sp, meta.gems);
        } else {
            std::snprintf(cost, sizeof(cost), "%u", meta.sp);
        }
        std::snprintf(line, sizeof(line), "%c %d-%d %-18s %-7s %s", known_bit ? '*' : ' ',
                      static_cast<int>(meta.level), static_cast<int>(meta.number), nam, cost,
                      castWhenTag(spellCastWhen(school, flat)));
        const int screen_row = 6 + i;
        if (sel) {
            drawTextRgb(c, screen_row, 2, line, 255, 255, 100);
        } else if (known_bit) {
            drawText(c, screen_row, 2, line);
        } else {
            drawTextRgb(c, screen_row, 2, line, 140, 140, 160);
        }
    }

    drawText(c, 19, 2, "Up/Dn  1-8 / L/R char");
    drawText(c, 20, 2, "Tab: pages  ESC/F11 close");
    if (st.status_line[0]) {
        drawTextRgb(c, 21, 2, st.status_line, 180, 255, 180);
    }
}

void renderMapEvents(gfx::ScreenCompositor &c, DevMenuState &st, const GameStateView &gs,
                     const world::MapWorld &world, const world::AutomapState &automap,
                     const gfx::EnvAssets &env, const events::EventRuntime &events)
{
    c.clear(0, 0, 0, 255);
    drawTextRgb(c, 0, 1, "DEV MAP (fog off)", 255, 255, 100);

    gfx::AutomapRenderParams params{};
    params.ignore_visibility = true;
    params.draw_party_marker = true;
    params.draw_edge_overlay = true;
    if (env.automapReady() && world.loaded() && gs.valid()) {
        gfx::renderAutomap(c, env, world, automap, gs, params);
    } else {
        drawText(c, 10, 4, "(automap assets unavailable)");
    }

    const Mm2EventLocation *loc = events.currentLocation();
    constexpr int kTileW = 14;
    constexpr int kTileH = 11;
    const int origin_x = params.origin_x;
    const int origin_y = params.origin_y;

    if (world.loaded()) {
        for (int y = 0; y < MM2_MAP_GRID_DIM; ++y) {
            for (int x = 0; x < MM2_MAP_GRID_DIM; ++x) {
                const int disk_y = (MM2_MAP_GRID_DIM - 1) - y;
                const int px = origin_x + x * kTileW;
                const int py = origin_y + disk_y * kTileH;
                const bool has_evt = tileHasTriplet(loc, x, y);
                const bool coll =
                    mm2_map_collision_has_event(world.collisionAt(x, y)) != 0;
                if (has_evt) {
                    drawMarker(c, px, py, 255, 0, 255);
                } else if (coll) {
                    drawMarker(c, px, py, 255, 220, 0);
                }
            }
        }
    }

    seedMapCursor(st, gs);
    {
        const int disk_y = (MM2_MAP_GRID_DIM - 1) - st.map_cursor_y;
        const int px = origin_x + st.map_cursor_x * kTileW;
        const int py = origin_y + disk_y * kTileH;
        c.drawBoxBorder(px, py, kTileW, kTileH, 255, 255, 255, 255);
    }

    char line[48];
    const int ntrip = countTripletsAt(loc, st.map_cursor_x, st.map_cursor_y);
    if (st.map_triplet_scroll >= ntrip) {
        st.map_triplet_scroll = ntrip > 0 ? ntrip - 1 : 0;
    }
    const Mm2EventTriplet *t =
        nthTripletAt(loc, st.map_cursor_x, st.map_cursor_y, st.map_triplet_scroll);
    const bool coll =
        world.loaded() &&
        mm2_map_collision_has_event(world.collisionAt(st.map_cursor_x, st.map_cursor_y)) != 0;
    if (t) {
        char cond[8];
        formatCond(t->cond, cond, sizeof(cond));
        std::snprintf(line, sizeof(line), "(%d,%d) evt=%u cond=%s coll=%s [%d/%d]",
                      st.map_cursor_x, st.map_cursor_y, static_cast<unsigned>(t->event), cond,
                      coll ? "80" : "--", st.map_triplet_scroll + 1, ntrip);
    } else {
        std::snprintf(line, sizeof(line), "(%d,%d) no evt  coll=%s", st.map_cursor_x, st.map_cursor_y,
                      coll ? "80" : "--");
    }
    drawTextRgb(c, 22, 1, line, 255, 255, 255);
    drawTextRgb(c, 23, 1, "Arrows [] Enter=warp ESC", 180, 180, 180);
}

void renderMonsters(gfx::ScreenCompositor &c, const DevMenuState &st,
                    const combat::CombatSession &combat)
{
    fillBlueWindow(c);
    drawText(c, 2, 12, "MONSTERS");

    char line[48];
    if (combat.active()) {
        std::snprintf(line, sizeof(line), "state=%s party=%d mon=%d",
                      combatStateName(combat.state()), combat.activePartySlot(),
                      combat.activeMonsterSlot());
        drawText(c, 4, 2, line);
    } else {
        drawText(c, 4, 2, "no fight");
    }

    const int action_count = static_cast<int>(DevMonsterRow::Count);
    for (int i = 0; i < action_count; ++i) {
        const bool sel = (i == st.monster_row);
        const DevMonsterRow kind = static_cast<DevMonsterRow>(i);
        if (kind == DevMonsterRow::KillSelected) {
            const combat::CombatMonster *m = combat.monsterSlot(st.monster_slot);
            const char letter = (st.monster_slot >= 0 && st.monster_slot < 10)
                                    ? static_cast<char>('A' + st.monster_slot)
                                    : '?';
            const bool live = m && m->alive;
            std::snprintf(line, sizeof(line), "%c %s  %c%s", sel ? '>' : ' ',
                          kMonsterActionLabels[i], letter, live ? "" : " (dead)");
        } else if (kind == DevMonsterRow::Invulnerable) {
            std::snprintf(line, sizeof(line), "%c %s  %s", sel ? '>' : ' ',
                          kMonsterActionLabels[i], combat.devInvulnerable() ? "ON" : "OFF");
        } else {
            std::snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', kMonsterActionLabels[i]);
        }
        if (sel) {
            drawTextRgb(c, 5 + i, 2, line, 255, 255, 100);
        } else {
            drawText(c, 5 + i, 2, line);
        }
    }

    if (combat.active()) {
        const gfx::CombatPanelView view = combat.panelView();
        int row = 12;
        for (int i = 0; i < MM2_GS_MONSTER_BATTLE_SLOTS && row <= 18; ++i) {
            const combat::CombatMonster *m = combat.monsterSlot(i);
            if (!m || (!m->alive && m->hp <= 0 && m->type == 0)) {
                continue;
            }
            char name[16] = "?";
            if (i < view.monster_line_count && view.monster_lines[i].occupied) {
                std::snprintf(name, sizeof(name), "%s", view.monster_lines[i].name);
            }
            const char letter = (i < 10) ? static_cast<char>('A' + i) : 'K';
            std::snprintf(line, sizeof(line), "%c%s %-10s HP %d/%d %s", letter,
                          (i == st.monster_slot) ? ">" : " ", name, m->hp, m->max_hp,
                          m->alive ? "live" : "dead");
            if (i == st.monster_slot) {
                drawTextRgb(c, row++, 2, line, 255, 255, 100);
            } else {
                drawText(c, row++, 2, line);
            }
        }
        if (view.overflow_more > 0) {
            std::snprintf(line, sizeof(line), "+%d more %s", view.overflow_more, view.overflow_name);
            drawText(c, row, 2, line);
        }
    }
    drawText(c, 19, 2, "Up/Dn  L/R target  Enter");
    drawText(c, 20, 2, "W/F/S/H/K  I invuln");
    if (st.status_line[0]) {
        drawTextRgb(c, 21, 2, st.status_line, 180, 255, 180);
    }
}

DevMenuAction runCheat(DevMenuState &st, GameStateView &gs, Mm2RosterFile &roster,
                       const Mm2PartyLaunch &launch, const world::MapWorld &world,
                       world::AutomapState &automap)
{
    const DevCheatRow row = static_cast<DevCheatRow>(st.cheat_row);
    char msg[48];
    switch (row) {
    case DevCheatRow::AddXp: {
        const int n = grantXpAll(roster, launch, grantAmountPreset(st.amount_idx_xp));
        std::snprintf(msg, sizeof(msg), "+%u XP x%d", grantAmountPreset(st.amount_idx_xp), n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::AddGold: {
        const int n = grantGoldAll(roster, launch, grantAmountPreset(st.amount_idx_gold));
        std::snprintf(msg, sizeof(msg), "+%u gold x%d", grantAmountPreset(st.amount_idx_gold), n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::AddGems: {
        const int n = grantGemsAll(roster, launch, grantAmountPreset(st.amount_idx_gems));
        std::snprintf(msg, sizeof(msg), "+%u gems x%d", grantAmountPreset(st.amount_idx_gems), n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::BoostStats: {
        const int n = boostStatsAll(roster, launch, statBoostPreset(st.amount_idx_stats));
        std::snprintf(msg, sizeof(msg), "+%u temp stats x%d", statBoostPreset(st.amount_idx_stats), n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::HealRevive: {
        const int n = healReviveAll(roster, launch);
        std::snprintf(msg, sizeof(msg), "healed x%d", n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::FillFood: {
        const int n = fillFoodAll(roster, launch);
        std::snprintf(msg, sizeof(msg), "food=40 x%d", n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::MaxSpells: {
        const int n = maxSpellsAll(roster, launch);
        std::snprintf(msg, sizeof(msg), "max spells x%d", n);
        setStatus(st, msg);
        break;
    }
    case DevCheatRow::PartyBuffs:
        applyPartyBuffs(gs);
        setStatus(st, "party buffs on");
        break;
    case DevCheatRow::RevealMap:
        if (world.loaded()) {
            automap.markScreenVisited(world.currentScreen());
            setStatus(st, "map revealed");
        } else {
            setStatus(st, "no map");
        }
        break;
    case DevCheatRow::UnlockHirelings:
        unlockHirelings(gs);
        setStatus(st, "hirelings A-X");
        break;
    default:
        break;
    }
    return DevMenuAction::None;
}

int nextAliveMonsterSlot(const combat::CombatSession &combat, int start, int dir)
{
    int slot = start;
    for (int n = 0; n < MM2_GS_MONSTER_BATTLE_SLOTS; ++n) {
        slot += dir;
        if (slot < 0) {
            slot = MM2_GS_MONSTER_BATTLE_SLOTS - 1;
        } else if (slot >= MM2_GS_MONSTER_BATTLE_SLOTS) {
            slot = 0;
        }
        const combat::CombatMonster *m = combat.monsterSlot(slot);
        if (m && m->alive) {
            return slot;
        }
    }
    const int first = combat.devFirstAliveSlot();
    return first >= 0 ? first : start;
}

DevMenuAction runMonsterAction(DevMenuState &st, GameStateView &gs, Mm2RosterFile &roster,
                               const Mm2PartyLaunch &launch, combat::CombatSession &combat)
{
    const DevMonsterRow row = static_cast<DevMonsterRow>(st.monster_row);
    char msg[48];
    if (row == DevMonsterRow::Invulnerable) {
        combat.setDevInvulnerable(!combat.devInvulnerable());
        setStatus(st, combat.devInvulnerable() ? "invuln ON" : "invuln OFF");
        return DevMenuAction::None;
    }
    if (row == DevMonsterRow::HealParty) {
        const int n = healReviveAll(roster, launch);
        std::snprintf(msg, sizeof(msg), "healed x%d", n);
        setStatus(st, msg);
        return DevMenuAction::None;
    }
    if (!combat.active()) {
        setStatus(st, "no fight");
        return DevMenuAction::None;
    }
    switch (row) {
    case DevMonsterRow::InstantWin:
        if (combat.devForceWin(gs)) {
            setStatus(st, "instant win");
            return DevMenuAction::Close;
        }
        setStatus(st, "win failed");
        break;
    case DevMonsterRow::InstantFlee:
        if (combat.devForceFlee(gs)) {
            setStatus(st, "instant flee");
            return DevMenuAction::Close;
        }
        setStatus(st, "flee failed");
        break;
    case DevMonsterRow::SleepAll: {
        const int n = combat.devSleepAll(gs);
        std::snprintf(msg, sizeof(msg), "slept x%d", n);
        setStatus(st, msg);
        break;
    }
    case DevMonsterRow::KillSelected:
        if (combat.devKillSlot(gs, st.monster_slot)) {
            setStatus(st, "killed");
            const int next = combat.devFirstAliveSlot();
            if (next >= 0) {
                st.monster_slot = next;
            }
            if (combat.state() == combat::CombatState::AwaitingVictoryDismiss) {
                return DevMenuAction::Close;
            }
        } else {
            setStatus(st, "kill failed");
        }
        break;
    default:
        break;
    }
    return DevMenuAction::None;
}

void cycleAmount(DevMenuState &st, int dir)
{
    const DevCheatRow row = static_cast<DevCheatRow>(st.cheat_row);
    auto cycle = [dir](int &idx, int count) {
        idx = (idx + dir) % count;
        if (idx < 0) {
            idx += count;
        }
    };
    switch (row) {
    case DevCheatRow::AddXp:
        cycle(st.amount_idx_xp, grantAmountPresetCount());
        break;
    case DevCheatRow::AddGold:
        cycle(st.amount_idx_gold, grantAmountPresetCount());
        break;
    case DevCheatRow::AddGems:
        cycle(st.amount_idx_gems, grantAmountPresetCount());
        break;
    case DevCheatRow::BoostStats:
        cycle(st.amount_idx_stats, statBoostPresetCount());
        break;
    default:
        break;
    }
}

void renderBattle(gfx::ScreenCompositor &c, const DevMenuState &st, const combat::CombatSession &combat,
                  const Mm2MonstersFile *monsters)
{
    fillBlueWindow(c);
    drawText(c, 2, 11, "BATTLE SPAWN");
    drawText(c, 3, 2, "Battle  [Tab: pages]");

    char type_buf[28];
    formatMonsterType(type_buf, sizeof(type_buf), st.battle_type, monsters);

    char line[48];
    const int count = static_cast<int>(DevBattleRow::Count);
    for (int i = 0; i < count; ++i) {
        const bool sel = (i == st.battle_row);
        const DevBattleRow kind = static_cast<DevBattleRow>(i);
        switch (kind) {
        case DevBattleRow::Random:
            std::snprintf(line, sizeof(line), "%c Random (this map)", sel ? '>' : ' ');
            break;
        case DevBattleRow::Difficulty:
            std::snprintf(line, sizeof(line), "%c Diff  %s", sel ? '>' : ' ',
                          devBattleDifficultyName(st.battle_difficulty));
            break;
        case DevBattleRow::Type:
            std::snprintf(line, sizeof(line), "%c Type  %s", sel ? '>' : ' ', type_buf);
            break;
        case DevBattleRow::Qty:
            std::snprintf(line, sizeof(line), "%c Count %d", sel ? '>' : ' ', st.battle_count);
            break;
        case DevBattleRow::SpawnFixed:
            std::snprintf(line, sizeof(line), "%c Spawn fixed", sel ? '>' : ' ');
            break;
        default:
            line[0] = '\0';
            break;
        }
        if (sel) {
            drawTextRgb(c, 5 + i, 2, line, 255, 255, 100);
        } else {
            drawText(c, 5 + i, 2, line);
        }
    }

    if (combat.active()) {
        drawTextRgb(c, 11, 2, "already in a fight", 255, 180, 120);
    }
    drawText(c, 18, 2, "Up/Dn  L/R type/count/diff");
    drawText(c, 19, 2, "Enter spawn  ESC/F11");
    if (st.status_line[0]) {
        drawTextRgb(c, 21, 2, st.status_line, 180, 255, 180);
    }
}

void renderMovement(gfx::ScreenCompositor &c, const DevMenuState &st)
{
    fillBlueWindow(c);
    drawText(c, 2, 12, "MOVEMENT");
    drawText(c, 3, 2, "Movement  [Tab: pages]");

    const bool flags[] = {st.noclip, st.no_encounters, st.skip_events};
    const int count = static_cast<int>(DevMoveRow::Count);
    char line[48];
    for (int i = 0; i < count; ++i) {
        const bool sel = (i == st.move_row);
        std::snprintf(line, sizeof(line), "%c %-18s %s", sel ? '>' : ' ', kMoveLabels[i],
                      flags[i] ? "ON" : "OFF");
        if (sel) {
            drawTextRgb(c, 5 + i, 2, line, 255, 255, 100);
        } else {
            drawText(c, 5 + i, 2, line);
        }
    }

    drawText(c, 18, 2, "Up/Dn  Enter toggle");
    drawText(c, 19, 2, "ESC/F11 close");
    if (st.status_line[0]) {
        drawTextRgb(c, 21, 2, st.status_line, 180, 255, 180);
    }
}

void renderTeleport(gfx::ScreenCompositor &c, const DevMenuState &st)
{
    fillBlueWindow(c);
    drawText(c, 2, 12, "TELEPORT");

    char dest[40];
    if (st.teleport_use_spawn) {
        std::snprintf(dest, sizeof(dest), "dest spawn  (%d,%d)", st.teleport_x, st.teleport_y);
    } else {
        std::snprintf(dest, sizeof(dest), "dest tile   (%d,%d)", st.teleport_x, st.teleport_y);
    }
    drawText(c, 3, 2, dest);

    constexpr int kVisible = 12;
    const int screen = clampDevScreenId(st.teleport_screen);
    int top = screen - (kVisible / 2);
    if (top < 0) {
        top = 0;
    }
    if (top > kDevMapScreenCount - kVisible) {
        top = std::max(0, kDevMapScreenCount - kVisible);
    }

    char line[40];
    for (int i = 0; i < kVisible; ++i) {
        const int id = top + i;
        if (id >= kDevMapScreenCount) {
            break;
        }
        const bool sel = (id == screen);
        const char *name = devMapName(id);
        if (name) {
            std::snprintf(line, sizeof(line), "%c %2d %s", sel ? '>' : ' ', id, name);
        } else {
            std::snprintf(line, sizeof(line), "%c %2d Area %d", sel ? '>' : ' ', id, id);
        }
        if (sel) {
            drawTextRgb(c, 5 + i, 2, line, 255, 255, 100);
        } else {
            drawText(c, 5 + i, 2, line);
        }
    }

    drawText(c, 18, 2, "Up/Dn map  L/R X  [] Y");
    drawText(c, 19, 2, "S spawn  Enter warp");
    if (st.status_line[0]) {
        drawTextRgb(c, 21, 2, st.status_line, 180, 255, 180);
    }
}

DevMenuAction runBattle(DevMenuState &st, GameStateView &gs, const combat::CombatSession &combat,
                        bool random_fight)
{
    if (combat.active()) {
        setStatus(st, "already fighting");
        return DevMenuAction::None;
    }
    if (!gs.valid()) {
        setStatus(st, "no game state");
        return DevMenuAction::None;
    }
    if (random_fight) {
        seedDevRandomEncounter(gs);
        char msg[48];
        std::snprintf(msg, sizeof(msg), "random %s",
                      devBattleDifficultyName(st.battle_difficulty));
        setStatus(st, msg);
    } else {
        seedDevFixedEncounter(gs, static_cast<uint8_t>(clampDevMonsterType(st.battle_type)),
                              st.battle_count);
        setStatus(st, "fixed fight");
    }
    return DevMenuAction::SpawnCombat;
}

void runMovementToggle(DevMenuState &st)
{
    const DevMoveRow row = static_cast<DevMoveRow>(st.move_row);
    switch (row) {
    case DevMoveRow::Noclip:
        st.noclip = !st.noclip;
        setStatus(st, st.noclip ? "no clip ON" : "no clip OFF");
        break;
    case DevMoveRow::NoEncounters:
        st.no_encounters = !st.no_encounters;
        setStatus(st, st.no_encounters ? "no fights ON" : "no fights OFF");
        break;
    case DevMoveRow::SkipEvents:
        st.skip_events = !st.skip_events;
        setStatus(st, st.skip_events ? "skip events ON" : "skip events OFF");
        break;
    default:
        break;
    }
}

DevMenuAction runTeleport(DevMenuState &st, const combat::CombatSession &combat)
{
    if (combat.active()) {
        setStatus(st, "end fight first");
        return DevMenuAction::None;
    }
    st.teleport_screen = clampDevScreenId(st.teleport_screen);
    st.teleport_x &= 0x0F;
    st.teleport_y &= 0x0F;
    setStatus(st, "teleport");
    return DevMenuAction::Teleport;
}

}  // namespace

void DevMenu::reset(DevMenuState &st) const
{
    st.page = DevMenuPage::Cheats;
    st.cheat_row = 0;
    st.monster_row = 0;
    st.monster_slot = 0;
    st.map_cursor_seeded = false;
    st.map_triplet_scroll = 0;
    st.enter_was_down = false;
    st.spell_party_slot = 0;
    st.spell_row = 0;
    st.battle_row = 0;
    st.move_row = 0;
    st.status_line[0] = '\0';
}

void DevMenu::render(gfx::ScreenCompositor &c, DevMenuState &st, GameStateView &gs,
                     Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                     const world::MapWorld &world, world::AutomapState &automap,
                     const gfx::EnvAssets &env, const events::EventRuntime &events,
                     const combat::CombatSession &combat, const Mm2MonstersFile *monsters,
                     const char *screen_label) const
{
    switch (st.page) {
    case DevMenuPage::Cheats:
        renderCheats(c, st);
        break;
    case DevMenuPage::Battle:
        renderBattle(c, st, combat, monsters);
        break;
    case DevMenuPage::Movement:
        renderMovement(c, st);
        break;
    case DevMenuPage::Teleport:
        renderTeleport(c, st);
        break;
    case DevMenuPage::StatusVm:
        renderStatusVm(c, st, gs, roster, launch, events, screen_label);
        break;
    case DevMenuPage::MapEvents:
        renderMapEvents(c, st, gs, world, automap, env, events);
        break;
    case DevMenuPage::Monsters:
        renderMonsters(c, st, combat);
        break;
    case DevMenuPage::Spells:
        renderSpells(c, st, roster, launch);
        break;
    default:
        break;
    }
}

DevMenuAction DevMenu::handleKey(const platform::KeyState &keys, DevMenuState &st, GameStateView &gs,
                                 Mm2RosterFile &roster, const Mm2PartyLaunch &launch,
                                 const world::MapWorld &world, world::AutomapState &automap,
                                 const events::EventRuntime &events,
                                 combat::CombatSession &combat) const
{
    /* SDL keeps enter level-triggered; edge so grants/warp fire once per press. */
    const bool enter_edge = keys.enter && !st.enter_was_down;
    st.enter_was_down = keys.enter;

    if (keys.dev_menu || keys.escape) {
        return DevMenuAction::Close;
    }
    if (keys.tab) {
        const int next = (static_cast<int>(st.page) + 1) % static_cast<int>(DevMenuPage::Count);
        st.page = static_cast<DevMenuPage>(next);
        if (st.page == DevMenuPage::MapEvents) {
            seedMapCursor(st, gs);
        }
        if (st.page == DevMenuPage::Monsters && combat.active()) {
            const int first = combat.devFirstAliveSlot();
            if (first >= 0) {
                st.monster_slot = first;
            }
        }
        if (st.page == DevMenuPage::Teleport && st.teleport_use_spawn) {
            applyTeleportSpawnPreview(st, world);
        }
        return DevMenuAction::None;
    }

    if (st.page == DevMenuPage::Cheats) {
        if (keys.up) {
            st.cheat_row =
                (st.cheat_row + static_cast<int>(DevCheatRow::Count) - 1) %
                static_cast<int>(DevCheatRow::Count);
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.cheat_row = (st.cheat_row + 1) % static_cast<int>(DevCheatRow::Count);
            return DevMenuAction::None;
        }
        if (keys.left || keys.last_ascii == '-' || keys.last_ascii == '_') {
            cycleAmount(st, -1);
            return DevMenuAction::None;
        }
        if (keys.right || keys.last_ascii == '+' || keys.last_ascii == '=') {
            cycleAmount(st, 1);
            return DevMenuAction::None;
        }
        if (enter_edge) {
            return runCheat(st, gs, roster, launch, world, automap);
        }
        return DevMenuAction::None;
    }

    if (st.page == DevMenuPage::Battle) {
        if (keys.up) {
            st.battle_row =
                (st.battle_row + static_cast<int>(DevBattleRow::Count) - 1) %
                static_cast<int>(DevBattleRow::Count);
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.battle_row = (st.battle_row + 1) % static_cast<int>(DevBattleRow::Count);
            return DevMenuAction::None;
        }
        if (keys.left || keys.last_ascii == '-' || keys.last_ascii == '_') {
            if (st.battle_row == static_cast<int>(DevBattleRow::Type)) {
                st.battle_type = (st.battle_type + 255) & 0xFF;
            } else if (st.battle_row == static_cast<int>(DevBattleRow::Qty)) {
                st.battle_count = clampDevMonsterCount(st.battle_count - 1);
            } else if (st.battle_row == static_cast<int>(DevBattleRow::Random) ||
                       st.battle_row == static_cast<int>(DevBattleRow::Difficulty)) {
                st.battle_difficulty =
                    (st.battle_difficulty + kDevBattleDifficultyCount - 1) %
                    kDevBattleDifficultyCount;
            }
            return DevMenuAction::None;
        }
        if (keys.right || keys.last_ascii == '+' || keys.last_ascii == '=') {
            if (st.battle_row == static_cast<int>(DevBattleRow::Type)) {
                st.battle_type = (st.battle_type + 1) & 0xFF;
            } else if (st.battle_row == static_cast<int>(DevBattleRow::Qty)) {
                st.battle_count = clampDevMonsterCount(st.battle_count + 1);
            } else if (st.battle_row == static_cast<int>(DevBattleRow::Random) ||
                       st.battle_row == static_cast<int>(DevBattleRow::Difficulty)) {
                st.battle_difficulty =
                    (st.battle_difficulty + 1) % kDevBattleDifficultyCount;
            }
            return DevMenuAction::None;
        }
        if (keys.last_ascii == '[' && st.battle_row == static_cast<int>(DevBattleRow::Type)) {
            st.battle_type = (st.battle_type + 256 - 16) & 0xFF;
            return DevMenuAction::None;
        }
        if (keys.last_ascii == ']' && st.battle_row == static_cast<int>(DevBattleRow::Type)) {
            st.battle_type = (st.battle_type + 16) & 0xFF;
            return DevMenuAction::None;
        }
        if (enter_edge) {
            const int row = st.battle_row;
            const bool random_fight = (row == static_cast<int>(DevBattleRow::Random) ||
                                       row == static_cast<int>(DevBattleRow::Difficulty));
            return runBattle(st, gs, combat, random_fight);
        }
        return DevMenuAction::None;
    }

    if (st.page == DevMenuPage::Movement) {
        if (keys.up) {
            st.move_row = (st.move_row + static_cast<int>(DevMoveRow::Count) - 1) %
                          static_cast<int>(DevMoveRow::Count);
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.move_row = (st.move_row + 1) % static_cast<int>(DevMoveRow::Count);
            return DevMenuAction::None;
        }
        if (enter_edge) {
            runMovementToggle(st);
            if (st.skip_events && gs.valid()) {
                mm2_gs_set_u8(gs.a4(), MM2_GS_PENDING_EVENT_LATCH, 0);
            }
            return DevMenuAction::None;
        }
        return DevMenuAction::None;
    }

    if (st.page == DevMenuPage::Teleport) {
        if (keys.up) {
            st.teleport_screen =
                (st.teleport_screen + kDevMapScreenCount - 1) % kDevMapScreenCount;
            if (st.teleport_use_spawn) {
                applyTeleportSpawnPreview(st, world);
            }
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.teleport_screen = (st.teleport_screen + 1) % kDevMapScreenCount;
            if (st.teleport_use_spawn) {
                applyTeleportSpawnPreview(st, world);
            }
            return DevMenuAction::None;
        }
        if (keys.left) {
            st.teleport_use_spawn = false;
            st.teleport_x = (st.teleport_x + 15) & 0x0F;
            return DevMenuAction::None;
        }
        if (keys.right) {
            st.teleport_use_spawn = false;
            st.teleport_x = (st.teleport_x + 1) & 0x0F;
            return DevMenuAction::None;
        }
        char ch = keys.last_ascii;
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 32);
        }
        if (ch == '[') {
            st.teleport_use_spawn = false;
            st.teleport_y = (st.teleport_y + 15) & 0x0F;
            return DevMenuAction::None;
        }
        if (ch == ']') {
            st.teleport_use_spawn = false;
            st.teleport_y = (st.teleport_y + 1) & 0x0F;
            return DevMenuAction::None;
        }
        if (ch == 'S') {
            st.teleport_use_spawn = !st.teleport_use_spawn;
            if (st.teleport_use_spawn) {
                applyTeleportSpawnPreview(st, world);
            }
            return DevMenuAction::None;
        }
        if (enter_edge) {
            return runTeleport(st, combat);
        }
        return DevMenuAction::None;
    }

    if (st.page == DevMenuPage::MapEvents) {
        seedMapCursor(st, gs);
        if (keys.left) {
            st.map_cursor_x = (st.map_cursor_x + 15) & 0x0F;
            st.map_triplet_scroll = 0;
            return DevMenuAction::None;
        }
        if (keys.right) {
            st.map_cursor_x = (st.map_cursor_x + 1) & 0x0F;
            st.map_triplet_scroll = 0;
            return DevMenuAction::None;
        }
        if (keys.up) {
            st.map_cursor_y = (st.map_cursor_y + 1) & 0x0F;
            st.map_triplet_scroll = 0;
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.map_cursor_y = (st.map_cursor_y + 15) & 0x0F;
            st.map_triplet_scroll = 0;
            return DevMenuAction::None;
        }
        if (keys.last_ascii == '[') {
            if (st.map_triplet_scroll > 0) {
                --st.map_triplet_scroll;
            }
            return DevMenuAction::None;
        }
        if (keys.last_ascii == ']') {
            const Mm2EventLocation *loc = events.currentLocation();
            const int n = countTripletsAt(loc, st.map_cursor_x, st.map_cursor_y);
            if (st.map_triplet_scroll + 1 < n) {
                ++st.map_triplet_scroll;
            }
            return DevMenuAction::None;
        }
        if (enter_edge && gs.valid()) {
            gs.setCoordX(static_cast<uint8_t>(st.map_cursor_x & 0x0F));
            gs.setCoordY(static_cast<uint8_t>(st.map_cursor_y & 0x0F));
            setStatus(st, "warped");
            return DevMenuAction::None;
        }
    }

    if (st.page == DevMenuPage::Monsters) {
        char ch = keys.last_ascii;
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 32);
        }
        if (keys.up) {
            st.monster_row =
                (st.monster_row + static_cast<int>(DevMonsterRow::Count) - 1) %
                static_cast<int>(DevMonsterRow::Count);
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.monster_row = (st.monster_row + 1) % static_cast<int>(DevMonsterRow::Count);
            return DevMenuAction::None;
        }
        if (keys.left) {
            st.monster_slot = nextAliveMonsterSlot(combat, st.monster_slot, -1);
            return DevMenuAction::None;
        }
        if (keys.right) {
            st.monster_slot = nextAliveMonsterSlot(combat, st.monster_slot, 1);
            return DevMenuAction::None;
        }
        if (ch == 'W') {
            st.monster_row = static_cast<int>(DevMonsterRow::InstantWin);
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        if (ch == 'F') {
            st.monster_row = static_cast<int>(DevMonsterRow::InstantFlee);
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        if (ch == 'S') {
            st.monster_row = static_cast<int>(DevMonsterRow::SleepAll);
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        if (ch == 'H') {
            st.monster_row = static_cast<int>(DevMonsterRow::HealParty);
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        if (ch == 'K') {
            st.monster_row = static_cast<int>(DevMonsterRow::KillSelected);
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        if (ch == 'I') {
            st.monster_row = static_cast<int>(DevMonsterRow::Invulnerable);
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        if (enter_edge) {
            return runMonsterAction(st, gs, roster, launch, combat);
        }
        return DevMenuAction::None;
    }

    if (st.page == DevMenuPage::Spells) {
        clampSpellPartySlot(st, launch);
        if (keys.up) {
            st.spell_row = (st.spell_row + kSpellsPerSchool - 1) % kSpellsPerSchool;
            return DevMenuAction::None;
        }
        if (keys.down) {
            st.spell_row = (st.spell_row + 1) % kSpellsPerSchool;
            return DevMenuAction::None;
        }
        if (keys.left && launch.party_count > 0) {
            st.spell_party_slot =
                (st.spell_party_slot + launch.party_count - 1) % launch.party_count;
            return DevMenuAction::None;
        }
        if (keys.right && launch.party_count > 0) {
            st.spell_party_slot = (st.spell_party_slot + 1) % launch.party_count;
            return DevMenuAction::None;
        }
        const char ch = keys.last_ascii;
        if (ch >= '1' && ch <= '8') {
            const int slot = ch - '1';
            if (slot < launch.party_count) {
                st.spell_party_slot = slot;
            }
            return DevMenuAction::None;
        }
        return DevMenuAction::None;
    }

    (void)events;
    return DevMenuAction::None;
}

}  // namespace mm2::gameplay
