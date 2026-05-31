#!/usr/bin/env python3
"""Annotate EXTRACTED/mm2.capstone.asm with known symbol labels.

This tool is idempotent and re-runnable. Each inserted banner is wrapped in
`;@LBL-START` / `;@LBL-END` sentinel lines, so on every run the previously
inserted banners are stripped first and then re-emitted from the LABELS table
below. To add a new label, just add an entry to LABELS and re-run.

Address keys are CAPSTONE addresses (code-hunk relative, segment 0 base 0) -
the value printed at the start of each disassembly line. They are NOT the same
as the IRA listing addresses (which are ~+0x20 and occasionally off-by-2 where
Capstone mis-decodes embedded data/padding).

Each label was verified to land on a real instruction boundary in the current
capstone file unless noted otherwise.
"""

from __future__ import annotations

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ASM = os.path.normpath(os.path.join(HERE, "..", "EXTRACTED", "mm2.capstone.asm"))

START = ";@LBL-START"
END = ";@LBL-END"

# addr (int) -> (name, [description lines])
LABELS: dict[int, tuple[str, list[str]]] = {
    # --- Hunk header / cold start (top of segment 0) ---
    0x000018: (
        "hunk_primary_jmp_init",
        [
            "Primary entry: JMP $248AE.L — all launches go to main_init_entry.",
            "Lines 0x000000–0x000017 are hunk overlay / ABCD marker (not code).",
        ],
    ),
    # --- Embedded read-only data (Capstone mis-decodes as instructions) ---
    0x000026: (
        "DATA_version_and_volume_strings",
        [
            "Embedded ASCII from 0x26: 'Version 1.01', 'MM2Boot:', 'MM2Play:'.",
        ],
    ),
    0x000048: (
        "DATA_world_location_names",
        [
            "Town/world name table: Middlegate, Atlantium, Vulcania, Sansobar, …",
        ],
    ),
    0x000064: (
        "DATA_class_name_table",
        [
            "Class names: Knight, Paladin, Archer, Cleric, Sorcerer, Robber,",
            "Ninja, Barbarian (used by UI formatters / A4-$7928 lookups).",
        ],
    ),
    0x00009E: (
        "DATA_race_name_table",
        [
            "Race names: Human, Elf, Dwarf, Gnome, Half-Orc, Goofball, …",
        ],
    ),
    0x000230: (
        "DATA_gfx_and_dat_filenames",
        [
            "Filename pointer table: disk.32, globe.32, event.dat, nwcp.32,",
            "town/cave/castle/outdoor .32 sets, map.dat, roster.dat, …",
        ],
    ),
    0x00025A: (
        "DATA_event_dat_filename",
        ["ASCII 'event.dat' in the embedded filename catalogue."],
    ),
    0x00035E: (
        "DATA_map_dat_filename",
        ["ASCII 'map.dat' / 'roster.dat' strings in filename catalogue."],
    ),
    0x000368: (
        "DATA_roster_dat_filename",
        [
            "Tail of filename table ('roster.dat'); real code resumes ~0x412.",
            "Capstone shows bogus opcodes over most of 0x26..0x36F.",
        ],
    ),

    # --- Early game UI / roster (0x400+) ---
    0x000412: (
        "ui_msg_escape_to_examine  (LAB_412)",
        [
            "Prints 'ESCAPE to examine' via -$7BE4; short modal before",
            "item_equip_slot_helper when arg word +8 is zero.",
        ],
    ),
    0x0006E4: (
        "party_item_slot_assign_ui  (LAB_6E4)",
        [
            "Assign/equip item to party slot: finds slot via $35AA, blits",
            "item icon (muls #$14) or backpack row; updates A4-$796A map.",
        ],
    ),
    0x000826: (
        "char_create_party_assemble  (LAB_826)",
        [
            "Character creation / party assembly UI: era display, roster slot",
            "picker (A4-$796A), validates good/evil mix (A4-$5E61/$5E62).",
        ],
    ),
    0x000F1C: (
        "game_world_boot  (LAB_F1C)",
        [
            "New-game world entry: loads UI panel, sets screen_mode $FF",
            "(-$79F2), seeds map coords, calls -$7FDA map init, enters",
            "main_scheduler_loop hotkey path (C/M/O/P/Q at $1062).",
        ],
    ),
    0x001062: (
        "main_loop_menu_hotkeys  (LAB_1062)",
        [
            "Tests key in -$2(A5) for C/M/O/P/Q ($43/$4D/$4F/$50/$51);",
            "branches to main_scheduler_loop command dispatch on match.",
        ],
    ),
    0x0035AA: (
        "roster_slot_find  (LAB_35AA)",
        [
            "Scans 8 party slots (A4-$796A) for index in arg +8; returns",
            "slot word in D0 (0=found, 1=not found).",
        ],
    ),
    0x0035E6: (
        "roster_slot_find_or_next  (LAB_35E6)",
        [
            "Like $35AA but advances -$2(A5) slot cursor until match or",
            "exhausted; used when iterating party for item/equip UI.",
        ],
    ),
    0x004476: (
        "sync_party_secondary_stats  (FUNC_SyncPartySecondaryStats)",
        [
            "Mirrors a character record's primary bytes into its derived/",
            "secondary stat fields (e.g. +$10 -> +$6B); stride $82, base A4-$2A3E.",
        ],
    ),
    0x004614: (
        "count_active_party_nibble_matches  (FUNC_CountActivePartyNibbleMatches)",
        [
            "Scans party slots; counts members whose class nibble (+$50)",
            "matches arg byte +$D. Called from session_interaction_gate.",
        ],
    ),
    0x00477E: (
        "get_party_member_ptr_by_slot  (FUNC_GetPartyMemberPtrBySlot)",
        [
            "Returns roster record ptr: slot_index from A4-$796A, * $82",
            "+ A4-$2A3E. BSR/JSR from party UI, combat, and event ops.",
        ],
    ),
    0x0047A2: (
        "roster_count_living_chars  (LAB_47A2)",
        [
            "Counts active roster entries (status byte +$26 & $E0 == 0).",
        ],
    ),
    0x00531A: (
        "format_party_status_line  (FUNC_FormatPartyStatusLine)",
        [
            "Formats one party-status HUD row (row index in arg +8);",
            "looks up A4-$7441/$744A/$8BBF… then JSR draw_party_status_panel.",
            "Callers JSR $5312(PC) land in 'Encounter!' string padding.",
        ],
    ),

    # --- Cold init / MANX / stdio (0x24800+) ---
    0x024874: (
        "boot_print_string  (LAB_24874)",
        [
            "Prints NUL-terminated string at (A5)+8 via $24DF4 putchar;",
            "emits LF if string empty. Used for boot banners/errors.",
        ],
    ),
    0x024A60: (
        "engine_process_bind  (LAB_24A60)",
        [
            "Binds MANX process block after AllocMem; sets flags in",
            "alloc header, optional callback registration path.",
        ],
    ),
    0x024A88: (
        "cmdline_argv_tokenizer  (LAB_24A88)",
        [
            "Shell-style argv parser: skip whitespace, quoted strings,",
            "builds pointer table at A4-$FCCC via AllocMem.",
        ],
    ),
    0x024C74: (
        "runtime_memcpy  (LAB_24C74)",
        [
            "Block copy helper used by cmdline_argv_tokenizer and loaders.",
        ],
    ),
    0x024DF4: (
        "stdio_putchar_wrapper  (LAB_24DF4)",
        [
            "Buffered putchar: pushes char + FILE* at A4-$6010, calls",
            "$24E0A; returns $FFFF on write error.",
        ],
    ),
    0x024E0A: (
        "stdio_putchar_dispatch  (LAB_24E0A)",
        [
            "Dispatches to $24E4C stream putc; LF ($0A) triggers flush",
            "via $24F34 when stream flag bit 7 set.",
        ],
    ),
    0x024E4C: (
        "stdio_stream_putc  (LAB_24E4C)",
        [
            "Writes one byte into MANX FILE stream buffer (A2); advances",
            "put pointer or returns error when buffer full.",
        ],
    ),
    0x024E88: (
        "stdio_buffer_clear_loop  (LAB_24E88)",
        [
            "Clears stdio buffer range A4-$6026..A4-$5E6E (MANX pool tail).",
        ],
    ),
    0x024EB0: (
        "stdio_fflush_check  (LAB_24EB0)",
        [
            "Tests stream flags (+$C): bit2 → flush via $24F34 + $2540C;",
            "bit1 → write-through path.",
        ],
    ),
    0x024F34: (
        "stdio_flush_write  (LAB_24F34)",
        [
            "Flush buffered stream to DOS handle via $2540C Write.",
        ],
    ),
    0x0244F8: (
        "runtime_strnicmp  (LAB_24F8)",
        [
            "Case-insensitive string compare with max length; returns",
            "0/equal, 1/greater, -1/less (used by path/volume matching).",
        ],
    ),
    0x025654: (
        "exec_allocmem  (FUNC_ExecAllocMem)",
        [
            "exec.library AllocMem wrapper: MOVEM.L 4(A7),D0-D1 (size,flags),",
            "A6 = ExecBase (A4-$346), JSR -198(A6). Backs the runtime allocator.",
        ],
    ),
    0x02593C: (
        "overlay_segment_init  (LAB_2593C)",
        [
            "Start of CODE hunk 2 (overlay) at load addr $2593C.",
            "Save/exit UI, extra loaders, and late-game routines live here.",
        ],
    ),

    # --- Engine / game routines (verified on instruction boundary) ---
    0x001280: (
        "main_scheduler_loop  (FUNC_MainLoopScheduler)",
        [
            "High-frequency control loop once runtime is active:",
            "poll timer/event bytes, pump input (-$83C2), fetch key (-$842E),",
            "dispatch key through table, handle mode transitions and refresh.",
        ],
    ),
    0x00545E: (
        "session_start_refresh  (FUNC_SessionStartRefresh)",
        [
            "Common 'resume interactive play' gate: paging refresh,",
            "session_interaction_gate ($53A0), show_command_reference ($5D54).",
        ],
    ),
    0x0172CA: (
        "event_script_interpreter  (LAB_172CA)",
        ["Executes event.dat tile-event script bytecodes (see docs/06-event-dat-format.md)."],
    ),
    0x01754A: (
        "event_system_init  (LAB_1754A)",
        ["Initialises event.dat runtime context (prologue link.w a5,#-8)."],
    ),
    0x0092F2: (
        "event_dat_loader  (LAB_92F2)",
        [
            "event.dat loader; reads blob via A4-$EEF4 (prologue link.w a5,#-$1c).",
            "NOTE: Capstone mis-decoded 2 padding bytes before this entry, so the",
            "true prologue sits inside the 0x92f0 line (bytes 4e55 ffe4).",
        ],
    ),
    0x017262: (
        "event_handler_pool_seek",
        [
            "Walks the 0xFF-delimited event-handler pool to find record N",
            "from the tile triplet event id (see docs/07-event-script-opcodes.md).",
        ],
    ),
    0x017494: (
        "DATA_event_opcode_jump_table",
        [
            "Jump table for event script opcodes 0x00..0x32; indexed from",
            "event_script_interpreter dispatch at $174FA (doc 07).",
        ],
    ),
    0x01748C: (
        "event_opcode_invalid_abort",
        [
            "Invalid opcode handler: sets abort flag A4-$8616 and exits script.",
        ],
    ),
    0x01750C: (
        "event_script_fetch_next",
        [
            "Fetches next bytecode byte from A4-$86AA; loops back to dispatcher",
            "after each opcode handler returns (doc 07 core loop).",
        ],
    ),
    0x0175E2: (
        "event_tile_scanner",
        [
            "Each step: derives tile (y<<4)|x from party coords, scans triplet",
            "table in A4-$B838 buffer, AND-masks cond byte, dispatches match.",
        ],
    ),
    0x0171AC: (
        "event_script_cleanup",
        [
            "End-of-script cleanup invoked by OP_0F and abort paths.",
        ],
    ),
    0x0157FC: (
        "event_token_skip_helper",
        [
            "IF/ELSE token-skip for opcodes 0x10/0x11/0x2B: reads count byte,",
            "advances A4-$86AA using length table A4-$6CC8.",
        ],
    ),
    0x015884: (
        "event_text_resolve_u8",
        [
            "Resolves u8 string index in current location's string table;",
            "called by OP_01..OP_06 text-display handlers.",
        ],
    ),
    0x015924: (
        "event_op01_show_text",
        [
            "Event opcode 0x01 handler: show one indexed string (u8 arg).",
        ],
    ),
    0x0160C2: (
        "event_op0e_selector_dispatch",
        [
            "Event opcode 0x0E: town-service selector dispatch (shops, temple,",
            "training, guilds, travel portals — see doc 07 OP_0E table).",
        ],
    ),
    # --- event.dat script opcode handlers (jump table at 0x17494; doc 07) ---
    0x015942: ("event_op02_show_text_block", ["OP_02: show text block (u8 string index; const $13)."]),
    0x0159CE: ("event_op03_show_text", ["OP_03: show text (u8 string index via $15942 const $11)."]),
    0x0159F4: ("event_op04_show_text_door", ["OP_04: show text above door (u8 string index)."]),
    0x015A46: ("event_op05_text_popup_a", ["OP_05: show text popup A (u8 string index)."]),
    0x015AEE: ("event_op06_text_popup_b", ["OP_06: show text popup B (u8 string index)."]),
    0x015CE6: ("event_op07_wait_space", ["OP_07: wait-loop until SPACE key pressed."]),
    0x015D26: ("event_op08_wait_key", ["OP_08: wait for key (cursor mode $FD, calls $15CE6)."]),
    0x015D3C: ("event_op09_yesno_prompt", ["OP_09: y/n prompt; writes result into condition flag."]),
    0x015D9A: ("event_op0a_yesno_variant", ["OP_0A: y/n prompt variant (mode $FD, calls $15D3C)."]),
    0x015DB0: ("event_op0b_service_window", ["OP_0B: open service/title window (u8 string, u8 position)."]),
    0x015E12: ("event_op0c_map_transition", ["OP_0C: map transition to (u8 dest, u8)."]),
    0x015EC4: ("event_op0d_engine_call", ["OP_0D: u8 -> engine thunk -$7E42 (generic engine call)."]),
    0x0162A6: ("event_op0f_end_script", ["OP_0F: end/return script; sets stop flag, cleanup $171AC."]),
    0x0162B8: ("event_op10_if_skip", ["OP_10: IF(cond) skip N token-stream entries (helper $157FC)."]),
    0x0162DC: ("event_op11_ifnot_skip", ["OP_11: IF(!cond) skip N token-stream entries (helper $157FC)."]),
    0x016386: ("event_op13_encounter_variant", ["OP_13: encounter setup variant (calls $16300 flag=1, 10-byte block)."]),
    0x016398: ("event_op14_clear_tile_event", ["OP_14: clear current tile event high-bit / runtime event flag."]),
    0x016426: ("event_op15_party_state_apply", ["OP_15: apply state to party members (u8 count,u8,u8)."]),
    0x016520: ("event_op16_monster_scan", ["OP_16: scan up to 6 monster slots; set cond on field match."]),
    0x0165A4: ("event_op17_load_cond_var", ["OP_17: load condition byte from resolved variable pointer."]),
    0x0165C6: ("event_op18_party_mask_apply", ["OP_18: OP_15 with flag=1 (u8 count,set,and,or) masked apply."]),
    0x0165D8: ("event_op19_add_entity", ["OP_19: add entity to party/roster (fields +$3A/+$40/+$46)."]),
    0x0166F8: ("event_op1a_store_var", ["OP_1A: store u8 into resolved variable pointer."]),
    0x016724: ("event_op1b_threshold_gate", ["OP_1B: clear cond unless cond >= u8 (threshold gate)."]),
    0x016742: ("event_op1c_engine_query", ["OP_1C: engine query -$7BB4(1,u8) -> cond."]),
    0x016762: ("event_op1d_engine_indexed", ["OP_1D: engine call indexed by u8*7+1 (-$7E84)."]),
    0x016780: ("event_op1e_timed_wait", ["OP_1E: timed wait/delay loop for u8 iterations."]),
    0x01690E: ("event_op1f_party_effect", ["OP_1F: apply effect to party members (selector + values via $167B0)."]),
    0x016A22: ("event_op20_party_effect_var", ["OP_20: party-effect variant (calls $1690E mode=1)."]),
    0x016A34: ("event_op21_set_tile_data", ["OP_21: set tile data at (y,x) into arrays -$55BA/-$54BA."]),
    0x016A9E: ("event_op22_era_gate", ["OP_22: set cond if current era (-$79B5) in [lo,hi] (time-range gate)."]),
    0x016ADA: ("event_op23_day_gate", ["OP_23: range-check day-of-year (-$79DE[era]) (calendar gate)."]),
    0x016B54: ("event_op24_gold_check", ["OP_24: check u16 gold amount -> condition flag."]),
    0x016B82: ("event_op25_code_check", ["OP_25: check non-gold 16-bit code (hi,lo) -> condition flag."]),
    0x016BC0: ("event_op26_select_party_member", ["OP_26/27: prompt to select a party member (ESC ends script)."]),
    0x016C86: ("event_op28_consume_item", ["OP_28: consume item id (arg1) from inventory -> cond flag."]),
    0x016D08: ("event_op29_force_abort", ["OP_29: force abort/exit flag (A4-$8616=1)."]),
    0x016D16: ("event_op2a_set_reward_block", ["OP_2A: set treasure block (u24 gold/exp, u16 gems, 3x item)."]),
    0x016D74: ("event_op2b_token_walk", ["OP_2B: u8 + token-skip walk (helper $157FC)."]),
    0x016D98: ("event_op2c_add_state_var", ["OP_2C: add u8 to state var -$79B8, flag redraw."]),
    0x016DBA: ("event_op2d_check_attribute", ["OP_2D: check party-member attribute (fields +$C/+$E/+$F) -> cond."]),
    0x016F50: ("event_op2e_set_attr_bit", ["OP_2E: set attribute bit on matching party class -> field +$51."]),
    0x016FEA: ("event_op2f_clear_input_buffer", ["OP_2F: clear/space-fill input buffer -$5C50."]),
    0x017034: ("event_op30_answer_check", ["OP_30: 10-byte answer/password check; char = 0x11A - byte."]),
    0x0170BC: ("event_op31_iterate_targets", ["OP_31: iterate target party members and call engine op."]),
    0x017190: ("event_op32_load_cond_from_var", ["OP_32: load condition from script variable id (-$7F2C)."]),
    0x016300: (
        "event_op12_encounter_setup",
        [
            "Event opcode 0x12: 10-byte encounter block + 2 trailing bytes;",
            "transitions into combat engine (doc 07).",
        ],
    ),
    # --- Cold start / init (verified) ---
    0x0248AE: (
        "main_init_entry  (JMP $248AE target)",
        [
            "Real engine entry: target of `JMP $248AE.L` at 0x20.",
            "bsr to set A4=$7FFE, then MANX heap clear + dos.library bring-up.",
        ],
    ),
    0x024920: (
        "set_a4_workspace_base  (FUNC_SetA4WorkspaceBase)",
        ["LEA $7FFE,A4 - establishes the global game-state base. All xxx(A4) globals hang off this."],
    ),
    0x024928: (
        "init_runtime_structures  (FUNC_InitRuntimeStructures)",
        [
            "AllocMem + write 'MANX' signature, build process/runtime structures,",
            "then bind engine thunks via JSR -$7FF8(A4) / JSR -$7B48(A4).",
        ],
    ),

    # --- dos.library LVO thunks (a6 = DOSBase via -$342(a4)) ---
    # Labelled from the actual `jmp -N(a6)` offset on each thunk.
    0x025458: ("dos_Close",          ["dos.library Close() thunk  (jmp -36(DOSBase))"]),
    0x025464: ("dos_CurrentDir",     ["dos.library CurrentDir() thunk  (jmp -126)"]),
    0x025470: ("dos_ParentDir",      ["dos.library ParentDir() thunk  (jmp -198)"]),
    0x02547C: ("dos_DeleteFile",     ["dos.library DeleteFile() thunk  (jmp -72)"]),
    0x025488: ("dos_SetProtection",  ["dos.library SetProtection() thunk  (jmp -174)"]),
    0x025494: ("dos_Input",          ["dos.library Input() thunk  (jmp -54)"]),
    0x02549C: ("dos_IoErr",          ["dos.library IoErr() thunk  (jmp -132)"]),
    0x0254A4: ("dos_PrintFault",     ["dos.library PrintFault() thunk  (jmp -216)"]),
    0x0254B0: ("dos_Lock",           ["dos.library Lock() thunk  (jmp -84)"]),
    0x0254BE: ("dos_Open",           ["dos.library Open() thunk  (jmp -30)"]),
    0x0254CC: ("dos_Output",         ["dos.library Output() thunk  (jmp -60)"]),
    0x0254D4: ("dos_Read",           ["dos.library Read() thunk  (jmp -42)"]),
    0x0254E2: ("dos_Seek",           ["dos.library Seek() thunk  (jmp -66)"]),
    0x0254F4: ("dos_UnLock",         ["dos.library UnLock() thunk  (jmp -90)"]),
    0x025500: ("dos_Write",          ["dos.library Write() thunk  (jmp -48)"]),

    # --- Items / equipment (segment 0) ---
    0x00044C: (
        "item_equip_slot_helper  (LAB_44C)",
        [
            "Indexes roster record (muls #$82, A4-$2A3E) then item slot",
            "(muls #$14) for equipment/backpack UI; calls -$7BFC blit helper.",
        ],
    ),

    # --- monsters.dat loaders / unpack ---
    0x0099C4: (
        "monsters_dat_record_copy  (LAB_99C4)",
        [
            "Copies one 26-byte ($1A) monster record from A4-$77C2 blob;",
            "index in arg byte +9, mulu.w #$1a; error path loads via $9B34.",
        ],
    ),
    0x004C8E: (
        "monster_stat_unpacker  (LAB_4C8E)",
        [
            "Authoritative per-monster combat stat unpack: calls $99C4, then",
            "explodes record bytes into A4-relative 'current monster' globals",
            "(name, HP/XP codes, Pabil/Sabil/Oabil, AC, damage, …).",
            "See docs/16-monster-ability-format.md.",
        ],
    ),

    # --- Combat: monster attacks & rewards ---
    0x010004: (
        "monster_group_attack_dispatch  (LAB_10002)",
        [
            "Pabil group-attack verb dispatch: Pabil&$1F indexes verb table",
            "at A4-$6E56 (master[40+] = 'sprays poison'…'frenzies').",
            "Callers JSR $10002(PC) land 2 bytes early (string padding).",
        ],
    ),
    0x00FEEA: (
        "monster_single_attack_message  (LAB_FEEA)",
        [
            "Sabil single-target effect: (Sabil&$1F)-1 indexes victim-status",
            "messages at A4-$6ECE (= master[10+] 'lost gold'…'assassinated').",
        ],
    ),
    0x010B74: (
        "monster_treasure_reward  (LAB_10B74)",
        [
            "Decodes monster record byte $10 (treasure pack): gold/gems/item/",
            "XP bonus — not a combat ability. Sets A4-$11BF type flags.",
        ],
    ),
    0x0100B0: (
        "monster_multiply_breed  (LAB_100B0)",
        [
            "Oabil bit7 'multiplies': doubles on-screen monster count when",
            "party size and live-monster count are in range.",
        ],
    ),
    0x0107A4: (
        "monster_archer_flag_check  (LAB_107A4)",
        [
            "Tests Sabil bit6 (archer) on current monster; gates ranged",
            "combat path before group/single attack dispatch.",
        ],
    ),
    0x010DFC: (
        "monster_flee_run  (LAB_10DFC)",
        [
            "Oabil bits5-6 flee tier: '<monster> runs' escape path.",
        ],
    ),
    0x011F0A: (
        "monster_adds_friends  (LAB_11F0A)",
        [
            "Oabil low-nibble (+1, x10 if bit4) reinforcement spawn for",
            "the 'adds friends!' combat action.",
        ],
    ),
    0x010E5E: (
        "combat_round_master  (LAB_10E5E)",
        [
            "Top-level combat-turn orchestrator: phases through $10C66,",
            "treasure $10B70, flee $10DFC, and post-round $10CCE cleanup.",
        ],
    ),
    0x010C66: (
        "combat_attack_phase  (LAB_10C66)",
        [
            "Mid-combat attack resolution phase; invoked from $10E5E loop.",
        ],
    ),
    0x01064C: (
        "combat_monster_turn_begin  (LAB_1064C)",
        [
            "Starts a monster combat action: picks active monster slot",
            "(-$4F7), rolls $1119E, applies group-attack / special checks.",
        ],
    ),
    0x01119E: (
        "combat_rng_roll  (LAB_1119E)",
        [
            "Combat random roll (0..99) used for hit/miss and ability gates.",
        ],
    ),
    0x010118: (
        "combat_special_action_handler  (LAB_10118)",
        [
            "Handles non-standard combat actions (sleep, etc.) when Pabil",
            "verb index is $1D or $1F; reached from $107C4.",
        ],
    ),
    0x010584: (
        "combat_monster_ranged_attack",
        [
            "Monster archer (Sabil bit6) ranged attack path from monster turn.",
        ],
    ),
    0x0107C4: (
        "combat_monster_special_branch",
        [
            "Branches to combat_special_action_handler ($10118) for sleep/",
            "non-standard Pabil verbs $1D/$1F (doc 17).",
        ],
    ),
    0x0111DA: (
        "combat_target_selector",
        [
            "Prompts 'which (A - x)?'; sets A4-$51D target index from",
            "monster-side (-$77BE) or party-side (-$524) pool.",
        ],
    ),
    0x01175C: (
        "combat_read_command_key",
        [
            "Reads and validates combat command key (A/F/S/C/U/B/R/E/V);",
            "gates on capability flags -$5E36..-$5E34 set by command bar.",
        ],
    ),
    0x01181E: (
        "combat_print_command_entry",
        [
            "Prints one combat command-bar line from string table A4-$6F9C",
            "('A-Attack F-Fight S-Shoot C-Cast …').",
        ],
    ),
    0x011866: (
        "combat_command_bar_build",
        [
            "Builds combat command bar; sets melee/shoot/cast flags from",
            "active character fields ($0F class, $4E bow, $26 silenced, …).",
        ],
    ),
    0x0119C2: (
        "combat_player_turn",
        [
            "Player combat turn: command bar, key read, dispatch via table",
            "at $11BD0 (Attack/Fight/Cast/Block/Run/Use/View).",
        ],
    ),
    0x011B0A: (
        "combat_player_block",
        [
            "Block command: JSR -$7D5E / -$7C3E defensive stance.",
        ],
    ),
    0x011B1A: (
        "combat_player_run_retreat",
        [
            "Run/retreat command: reorders party via -$7CC2 thunk.",
        ],
    ),
    0x011B62: (
        "combat_player_use_item",
        [
            "Use-item command during combat; JSR combat_use_item_handler ($133EC).",
        ],
    ),
    0x011B6E: (
        "combat_player_view_character",
        [
            "View-character command during combat (-$7E00 character inspect).",
        ],
    ),
    0x011C2C: (
        "combat_monster_slot_init_one",
        [
            "Instantiates one monster slot from scratch unpack globals",
            "into parallel battle arrays (-$11DE, -$53A, -$519, …).",
        ],
    ),
    0x011C82: (
        "combat_monster_slots_init_all",
        [
            "Loops combat_monster_slot_init_one over all live monsters at",
            "encounter start and each combat round (doc 17).",
        ],
    ),
    0x011D0C: (
        "combat_state_recompute",
        [
            "Mid-combat display/recompute pass in round loop (doc 17 flow).",
        ],
    ),
    0x011646: (
        "combat_defeat_retreat",
        [
            "Party defeat / failed retreat handler when combat ends badly.",
        ],
    ),
    0x012430: (
        "combat_victory_rewards",
        [
            "Victory path when A4-$77BE (live monster count) hits 0; runs",
            "per-monster treasure decode ($10B74) and XP tally (-$6FC6).",
        ],
    ),
    0x012A22: (
        "combat_round_loop",
        [
            "Main combat round loop: reset acted flags, initiative scan,",
            "player ($119C2) or monster ($1064C) turns, round-end check.",
            "See docs/17-combat-system.md.",
        ],
    ),
    0x013282: (
        "combat_round_end_check",
        [
            "Decides whether combat continues after a round; branches to",
            "victory ($12430) or defeat ($11646) when appropriate.",
        ],
    ),
    0x0133EC: (
        "combat_use_item_handler",
        [
            "In-combat item use implementation (called from $11B62).",
        ],
    ),
    0x0135BE: (
        "combat_display_refresh",
        [
            "Combat UI refresh / round banner update in round loop.",
        ],
    ),
    0x00BC3A: (
        "spell_handler_stub",
        [
            "Representative per-spell effect handler in the $BC00..$C800 block;",
            "spell_effect_jump_table at $D002 indexes many such stubs.",
        ],
    ),
    0x00CB52: (
        "combat_player_attack",
        [
            "Single-target Attack command handler (key 'A'); to-hit/damage",
            "resolution against chosen A4-$51D target.",
        ],
    ),
    0x00CFD0: (
        "skill_spell_effect_dispatch  (LAB_CFD0)",
        [
            "Reads skill/spell index (arg +8) and runs the shared effect",
            "dispatcher: bra $D23E -> jmp via per-effect jsr-stub table.",
            "Fight stubs $B644.. ; Cast stubs $B982.. ; shared exit $D256.",
        ],
    ),
    0x00B644: (
        "spell_effect_handlers_block",
        [
            "Start of the per-spell/skill effect handler block ($B644..$C800):",
            "~40 short JSR-stub routines invoked by skill_spell_effect_dispatch.",
            "Per-spell semantics not yet fully reduced (doc 17 open items).",
        ],
    ),
    0x00D186: (
        "DATA_spell_effect_offset_table",
        [
            "Word offset table (spell index -> handler) consumed by the",
            "$D23E jump executor; entries are negative .w offsets ($FDxx/$FExx).",
        ],
    ),
    0x00D23E: (
        "skill_spell_jump_executor",
        [
            "Indexes DATA_spell_effect_offset_table by effect number and",
            "jmp's to the matching stub; common landing for Fight + Cast.",
        ],
    ),
    0x00D002: (
        "spell_effect_jump_table",
        [
            "Cast-command dispatch: sequence of `jsr stub ; bra $D256` pairs",
            "for spell handlers in the $B900..$C800 block (doc 17).",
        ],
    ),
    0x00CD98: (
        "combat_action_menu_wrapper",
        [
            "Combat menu/action wrapper; callers JSR $CD90(PC) land in",
            "embedded-string padding — real entry is ~$CD98.",
        ],
    ),
    0x00D390: (
        "combat_attack_target_menu",
        [
            "Attack target-selection UI ('which monster?').",
        ],
    ),
    0x00D43C: (
        "combat_attack_auto_target",
        [
            "Auto-target when only one monster remains (Attack shortcut).",
        ],
    ),

    # --- First-person view / encounter sprites ---
    0x002ECE: (
        "view_3d_master  (LAB_2ECE)",
        [
            "First-person dungeon view: floor/ceiling blit, frustum build",
            "($2900), back-to-front wall pieces, flush -$7D04.",
            "See docs/15-3d-view-and-game-screen.md.",
        ],
    ),
    0x002900: (
        "frustum_wall_cell_builder  (LAB_2900)",
        [
            "Reads map page-1 wall codes around party (A4-$55C9..) into",
            "frustum slots A4-$F0D..-$F20 for the 3D painter's pass.",
        ],
    ),
    0x002C1A: (
        "wall_piece_front  (LAB_2C1A)",
        ["Blits front wall from env wall set A4-$7A06 at depth slot."],
    ),
    0x002C9A: (
        "wall_piece_left  (LAB_2C9A)",
        ["Blits left-side wall piece (+$10 mirror flag)."],
    ),
    0x002DB4: (
        "wall_piece_right  (LAB_2DB4)",
        ["Blits right-side wall piece (+$08 mirror flag)."],
    ),
    0x002BCC: (
        "frustum_solid_edge_test  (LAB_2BCC)",
        [
            "Mask-tests map edge against A4-$75BE; decides side-wall visibility.",
        ],
    ),
    0x00316E: (
        "encounter_sprite_placer  (LAB_316E)",
        [
            "Layers up to 24 monster/encounter .anm sprites over the viewport",
            "using placement table A4-$7538.",
        ],
    ),
    0x001622: (
        "load_resource_by_path  (FUNC_LoadResourceByPath)",
        [
            "Loads a .32/.anm/.dat resource by filename via -$7C2C; returns",
            "handle in D0. Called from the area_env gfx dispatcher and loaders.",
        ],
    ),
    0x00163C: (
        "area_env_gfx_loader  (LAB_163C)",
        [
            "Per-area tileset loader: reads env id (A4-$7660), dispatches",
            "through jump table at $1880 to load town/cave/castle/outdoor",
            "wall/floor/ceiling/auto-map .32 sets into A4-$7A06..-$85EA.",
        ],
    ),

    # --- Map / overland / session UI ---
    0x00223A: (
        "overland_map_view  (LAB_223A)",
        [
            "16x16 world-map renderer: N/S/E/W facing keys, tile blit via",
            "-$7C20, ESC sets A4-$7950 exit flag → session_start_refresh.",
        ],
    ),
    0x001786: (
        "area_env_gfx_dispatch_town",
        [
            "Area tileset load dispatcher (7 env types): jump table at",
            "$1880 loads wall/floor/ceiling/auto-map .32 sets into A4-$7A06..",
            "See docs/15-3d-view-and-game-screen.md §1.",
        ],
    ),
    0x001880: (
        "DATA_area_env_jump_table",
        [
            "Seven-entry jump table for area_env_gfx_loader env dispatch",
            "(town/cave/castle/outdoor variants).",
        ],
    ),
    0x002546: (
        "load_screen_context  (FUNC_LoadScreenContext)",
        [
            "On screen_id (A4-$79F2) change: reload map.dat pages into",
            "A4-$54BA / A4-$55BA, refresh env gfx, event buffer, automap.",
        ],
    ),
    0x0024BC: (
        "overland_map_view_loop  (FUNC_OverlandMapViewLoop)",
        [
            "Inner loop of overland map view: blit tile, poll key ($8440),",
            "ESC → session_start_refresh. Outer setup at $223A.",
        ],
    ),
    0x003D8C: (
        "read_key_with_spinner  (FUNC_ReadKeyWithSpinner)",
        [
            "Blocking key poll with spinner animation (-$77B4 cycle);",
            "returns key in D0. Used across map, UI, and event wait ops.",
        ],
    ),
    0x0042DC: (
        "draw_party_status_panel  (FUNC_DrawPartyStatusPanel)",
        [
            "Blits party member portrait/stats into nwcp.32 HUD panel;",
            "called from format_party_status_line ($5312).",
        ],
    ),
    0x005030: (
        "print_c_string  (FUNC_PrintCString)",
        [
            "Prints NUL-terminated string at arg +8 via putchar loop.",
        ],
    ),
    0x005080: (
        "load_deferred_resources  (FUNC_LoadDeferredResources)",
        [
            "Lazy-loads .32/.anm resources listed in filename table once",
            "play session is active (sky, walls, nwcp panel, …).",
        ],
    ),
    0x005382: (
        "clear_modal_busy_state  (FUNC_ClearModalBusyState)",
        [
            "Clears modal-busy flag A4-$79E5; pumps -$7C3E and -$7EBA if set.",
        ],
    ),
    0x0053A0: (
        "session_interaction_gate  (FUNC_SessionInteractionGate)",
        [
            "Per-frame play gate: party nibble check, darkness/3D viewport",
            "switch (-$79E2 → $2ECE or -$7D34), 'Can't see' overlay.",
        ],
    ),
    0x005444: (
        "play_frame_draw",
        [
            "Draws one play frame: paging counter A4-$7438, party panel",
            "format ($5312), then session_interaction_gate ($53A0).",
        ],
    ),
    0x005D54: (
        "show_command_reference_panel  (FUNC_ShowCommandReferencePanel)",
        [
            "Draws the in-game command-reference / help panel (nwcp chrome);",
            "called from session_start_refresh when not in special mode.",
        ],
    ),
    0x025FA6: (
        "sky_32_global_loader",
        [
            "Loads global sky.32 (filename table entry 6) into A4-$7A02;",
            "2-frame sheet for open/closed ceiling in 3D view (doc 15).",
        ],
    ),
    0x012848: (
        "combat_message_helper  (LAB_12848)",
        [
            "Formats/prints combat log lines (monster name + verb/status).",
        ],
    ),
    0x0054B6: (
        "play_frame_paging_wrapper  (LAB_54B6)",
        [
            "Brackets play_frame_draw with paging counter A4-$7438",
            "(animate panel/stat refresh while counter > 0).",
        ],
    ),
    0x0051C2: (
        "party_setup_roster_copy  (LAB_51C2)",
        [
            "New-game party setup: copies 8 roster slot words A4-$796A →",
            "A4-$5E5E, loads per-character records ($477E/$4476), engine init.",
        ],
    ),
    0x001482: (
        "ingame_command_dispatch  (LAB_1482)",
        [
            "Hotkey command switch (C/M/O/P/Q…): rest, magic, inventory,",
            "options, etc. — reached from main_scheduler_loop key index.",
        ],
    ),
    0x0028CE: (
        "title_encounter_gate  (LAB_28CE)",
        [
            "Title/encounter transition helper; reached from session gate",
            "when count_active_party_nibble_matches ($4614) returns nonzero.",
        ],
    ),
}

ADDR_RE = re.compile(r"^([0-9a-fA-F]{6})\s")
BAR = "; " + "=" * 72


def banner(addr: int, name: str, desc: list[str]) -> list[str]:
    out = [START, BAR, f"; {addr:06x}  {name}"]
    for line in desc:
        out.append(f";   {line}")
    out.append(BAR)
    out.append(END)
    return out


def strip_existing(lines: list[str]) -> list[str]:
    out, skipping = [], False
    for ln in lines:
        s = ln.strip()
        if s == START:
            skipping = True
            continue
        if s == END:
            skipping = False
            continue
        if not skipping:
            out.append(ln)
    return out


def main() -> int:
    if not os.path.exists(ASM):
        print(f"capstone file not found: {ASM}", file=sys.stderr)
        return 1

    with open(ASM, "r", encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")

    lines = strip_existing(lines)

    # exact address -> line index, and sorted addr list for containing lookup
    addr_at: dict[int, int] = {}
    for i, ln in enumerate(lines):
        m = ADDR_RE.match(ln)
        if m:
            addr_at[int(m.group(1), 16)] = i
    sorted_addrs = sorted(addr_at)

    def containing_index(target: int) -> int | None:
        # greatest instruction address <= target
        lo, hi, best = 0, len(sorted_addrs) - 1, None
        while lo <= hi:
            mid = (lo + hi) // 2
            if sorted_addrs[mid] <= target:
                best = sorted_addrs[mid]
                lo = mid + 1
            else:
                hi = mid - 1
        return addr_at[best] if best is not None else None

    # Build insertions: line index -> banner lines
    inserts: dict[int, list[str]] = {}
    exact = misaligned = missing = 0
    for addr, (name, desc) in LABELS.items():
        if addr in addr_at:
            idx = addr_at[addr]
            inserts[idx] = banner(addr, name, desc)
            exact += 1
        else:
            idx = containing_index(addr)
            if idx is None:
                print(f"  ! {addr:06x} {name}: address not found, skipped")
                missing += 1
                continue
            note = desc + [f"(no exact capstone boundary; attached to containing line)"]
            inserts[idx] = banner(addr, name, note)
            misaligned += 1

    out: list[str] = []
    for i, ln in enumerate(lines):
        if i in inserts:
            out.extend(inserts[i])
        out.append(ln)

    with open(ASM, "w", encoding="utf-8") as f:
        f.write("\n".join(out))

    print(f"labels: {exact} exact, {misaligned} misaligned, {missing} missing "
          f"-> {len(inserts)} banners written to {ASM}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
