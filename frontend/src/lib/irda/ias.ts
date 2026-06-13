// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IAS — IrDA Information Access Service client. Before we can reach the
// device's OBEX inbox server we must learn which LSAP-SEL it listens on.
// That is published in the device's IAS database under class "OBEX",
// attribute "IrDA:TinyTP:LsapSel". We open an LM-MUX connection to the
// well-known IAS LSAP (0x00) and issue a GetValueByClass query.
//
// GetValueByClass request:
//   op(0x04|last=0x84) | classLen | className | attrLen | attrName
// GetValueByClass response:
//   op(0x84) | retCode | listLen(2 BE) | { objId(2 BE) type value }…
// where an INTEGER value (type 0x01) is a 4-byte big-endian number.

import type { IrLmp } from './irlmp.ts';
import {
  IAS_OP_GET_VALUE_BY_CLASS,
  IAS_LAST_FRAME,
  IAS_RET_SUCCESS,
  IAS_TYPE_INTEGER,
  IAS_TYPE_OCTET_SEQ,
  LSAP_IAS,
  LSAP_CLIENT_IAS,
} from './types.ts';

export function encodeGetValueByClass(className: string, attrName: string): Uint8Array {
  const out: number[] = [IAS_OP_GET_VALUE_BY_CLASS | IAS_LAST_FRAME];
  out.push(className.length & 0xff);
  for (const c of className) out.push(c.charCodeAt(0) & 0xff);
  out.push(attrName.length & 0xff);
  for (const c of attrName) out.push(c.charCodeAt(0) & 0xff);
  return new Uint8Array(out);
}

export interface IasResult {
  retCode: number;
  // First INTEGER value in the result list, if any.
  integer: number | null;
}

export function parseGetValueByClassReply(resp: Uint8Array): IasResult {
  // A reply is at minimum op(1) + retCode(1); failure replies stop there.
  if (resp.length < 2) return { retCode: -1, integer: null };
  const retCode = resp[1];
  if (retCode !== IAS_RET_SUCCESS) return { retCode, integer: null };
  if (resp.length < 4) return { retCode, integer: null };
  // resp[2..3] = list length; we read the first element only.
  let p = 4;
  // objId (2 bytes) precedes the typed value.
  if (p + 3 > resp.length) return { retCode, integer: null };
  p += 2; // skip object identifier
  const type = resp[p++];
  if (type === IAS_TYPE_INTEGER) {
    if (p + 4 > resp.length) return { retCode, integer: null };
    const v = ((resp[p] << 24) | (resp[p + 1] << 16) | (resp[p + 2] << 8) | resp[p + 3]) >>> 0;
    return { retCode, integer: v };
  }
  if (type === IAS_TYPE_OCTET_SEQ) {
    // length(2) + bytes — not what we expect for LsapSel, ignore value.
    return { retCode, integer: null };
  }
  // USER_STRING / MISSING / anything else: no integer to surface.
  return { retCode, integer: null };
}

// High-level helper: open an IAS LM-MUX connection, query
// class/attribute, and resolve with the integer value (the OBEX
// LSAP-SEL). Rejects on connect timeout, query timeout, or a non-success
// IAS return code.
export async function iasGetIntegerByClass(
  lmp: IrLmp,
  className: string,
  attrName: string,
  timeoutMs: number,
): Promise<number> {
  await lmp.connect(LSAP_CLIENT_IAS, LSAP_IAS, timeoutMs);
  const reply = await new Promise<Uint8Array>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('IAS query timed out')), timeoutMs);
    lmp.setDataHandler(LSAP_CLIENT_IAS, (data) => {
      clearTimeout(timer);
      resolve(data);
    });
    lmp.sendData(LSAP_CLIENT_IAS, encodeGetValueByClass(className, attrName));
  });
  const result = parseGetValueByClassReply(reply);
  lmp.disconnect(LSAP_CLIENT_IAS);
  if (result.retCode !== IAS_RET_SUCCESS || result.integer === null) {
    throw new Error(`IAS GetValueByClass(${className}, ${attrName}) failed: ret ${result.retCode}`);
  }
  return result.integer;
}
