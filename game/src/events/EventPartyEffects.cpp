#include "mm2/events/EventPartyEffects.h"

#include "mm2/events/EventFieldMap.h"
#include "mm2/events/EventVmHelpers.h"

#include "mm2/CppStdCompat.h"

namespace mm2::events {

namespace {

enum class FieldKind : uint8_t { Byte, Word, Long };

struct FieldSpec {
    int offset;
    FieldKind kind;
};

// OP_1F/20 (event party value op @ 0x1690E -> per-member applier @ 0x167B0)
// does NOT set bit7 on the selector, so the field engine (0x17766) accesses the
// field at its natural width. Resolve the script selector to its real record
// offset+width via the ROM jump table @ 0x17FEA (EventFieldMap.h). Returns false
// for the two computed getters (sel 0x00/0x01 = base max HP/SP @ 0x181B0), which
// have no writable record offset.
bool resolveField(uint8_t selector, FieldSpec *out)
{
    if (!out) {
        return false;
    }
    int off = -1;
    int width = 1;
    if (!eventVmResolveMemberField(selector, &off, &width)) {
        return false;
    }
    out->offset = off;
    out->kind = (width == 4) ? FieldKind::Long : (width == 2) ? FieldKind::Word : FieldKind::Byte;
    return true;
}

Mm2RosterRecord *memberBySlot(Mm2RosterFile *roster, const Mm2PartyLaunch *launch, int slot_0_to_7)
{
    if (!roster || !launch || slot_0_to_7 < 0 || slot_0_to_7 >= launch->party_count) {
        return nullptr;
    }
    const int idx = launch->roster_slots[slot_0_to_7];
    if (idx < 0 || idx >= MM2_ROSTER_RECORD_COUNT ||
        mm2_roster_slot_is_empty(&roster->records[idx])) {
        return nullptr;
    }
    return &roster->records[idx];
}

uint32_t fieldLoad(const Mm2RosterRecord &rec, const FieldSpec &spec)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&rec) + spec.offset;
    switch (spec.kind) {
    case FieldKind::Byte:
        return p[0];
    case FieldKind::Word:
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8);
    case FieldKind::Long:
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }
    return 0;
}

uint32_t fieldMax(FieldKind kind)
{
    switch (kind) {
    case FieldKind::Byte:
        return 0xFFu;
    case FieldKind::Word:
        return 0xFFFFu;
    case FieldKind::Long:
        return 0xFFFFFFFFu;
    }
    return 0;
}

// Writeback @ 0x168CA..0x16908: script width_op selects how many bytes are
// stored (3→4). Bytes are taken from the low end of the LE result — matching
// the ASM's BE scratch at -$4(a5) + start_index=(4-width_op). May write past
// the natural field width into adjacent record bytes when width_op > field width.
void fieldStoreWidth(Mm2RosterRecord &rec, int offset, uint32_t v, int write_bytes)
{
    if (write_bytes <= 0) {
        return;
    }
    if (write_bytes > 4) {
        write_bytes = 4;
    }
    auto *raw = reinterpret_cast<uint8_t *>(&rec);
    if (offset < 0 || offset + write_bytes > MM2_ROSTER_RECORD_SIZE) {
        return;
    }
    for (int i = 0; i < write_bytes; ++i) {
        raw[offset + i] = static_cast<uint8_t>(v >> (8 * i));
    }
}

// Writeback byte count from script width_op @ 0x168CA (cmpi #3 → addq → 4).
int writebackBytes(uint8_t width_op)
{
    return (width_op == 3) ? 4 : static_cast<int>(width_op);
}

// One member's arithmetic, faithfully replicating event_party_effect_apply @
// 0x167B0: add (mode 0) saturates at the field-width max; subtract (mode 1)
// fails atomically when the field can't cover the amount — it clears cond_flag
// and writes nothing. The caller pre-sets cond_flag=1; the writeback is gated on
// cond_flag for *every* member (so once a subtract underflows, later members in
// an all-party op are also skipped, matching the ASM's shared -$7951 gate).
// Arithmetic uses the field-map natural width; store uses script width_op.
void applyMemberValueOp(GameStateView &gs, Mm2RosterRecord &rec, const FieldSpec &spec,
                        uint32_t amount, bool subtract, int write_bytes)
{
    const uint32_t cur = fieldLoad(rec, spec);
    uint32_t newval;
    if (subtract) {
        if (cur < amount) {
            mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 0);
            return;  // can't afford: no write
        }
        newval = cur - amount;
    } else {
        const uint64_t sum = static_cast<uint64_t>(cur) + amount;
        const uint32_t cap = fieldMax(spec.kind);
        newval = (sum > cap) ? cap : static_cast<uint32_t>(sum);
    }
    if (mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG) != 0) {
        fieldStoreWidth(rec, spec.offset, newval, write_bytes);
    }
}

// Mask the 24-bit script value to the field width (the ASM hands 0x167B0 the
// value pre-decomposed into byte/word/long views and selects by field type).
uint32_t amountForField(uint32_t value24, FieldKind kind)
{
    switch (kind) {
    case FieldKind::Byte:
        return value24 & 0xFFu;
    case FieldKind::Word:
        return value24 & 0xFFFFu;
    case FieldKind::Long:
        return value24;
    }
    return 0;
}

}  // namespace

void eventApplyPartyEffect(GameStateView &gs, Mm2RosterFile *roster, const Mm2PartyLaunch *launch,
                           uint8_t sel, const uint8_t args[5], bool mode_b)
{
    if (!args || !roster || !launch) {
        return;
    }

    const uint8_t selector = args[0];                  // field selector (-$5d3e)
    const uint8_t width_op = args[1];                   // writeback byte count @ 0x168CA
    const uint32_t value24 =                            // 3-byte LE value ($155FC)
        static_cast<uint32_t>(args[2]) | (static_cast<uint32_t>(args[3]) << 8) |
        (static_cast<uint32_t>(args[4]) << 16);
    const bool subtract = mode_b;                       // OP_20 = subtract

    // event_op1f_party_effect @ 0x1690E member selection.
    const uint8_t incoming_cond = mm2_gs_u8(gs.a4(), MM2_GS_COND_FLAG);
    uint8_t member_spec = sel;
    uint32_t amount_value = value24;
    if (member_spec & 0x80) {
        // bit7 variant: the byte addend is the live cond_flag, not the value.
        member_spec = static_cast<uint8_t>(member_spec & 0x7F);
        amount_value = incoming_cond;
    }
    if (member_spec == 0x09) {
        const int slot = eventVmSelectedPartySlot(gs.a4());
        member_spec = (slot >= 0) ? static_cast<uint8_t>(slot + 1) : incoming_cond;
    }

    mm2_gs_set_u8(gs.a4(), MM2_GS_COND_FLAG, 1);

    const int party_count = launch->party_count;
    if (member_spec > party_count) {
        return;  // out of range: no-op (cond stays 1, matching addq #5 @ 0x16A1A)
    }

    FieldSpec spec{};
    if (!resolveField(selector, &spec)) {
        return;  // computed getter selector — nothing writable
    }
    const uint32_t amount = amountForField(amount_value, spec.kind);
    const int wb = writebackBytes(width_op);

    if (member_spec == 0) {
        // member_spec 0 -> all party members (loop 1..party_count @ 0x169DA).
        for (int i = 0; i < party_count && i < MM2_PARTY_LAUNCH_SLOTS; ++i) {
            if (Mm2RosterRecord *rec = memberBySlot(roster, launch, i)) {
                applyMemberValueOp(gs, *rec, spec, amount, subtract, wb);
            }
        }
    } else if (Mm2RosterRecord *rec = memberBySlot(roster, launch, member_spec - 1)) {
        applyMemberValueOp(gs, *rec, spec, amount, subtract, wb);
    }
}

}  // namespace mm2::events
