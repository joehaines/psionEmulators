// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC Record (.wve) -> standard RIFF WAVE.
//
// Sound files on Series 5 (Record app, voice memos, alarm sounds) use
// a Direct File Store wrapper around a small section table:
//
//   [16-byte UID header]
//   [4-byte section-table-offset]
//   ...section payloads...
//   [section table: 1-byte count + N x (4-byte UID, 4-byte offset)]
//
// The Record Section (UID 0x10000052) holds the actual audio:
//
//   Offset Size  Description
//   0x00   L     Uncompressed sample count
//   0x04   L     Compression: 0x00000000 = standard 8-bit PCM
//                              0x100001A1 = ADPCM
//   0x08   W     Repeat count - 1
//   0x0A   B     Volume (1..5)
//   0x0B   B     Reserved
//   0x0C   L     Time between repeats (microseconds)
//   0x10   L     Sound data byte count
//   0x14   ...   Sound data
//
// Standard compression is unsigned 8-bit PCM at 8 kHz mono — straight
// from the EPOC SDK Sound API.  We convert to 16-bit signed PCM in a
// RIFF WAVE container that plays in every browser and audio app.
//
// ADPCM isn't implemented — voice memos in the wild are all standard
// PCM, and writing an EPOC-flavoured ADPCM decoder needs a separate
// reverse-engineering pass.  We return null on ADPCM so the caller
// falls back to downloading the raw .wve.
//
// Spec: psiconv formats/psion/Record_File.psi + Record_Section.psi.

import { EPOC_HEADER_BYTES, readUint32LE } from './epoc-uids.ts';

const RECORD_SECTION_UID            = 0x10000052;
const COMPRESSION_PCM               = 0x00000000;
const COMPRESSION_ADPCM             = 0x100001A1;

// Series 5 Record app's documented native sample rate.
const SAMPLE_RATE_HZ                = 8000;

export function convertRecordToWav(input: Uint8Array): Uint8Array | null {
  if (input.length < EPOC_HEADER_BYTES + 4) return null;

  // Walk the section table to find the Record Section.
  const tableOffset = readUint32LE(input, EPOC_HEADER_BYTES);
  if (tableOffset < EPOC_HEADER_BYTES + 4 || tableOffset >= input.length) return null;

  const countByte = input[tableOffset];
  if ((countByte & 1) !== 0) return null;       // need single-byte cardinal
  const count = countByte >> 1;
  if (count === 0 || count > 16) return null;

  let recordSectionOffset = -1;
  for (let i = 0; i < count; i++) {
    const off = tableOffset + 1 + i * 8;
    if (off + 8 > input.length) return null;
    const uid    = readUint32LE(input, off);
    const target = readUint32LE(input, off + 4);
    if (uid === RECORD_SECTION_UID) {
      recordSectionOffset = target;
      break;
    }
  }
  if (recordSectionOffset < 0 || recordSectionOffset + 20 > input.length) return null;

  const compression = readUint32LE(input, recordSectionOffset + 4);
  if (compression === COMPRESSION_ADPCM) return null;       // not implemented
  if (compression !== COMPRESSION_PCM)   return null;       // unknown variant

  const soundDataLen = readUint32LE(input, recordSectionOffset + 16);
  const soundStart   = recordSectionOffset + 20;
  if (soundStart + soundDataLen > input.length) return null;

  const samples = input.subarray(soundStart, soundStart + soundDataLen);
  return wrapAsWav(samples, SAMPLE_RATE_HZ, 1);
}

// Wrap unsigned 8-bit PCM as a 16-bit signed PCM RIFF WAVE.  We expand
// to 16-bit because raw 8-bit WAV uses a fixed unsigned encoding that
// most browsers handle correctly, BUT going to 16-bit signed gives
// noticeably better playback fidelity in apps that resample.
function wrapAsWav(pcm8: Uint8Array, sampleRate: number, channels: number): Uint8Array {
  const bytesPerSample = 2;
  const sampleCount    = pcm8.length;
  const dataBytes      = sampleCount * bytesPerSample;
  const byteRate       = sampleRate * channels * bytesPerSample;

  const buffer = new Uint8Array(44 + dataBytes);
  const dv = new DataView(buffer.buffer);

  // RIFF chunk
  writeAscii(buffer, 0, 'RIFF');
  dv.setUint32(4, 36 + dataBytes, true);
  writeAscii(buffer, 8, 'WAVE');

  // fmt sub-chunk
  writeAscii(buffer, 12, 'fmt ');
  dv.setUint32(16, 16, true);                       // PCM fmt chunk size
  dv.setUint16(20, 1, true);                        // PCM format
  dv.setUint16(22, channels, true);
  dv.setUint32(24, sampleRate, true);
  dv.setUint32(28, byteRate, true);
  dv.setUint16(32, channels * bytesPerSample, true);
  dv.setUint16(34, 16, true);                       // bits/sample

  // data sub-chunk
  writeAscii(buffer, 36, 'data');
  dv.setUint32(40, dataBytes, true);

  // Unsigned 8-bit -> signed 16-bit: subtract 128, scale by 256.
  // Wait — this WAS what we did, but the reference WAVs (and the
  // EPOC Sound API spec) show samples are stored as SIGNED 8-bit PCM.
  // Verified against tests/fixtures/123 + tests/fixtures/123_as_*khz.wav:
  // each source byte equals (sample value >> 8) treated as signed.
  // E.g. source byte 0x4B (75) -> sample 19200 = 75 << 8 (signed);
  //      source byte 0xFF (-1) -> sample  -256 = -1 << 8 (signed).
  for (let i = 0; i < sampleCount; i++) {
    const signed = (pcm8[i] << 24) >> 24;          // sign-extend the u8
    dv.setInt16(44 + i * 2, signed << 8, true);
  }
  return buffer;
}

function writeAscii(buffer: Uint8Array, offset: number, s: string): void {
  for (let i = 0; i < s.length; i++) buffer[offset + i] = s.charCodeAt(i);
}
