// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import type { PsionModule } from '../types/emulator';

// Connects the emulator's synthetic codec to the browser's WebAudio graph:
//   • DAC queue → AudioWorkletNode → AudioContext.destination (speaker)
//   • Browser mic → getUserMedia → AudioWorkletNode → mod.writeAudioInput
//
// AudioWorklet processors are built as Blob URLs so this file is fully
// self-contained — no extra static asset plumbing needed for dev or prod.

// Speaker worklet: receives int16 samples from the main thread and
// linearly resamples them from the emulator rate (emuRate, e.g. 8000) to
// the AudioContext's sampleRate. The internal queue is a plain Float32Array
// ring; overflow drops the oldest samples, underflow emits silence.
const SPEAKER_PROCESSOR = `
class SpeakerProcessor extends AudioWorkletProcessor {
  constructor(opts) {
    super();
    const p = (opts && opts.processorOptions) || {};
    this.emuRate = p.emuRate || 8000;
    this.outRate = sampleRate;
    this.ratio = this.emuRate / this.outRate;
    // ~200 ms of headroom at 48 kHz output — 9600 samples. Oversized the
    // emu-rate queue to avoid drops when RAF is bursty.
    this.queue = new Float32Array(8192);
    this.qHead = 0;
    this.qTail = 0;
    this.phase = 0;
    this.lastSample = 0;
    this.samplesReceived = 0;  // debug counters
    this.samplesEmitted = 0;
    this.peakAbs = 0;
    this.port.onmessage = e => {
      const data = e.data;
      if (!data) return;
      if (data.type === 'samples') {
        const arr = data.samples;
        this.samplesReceived += arr.length;
        for (let i = 0; i < arr.length; i++) {
          const next = (this.qTail + 1) & (this.queue.length - 1);
          if (next === this.qHead) {
            // Overflow — drop oldest.
            this.qHead = (this.qHead + 1) & (this.queue.length - 1);
          }
          this.queue[this.qTail] = arr[i];
          this.qTail = next;
          const a = arr[i] >= 0 ? arr[i] : -arr[i];
          if (a > this.peakAbs) this.peakAbs = a;
        }
      } else if (data.type === 'query') {
        this.port.postMessage({
          type: 'stats',
          received: this.samplesReceived,
          emitted: this.samplesEmitted,
          peakAbs: this.peakAbs,
          qFill: (this.qTail - this.qHead + this.queue.length) & (this.queue.length - 1),
        });
      }
    };
  }
  readOne() {
    if (this.qHead === this.qTail) return this.lastSample; // underflow
    const s = this.queue[this.qHead];
    this.qHead = (this.qHead + 1) & (this.queue.length - 1);
    this.lastSample = s;
    return s;
  }
  process(_inputs, outputs) {
    const out = outputs[0][0];
    if (!out) return true;
    for (let i = 0; i < out.length; i++) {
      const frac = this.phase;
      // Linear interpolation between previous (lastSample) and next read.
      if (frac >= 1) {
        const whole = Math.floor(frac);
        for (let k = 0; k < whole; k++) this.readOne();
        this.phase -= whole;
      }
      const prev = this.lastSample;
      // Peek next without advancing; we'll advance when phase crosses 1.
      const next = (this.qHead === this.qTail)
        ? prev
        : this.queue[this.qHead];
      out[i] = prev + (next - prev) * this.phase;
      this.phase += this.ratio;
    }
    this.samplesEmitted += out.length;
    // Mirror mono to any additional output channels.
    for (let c = 1; c < outputs[0].length; c++) outputs[0][c].set(out);
    return true;
  }
}
registerProcessor('speaker-processor', SpeakerProcessor);
`;

// Mic worklet: downsamples the AudioContext's input rate (typically
// 44.1 / 48 kHz) to the emulator codec rate (8 kHz) via a Hamming-
// windowed-sinc FIR low-pass with cutoff at 3.4 kHz — the telephone-
// quality speech band the Psion's 8-bit PCM codec was designed for.
//
// Why not box-average: the previous implementation averaged N samples
// per output (N=6 for 48 kHz → 8 kHz). Box-averaging is cheap but its
// frequency response sin(πf/N)/(πf/N) has slow rolloff and high
// sidelobes, so any high-frequency content in the input (sibilants,
// room hiss, system noise) aliases back into the speech band and
// arrives at the codec as crunchy artifacts. A 32-tap windowed-sinc
// at the same compute cost (decimation reduces work by N×) gives a
// clean 3.4 kHz brick-wall response, recovering the "warmer" sound
// the user remembers.
//
// Gain stage: after filtering, apply a modest linear gain so the
// browser-AGC'd typical speech peak (~0.3 fs) maps to a healthy
// portion of the int8 dynamic range without clipping on loud spikes.
// The main-thread converter still hard-limits to [-1, 1] as a final
// safety net.
const MIC_PROCESSOR = `
// Linear gain applied after the anti-alias filter. The browser's AGC
// (enabled in getUserMedia constraints below) normalises typical
// speech peaks to ~0.3 fs; 1.6× lifts that to ~0.5 fs (≈ ±64 in int8
// after the codec's >>8 shift) — well below the ±127 clip point so
// loud transients still survive without distortion.
const MIC_GAIN = 1.6;
// Anti-alias filter design parameters. Cutoff 3.4 kHz is the
// telephone speech band — covers speech intelligibility without
// reaching the codec's 4 kHz Nyquist, leaving a transition band for
// rolloff.
//
// Filter length scales with the AudioContext sample rate so the
// filter spans a constant time window (~660 µs) regardless of
// browser. A fixed 32-tap design would have too gentle a rolloff
// at 96 kHz sample rate (only -7 dB at 4 kHz vs -10 dB at 48 kHz),
// allowing aliasing into the speech band. Scaling the tap count
// keeps the same -10 dB / -23 dB attenuation profile at 4 / 5 kHz
// across 44.1 / 48 / 88.2 / 96 kHz browser sample rates.
const MIC_CUTOFF_HZ = 3400;
const MIC_REF_TAPS = 32;       // taps at the reference sample rate
const MIC_REF_SAMPLE_RATE = 48000;
const MIC_MIN_TAPS = 24;
const MIC_MAX_TAPS = 96;
class MicProcessor extends AudioWorkletProcessor {
  constructor(opts) {
    super();
    const p = (opts && opts.processorOptions) || {};
    this.emuRate = p.emuRate || 8000;
    this.ratio = sampleRate / this.emuRate; // e.g. 48000/8000 = 6
    // Generate a Hamming-windowed-sinc FIR low-pass at runtime, sized
    // to the actual AudioContext sample rate. Coefficients normalised
    // so sum(h) = 1 (unity gain at DC). Tap count scales with the
    // sample rate so the filter's time-domain span (and therefore
    // its frequency-domain transition width) stays constant. Verified
    // response targets at all common browser sample rates:
    //   ~0 dB to 1 kHz, ~-4 dB at 3 kHz, ~-10 dB at 4 kHz
    //   (codec Nyquist), <-23 dB above 5 kHz.
    const N = Math.max(
      MIC_MIN_TAPS,
      Math.min(MIC_MAX_TAPS,
        Math.round(MIC_REF_TAPS * sampleRate / MIC_REF_SAMPLE_RATE)));
    this.filterLen = N;
    this.taps = new Float32Array(N);
    let tapSum = 0;
    for (let n = 0; n < N; n++) {
      const w = 0.54 - 0.46 * Math.cos(2 * Math.PI * n / (N - 1));
      const arg = 2 * MIC_CUTOFF_HZ * (n - (N - 1) / 2) / sampleRate;
      const sinc = arg === 0 ? 1 : Math.sin(Math.PI * arg) / (Math.PI * arg);
      this.taps[n] = w * sinc;
      tapSum += this.taps[n];
    }
    for (let n = 0; n < N; n++) this.taps[n] /= tapSum;
    // Phase accumulator — emits one output sample each time it
    // crosses 'ratio'. Fractional ratios (e.g. 44100/8000 = 5.5125)
    // are handled naturally.
    this.phase = 0;
    // Circular history buffer for the FIR convolution; one filter
    // window's worth of past input.
    this.history = new Float32Array(N);
    this.histWrite = 0;
    // Output batch — ~20 ms of emu samples per postMessage at 8 kHz.
    this.batch = new Float32Array(512);
    this.batchLen = 0;
  }
  flush() {
    if (this.batchLen === 0) return;
    const slice = this.batch.slice(0, this.batchLen);
    this.port.postMessage({ type: 'samples', samples: slice }, [slice.buffer]);
    this.batchLen = 0;
  }
  process(inputs) {
    const ch = inputs[0] && inputs[0][0];
    if (!ch) return true;
    const taps = this.taps;
    const tapsN = this.filterLen;
    for (let i = 0; i < ch.length; i++) {
      // Push the new sample into the history ring.
      this.history[this.histWrite] = ch[i];
      this.histWrite = (this.histWrite + 1) % tapsN;
      // Phase accumulator decimation. The fractional 'ratio' (e.g.
      // 48000/8000 = 6 exactly, but at 44.1 kHz it's 5.5125) is
      // handled by accumulating and emitting whenever we cross the
      // threshold.
      this.phase += 1;
      if (this.phase >= this.ratio) {
        this.phase -= this.ratio;
        // Convolve: y = sum(h[k] * x[n-k]) walking the ring
        // backwards from the most recent sample.
        let acc = 0;
        let idx = this.histWrite;  // points one past newest
        for (let k = 0; k < tapsN; k++) {
          idx = (idx === 0 ? tapsN - 1 : idx - 1);
          acc += taps[k] * this.history[idx];
        }
        if (this.batchLen < this.batch.length) {
          this.batch[this.batchLen++] = acc * MIC_GAIN;
        }
        if (this.batchLen >= this.batch.length) this.flush();
      }
    }
    if (this.batchLen > 0) this.flush();
    return true;
  }
}
registerProcessor('mic-processor', MicProcessor);
`;

function blobUrl(src: string): string {
  return URL.createObjectURL(new Blob([src], { type: 'application/javascript' }));
}

export interface AudioEngine {
  setSpeaker(enabled: boolean): Promise<void>;
  setMic(enabled: boolean): Promise<void>;
  pump(): void;
  destroy(): void;
  // Re-publish the host-side speaker/mic enable state to the WASM
  // module. Used after a device switch: the same WASM `mod` handle
  // is reused across devices, but `loadROM` constructs a fresh
  // emulator instance internally whose `setHostAudioEnabled` flags
  // start at false. Calling this re-syncs the new emulator with the
  // engine's persisted state so the audio path keeps working without
  // requiring the user to toggle Speaker / Mic off and on again.
  republishHostState(): void;
  // Test-only: return the speaker worklet's running counters.
  getSpeakerStats(): Promise<{
    received: number; emitted: number; peakAbs: number; qFill: number;
    contextState: string; sampleRate: number;
  } | null>;
}

// Module-level handle to a pre-warmed AudioContext. The browser's
// autoplay policy lets resume() succeed only inside a user gesture
// handler, but it also lets us *construct* and *kick off* the resume
// from inside the gesture even if the rest of the AudioWorklet setup
// happens asynchronously afterwards. The 5mx Pro device-load click is
// a user gesture; calling primeAudioContext() from inside that
// click's synchronous prefix puts the AudioContext into the running
// state by the time createAudioEngine() picks it up later.
//
// Returns null if AudioContext isn't available (e.g. SSR, some test
// environments). The optional ctx is reused by the next createAudio-
// Engine call.
let primedContext: AudioContext | null = null;
export function primeAudioContext(): AudioContext | null {
  if (primedContext) return primedContext;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const Ctor = (window.AudioContext || (window as any).webkitAudioContext) as typeof AudioContext | undefined;
  if (!Ctor) return null;
  const ctx = new Ctor();
  // resume() returns a promise; by initiating it synchronously inside
  // the gesture handler we satisfy the autoplay policy. We don't await
  // — the caller is on the gesture's call stack and can't yield.
  void ctx.resume().catch(() => { /* will retry on first explicit toggle */ });
  primedContext = ctx;
  return ctx;
}

// ── Mobile audio-session unlock ─────────────────────────────────────────
// iOS Safari (and other mobile browsers) gate AudioContext output behind
// a successful HTMLMediaElement playback inside a user gesture handler.
// "Inside a gesture" is strictly enforced — once any `await` yields to
// the microtask queue iOS may have already revoked the gesture token,
// even when the awaited promise resolves immediately. So the unlock
// has to be issued from the SYNCHRONOUS prefix of the click handler,
// before the caller's `await ensureAudioEngine()` chain runs.
//
// Without this, every speaker toggle in a fresh session needed to be
// paired with a mic toggle to keep the destination unmuted — the
// getUserMedia stream activates Chrome's "communications" audio
// category, which independently keeps output alive. Symptom: "speaker
// only works if the mic is enabled" reproduced on iOS / Android mobile.
//
// Implementation: keep a module-level silent <audio> element that's
// created lazily on the first prime call. Subsequent prime calls just
// .play() it — iOS only needs to see a successful media play once per
// page lifetime to release the gate, and the element loops a slightly
// non-silent buffer (volume=0 so it's still inaudible) so the OS
// counts it as ongoing media playback for the rest of the session.
let mobileUnlockEl: HTMLAudioElement | null = null;
let mobileUnlockUrl: string | null = null;
function makeMobileUnlockAudio(): HTMLAudioElement {
  // ~0.5 s of low-level non-zero samples at 8 kHz mono 16-bit. iOS
  // treats all-zero buffers as "silent media" and won't always count
  // them as evidence of media playback; a tiny ±1-LSB triangle wave
  // is still inaudible at volume=0 but satisfies the heuristic.
  const sampleRate = 8000;
  const seconds = 0.5;
  const sampleCount = (sampleRate * seconds) | 0;
  const dataBytes = sampleCount * 2;
  const buf = new ArrayBuffer(44 + dataBytes);
  const view = new DataView(buf);
  // RIFF header
  view.setUint32(0, 0x52494646, false);    // "RIFF"
  view.setUint32(4, 36 + dataBytes, true); // file size - 8
  view.setUint32(8, 0x57415645, false);    // "WAVE"
  view.setUint32(12, 0x666d7420, false);   // "fmt "
  view.setUint32(16, 16, true);            // fmt chunk size
  view.setUint16(20, 1, true);             // PCM
  view.setUint16(22, 1, true);             // mono
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * 2, true); // byte rate
  view.setUint16(32, 2, true);              // block align
  view.setUint16(34, 16, true);             // bits per sample
  view.setUint32(36, 0x64617461, false);    // "data"
  view.setUint32(40, dataBytes, true);
  // Body: alternate ±1 to give a non-zero waveform (still inaudible
  // at volume=0, but iOS sees actual sample variation).
  for (let i = 0; i < sampleCount; i++) {
    view.setInt16(44 + i * 2, (i & 1) ? 1 : -1, true);
  }
  if (!mobileUnlockUrl) {
    mobileUnlockUrl = URL.createObjectURL(new Blob([buf], { type: 'audio/wav' }));
  }
  const a = new Audio(mobileUnlockUrl);
  a.loop = true;
  a.volume = 0;
  a.preload = 'auto';
  return a;
}

// Synchronous mobile-audio-session unlock. Call this from inside the
// click handler that toggles the speaker (or any other gesture that
// precedes audio output) BEFORE the first `await`. Safe to call from
// any gesture; subsequent calls just re-issue .play() in case the
// element got auto-paused.
export function primeMobileAudioSession(): void {
  if (!mobileUnlockEl) mobileUnlockEl = makeMobileUnlockAudio();
  // Promise from play() can reject if autoplay policy hasn't been
  // satisfied yet — that's expected on the very first attempt outside
  // a gesture; swallow silently and the next gesture will retry.
  void mobileUnlockEl.play().catch(() => { /* retry on next gesture */ });
}

function pauseMobileAudioSession(): void {
  if (mobileUnlockEl) mobileUnlockEl.pause();
}

export async function createAudioEngine(
  mod: PsionModule,
  emuSampleRate: number,
): Promise<AudioEngine> {
  let ctx: AudioContext | null = primedContext;
  primedContext = null;  // adopt — subsequent prime calls re-prime
  let speakerNode: AudioWorkletNode | null = null;
  let micStream: MediaStream | null = null;
  let micSource: MediaStreamAudioSourceNode | null = null;
  let micNode: AudioWorkletNode | null = null;
  let speakerOn = false;
  // `micOn` is the user's DESIRED mic state (the toggle). `micCapturing`
  // is whether a live getUserMedia stream is currently attached. They
  // diverge while the page is hidden: we release the stream (so the OS
  // "microphone in use" indicator clears and we genuinely stop listening
  // in the background) but keep micOn=true so capture resumes when the
  // page becomes active again. The speaker path is completely independent
  // of both — enabling the speaker never touches the mic.
  let micOn = false;
  let micCapturing = false;
  let speakerUrl: string | null = null;
  let micUrl: string | null = null;
  let micScratchPtr = 0;
  let micScratchCap = 0;
  let outScratchPtr = 0;
  let outScratchCap = 0;

  // Silent driver node. A ConstantSourceNode at offset 0 is technically
  // a valid source, but some browsers optimise it away — the audio
  // thread sees "this node will always output zero" and skips it,
  // leaving the destination idle. A muted OscillatorNode produces
  // genuinely time-varying samples (computed every render quantum) so
  // the audio pipeline has to keep running, while the gain=0 sink
  // ahead of the destination keeps it inaudible.
  //
  // IMPORTANT: only attach the silent driver while the speaker is on.
  // Browsers light up the tab "audio is playing" indicator whenever
  // anything (even a gain=0 source) is connected to destination, so
  // keeping this node permanently wired makes us look perpetually
  // noisy even when the user has muted the speaker.
  let silentDriver: OscillatorNode | null = null;
  let silentDriverGain: GainNode | null = null;

  // Master speaker gain. Enabling the browser mic puts Chrome's audio output
  // into the "communications" category, which uses a noticeably louder gain
  // curve than the default "media" path. Compensate by lowering the speaker
  // gain whenever the mic stream is live so the perceived loudness stays
  // roughly consistent between the two states.
  //
  // Lifecycle parallels silentDriver: connected only while the speaker
  // is on so we don't keep the destination "active" from the browser's
  // point of view.
  let speakerGain: GainNode | null = null;
  const kSpeakerGainMicOff = 1.0;
  const kSpeakerGainMicOn  = 0.5;

  // The mobile-audio-session unlock element now lives at module scope
  // (see primeMobileAudioSession above) so it can be played from inside
  // the synchronous prefix of a click handler, before the caller's
  // ensureAudioEngine() await chain yields the gesture token.

  let workletsAdded = false;
  // Track whether we've attached the gesture / visibility listeners.
  // Browsers reject `ctx.resume()` outside a user-gesture handler, so a
  // resume() from RAF or any async callback may silently do nothing
  // even though it returns successfully. Attaching listeners to the
  // page lets us catch the next pointer / key / touch event and resume
  // the context from inside its handler — at which point the speaker
  // worklet actually starts emitting samples. visibilitychange covers
  // the mobile-browser case where backgrounding the app moves the
  // context into 'interrupted' state and returning needs an explicit
  // resume() before audio comes back.
  let gestureListenersAttached = false;
  function attemptResume(reason: string): void {
    void reason;
    if (ctx && ctx.state !== 'running') {
      ctx.resume().catch(() => { /* will retry on next event */ });
    }
  }
  // Suspend the live mic stream while the page isn't active and bring it
  // back when the user returns. We fully stop the MediaStream tracks
  // (rather than just muting them) so the browser/OS drops the
  // "microphone in use" indicator and we honour the user's expectation
  // that we only listen while the page is in the foreground — on mobile
  // a getUserMedia stream otherwise keeps capturing after the user
  // switches away from the browser. `micOn` (the desired state) is left
  // untouched so the toggle stays lit and capture resumes on return.
  function onPageHidden(): void {
    if (micCapturing) stopMicCapture();
  }
  function onPageVisible(): void {
    attemptResume('visibility');
    // Re-acquire the mic only if the user still has it enabled and we
    // released it on hide. The session-level permission grant means this
    // does not re-prompt. Guard on ctx so we never spin one up just to
    // restore the mic.
    if (micOn && !micCapturing && ctx) {
      void startMicCapture(ctx).catch(() => { /* surfaced via toggle path */ });
    }
  }
  function attachGestureListeners(): void {
    if (gestureListenersAttached) return;
    gestureListenersAttached = true;
    const gestureEvents = ['pointerdown', 'touchstart', 'mousedown', 'keydown'];
    const onGesture = () => attemptResume('gesture');
    for (const evt of gestureEvents) {
      window.addEventListener(evt, onGesture, { passive: true });
    }
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') onPageVisible();
      else onPageHidden();
    });
    // iOS Safari doesn't always fire visibilitychange when the user
    // backgrounds the tab via the app switcher; pagehide is the reliable
    // signal there. Mirror it to releasing the mic. (No 'pageshow'
    // counterpart is needed — visibilitychange fires on return.)
    window.addEventListener('pagehide', onPageHidden);
  }

  async function ensureContext(): Promise<AudioContext> {
    if (!ctx) {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const Ctor = (window.AudioContext || (window as any).webkitAudioContext) as typeof AudioContext;
      ctx = new Ctor();
    }
    // Always await resume() — even when state is reported as 'running' some
    // browsers leave the destination silently muted until resume() is
    // explicitly re-called from a gesture / async-following-a-gesture path.
    // Cheap no-op when the context really is fully active.
    await ctx.resume().catch(() => { /* ignore — caller surfaces failures */ });
    if (!workletsAdded) {
      // Whether ctx came from primeAudioContext() or was just created
      // here, the worklets still need to be registered before we can
      // construct AudioWorkletNode instances. addModule is async but
      // doesn't need a user gesture.
      speakerUrl = blobUrl(SPEAKER_PROCESSOR);
      micUrl = blobUrl(MIC_PROCESSOR);
      await ctx.audioWorklet.addModule(speakerUrl);
      await ctx.audioWorklet.addModule(micUrl);
      workletsAdded = true;
    }
    attachGestureListeners();
    return ctx;
  }

  // Bring up the speaker-side graph (silentDriver + speakerGain
  // connected to destination). Called from setSpeaker(true) only. Doing
  // this in ensureContext meant the graph stayed up across speaker
  // disables — keeping the tab's "audio playing" indicator lit even
  // when the user had toggled the speaker off.
  function ensureSpeakerGraph(c: AudioContext): void {
    if (!speakerGain) {
      speakerGain = new GainNode(c, { gain: micOn ? kSpeakerGainMicOn : kSpeakerGainMicOff });
      speakerGain.connect(c.destination);
    }
    if (!silentDriver) {
      // CRITICAL: the gain MUST be technically non-zero. A gain of
      // exactly 0 lets Chrome / Safari mark the destination as idle
      // even though an OscillatorNode is feeding it — the engine
      // sees "final output is zero" and short-circuits the audio
      // graph. Symptom: with speaker enabled but mic off the user
      // hears no buzzer clicks at all (only when the mic stream is
      // also live does the destination stay pulled). 0.00001 is
      // -100 dB, well below the noise floor of any output device,
      // so it's perceptually identical to silence while keeping
      // the destination "active" from the browser's point of view.
      // This recurring regression has been re-introduced every time
      // someone bumps the silent-driver lifecycle around — see
      // commit history.
      silentDriverGain = new GainNode(c, { gain: 0.00001 });
      silentDriver = new OscillatorNode(c, { frequency: 440, type: 'sine' });
      silentDriver.connect(silentDriverGain);
      silentDriverGain.connect(c.destination);
      silentDriver.start();
    }
  }

  function teardownSpeakerGraph(): void {
    if (silentDriver) {
      try { silentDriver.stop(); } catch { /* may already be stopped */ }
      silentDriver.disconnect();
      silentDriver = null;
    }
    if (silentDriverGain) {
      silentDriverGain.disconnect();
      silentDriverGain = null;
    }
    if (speakerGain) {
      speakerGain.disconnect();
      speakerGain = null;
    }
  }

  // When both speaker and mic are off, suspend the AudioContext so the
  // browser's tab indicator drops the "audio is playing" badge and the
  // OS can release the audio session. Re-resume on next setSpeaker/
  // setMic enable. Best-effort: ignore failures.
  function maybeSuspendContext(): void {
    if (!speakerOn && !micOn && ctx && ctx.state === 'running') {
      void ctx.suspend().catch(() => { /* best-effort */ });
    }
  }

  function publishHostState() {
    mod.setHostAudioEnabled(speakerOn, micOn);
  }

  async function setSpeaker(enabled: boolean): Promise<void> {
    if (enabled === speakerOn) return;
    if (enabled) {
      // Belt-and-braces: re-issue the mobile-audio-session unlock here
      // too, even though the caller is expected to call
      // primeMobileAudioSession() synchronously from inside the click
      // handler BEFORE awaiting ensureAudioEngine(). This retry covers
      // engines instantiated outside the toggleSpeaker path.
      primeMobileAudioSession();

      const c = await ensureContext();
      // Stand the speaker graph up only now that we know the user
      // wants audio out. Keeps the tab's "audio playing" indicator
      // off while the speaker is muted.
      ensureSpeakerGraph(c);
      // Belt-and-braces unlock for desktop Chrome too: a 1-sample
      // silent BufferSource inside the same gesture chain commits the
      // WebAudio output pipeline.
      try {
        const silentBuf = c.createBuffer(1, 1, c.sampleRate);
        const unlockSrc = c.createBufferSource();
        unlockSrc.buffer = silentBuf;
        unlockSrc.connect(c.destination);
        unlockSrc.start();
      } catch { /* createBuffer may throw on extreme sample rates; ignore */ }
      speakerNode = new AudioWorkletNode(c, 'speaker-processor', {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [1],
        processorOptions: { emuRate: emuSampleRate },
      });
      // Route through the shared GainNode so the mic-on/off volume
      // compensation applies. ensureSpeakerGraph above guarantees
      // speakerGain is initialised before we reach here.
      speakerNode.connect(speakerGain!);
      speakerOn = true;
    } else {
      if (speakerNode) {
        speakerNode.disconnect();
        speakerNode.port.close();
        speakerNode = null;
      }
      // Take the speaker graph back down so the browser's tab audio
      // indicator drops the "playing" badge. The silent driver in
      // particular kept the destination "active" for the browser even
      // at gain=0 — leaving it wired meant we always looked noisy.
      teardownSpeakerGraph();
      // Pause the silent unlock element while the speaker is off so
      // we're not consuming the iOS audio session unnecessarily. A
      // future re-enable will play() it again.
      pauseMobileAudioSession();
      speakerOn = false;
      maybeSuspendContext();
    }
    publishHostState();
  }

  // Acquire the browser mic and wire it into the codec. Idempotent: a
  // no-op when a stream is already live. Throws on permission denial /
  // no device — the caller decides whether to roll the desired state
  // back. Does NOT touch `micOn` (the desired-state flag) — that's the
  // caller's responsibility — so the page-visibility resume path can
  // reattach a stream without changing what the user asked for.
  async function startMicCapture(c: AudioContext): Promise<void> {
    if (micCapturing) return;
    // Permission prompt happens here; throws on denial.
    // echoCancellation stays on because Chrome's "communications"
    // audio category — which the AEC stream activates — is what
    // actually keeps the destination reliably pulled in some
    // browsers. Disabling AEC silenced the speaker entirely when
    // the mic was off (in addition to fixing the volume jump). The
    // volume difference is compensated for via the speakerGain
    // GainNode, which gets a softer setting whenever the mic is live.
    const stream = await navigator.mediaDevices.getUserMedia({
      audio: { echoCancellation: true, noiseSuppression: true, autoGainControl: true },
    });
    // getUserMedia is async; the user may have toggled the mic back off
    // (or the page may have been hidden) while the permission prompt was
    // up. If so, release the freshly-granted stream immediately rather
    // than leaving a zombie capture running.
    if (!micOn || (typeof document !== 'undefined' && document.visibilityState === 'hidden')) {
      stream.getTracks().forEach(t => t.stop());
      return;
    }
    micStream = stream;
    micSource = c.createMediaStreamSource(micStream);
    micNode = new AudioWorkletNode(c, 'mic-processor', {
      numberOfInputs: 1,
      numberOfOutputs: 0,
      processorOptions: { emuRate: emuSampleRate },
    });
    micNode.port.onmessage = e => {
      // Drop samples that arrive after the user disabled the mic or the
      // stream was suspended (worklet teardown isn't instantaneous).
      if (!micOn || !micCapturing) return;
      const f32 = e.data?.samples as Float32Array | undefined;
      if (!f32 || f32.length === 0) return;
      // Convert Float32 [-1, 1] → int16, push into WASM.
      const bytes = f32.length * 2;
      if (micScratchCap < bytes) {
        if (micScratchPtr) mod._free(micScratchPtr);
        micScratchPtr = mod._malloc(bytes);
        micScratchCap = bytes;
      }
      const heap16 = new Int16Array(mod.HEAPU8.buffer, micScratchPtr, f32.length);
      for (let i = 0; i < f32.length; i++) {
        const v = Math.max(-1, Math.min(1, f32[i]));
        heap16[i] = (v * 32767) | 0;
      }
      mod.writeAudioInput(micScratchPtr, f32.length);
    };
    micSource.connect(micNode);
    micCapturing = true;
  }

  // Tear the live mic capture down and stop the underlying tracks so the
  // browser releases the device (and drops the "mic in use" indicator).
  // Leaves `micOn` alone so a later resume knows whether to reattach.
  function stopMicCapture(): void {
    if (micNode) {
      micNode.port.close();
      micNode.disconnect();
      micNode = null;
    }
    if (micSource) {
      micSource.disconnect();
      micSource = null;
    }
    if (micStream) {
      micStream.getTracks().forEach(t => t.stop());
      micStream = null;
    }
    micCapturing = false;
  }

  async function setMic(enabled: boolean): Promise<void> {
    if (enabled === micOn) return;
    // Record the desired state up front so startMicCapture's post-await
    // guard sees the latest intent.
    micOn = enabled;
    if (enabled) {
      const c = await ensureContext();
      try {
        await startMicCapture(c);
      } catch (err) {
        // Permission denied / no device: roll the desired state back so
        // we don't keep believing the mic is on (and don't try to resume
        // it on the next visibility change).
        micOn = false;
        stopMicCapture();
        throw err;
      }
    } else {
      stopMicCapture();
    }
    // Compensate for the comms-mode loudness boost while the mic stream
    // is live. setTargetAtTime gives a brief 50 ms taper so the change
    // doesn't pop. Only applies when speakerGain is live (speaker on);
    // when the speaker is off the graph is torn down and the
    // micOn-state will be picked up the next time it's brought back up
    // via ensureSpeakerGraph's gain seed.
    if (speakerGain && ctx) {
      const t = ctx.currentTime;
      const target = micOn ? kSpeakerGainMicOn : kSpeakerGainMicOff;
      speakerGain.gain.cancelScheduledValues(t);
      speakerGain.gain.setTargetAtTime(target, t, 0.05);
    }
    // If mic just turned off and speaker is also off, suspend the
    // context to release the tab's audio indicator.
    maybeSuspendContext();
    publishHostState();
  }

  // Called per RAF tick to drain the DAC queue. We always drain (even when
  // the speaker is off) so the guest's FIFO doesn't back up — when off, the
  // core substitutes silence.
  function pump(): void {
    // Browsers can suspend an AudioContext when the page loses focus, when
    // the autoplay policy decides too much time has passed without a
    // gesture, or — frustratingly — when nothing live is feeding the
    // destination. Once suspended, the speaker worklet stops being
    // pulled and the user reports "no sound until I toggle mic on". A
    // resume() per RAF tick is cheap (no-op when already running) and
    // reliably keeps the destination active without requiring the
    // MediaStream side effect from a getUserMedia call. We only do this
    // when the user has actually opted into audio (speaker or mic on);
    // if both are off we leave the context suspended so the tab's
    // audio-playing indicator drops.
    if ((speakerOn || micOn) && ctx && ctx.state !== 'running') {
      void ctx.resume().catch(() => { /* will retry next tick */ });
    }
    // Burst sized for ~1.5 RAF frames @ emuSampleRate — roughly 200 samples
    // at 8 kHz 60 fps. Keeps the worklet queue topped up without flooding.
    const MAX = Math.max(64, Math.ceil(emuSampleRate / 30));
    const bytes = MAX * 2;
    if (outScratchCap < bytes) {
      if (outScratchPtr) mod._free(outScratchPtr);
      outScratchPtr = mod._malloc(bytes);
      outScratchCap = bytes;
    }
    const got = mod.readAudioOutput(outScratchPtr, MAX);
    if (got === 0) return;
    const heap16 = new Int16Array(mod.HEAPU8.buffer, outScratchPtr, got);
    // Only forward to the worklet when the speaker is actually on — saves
    // a postMessage per frame while muted.
    if (speakerOn && speakerNode) {
      const f32 = new Float32Array(got);
      for (let i = 0; i < got; i++) f32[i] = heap16[i] / 32768;
      speakerNode.port.postMessage({ type: 'samples', samples: f32 }, [f32.buffer]);
    }
  }

  function destroy(): void {
    void setMic(false);
    void setSpeaker(false);
    // setSpeaker(false) calls teardownSpeakerGraph() which already
    // cleared silentDriver / silentDriverGain / speakerGain. Belt and
    // braces in case setSpeaker(false) early-returned.
    teardownSpeakerGraph();
    if (ctx) {
      void ctx.close();
      ctx = null;
    }
    if (speakerUrl) { URL.revokeObjectURL(speakerUrl); speakerUrl = null; }
    if (micUrl)     { URL.revokeObjectURL(micUrl);     micUrl = null; }
    // mobileUnlockEl is module-scoped and survives engine destroy /
    // device switches — once iOS has been told this page does media
    // playback, we want to keep that grant for the next engine.
    pauseMobileAudioSession();
    if (micScratchPtr) { mod._free(micScratchPtr); micScratchPtr = 0; micScratchCap = 0; }
    if (outScratchPtr) { mod._free(outScratchPtr); outScratchPtr = 0; outScratchCap = 0; }
  }

  async function getSpeakerStats() {
    if (!speakerNode || !ctx) return null;
    return new Promise<{
      received: number; emitted: number; peakAbs: number; qFill: number;
      contextState: string; sampleRate: number;
    } | null>(resolve => {
      const timeout = setTimeout(() => resolve(null), 500);
      const handler = (e: MessageEvent) => {
        if ((e.data as { type?: string })?.type !== 'stats') return;
        clearTimeout(timeout);
        speakerNode!.port.removeEventListener('message', handler);
        const d = e.data as { received: number; emitted: number; peakAbs: number; qFill: number };
        resolve({
          received: d.received, emitted: d.emitted, peakAbs: d.peakAbs, qFill: d.qFill,
          contextState: ctx!.state,
          sampleRate: ctx!.sampleRate,
        });
      };
      speakerNode!.port.addEventListener('message', handler);
      speakerNode!.port.start();
      speakerNode!.port.postMessage({ type: 'query' });
    });
  }

  // Public alias for the internal publishHostState — the engine
  // survives across device switches (the WASM `mod` is reused) and
  // the hook calls this after each device load so the new emulator
  // instance inherits the engine's persisted speaker/mic state.
  function republishHostState() { publishHostState(); }

  publishHostState();
  return { setSpeaker, setMic, pump, destroy, republishHostState, getSpeakerStats };
}
