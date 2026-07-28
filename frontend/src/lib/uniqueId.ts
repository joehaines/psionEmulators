// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Formatting and parsing for the EPOC "Unique id" (System → Information →
// Machine), as used by the debug panel.
//
// EPOC prints a 64-bit value in four hex groups — 1000-118A-CAFE-BABE. It is
// assembled from two places, and the panel can set both: the low half is the
// machine's identity chip, the high half a model UID patched into the OS
// image (the ROM, or on the card-booted 5mx Pro / netBook the OS image on the
// CF card). On machines where the emulator can't locate that constant the
// high half stays as the image has it, and on machines whose value has never
// been read out of the dialog it isn't shown at all — hence the null-prefix
// mode.

/** 64-bit value as 16 upper-case hex digits. */
export const hex16 = (v: bigint) => (v & 0xFFFFFFFFFFFFFFFFn).toString(16).toUpperCase().padStart(16, '0');
/** 32-bit value as 8 upper-case hex digits. */
export const hex8 = (v: number) => (v >>> 0).toString(16).toUpperCase().padStart(8, '0');

/** Inserts EPOC's group separator every four digits. */
export const group4 = (digits: string) => digits.replace(/(.{4})(?=.)/g, '$1-');

/** Composes the 64-bit id from the two halves the emulator reports. */
export const composeUniqueId = (prefix: number | null, id: number): bigint =>
  (BigInt(prefix ?? 0) << 32n) | BigInt(id >>> 0);

/** The value as the OS prints it: ROM half (when known) + identity-chip half. */
export function formatUniqueId(prefix: number | null, id: number): string {
  return group4((prefix != null ? hex8(prefix) : '') + hex8(id));
}

/**
 * Normalises typed or pasted input to at most `digits` hex characters, keeping
 * EPOC's grouping. An 0x prefix, grouping dashes and anything non-hex are
 * dropped rather than left to invalidate the field — so an extra keystroke in
 * a full field can't dead-end the panel with "Set" greyed out.
 */
export function sanitiseUniqueId(raw: string, digits: number): string {
  const hex = raw.replace(/^\s*0[xX]/, '').replace(/[^0-9a-fA-F]/g, '')
                 .slice(0, digits).toUpperCase();
  return group4(hex);
}

/**
 * What's in the field, as the 64-bit id to program. Short input is treated as
 * the low (identity-chip) digits, so typing just the 8 that machine can change
 * still works; `prefix` fills the high half in that case so the caller always
 * sends a complete id. Null only when the field is empty.
 */
export function parseUniqueId(text: string, prefix: number | null): bigint | null {
  const digits = text.replace(/-/g, '');
  if (digits.length === 0) return null;
  const typed = BigInt('0x' + digits);
  if (digits.length > 8) return typed & 0xFFFFFFFFFFFFFFFFn;
  return (BigInt(prefix ?? 0) << 32n) | (typed & 0xFFFFFFFFn);
}
