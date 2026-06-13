// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Public API: detect a Psion file's format from its UID header and, if
// we have a converter for it, transform it to a modern equivalent.
//
// The CFCardDialog calls `tryConvert(bytes, originalName)` for each
// download when the "Convert on download" checkbox is on.  A null
// return means "no converter ran" — the caller should download the
// original bytes unchanged.

import {
  EPOC_HEADER_BYTES, readEpocHeader, isValidEpocHeader,
  UID3_WORD, UID3_SHEET, UID3_RECORD,
  UID3_MBM, UID3_SKETCH_APP, UID3_SKETCH_FILE,
} from './epoc-uids.ts';
import { convertSketchToPng } from './sketch.ts';
import { convertRecordToWav } from './record.ts';
import { convertWordToRtf } from './word.ts';
import { convertSheetToCsv } from './sheet.ts';

export interface ConversionResult {
  bytes: Uint8Array;
  filename: string;
  mime: string;
  formatLabel: string; // human label for status messages: "Word -> text"
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
): ConversionResult | null {
  if (bytes.length < EPOC_HEADER_BYTES) return null;

  const header = readEpocHeader(bytes);
  if (!header) return null;
  if (!isValidEpocHeader(header)) return null;

  const def = REGISTRY[header.uid3];
  if (!def) return null;

  let converted: Uint8Array | null = null;
  try {
    converted = def.convert(bytes);
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
  };
}

// Replace the last extension (after final '.') with `newExt`.  If the
// name has no extension, append '.<newExt>' instead.
function replaceExtension(name: string, newExt: string): string {
  const dot = name.lastIndexOf('.');
  if (dot <= 0) return `${name}.${newExt}`;
  return `${name.substring(0, dot)}.${newExt}`;
}
