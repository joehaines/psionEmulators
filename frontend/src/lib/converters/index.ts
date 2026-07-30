// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Public API: detect a Psion file's format from its UID header and, if
// we have a converter for it, transform it to a modern equivalent.
//
// The CFCardDialog calls `tryConvert(bytes, originalName)` for each
// download when the "Convert on download" checkbox is on.  A null
// return means "no converter ran" — the caller should download the
// original bytes unchanged.
//
// Password-protected Word documents (SIBO .WRD and EPOC32 .wrd) are
// recovered on the way through, so a protected file converts to the same
// thing an unprotected one would.  `crib` is the optional "type the
// document's first characters" hint the dialogs offer for documents too
// short for the automatic key search to settle on its own.

import {
  EPOC_HEADER_BYTES, readEpocHeader, isValidEpocHeader,
  UID3_WORD, UID3_SHEET, UID3_RECORD,
  UID3_MBM, UID3_SKETCH_APP, UID3_SKETCH_FILE,
} from './epoc-uids.ts';
import { convertSketchToPng } from './sketch.ts';
import { convertRecordToWav } from './record.ts';
import { convertWordToRtf } from './word.ts';
import { convertSheetToCsv } from './sheet.ts';
import {
  isSiboWord, isSiboWordEncrypted, extractSiboWordText,
} from './psion-word-sibo.ts';
import {
  isEpocPasswordProtected, isProtectedEpocWord, recoverEpocWord,
} from './epoc-password.ts';
import { isProtectedEpocSheet, recoverEpocSheet } from './epoc-password-sheet.ts';

export interface ConversionResult {
  bytes: Uint8Array;
  filename: string;
  mime: string;
  formatLabel: string; // human label for status messages: "Word -> text"
  // Set for Word documents that were password-protected (SIBO .WRD or
  // EPOC32 .wrd alike): true means the password was recovered
  // automatically for this download.
  recoveredPassword?: boolean;
  // Recovery confidence 0..1 (only meaningful when recoveredPassword).
  confidence?: number;
  // Set when a recovery was under-determined — the document is too short
  // for the statistical key search to settle, and the caller should offer
  // the crib ("type the first few characters") route for an exact result.
  needsCrib?: boolean;
}

interface ConverterDef {
  label: string;
  ext: string;
  mime: string;
  convert: (bytes: Uint8Array) => Uint8Array | null;
}

const REGISTRY: Record<number, ConverterDef> = {
  [UID3_WORD]:   { label: 'Word -> RTF',    ext: 'rtf', mime: 'application/rtf', convert: convertWordToRtf },
  [UID3_SHEET]:  { label: 'Sheet -> CSV',   ext: 'csv', mime: 'text/csv',       convert: convertSheetToCsv },
  [UID3_RECORD]: { label: 'Record -> WAV',  ext: 'wav', mime: 'audio/wav',      convert: convertRecordToWav },
  // Raw multi-bitmap container (system icons, etc.).
  [UID3_MBM]:         { label: 'Sketch -> PNG', ext: 'png', mime: 'image/png', convert: convertSketchToPng },
  // Saved Sketch *picture* file — what the Sketch app writes when you
  // save a drawing.  Has a Sketch-specific envelope around an embedded
  // bitmap header.  This is the UID3 of every real Sketch file we've
  // seen in the wild (verified against reference/Sketch).
  [UID3_SKETCH_FILE]: { label: 'Sketch -> PNG', ext: 'png', mime: 'image/png', convert: convertSketchToPng },
  // The Sketch application UID itself occasionally turns up as the UID3
  // on older saves; treat it as an alias.
  [UID3_SKETCH_APP]:  { label: 'Sketch -> PNG', ext: 'png', mime: 'image/png', convert: convertSketchToPng },
};

// Returns:
//   null   - input is not a recognised convertible EPOC file (caller
//            should download the original bytes)
//   ConversionResult - successfully converted to a modern format
export function tryConvert(
  bytes: Uint8Array,
  originalName: string,
  crib?: Uint8Array,
): ConversionResult | null {
  // SIBO / EPOC16 Word (.WRD) has its own "PSIONWPDATAFILE" magic rather
  // than a UID header, and may be password-protected — handle it (and
  // recover the password automatically) before the UID-based dispatch.
  if (isSiboWord(bytes)) return convertSiboWord(bytes, originalName, crib);

  if (bytes.length < EPOC_HEADER_BYTES) return null;

  const header = readEpocHeader(bytes);
  if (!header) return null;
  if (!isValidEpocHeader(header)) return null;

  // An EPOC32 document can be password-protected, which enciphers the
  // parts of it a converter needs. Recover it first and convert the
  // recovered copy; the UID3 dispatch below then runs as usual.
  let source = bytes;
  let recovery: { confidence: number; needsCrib: boolean } | null = null;
  if (isEpocPasswordProtected(bytes)) {
    try {
      if (isProtectedEpocWord(bytes)) {
        const recovered = recoverEpocWord(bytes, crib);
        if (!recovered) return null;
        source = recovered.file;
        recovery = {
          confidence: recovered.confidence,
          // The statistical half of Word's recovery needs a few hundred
          // characters to be sure of every key byte; under about eight
          // samples each it can still get the odd character wrong, so say
          // so and offer the crib. A document whose key came out of known
          // plaintext alone needs no such warning.
          needsCrib: recovered.pinnedKeyBytes < 32 && recovered.samplesPerKeyByte < 8,
        };
      } else if (isProtectedEpocSheet(bytes)) {
        // Sheet's recovery is exact or it fails — its structures supply
        // all the known plaintext the key needs, so there is nothing to
        // be uncertain about and no crib to ask for.
        const recovered = recoverEpocSheet(bytes);
        if (!recovered) return null;
        source = recovered.file;
        recovery = { confidence: 1, needsCrib: false };
      } else {
        // Some other protected document type (Data, an Agenda file):
        // same scheme over structures we don't decode.
        return null;
      }
    } catch {
      return null;      // malformed file — hand back the original
    }
  }

  const def = REGISTRY[header.uid3];
  if (!def) return null;

  let converted: Uint8Array | null = null;
  try {
    converted = def.convert(source);
  } catch {
    // Format parsing errors must not crash the dialog — fall back to
    // raw download instead.
    converted = null;
  }
  if (!converted) return null;

  return {
    bytes: converted,
    filename: replaceExtension(originalName, def.ext),
    mime: def.mime,
    formatLabel: def.label,
    recoveredPassword: recovery ? true : undefined,
    confidence: recovery?.confidence,
    needsCrib: recovery?.needsCrib || undefined,
  };
}

// True when this file is a password-protected Psion document of any
// flavour — used by the file dialogs to badge it before the user asks
// for a download.
export function isPasswordProtected(bytes: Uint8Array): boolean {
  return isSiboWordEncrypted(bytes) || isEpocPasswordProtected(bytes);
}

// True when we can actually read the contents back out: the protected
// formats whose recovery is implemented (SIBO Word, EPOC32 Word, EPOC32
// Sheet).
export function canRecoverPassword(bytes: Uint8Array): boolean {
  return isSiboWordEncrypted(bytes) || isProtectedEpocWord(bytes) ||
         isProtectedEpocSheet(bytes);
}

// True when a crib — the document's first characters, as the user
// remembers them — can improve the recovery.  It can for the word
// processors, whose keys are partly recovered statistically; Sheet's key
// comes out of its own structures exactly, so there is nothing to help.
export function recoveryAcceptsCrib(bytes: Uint8Array): boolean {
  return isSiboWordEncrypted(bytes) || isProtectedEpocWord(bytes);
}

// SIBO Word -> UTF-8 text, recovering the password automatically for
// protected documents, or exactly when the caller passes the crib (the
// document's known first characters).
function convertSiboWord(
  bytes: Uint8Array, originalName: string, crib?: Uint8Array,
): ConversionResult | null {
  let extracted;
  try {
    extracted = extractSiboWordText(bytes, crib);
  } catch {
    return null;
  }
  if (!extracted) return null;
  const encrypted = isSiboWordEncrypted(bytes);
  return {
    bytes: new TextEncoder().encode(extracted.text),
    filename: replaceExtension(originalName, 'txt'),
    mime: 'text/plain;charset=utf-8',
    formatLabel: encrypted ? 'Word (password recovered) -> text' : 'Word -> text',
    recoveredPassword: encrypted || undefined,
    confidence: encrypted ? extracted.confidence : undefined,
  };
}

// Replace the last extension (after final '.') with `newExt`.  If the
// name has no extension, append '.<newExt>' instead.
function replaceExtension(name: string, newExt: string): string {
  const dot = name.lastIndexOf('.');
  if (dot <= 0) return `${name}.${newExt}`;
  return `${name.substring(0, dot)}.${newExt}`;
}
