// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// A tiny character-level language model used to score candidate
// decryptions of Psion word-processor documents.
//
// Both password schemes we can undo — SIBO Word's 9-byte additive key
// (psion-word-sibo.ts) and EPOC32 Word's 32-byte additive key
// (epoc-password.ts) — repeat a short keystream over highly structured
// English text.  Each key byte is therefore independent, and the right
// value is the one that makes its share of the document read like prose.
// This module supplies the unigram/bigram statistics that "read like
// prose" is measured against.
//
// The only per-format difference is which byte value ends a paragraph:
// SIBO Word writes 0x00, EPOC32 Word writes 0x06.  The sample text's
// sentence and line breaks are mapped to that byte so the model learns
// which letters sit either side of a paragraph mark.
//
// Everything is self-contained: no data is fetched at runtime, and the
// models are built once per paragraph byte and cached.

export interface CharModel {
  uni: Float64Array;      // log P(byte)
  bi:  Float64Array[];    // log P(byte | previous byte)
}

const CACHE = new Map<number, CharModel>();

export function englishModel(paragraphByte: number): CharModel {
  const cached = CACHE.get(paragraphByte);
  if (cached) return cached;

  const V = 256;
  const uniC = new Float64Array(V).fill(0.02);
  const biC: Float64Array[] = Array.from({ length: V }, () => new Float64Array(V).fill(0.02));

  const sample = ENGLISH_SAMPLE.replace(/[.\n]/g, String.fromCharCode(paragraphByte));
  let prev = -1;
  for (let i = 0; i < sample.length; i++) {
    const c = sample.charCodeAt(i) & 0xff;
    uniC[c]++;
    if (prev >= 0) biC[prev][c]++;
    prev = c;
  }

  const uni = new Float64Array(V);
  const uniSum = uniC.reduce((a, b) => a + b, 0);
  for (let i = 0; i < V; i++) uni[i] = Math.log(uniC[i] / uniSum);
  const bi: Float64Array[] = [];
  for (let a = 0; a < V; a++) {
    const rowSum = biC[a].reduce((s, x) => s + x, 0);
    const row = new Float64Array(V);
    for (let b = 0; b < V; b++) row[b] = Math.log(biC[a][b] / rowSum);
    bi.push(row);
  }

  const model: CharModel = { uni, bi };
  CACHE.set(paragraphByte, model);
  return model;
}

// A few KB of ordinary English prose.  Content is irrelevant beyond being
// representative of everyday letter/word statistics; it is only used to
// score candidate decryptions.
export const ENGLISH_SAMPLE = `
The morning light came slowly across the fields and the town began to wake.
People walked to work along the quiet streets, talking about the weather and
the news of the day. In the small office by the river a woman opened her diary
and started to write a letter to an old friend. She wrote about her family, her
garden and the long summer that had passed, and she asked after his children and
his health. The words came easily because there was so much to say and so little
time to say it. When the letter was finished she read it through once more, made a
few small changes, and folded it carefully into an envelope. Outside the window a
boy was selling papers on the corner and a bus went past with its lights still on.
It is a simple thing to write down what you think and feel, but it is one of the
oldest and most useful things that people do. A note left on the kitchen table, a
list of jobs for the week, a report for the office, a story for a child at night:
all of these begin with a single word and grow from there. Good writing is clear
and honest. It says what it means and it does not waste the reader's time. The
best advice is to write the way you speak, to keep your sentences short, and to
read your work aloud so that you can hear where it stumbles. Every document, long
or short, is really just a conversation between the writer and the reader, carried
across time and distance by a handful of letters on a page. When you save your work
you keep that conversation safe, and years later you can open the file again and
find the same words waiting for you, exactly as you left them, ready to be read.
This is the writing of some words and the making of a plain and ordinary record.
`;
