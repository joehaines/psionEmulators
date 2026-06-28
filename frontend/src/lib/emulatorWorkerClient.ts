// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Typed main-thread client for emulator-worker.js (Phase 1 of the web-worker
// migration — see docs/web-worker-emulation-scope.md). Wraps the postMessage
// protocol: request/reply RPC for cold calls, fire-and-forget input, and a
// status push the UI polls without a round-trip.
import type { DeviceInfo } from '../types/emulator';

export interface WorkerStatus {
  paused: boolean;
  simCycles: number;
  backlight: boolean;
  cfGap: boolean;
  cardAttached: boolean;
}

type RpcResolver = { resolve: (v: unknown) => void; reject: (e: Error) => void };

export class EmulatorWorkerClient {
  private worker: Worker;
  private nextId = 1;
  private pending = new Map<number, RpcResolver>();
  private readyPromise: Promise<void>;
  private resolveReady!: () => void;

  status: WorkerStatus = { paused: false, simCycles: 0, backlight: false, cfGap: false, cardAttached: false };
  onStatus: ((s: WorkerStatus) => void) | null = null;
  onError: ((message: string) => void) | null = null;
  /** Device-load progress pushes (value 0..1, or null = indeterminate). */
  onLoadProgress: ((value: number | null, status: string | null) => void) | null = null;
  onAudio: ((samples: Int16Array) => void) | null = null;
  onLog: ((text: string, err: boolean) => void) | null = null;
  onSerialRx: ((uart: number, bytes: Uint8Array) => void) | null = null;

  constructor(workerUrl: string) {
    this.worker = new Worker(workerUrl);
    this.readyPromise = new Promise<void>(r => { this.resolveReady = r; });
    this.worker.onmessage = (e: MessageEvent) => this.handle(e.data);
    this.worker.onerror = (e) => this.onError?.(`worker error: ${e.message}`);
  }

  /** Loads the WASM module in the worker and resolves when it's ready. */
  async init(baseUrl: string): Promise<void> {
    this.worker.postMessage({ type: 'init', baseUrl });
    return this.readyPromise;
  }

  /** Transfers the OffscreenCanvas to the worker (it then paints the LCD). */
  setCanvas(canvas: OffscreenCanvas): void {
    this.worker.postMessage({ type: 'setCanvas', canvas }, [canvas]);
  }

  private handle(m: { type: string; [k: string]: unknown }): void {
    switch (m.type) {
      case 'ready':
        this.resolveReady();
        break;
      case 'status':
        this.status = m as unknown as WorkerStatus;
        this.onStatus?.(this.status);
        break;
      case 'error':
        this.onError?.(String(m.message));
        break;
      case 'loadProgress':
        this.onLoadProgress?.(
          typeof m.value === 'number' ? (m.value as number) : null,
          m.status != null ? String(m.status) : null,
        );
        break;
      case 'audio':
        this.onAudio?.(m.samples as Int16Array);
        break;
      case 'serialRx':
        this.onSerialRx?.(m.uart as number, new Uint8Array(m.bytes as ArrayBuffer));
        break;
      case 'log':
        if (m.err) console.error('[emu]', m.text); else console.log('[emu]', m.text);
        this.onLog?.(String(m.text), !!m.err);
        break;
      case 'rpcResult': {
        const p = this.pending.get(m.id as number);
        if (!p) break;
        this.pending.delete(m.id as number);
        if (m.error) p.reject(new Error(String(m.error)));
        else p.resolve(m.result);
        break;
      }
    }
  }

  private call<T>(rpc: string, args?: Record<string, unknown>, transfer?: Transferable[]): Promise<T> {
    const id = this.nextId++;
    return new Promise<T>((resolve, reject) => {
      this.pending.set(id, { resolve: resolve as (v: unknown) => void, reject });
      this.worker.postMessage({ type: 'rpc', id, rpc, args }, transfer ?? []);
    });
  }

  // ── RPC (cold/rare) ──
  getProfiles(): Promise<unknown[]> { return this.call('getProfiles'); }
  loadDevice(deviceId: string, romUrl: string, preroll: number, restore = true):
      Promise<{ deviceName: string; info: DeviceInfo;
                ssdAttached?: boolean[]; datapakAttached?: boolean[];
                serialAttached?: Record<number, boolean> }> {
    return this.call('loadDevice', { deviceId, romUrl, preroll, restore });
  }
  pause(): Promise<boolean> { return this.call('pause'); }
  resume(): Promise<boolean> { return this.call('resume'); }
  reset(): Promise<boolean> { return this.call('reset'); }
  setDeviceMode(on: boolean): Promise<boolean> { return this.call('setDeviceMode', { on }); }
  setLoggingEnabled(on: boolean): void { this.worker.postMessage({ type: 'setLoggingEnabled', on }); }
  /** Enable/disable host audio in the worker (fire-and-forget). */
  setHostAudio(speaker: boolean, mic: boolean): void {
    this.worker.postMessage({ type: 'setHostAudio', spk: speaker, mic });
  }
  /** Push captured mic samples to the worker (→ emulator writeAudioInput). */
  sendMic(samples: Int16Array): void {
    this.worker.postMessage({ type: 'mic', samples }, [samples.buffer]);
  }
  saveState(): Promise<boolean> { return this.call('saveState'); }
  revertToSaved(): Promise<boolean> { return this.call('revertToSaved'); }
  async attachCard(bytes: Uint8Array): Promise<boolean> {
    const copy = bytes.slice();                              // transfer a copy
    return this.call('attachCard', { bytes: copy.buffer }, [copy.buffer]);
  }
  async updateCard(bytes: Uint8Array): Promise<boolean> {
    const copy = bytes.slice();                              // in-place "Update files"
    return this.call('updateCard', { bytes: copy.buffer }, [copy.buffer]);
  }
  detachCard(): Promise<boolean> { return this.call('detachCard'); }
  async getRamSnapshot(): Promise<Uint8Array | null> {
    const r = await this.call<{ bytes: ArrayBuffer; byteLength: number } | null>('getRamSnapshot');
    return r ? new Uint8Array(r.bytes) : null;
  }
  async getCardBytes(): Promise<Uint8Array | null> {
    const r = await this.call<{ bytes: ArrayBuffer; byteLength: number } | null>('getCardBytes');
    return r ? new Uint8Array(r.bytes) : null;
  }
  async attachSSD(slot: number, bytes: Uint8Array, ssdType?: number): Promise<boolean> {
    const c = bytes.slice();
    return this.call('attachSSD', { slot, bytes: c.buffer, ssdType }, [c.buffer]);
  }
  async getSSDBytes(slot: number): Promise<Uint8Array | null> {
    const r = await this.call<{ bytes: ArrayBuffer; byteLength: number } | null>('getSSDBytes', { slot });
    return r ? new Uint8Array(r.bytes) : null;
  }
  detachSSD(slot: number): Promise<boolean> { return this.call('detachSSD', { slot }); }
  async attachDatapak(slot: number, bytes: Uint8Array): Promise<{ ok: boolean; kind: number }> {
    const c = bytes.slice();
    return this.call('attachDatapak', { slot, bytes: c.buffer }, [c.buffer]);
  }
  detachDatapak(slot: number): Promise<boolean> { return this.call('detachDatapak', { slot }); }
  serialAttach(uart: number): Promise<boolean> { return this.call('serialAttach', { uart }); }
  serialDetach(uart: number): Promise<boolean> { return this.call('serialDetach', { uart }); }
  serialWrite(uart: number, bytes: Uint8Array): void {
    const c = bytes.slice();
    this.worker.postMessage({ type: 'serialWrite', uart, bytes: c.buffer }, [c.buffer]);
  }
  /** Transfer-mode pump flag (worker-side mirror of setSerialPumpEnabled). */
  setSerialPump(on: boolean): void {
    this.worker.postMessage({ type: 'setSerialPump', on });
  }
  /** Latest link keepalive frame for the worker to replay in sim-time (or null
   *  to stop). See wasmBridge.setSimKeepAliveFrame / emulator-worker.js. */
  setSimKeepAlive(bytes: Uint8Array | null): void {
    if (bytes && bytes.length) {
      const c = bytes.slice();
      this.worker.postMessage({ type: 'simKeepAlive', bytes: c.buffer }, [c.buffer]);
    } else {
      this.worker.postMessage({ type: 'simKeepAlive', bytes: null });
    }
  }

  // ── Input (fire-and-forget) ──
  sendKey(key: number, down: boolean): void { this.worker.postMessage({ type: 'key', key, down }); }
  enqueue(events: { key: number; down: boolean }[]): void { this.worker.postMessage({ type: 'enqueue', events }); }
  sendTouch(x: number, y: number, down: boolean): void { this.worker.postMessage({ type: 'touch', x, y, down }); }

  destroy(): void { this.worker.terminate(); this.pending.clear(); }
}
