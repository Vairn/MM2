/**
 * OP_0B service-sign .anm resolver — mirrors ServiceSignResolver.cpp.
 *
 * Tables @ A4-$6C62/$6C4C/$6C32/$6C1A/$6C02 (data 0x139C…). Env→table mapping
 * is scrambled (OP_0B jump table @ 0x157D2); do NOT index towns in order.
 */
"use strict";

const TABLE_LEN = 24;

/* env0 → $6C62 */
const TBL_6C62 = [70, 62, 63, 66, 67, 68, 65, 2, 19, 26, 51, 54, 53, 12, 60, 27, 39, 4, 45, 37, 57, 47, 73, 33];
/* env3 → $6C4C */
const TBL_6C4C = [73, 33, 42, 43, 17, 14, 69, 34, 9, 26, 24, 52, 53, 21, 25, 28, 44, 49, 11, 31, 55, 36, 1, 61];
/* env1 → $6C32 */
const TBL_6C32 = [72, 16, 10, 23, 6, 51, 15, 8, 5, 49, 40, 11, 30, 39, 4, 46, 20, 36, 1, 57, 13, 58, 18, 47];
/* env4/6 → $6C1A */
const TBL_6C1A = [74, 42, 2, 17, 14, 69, 19, 34, 9, 26, 24, 52, 54, 8, 21, 25, 3, 29, 44, 50, 27, 39, 61, 48];
/* env2/5 → $6C02 */
const TBL_6C02 = [71, 59, 33, 19, 35, 10, 24, 6, 75, 51, 15, 7, 60, 56, 29, 5, 22, 50, 30, 41, 46, 37, 58, 0];

const ENV_LO = [0, 5, 17, 33, 41, 45, 55];
const ENV_HI = [4, 16, 32, 40, 44, 54, 59];
const ENV_ID = [0, 3, 1, 6, 4, 5, 2];

function tableForEnv(envId) {
  /* ServiceSignResolver::tableForEnv — scrambled jump table @ 0x157D2. */
  switch (envId) {
    case 0:
      return TBL_6C62;
    case 1:
      return TBL_6C32;
    case 2:
      return TBL_6C02;
    case 3:
      return TBL_6C4C;
    case 4:
      return TBL_6C1A;
    case 5:
      return TBL_6C02;
    case 6:
      return TBL_6C1A;
    default:
      return null;
  }
}

export function signEnvIdForScreen(screenId) {
  const sid = screenId | 0;
  for (let i = 0; i < ENV_LO.length; i++) {
    if (sid >= ENV_LO[i] && sid <= ENV_HI[i]) return ENV_ID[i];
  }
  return 7;
}

export function resolveSignAnmId(envId, strIdx) {
  if ((strIdx & 0xff) >= 0x80) return 0x4b;
  if (envId < 0 || envId > 6) return -1;
  if ((strIdx & 0xff) === 0) return -1;
  const tbl = tableForEnv(envId);
  if (!tbl) return -1;
  const idx = (strIdx & 0xff) - 1;
  if (idx < 0 || idx >= TABLE_LEN) return -1;
  return tbl[idx] | 0;
}

/** @returns {{ sheet: string, frame: string } | null} */
export function resolveOp0bSprite(session, screenId, strIdx) {
  let env = session?.signEnvId;
  if (env == null || env > 6) {
    env = signEnvIdForScreen(screenId);
    if (session) session.signEnvId = env;
  }
  const anm = resolveSignAnmId(env, strIdx);
  if (anm <= 0) return null;
  return { sheet: `${String(anm).padStart(2, "0")}_anm`, frame: "0" };
}
