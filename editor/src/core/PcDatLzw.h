#pragma once
// PC/GOG .DAT LZW (same codec as PcGfx.h, MM2.EXE @0x2A42).
//
//   flat  — u32 LE decompressed_size @ 0, LZW @ 4. ATTRIB / MONSTERS / STR.
//   table — LE offset table; entry[0] == table byte size; 0 = empty slot.
//           MAP.DAT: u16 LE[60], each blob → 512 bytes.
//           EVENTSI.DAT / EVENTSO.DAT: u32 LE[71] indoor/outdoor halves of event.dat.
//   plain — ITEMS.DAT; ROSTER.DAT is 8292 bytes (Amiga 8320 with 28-byte EOF pad).

#include <string>

#include "core/ByteIO.h"
#include "core/EventOps.h"

namespace mm2 {

// --- flat container (ATTRIB.DAT / MONSTERS.DAT / STR.DAT) ------------------

// True if `raw` is a flat LZW container (u32 LE size @ 0, stream @ 4).
bool pcDatIsFlatLzw(const Bytes& raw);

// Decompress flat LZW, or return `raw` unchanged if it is not that container.
Bytes pcDatDecompressFlat(const Bytes& raw);

// --- table container: MAP.DAT ------------------------------------------------

constexpr int kPcMapScreenCount = 60;
constexpr int kPcMapScreenSize = 512;

// True if `raw` is MAP.DAT's 60-entry u16 offset table (each blob = 512 bytes).
bool pcDatIsMapTable(const Bytes& raw);

// Reassemble 60×512 map.dat from GOG MAP.DAT; returns `raw` if it doesn't match.
Bytes pcDatDecompressMap(const Bytes& raw);

// --- table container: EVENTSI.DAT / EVENTSO.DAT -----------------------------

// True if `raw` is EVENTSI.DAT/EVENTSO.DAT's 71-entry u32 offset table.
bool pcDatIsEventTableHalf(const Bytes& raw);

// Merge indoor+outdoor halves into Amiga event.dat (71 × BE offset/length).
// Empty if a slot is present in both, or either input is the wrong shape.
Bytes pcDatMergeEvent(const Bytes& indoorRaw, const Bytes& outdoorRaw);

// Load dir/event.dat, or merge EVENTSI.DAT + EVENTSO.DAT.
bool pcDatLoadEventAuto(const std::string& dir, Bytes& out);

// --- misc --------------------------------------------------------------------

// Read `path`, then the upper-cased filename in the same directory (GOG ALL-CAPS).
bool pcDatReadFlexible(const std::string& path, Bytes& out);

}  // namespace mm2
