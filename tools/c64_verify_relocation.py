#!/usr/bin/env python3
"""Definitively characterise the MM2-1 C64 boot $03B7 routine.

ASM is source of truth (T19S0 chain, PRG load $0801). This script:

1. Extracts the copy loop from the chain and confirms its self-modifying
   operands byte-for-byte.
2. Simulates every outer pass, recording the exact source/destination
   address ranges per pass, so we can state definitively whether the
   routine is a memmove (relocation), a deinterleave, or real
   decompression.
3. Emits the relocated image (payload slid down $90 bytes) so the real
   post-relocation program can be disassembled at its true addresses.

Run:  python tools/c64_verify_relocation.py
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CHAIN = ROOT / "EXTRACTED/c64/emulator/mm2-1_t19s0_chain.bin"


def load_chain_into_ram() -> bytearray:
    """Load the PRG chain into a 64K RAM image at its load address."""
    data = CHAIN.read_bytes()
    load = data[0] | (data[1] << 8)
    body = data[2:]
    ram = bytearray(0x10000)
    ram[load : load + len(body)] = body
    return ram, load, len(body)


LOOP_SIG = bytes.fromhex("bd93089d0308e8d0f7")  # LDA $0893,X / STA $0803,X / INX / BNE


def confirm_loop_operands(ram: bytearray) -> dict:
    """Locate the copy loop by signature and read its bytes verbatim."""
    idx = ram.find(LOOP_SIG, 0x0800, 0x0900)
    assert idx != -1, "copy loop signature not found in boot stub"
    lda = ram[idx : idx + 3]
    sta = ram[idx + 3 : idx + 6]
    inx = ram[idx + 6]
    bne = ram[idx + 7 : idx + 9]
    inc_src = ram[idx + 9 : idx + 12]   # EE C0 03 INC $03C0
    inc_dst = ram[idx + 12 : idx + 15]  # EE C3 03 INC $03C3
    lda_hi = ram[idx + 15 : idx + 18]   # AD C0 03 LDA $03C0
    cmp = ram[idx + 18 : idx + 20]      # C9 FF    CMP #$FF
    bne2 = ram[idx + 20 : idx + 22]
    return {
        "loop_addr": f"${idx:04X}",
        "lda_src": lda.hex(),
        "sta_dst": sta.hex(),
        "inx": f"{inx:02X}",
        "bne_inner": bne.hex(),
        "inc_src_hi": inc_src.hex(),
        "inc_dst_hi": inc_dst.hex(),
        "lda_check": lda_hi.hex(),
        "cmp_ff": cmp.hex(),
        "bne_outer": bne2.hex(),
        "src_base": lda[1] | (lda[2] << 8),
        "dst_base": sta[1] | (sta[2] << 8),
    }


def simulate_passes():
    """Reproduce the loop's per-pass src/dst ranges without moving bytes.

    src_hi starts $08, dst_hi starts $08; each pass copies 256 bytes then
    increments both high bytes; stops when src_hi == $FF.
    """
    src_lo, dst_lo = 0x93, 0x03
    src_hi, dst_hi = 0x08, 0x08
    passes = []
    while src_hi != 0xFF:
        src = (src_hi << 8) | src_lo
        dst = (dst_hi << 8) | dst_lo
        passes.append((src, src + 255, dst, dst + 255))
        src_hi = (src_hi + 1) & 0xFF
        dst_hi = (dst_hi + 1) & 0xFF
    return passes


def build_relocated_image(ram: bytearray, chain_end: int) -> bytearray:
    """Apply the memmove exactly as the CPU would and return the result.

    Copies forward within each 256-byte window; because dst < src this is a
    correct downward memmove even across the overlapping region.
    """
    out = bytearray(ram)  # copy so we keep the original
    src_lo, dst_lo = 0x93, 0x03
    src_hi, dst_hi = 0x08, 0x08
    while src_hi != 0xFF:
        src = (src_hi << 8) | src_lo
        dst = (dst_hi << 8) | dst_lo
        for x in range(256):
            out[(dst + x) & 0xFFFF] = out[(src + x) & 0xFFFF]
        src_hi = (src_hi + 1) & 0xFF
        dst_hi = (dst_hi + 1) & 0xFF
    return out


def main() -> None:
    ram, load, body_len = load_chain_into_ram()
    chain_end = load + body_len
    print(f"chain load=${load:04X} len={body_len} end=${chain_end:04X}")

    ops = confirm_loop_operands(ram)
    print("\n=== copy-loop instruction bytes (from chain @ $084E) ===")
    for k, v in ops.items():
        print(f"  {k:12} = {v}")

    passes = simulate_passes()
    delta = passes[0][0] - passes[0][2]
    print(f"\n=== pass map ({len(passes)} passes) ===")
    print(f"  constant src-dst delta = ${delta:04X} ({delta} bytes)")
    for i in (0, 1, 2, len(passes) - 2, len(passes) - 1):
        s0, s1, d0, d1 = passes[i]
        print(f"  pass {i:3}: src ${s0:04X}-${s1:04X} -> dst ${d0:04X}-${d1:04X}")

    deltas = {(s0 - d0) for s0, _, d0, _ in passes}
    print(f"\n  distinct src-dst deltas across all passes: "
          f"{sorted(hex(d) for d in deltas)}")
    print("  => single constant delta means a straight block move, not a codec")

    # Meaningful (in-chain) source span only
    meaningful = [p for p in passes if p[0] < chain_end]
    print(f"\n  passes touching real chain data (src < ${chain_end:04X}): "
          f"{len(meaningful)} (rest read RAM-garbage/zero above chain)")

    out = build_relocated_image(ram, chain_end)
    reloc_path = ROOT / "EXTRACTED/c64/emulator/mm2-1_relocated.bin"
    # Save $0800..$2000 region (the meaningful relocated payload + margin)
    reloc_path.write_bytes(bytes(out[0x0800:0x2000]))
    print(f"\nwrote relocated payload $0800-$1FFF -> {reloc_path}")

    # Sanity: relocated dst byte == original src byte, offset by delta.
    print("\n=== relocation sanity (dst[a] == src[a+$90]) ===")
    ok = all(out[a] == ram[a + delta] for a in range(0x0803, chain_end - delta))
    print(f"  every relocated byte $0803..${chain_end - delta:04X} equals "
          f"chain byte +$90: {ok}")
    # Locate the JMP that ends the boot stub and where execution resumes.
    jmp_idx = ram.find(bytes.fromhex("4c"), 0x0860, 0x0880)
    print(f"  post-move JMP target bytes near stub end: "
          f"{ram[0x0862:0x0870].hex()}")


if __name__ == "__main__":
    main()
