#pragma once
// Decompression for MM2 PC/GOG .DAT files.
//
// The GOG DOS build wraps *some* (not all) .DAT files in the same custom LZW
// codec used by the PC .4/.16 graphics (pcLzwDecompress in PcGfx.h, traced to
// MM2.EXE @0x2A42). See tools/pc_dat_lzw.py (source of truth for this codec,
// validated against a real GOG install) and
// EXTRACTED/docs/07-dat-files-and-formats.md for the full write-up.
//
// Container shapes:
//   flat  -- whole file is one LZW blob: u32 LE decompressed_size @ 0, LZW
//            bitstream @ 4. Used by ATTRIB.DAT, MONSTERS.DAT, STR.DAT.
//            Confirmed byte-identical (ATTRIB/MONSTERS) or a clean but
//            platform-different-length decode (STR) vs the Amiga files.
//   table -- a flat table of LE offsets where entry[0] == table byte size
//            (MONSTERS.4/.16 combat-atlas convention); entry[k] == 0 means
//            "no data for slot k". Each non-zero entry -> a blob: u32 LE
//            decompressed_size + LZW bitstream.
//              MAP.DAT: u16 LE[60] table, one 512-byte screen per slot;
//                reassembling all 60 gives byte-identical map.dat.
//              EVENTSI.DAT / EVENTSO.DAT: u32 LE[71] tables (indoor /
//                outdoor halves of event.dat's 71 locations, mutually
//                exclusive per slot); merging reproduces event.dat to
//                within 18 bytes out of 95687 (genuine platform content
//                differences, not decode errors).
//   plain -- ITEMS.DAT is already byte-identical to items.dat. ROSTER.DAT is also
//            plain: 8292 bytes (Amiga 8320 minus 28 zero pad bytes at EOF).
//            Same 130-byte records + 2052-byte global stream; see RosterFile.h.
//            SPELLS.DAT / DEFAULT.DAT remain PC-specific sizes — not handled here.

#include <string>

#include "core/ByteIO.h"

namespace mm2 {

// --- flat container (ATTRIB.DAT / MONSTERS.DAT / STR.DAT) ------------------

// True if `raw` looks like the flat LZW dat container: a plausible u32 LE
// size header @ 0 whose LZW stream (@ 4) fully decompresses to exactly that
// many bytes.
bool pcDatIsFlatLzw(const Bytes& raw);

// Decompress the flat container. Returns `raw` unchanged if it doesn't look
// like a valid flat LZW container (e.g. already-plain files) -- safe to call
// unconditionally.
Bytes pcDatDecompressFlat(const Bytes& raw);

// --- table container: MAP.DAT ------------------------------------------------

constexpr int kPcMapScreenCount = 60;
constexpr int kPcMapScreenSize = 512;

// True if `raw` matches MAP.DAT's 60-entry u16 offset table container (every
// slot's blob must decompress to exactly 512 bytes).
bool pcDatIsMapTable(const Bytes& raw);

// Reassemble the flat 60x512-byte map.dat from GOG MAP.DAT's per-screen LZW
// table. Returns `raw` unchanged if it doesn't match.
Bytes pcDatDecompressMap(const Bytes& raw);

// --- table container: EVENTSI.DAT / EVENTSO.DAT -----------------------------

constexpr int kPcEventLocationCount = 71;  // == kEventLocationCount (EventOps.h)

// True if `raw` matches the EVENTSI.DAT/EVENTSO.DAT 71-entry u32 offset
// table shape (used by both the indoor and outdoor half; slot 0 may be
// empty, unlike MAP.DAT).
bool pcDatIsEventTableHalf(const Bytes& raw);

// Merge EVENTSI.DAT (indoor) + EVENTSO.DAT (outdoor) into an Amiga-style
// event.dat blob: a 71 x (u32 BE offset, u16 BE length) header followed by
// the concatenated per-location decompressed blobs. Each location must have
// data in at most one of the two inputs. Returns an empty Bytes if either
// input doesn't match the expected table shape or a slot is defined in both.
Bytes pcDatMergeEvent(const Bytes& indoorRaw, const Bytes& outdoorRaw);

// Looks for `dir/event.dat` first (case-insensitive); if missing, looks for
// `dir/EVENTSI.DAT` + `dir/EVENTSO.DAT` and merges them via pcDatMergeEvent.
// Returns false if neither is found/valid.
bool pcDatLoadEventAuto(const std::string& dir, Bytes& out);

// --- misc --------------------------------------------------------------------

// Case-flexible file read: tries `path` as-is, then the upper-cased filename
// in the same directory (GOG ships ALL-CAPS names; this matters on
// case-sensitive filesystems -- Windows/macOS are already case-insensitive).
bool pcDatReadFlexible(const std::string& path, Bytes& out);

}  // namespace mm2
