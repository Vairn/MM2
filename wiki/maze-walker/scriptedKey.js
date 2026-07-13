/**
 * Scripted key buffer — mirrors EventVmHelpers eventVmScriptedKey* @ 0x97A2.
 * Used by OP_08/0A ($FD mode) and OP_0E FD print chrome.
 */
"use strict";

export function scriptedKeyReset(session) {
  session.scriptedKeyIdx = 0xffff;
  session.scriptedKeyRep = 0xffff;
  session.scriptedKeyDly = 0;
  session.scriptedKeyDy = 0x40;
  session.scriptedKeyDx = 0x20;
}

/** @param {object} session @param {number[]|Uint8Array} bytes */
export function scriptedKeyQueue(session, bytes) {
  const src = bytes ?? [];
  let len = src.length;
  if (len > 255) len = 255;
  const buf = new Array(256).fill(0);
  for (let i = 0; i < len; i++) buf[i] = (src[i] ?? 0) & 0xff;
  buf[len] = 0xff;
  session.scriptedKeyBuf = buf;
  scriptedKeyReset(session);
}

function skipFfRecords(session, n) {
  if (n <= 0) return;
  let rep = (session.scriptedKeyRep << 16) >> 16; /* int16 */
  if (rep !== -1) return;
  rep = -1;
  const buf = session.scriptedKeyBuf ?? [];
  while (n > 0) {
    ++rep;
    if (rep < 0 || rep > 255) break;
    if ((buf[rep] ?? 0) === 0xff) --n;
  }
  session.scriptedKeyRep = rep & 0xffff;
}

/**
 * @returns {{ key: number, place: { active: boolean, clear: boolean, placement: number, dstX: number, dstY: number } | null }}
 * key: ASCII code, or -1 if none this tick.
 */
export function scriptedKeyPoll(session) {
  const place = { active: false, clear: false, placement: 0, dstX: 0, dstY: 0 };
  if (!session) return { key: -1, place: null };

  let delay = session.scriptedKeyDly | 0;
  if (delay !== 0) {
    const burn = delay > 3 ? 3 : delay;
    session.scriptedKeyDly = (delay - burn) & 0xff;
    return { key: -1, place: null };
  }

  let rep = (session.scriptedKeyRep << 16) >> 16;
  if (rep !== -1) {
    if (!placeOne(session, place)) {
      /* choreography done */
    }
    return { key: -1, place: place.active ? place : null };
  }

  let idx = (session.scriptedKeyIdx << 16) >> 16;
  if (idx < 0) {
    idx = 0;
    session.scriptedKeyIdx = 0;
    session.scriptedKeyRep = 0xffff;
  }

  const buf = session.scriptedKeyBuf ?? [];
  const mode = session.scriptedKeyMode | 0;

  for (let guard = 0; guard < 64; guard++) {
    if (idx < 0 || idx > 255) return { key: -1, place: null };
    const b = (buf[idx] ?? 0) & 0xff;
    if (b === 0xff) {
      if (mode === 0xfd) {
        idx = 0;
        session.scriptedKeyIdx = 0;
        continue;
      }
      return { key: -1, place: null };
    }
    if (b === 0) return { key: -1, place: null };

    if ((b & 0x80) !== 0) {
      let n = b & 0x7f;
      if (n <= 0) n = 1;
      skipFfRecords(session, n);
      placeOne(session, place);
      ++idx;
      if (idx <= 255) {
        const d = (buf[idx] ?? 0) & 0xff;
        if (d !== 0 && d !== 0xff) {
          if ((session.scriptedKeyDly | 0) === 0) session.scriptedKeyDly = d;
          ++idx;
        }
      }
      session.scriptedKeyIdx = idx & 0xffff;
      return { key: -1, place: place.active ? place : null };
    }

    const key = b;
    ++idx;
    if (idx <= 255) {
      const d = (buf[idx] ?? 0) & 0xff;
      if (d !== 0 && d !== 0xff) {
        session.scriptedKeyDly = d;
        ++idx;
      }
    }
    session.scriptedKeyIdx = idx & 0xffff;
    return { key, place: null };
  }
  return { key: -1, place: null };
}

function placeOne(session, place) {
  let rep = (session.scriptedKeyRep << 16) >> 16;
  ++rep;
  if (rep < 0 || rep > 255) {
    session.scriptedKeyRep = 0xffff;
    return false;
  }
  const buf = session.scriptedKeyBuf ?? [];
  let d1 = (buf[rep] ?? 0) & 0xff;
  if (d1 === 0xff) {
    session.scriptedKeyRep = 0xffff;
    return false;
  }
  const maxp = session.scriptedKeyMaxp | 0;
  if (maxp !== 0 && maxp !== 0xff && d1 >= maxp) d1 = 0;
  const argY = session.scriptedKeyDy | 0;
  const argX = session.scriptedKeyDx | 0;
  place.active = true;
  place.clear = false;
  place.placement = d1;
  place.dstX = argY;
  place.dstY = (argX + 8) & 0xffff;
  ++rep;
  if (rep > 255) {
    session.scriptedKeyRep = 0xffff;
    return false;
  }
  const dly = (buf[rep] ?? 0) & 0xff;
  if (dly === 0xff) {
    session.scriptedKeyRep = 0xffff;
    return false;
  }
  session.scriptedKeyDly = dly;
  session.scriptedKeyRep = rep & 0xffff;
  return true;
}

/**
 * Wait for SPACE while polling scripted buffer (OP_08 / EventRuntime Space wait).
 * Resolves on scripted space OR real space from waitForSpace.
 */
export async function waitSpaceWithScriptedKey(ctx, text, sprite, vmOp) {
  const { session, waitForSpace } = ctx;
  /* Drain any immediate scripted SPACE before showing the prompt. */
  for (let i = 0; i < 32; i++) {
    const { key, place } = scriptedKeyPoll(session);
    if (place?.active && ctx.onScriptedPlace) ctx.onScriptedPlace(place);
    if (key === 0x20 || key === 0x0d || key === 0x0a) return;
    if (key < 0 && (session.scriptedKeyDly | 0) === 0) break;
  }
  await waitForSpace(text ?? "", sprite, vmOp);
}
