// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal `mod` shim that lets the worker-mode hook reuse the ENTIRE main-thread
// AudioEngine for SPEAKER output, with zero changes to audioEngine.ts.
//
// In worker mode the emulator (and its DAC) live in the worker, so the worker
// drains audio and posts int16 batches to the main thread. This shim presents
// those batches through the same `mod` surface the AudioEngine already drives:
// its pump() calls readAudioOutput(ptr, max) into mod.HEAPU8 and feeds the
// speaker worklet — so all the worklet / AudioContext / autoplay-gesture /
// gain-graph machinery is reused unchanged. setHostAudioEnabled is routed to the
// worker (which actually tells the emulator to produce audio); mic input is
// deferred (writeAudioInput is a no-op).
import type { PsionModule } from '../types/emulator';

export interface WorkerAudioShim {
  mod: PsionModule;
  /** Feed an int16 batch delivered by the worker into the speaker queue. */
  pushSamples(samples: Int16Array): void;
}

export function createWorkerAudioShim(
  setHostAudio: (speaker: boolean, mic: boolean) => void,
  sendMic: (samples: Int16Array) => void,
): WorkerAudioShim {
  const CAP = 1 << 16;                       // 64 KB scratch backing
  const ab = new ArrayBuffer(CAP);
  const HEAPU8 = new Uint8Array(ab);
  let bump = 16;                             // simple bump allocator for scratch
  const queue: { arr: Int16Array; off: number }[] = [];
  let queued = 0;
  const MAX_QUEUED = 16000;                  // ~2 s @ 8 kHz; drop oldest if we fall behind

  const mod = {
    HEAPU8,
    _malloc(n: number): number {
      const p = bump; bump = (bump + n + 7) & ~7;
      if (bump > CAP) { bump = 16; return 16; }     // engine allocs scratch once; never wraps in practice
      return p;
    },
    _free(_p: number): void { /* bump allocator: no-op */ },
    setHostAudioEnabled(speaker: boolean, mic: boolean): void { setHostAudio(!!speaker, !!mic); },
    // Copy up to `max` queued int16 samples into HEAPU8 at `ptr`; return the count.
    readAudioOutput(ptr: number, max: number): number {
      const out = new Int16Array(ab, ptr, max);
      let n = 0;
      while (n < max && queue.length) {
        const head = queue[0];
        const take = Math.min(max - n, head.arr.length - head.off);
        out.set(head.arr.subarray(head.off, head.off + take), n);
        n += take; head.off += take;
        if (head.off >= head.arr.length) queue.shift();
      }
      queued -= n;
      return n;
    },
    // Mic capture (main-thread getUserMedia/MicProcessor) writes int16 into
    // HEAPU8 here; ship a copy to the worker, which feeds the emulator's
    // writeAudioInput. Only called while the mic is live (engine-side guarded).
    writeAudioInput(ptr: number, count: number): void {
      if (count <= 0) return;
      sendMic(new Int16Array(ab, ptr, count).slice());   // copy → transferable to the worker
    },
  } as unknown as PsionModule;

  return {
    mod,
    pushSamples(samples: Int16Array): void {
      if (samples.length === 0) return;
      queue.push({ arr: samples, off: 0 });
      queued += samples.length;
      // Cap latency: if the consumer (worklet) falls behind, drop the oldest.
      while (queued > MAX_QUEUED && queue.length > 1) {
        queued -= queue[0].arr.length - queue[0].off;
        queue.shift();
      }
    },
  };
}
