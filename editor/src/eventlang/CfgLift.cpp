#include "eventlang/CfgLift.h"

#include "eventlang/OpcodeTable.h"
#include "eventlang/Semantics.h"
#include "eventlang/TokenSkip.h"

#include <optional>

namespace mm2::eventlang {
namespace {

int indexAtOffset(const std::vector<LoweredOp>& ops, int off) {
    for (int i = 0; i < static_cast<int>(ops.size()); ++i) {
        if (ops[i].off >= off) return i;
    }
    return static_cast<int>(ops.size());
}

Expr opToCondExpr(const LoweredOp& op) {
    const uint8_t o = op.op;
    const auto& a = op.args;
    if (o == 0x09) return Expr::make("yes_no").set("mode", 0);
    if (o == 0x0A) return Expr::make("yes_no").set("mode", 1);
    // OP_16 @ 0x16520: arg1 discarded; only arg2 (item id) is scanned.
    if (o == 0x16 && a.size() >= 2)
        return Expr::make("party_has_item").set("a", a[0]).set("b", a[1]).set("item", a[1]);
    if (o == 0x1B && !a.empty()) return Expr::make("threshold").set("value", a[0]);
    // OP_1C @ 0x16742: cond = rng_roll(1..hi) raw byte.
    if (o == 0x1C && !a.empty()) return Expr::make("rng_roll").set("max", a[0]);
    if (o == 0x22 && a.size() >= 2) return Expr::make("era_in").set("lo", a[0]).set("hi", a[1]);
    if (o == 0x23 && a.size() >= 2) {
        if (a[0] == 0xB5) return Expr::make("day_odd");
        if (a[0] == 0xB6) return Expr::make("day_even");
        return Expr::make("day_in").set("lo", a[0]).set("hi", a[1]);
    }
    if (o == 0x24) {
        int v = decodeU16Arg(o, a.data(), static_cast<int>(a.size()));
        return Expr::make("gold_at_least").set("amount", v);
    }
    // OP_25 @ 0x16B82: pay gems (BE u16), NOT a "code" check.
    if (o == 0x25) {
        int v = decodeU16Arg(o, a.data(), static_cast<int>(a.size()));
        return Expr::make("gems_at_least").set("amount", v);
    }
    // OP_28 @ 0x16C86: consume backpack item → cond.
    if (o == 0x28 && a.size() >= 2)
        return Expr::make("consume_item").set("item", a[1]).set("probe", a[0]);
    if (o == 0x2D && a.size() >= 2)
        return Expr::make("member_attr").set("arg1", a[0]).set("arg2", a[1]);
    if (o == 0x17 && a.size() >= 2)
        return Expr::make("load_var8").set("group", a[0]).set("index", a[1]);
    if (o == 0x32 && !a.empty())
        return Expr::make("count_title_nibble").set("id", a[0]);
    if (o == 0x15 && a.size() >= 3) {
        return Expr::make("party_bits")
            .set("member", static_cast<int>(a[0]))
            .set("field", static_cast<int>(a[1]))
            .set("mask", static_cast<int>(a[2]));
    }
    // OP_19 sets cond=1 when item placed on a member.
    if (o == 0x19 && a.size() >= 4)
        return Expr::make("give_item_ok")
            .set("member", a[0])
            .set("item", a[1])
            .set("charges", a[2])
            .set("flags", a[3]);
    // OP_1F/20 @ 0x1690E: party field add/sub; cond=1 entry, OP_20 clears on underflow.
    if ((o == 0x1F || o == 0x20) && a.size() >= 6) {
        std::vector<int> args;
        for (uint8_t x : a) args.push_back(x);
        return Expr::make("party_effect_ok")
            .set("mode", o == 0x20 ? 1 : 0)
            .set("sel", a[0])
            .setList("args", args);
    }
    if (o == 0x30) {
        std::string decoded;
        for (uint8_t b : a) {
            int ch = (0x11A - b) & 0xFF;
            if (ch >= 0x20 && ch <= 0x7E) decoded += static_cast<char>(ch);
            else decoded += '?';
        }
        while (!decoded.empty() && decoded.back() == ' ') decoded.pop_back();
        return Expr::make("answer_eq").set("text", decoded);
    }
    Expr e = Expr::make("raw_op").set("op", o);
    std::vector<int> args;
    for (uint8_t x : a) args.push_back(x);
    e.setList("args", args);
    return e;
}

std::optional<Stmt> opToStmt(const LoweredOp& op) {
    const uint8_t o = op.op;
    const auto& a = op.args;
    // Cond-set ops are normally consumed by liftRange; side-effecting cond ops may
    // also appear as bare statements when not immediately followed by a branch
    // (OP_15/19/1F/20). Pure cond ops return nullopt here.
    if (isCondSetOp(o) && o != 0x15 && o != 0x19 && o != 0x1F && o != 0x20) return std::nullopt;
    if (o == 0x10 || o == 0x11 || o == 0x2B) return std::nullopt;
    if (o >= 0x01 && o <= 0x06 && !a.empty()) {
        return Stmt::make("say").set("variant", sayVariantForOp(o)).set("string", static_cast<int>(a[0]));
    }
    if (o == 0x07) return Stmt::make("wait").set("kind", "space").set("mode", 0);
    // OP_08 @ 0x15D26: still a SPACE wait; sets SCRIPTED_KEY_MODE=$FD first.
    if (o == 0x08) return Stmt::make("wait").set("kind", "space").set("mode", 1);
    if (o == 0x09) return Stmt::make("ask_yes_no").set("mode", 0);
    if (o == 0x0A) return Stmt::make("ask_yes_no").set("mode", 1);
    if (o == 0x0B && a.size() >= 2)
        return Stmt::make("service_title").set("sign", static_cast<int>(a[0])).set("mode", a[1]);
    if (o == 0x0C && a.size() >= 2)
        return Stmt::make("go_to").set("screen", a[0]).set("pos", a[1]);
    // OP_0D → play_sound_seq (ids 0..9), not a generic "engine" trap.
    if (o == 0x0D && !a.empty()) return Stmt::make("play_sound").set("id", a[0]);
    if (o == 0x0E && !a.empty()) return Stmt::make("selector").set("value", a[0]);
    if (o == 0x0F) return Stmt::make("end");
    if (o == 0x12 && a.size() >= 12) {
        std::vector<int> m, f;
        for (int i = 0; i < 10; ++i) m.push_back(a[i]);
        f.push_back(a[10]);
        f.push_back(a[11]);
        return Stmt::make("fight").setList("monsters", m).setList("flags", f);
    }
    if (o == 0x13 && a.size() >= 10) {
        std::vector<int> d;
        for (uint8_t x : a) d.push_back(x);
        return Stmt::make("fight_b").setList("data", d);
    }
    if (o == 0x14) return Stmt::make("clear_tile_event");
    if (o == 0x15 && a.size() >= 3)
        return Stmt::make("apply_party").set("count", a[0]).set("op", a[1]).set("val", a[2]);
    if (o == 0x18 && a.size() >= 4)
        return Stmt::make("set_party_bits")
            .set("member", a[0])
            .set("field", a[1])
            .set("and", a[2])
            .set("or", a[3]);
    if (o == 0x17 && a.size() >= 2)
        return Stmt::make("load_var8").set("group", a[0]).set("index", a[1]);
    if (o == 0x1A && a.size() >= 2)
        return Stmt::make("store_var8").set("group", a[0]).set("value", a[1]);
    if (o == 0x1D && !a.empty()) return Stmt::make("audio_wait").set("index", a[0]);
    if (o == 0x1E && !a.empty()) return Stmt::make("delay").set("ticks", a[0]);
    if ((o == 0x1F || o == 0x20) && a.size() >= 6) {
        std::vector<int> args;
        for (uint8_t x : a) args.push_back(x);
        return Stmt::make("party_effect")
            .set("mode", o == 0x20 ? 1 : 0)
            .set("sel", a[0])
            .setList("args", args);
    }
    if (o == 0x21 && a.size() >= 3) {
        int y = (a[0] >> 4) & 0xF, x = a[0] & 0xF;
        return Stmt::make("set_tile").set("y", y).set("x", x).set("a", a[1]).set("b", a[2]);
    }
    if (o == 0x26 || o == 0x27)
        return Stmt::make("select_member").set("mode", o == 0x27 ? 1 : 0);
    if (o == 0x19 && a.size() >= 4)
        return Stmt::make("give_item")
            .set("member", a[0])
            .set("item", a[1])
            .set("charges", a[2])
            .set("flags", a[3]);
    if (o == 0x29) return Stmt::make("abort");
    if (o == 0x2A && a.size() >= 14) {
        std::vector<int> d;
        for (uint8_t x : a) d.push_back(x);
        return Stmt::make("treasure").setList("data", d);
    }
    if (o == 0x2C && !a.empty()) return Stmt::make("add_counter").set("amount", a[0]);
    if (o == 0x2E && a.size() >= 2)
        return Stmt::make("or_member_field").set("arg1", a[0]).set("arg2", a[1]);
    if (o == 0x2F) return Stmt::make("read_answer");
    if (o == 0x31 && a.size() >= 3) {
        int v = a[1] | (a[2] << 8);
        return Stmt::make("party_damage").set("member", a[0]).set("value", v);
    }

    Stmt s = Stmt::make("raw_op").set("op", o);
    std::vector<int> args;
    for (uint8_t x : a) args.push_back(x);
    s.setList("args", args);
    return s;
}

std::vector<Stmt> liftRange(const std::vector<LoweredOp>& ops, const uint8_t* seg, size_t segLen,
                            int start, int end);

// VM: OP_10 skips N tokens when cond!=0; OP_11 skips when cond==0.
// Retail layout is fall-through: the region after the skip still runs unless
// the taken path aborted/ended. Exclusive if/else is a two-skip idiom:
//   cond; OP_11 n=len(then)+1; then; OP_10 m=len(else); else
std::vector<Stmt> liftRange(const std::vector<LoweredOp>& ops, const uint8_t* seg, size_t segLen,
                            int start, int end) {
    std::vector<Stmt> body;
    int i = start;
    // Live cond survives say/wait/etc. until OP_10/11 (VM cond_flag is sticky).
    // If the cond-set was already emitted as a side-effect stmt, branch uses prior_cond
    // so lower does not duplicate the opcode.
    std::optional<Expr> pendingCond;
    bool pendingEmitted = false;

    while (i < end) {
        const LoweredOp& op = ops[i];

        if (isCondSetOp(op.op)) {
            const bool branched =
                (i + 1 < end) && (ops[i + 1].op == 0x10 || ops[i + 1].op == 0x11);
            pendingCond = opToCondExpr(op);
            if (branched) {
                // Consume into the following if — do not also emit as a stmt.
                pendingEmitted = false;
                ++i;
                continue;
            }
            // Side-effecting cond ops (party write / give / party_effect): emit stmt,
            // keep cond live for a later OP_10/11 (e.g. give_item; wait; if …).
            if (op.op == 0x15 || op.op == 0x19 || op.op == 0x1F || op.op == 0x20) {
                auto st = opToStmt(op);
                if (st) body.push_back(*st);
                pendingEmitted = true;
                ++i;
                continue;
            }
            // Pure cond op not yet branched: keep live without emitting; a later
            // OP_10/11 will attach it. Orphan (no branch) stays lost — same as before.
            pendingEmitted = false;
            ++i;
            continue;
        }

        if ((op.op == 0x10 || op.op == 0x11) && !op.args.empty()) {
            int n = op.args[0];
            size_t skipStart = static_cast<size_t>(op.off + 2);
            auto targetOff = skipNTokens(seg, segLen, skipStart, n);
            if (!targetOff) {
                Stmt s = Stmt::make("raw_op").set("op", op.op);
                std::vector<int> args;
                for (uint8_t x : op.args) args.push_back(x);
                s.setList("args", args);
                body.push_back(s);
                ++i;
                pendingCond.reset();
                pendingEmitted = false;
                continue;
            }
            int skipIdx = indexAtOffset(ops, static_cast<int>(*targetOff));
            const int rawSkipIdx = skipIdx;
            if (skipIdx > end) skipIdx = end;

            Expr cond;
            if (pendingEmitted)
                cond = Expr::make("prior_cond");
            else if (pendingCond)
                cond = *pendingCond;
            else
                cond = Expr::make("unknown");
            pendingCond.reset();
            pendingEmitted = false;
            Stmt ifs = Stmt::make("if");
            ifs.cond = cond;

            // Exclusive if/else: OP_11 then immediately OP_10 after then-body.
            if (op.op == 0x11 && skipIdx < end && ops[skipIdx].op == 0x10 &&
                !ops[skipIdx].args.empty() && n == (skipIdx - (i + 1)) + 1) {
                const LoweredOp& op10 = ops[skipIdx];
                int m = op10.args[0];
                size_t elseStart = static_cast<size_t>(op10.off + 2);
                auto elseEndOff = skipNTokens(seg, segLen, elseStart, m);
                int elseEndIdx = elseEndOff ? indexAtOffset(ops, static_cast<int>(*elseEndOff)) : end;
                if (elseEndIdx > end) elseEndIdx = end;
                ifs.thenBody = liftRange(ops, seg, segLen, i + 1, skipIdx);
                ifs.elseBody = liftRange(ops, seg, segLen, skipIdx + 1, elseEndIdx);
                body.push_back(std::move(ifs));
                auto rest = liftRange(ops, seg, segLen, elseEndIdx, end);
                body.insert(body.end(), rest.begin(), rest.end());
                return body;
            }

            if (op.op == 0x10) {
                // cond true → skip else region; false → run else then fall into rest.
                // When the skip lands PAST our parent range (rawSkipIdx > end), this is
                // the locksmith/pay pattern: success jumps over a shared join (often
                // go_to/abort that OP_0C/29 ends on), so pull success ops into then.
                if (rawSkipIdx > end) {
                    ifs.elseBody = liftRange(ops, seg, segLen, i + 1, end);
                    ifs.thenBody = liftRange(ops, seg, segLen, rawSkipIdx, static_cast<int>(ops.size()));
                    body.push_back(std::move(ifs));
                    // Ops at [end, rawSkipIdx) belong to the parent join / shared exit.
                    return body;
                }
                ifs.elseBody = liftRange(ops, seg, segLen, i + 1, skipIdx);
                ifs.thenBody.clear();
            } else {
                // cond false → skip then region; true → run then then fall into rest.
                ifs.thenBody = liftRange(ops, seg, segLen, i + 1, skipIdx);
                ifs.elseBody.clear();
            }
            body.push_back(std::move(ifs));
            auto rest = liftRange(ops, seg, segLen, skipIdx, end);
            body.insert(body.end(), rest.begin(), rest.end());
            return body;
        }

        if (op.op == 0x2B && !op.args.empty()) {
            // OP_2B @ 0x16D74: skip N tokens when COMBAT_VICTORY_LATCH set — same
            // shape as OP_10 (true → skip else region).
            int n = op.args[0];
            size_t skipStart = static_cast<size_t>(op.off + 2);
            auto targetOff = skipNTokens(seg, segLen, skipStart, n);
            if (targetOff) {
                int skipIdx = indexAtOffset(ops, static_cast<int>(*targetOff));
                if (skipIdx > end) skipIdx = end;
                pendingCond.reset();
                pendingEmitted = false;
                Stmt ifs = Stmt::make("if");
                ifs.cond = Expr::make("combat_victory");
                ifs.elseBody = liftRange(ops, seg, segLen, i + 1, skipIdx);
                ifs.thenBody.clear();
                body.push_back(std::move(ifs));
                auto rest = liftRange(ops, seg, segLen, skipIdx, end);
                body.insert(body.end(), rest.begin(), rest.end());
                return body;
            }
        }

        auto st = opToStmt(op);
        if (st) body.push_back(*st);
        // Do NOT clear pendingCond here — VM cond_flag survives say/wait/select_member
        // until the next cond-set or OP_10/11 (delayed-cond patterns).
        ++i;

        // OP_0C / OP_29 / OP_0F stop the script — anything still in this range is
        // only reachable via an earlier skip (often duplicated on the fail path).
        if (st && (st->kind == "go_to" || st->kind == "abort" || st->kind == "end") && i < end) {
            auto dead = liftRange(ops, seg, segLen, i, end);
            if (!dead.empty()) {
                Stmt u = Stmt::make("unreachable");
                u.body = std::move(dead);
                body.push_back(std::move(u));
            }
            return body;
        }
    }
    return body;
}

}  // namespace

std::vector<Stmt> liftSegment(const std::vector<LoweredOp>& ops, const uint8_t* seg, size_t segLen) {
    if (ops.empty()) return {};
    auto lifted = liftRange(ops, seg, segLen, 0, static_cast<int>(ops.size()));
    if (!lifted.empty()) return lifted;
    std::vector<Stmt> fallback;
    for (const auto& o : ops) {
        auto st = opToStmt(o);
        if (st) fallback.push_back(*st);
        else {
            Stmt s = Stmt::make("raw_op").set("op", o.op);
            std::vector<int> args;
            for (uint8_t x : o.args) args.push_back(x);
            s.setList("args", args);
            fallback.push_back(s);
        }
    }
    return fallback;
}

}  // namespace mm2::eventlang
