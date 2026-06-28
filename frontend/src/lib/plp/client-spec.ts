// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PlpClient — top-level wire-correct PLP / PsiWin client.
//
// Wires LinkLayer (data link) → Ncp (session) → RfsvClient
// (presentation) into a single object the Remote Link dialog drives.
//
// Connection lifecycle:
//   1. start() — begin polling the underlying byte bridge.
//   2. connect() — wait for the link handshake to complete, send our
//      NCP Information frame, then open the SYS$RFSV.* channel. The
//      LINK.* Connect that the device sends on its own initiative is
//      auto-acked inside Ncp without further work here.
//   3. listDrives() / listDirectory() / downloadFile() / etc. —
//      ordinary RFSV-32 operations.
//   4. stop() — tear down polling and the bridge.
//
// Verified end-to-end against the real Revo via harness/run.cpp's
// --serial-auto-rule surface (a transcript of an OPEN_DIR + READ_DIR
// → "System" + "Documents" with short name "DOCUMENT" is included in
// the rfsv32-spec test suite as a regression vector).

import { FrameDecoder } from './framing.ts';
import { LinkLayer, type LinkState, type Pdu } from './link.ts';
import { Ncp } from './ncp-spec.ts';
import { RfsvClient, LinkResetError, type DirEntry } from './rfsv32-spec.ts';
import { Rfsv16Client } from './rfsv16-spec.ts';
import { WprtClient, WprtJobAssembler, type WprtJob } from './wprt-spec.ts';
import {
  NCP_SERVICE_RFSV, NCP_SERVICE_WPRT, NCP_REGISTER_WPRT,
  NCP_VERSION, FILE_MODE, ATTR, EpocErr,
} from './types.ts';
import { setSerialPumpEnabled, setPumpKeepAlive, setSimKeepAliveFrame } from '../wasmBridge.ts';
import {
  buildOpenFile, buildReplaceFile, buildReadFile, buildWriteFile, buildCloseHandle,
  buildDelete,
  decodeReply,
} from './rfsv32-spec.ts';
export type { DirEntry } from './rfsv32-spec.ts';
export type { WprtJob } from './wprt-spec.ts';

const u32leFromBytes = (b: Uint8Array, off: number) =>
  (((b[off] | (b[off+1] << 8) | (b[off+2] << 16) | (b[off+3] << 24)) >>> 0));

// How often (wall-clock ms) to send a link keepalive while connected. Each
// keepalive Ack lands as a serial-RX IRQ on the device, which reschedules its
// RemoteLinkServer thread — that reschedule pressure is what keeps a transfer
// flowing between our request frames.
//
// In WORKER mode (the default in the browser) this is the ONLY source of that
// pressure: the main-thread pump's in-pump keepalive (wasmBridge.serialReadBytes
// → pumpKeepAlive, fired on every pump step) does not run when the emulator
// lives in the worker. At 500 ms the gaps between Acks were wide enough that the
// device's RemoteLinkServer fell behind and wedged partway through a large
// upload — the "trying an app fails around the 90 KB mark" report: verified in
// the browser against the 5mx ROM, a Jumpy!p5 (.SIS) upload stalled mid-stream
// at 500 ms and ran clean to completion at 150 ms. Keep it short so the device
// thread is serviced often enough to drain a sustained transfer; it's a cheap
// duplicate Ack on the wire, harmless on every device.
const KEEPALIVE_INTERVAL_MS = 150;

export type ClientState =
  | 'idle'           // start() not yet called
  | 'attaching'      // poll loop running, waiting for first frame
  | 'link-up'        // LinkLayer in 'connected' state
  | 'rfsv-opening'   // sent Connect for SYS$RFSV.*
  | 'connected'      // RFSV channel open, ready for file ops
  | 'failed';        // gave up; see lastError

export interface ClientConfig {
  // Bridge poll rate. Each RFSV round-trip costs at least one tick to
  // pick the reply up, so this bounds transfer throughput; 200 Hz
  // (browsers clamp setInterval around 4 ms anyway) keeps the dead
  // time per exchange to a few ms.
  pollHz?: number;             // default 200
  // Protocol family the device speaks: 'rfsv32' (EPOC32, default) or
  // 'rfsv16' (EPOC16 SIBO — Series 3c / 3mx / Siena / Workabout MX).
  // Selects the link handshake variant, the advertised NCP Info
  // version, and the file services dialect; the dialog-facing API is
  // identical either way.
  protocol?: 'rfsv32' | 'rfsv16';
  // Req_Con_Pdu seq flavour, forwarded to LinkLayer (see LinkConfig.conSeq):
  // default 4 (R5 Windermere/SA-1100); 2 for the ER3/ER4 CL-PS711x devices
  // (Series 5, Osaris) which silently drop the 0x24 form.
  conSeq?: number;
  // EPOC32 file-op chunk size (bytes per FREAD/FWRITE round-trip).
  // Defaults to 1024 — the largest a chunk's NCP fragments can be
  // without overflowing the Windermere host RX FIFO (hardware.h
  // kRxFifoCap = 4 KB; worst-case DLE stuffing of 1 KB is ~2.2 KB). The
  // SA-1100 (Series 7 / netBook) UART has no such FIFO cap — its rx
  // queue is unbounded — so those flows pass 2048 to halve the
  // round-trip count, which dominates SA-1100 transfer time (each
  // round-trip pays a large fixed EKA1 F32 wake; see
  // docs/series7-remote-link-speed-2026-06-13.md). 2048 is the safe
  // ceiling: the device fills reads up to ~2 KB per reply, so a larger
  // chunk short-reads and the download loop mistakes it for EOF.
  chunkSize?: number;          // default 1024
  // Per-request RFSV inactivity timeout (ms). A request fails only after
  // this long with NO inbound bytes from the device — see
  // RfsvClient.setRequestTimeout. Default 30 s, generous enough that a
  // device kept busy by user interaction (tapping icons mid-transfer)
  // doesn't time a healthy round-trip out.
  requestTimeoutMs?: number;   // default 30000
  onState?(state: ClientState): void;
  onPduIn?(pdu: Pdu): void;
  onPduOut?(pdu: Pdu): void;
  onRawRx?(chunk: Uint8Array): void;
  onRawTx?(chunk: Uint8Array): void;
}

// Every polling PlpClient registers itself here (start → add, stop →
// remove) so the device-switch / save paths can flush them before the
// emulator state is snapshotted. A device-switch autosave that captures
// the link mid-frame (a device reply the host hasn't acked yet) bakes a
// session whose retransmit budget then expires shortly after restore —
// the device's link server goes dormant and the next connect can't
// adopt it. Quiescing first leaves a cleanly-acked, idle session in the
// snapshot that survives restore (and export/import) and re-adopts.
const activeSessions = new Set<PlpClient>();

// Flush every live link session so the next state snapshot captures a
// clean, fully-acked session. With `release` the host side is also
// stopped (device switch — the session parks for the next client to
// adopt); without it the clients keep running (an explicit Save / Export
// while the user is mid-session). No-op when nothing is connected.
export async function quiesceActiveSessions(
  opts: { release?: boolean } = {},
): Promise<void> {
  const clients = [...activeSessions];
  await Promise.all(clients.map(c =>
    opts.release ? c.releaseQuiesced() : c.quiesce()));
}

export class PlpClient {
  private readBytes: () => Uint8Array;
  private writeBytes: (data: Uint8Array) => number;
  private cfg: ClientConfig;

  private decoder = new FrameDecoder();
  private link: LinkLayer;
  private ncp: Ncp;
  private rfsv: RfsvClient | null = null;
  private rfsv16: Rfsv16Client | null = null;
  private get sibo(): boolean { return this.cfg.protocol === 'rfsv16'; }
  // Most recent link-state observed via the LinkLayer callback. Used
  // to gate transitions in connect().
  private linkState_: LinkState = 'idle';

  private state_: ClientState = 'idle';
  private intervalId: ReturnType<typeof setInterval> | null = null;
  // Wall-clock of the last link keepalive (see tick()).
  private lastKeepAliveAt = 0;
  // Wall-clock of the last inbound bytes — releaseQuiesced waits for
  // this to go quiet so the device's final reply gets acked.
  private lastRxAt = 0;
  // Resolves when the link comes up (LinkLayer reaches 'connected').
  private linkReadyResolvers: Array<() => void> = [];
  // Bumped every time the data-link resets under us (peer re-handshake).
  // withRfsv() snapshots it around each op so it can tell a link bounce
  // apart from an ordinary failure and re-open the channel.
  private linkGen = 0;
  lastError: string = '';

  constructor(
    readBytes: () => Uint8Array,
    writeBytes: (data: Uint8Array) => number,
    cfg: ClientConfig = {},
  ) {
    this.readBytes = readBytes;
    this.writeBytes = writeBytes;
    this.cfg = cfg;
    if (cfg.chunkSize) this.setChunkSize(cfg.chunkSize);

    // Splice the LinkLayer + Ncp together. Ncp's constructor
    // intentionally replaces the Link's onData hook so packets flow
    // through the right dispatcher.
    this.link = new LinkLayer({
      variant: cfg.protocol === 'rfsv16' ? 'sibo' : 'epoc',
      conSeq: cfg.conSeq,
      sendBytes: bytes => {
        this.writeBytes(bytes);
        this.cfg.onRawTx?.(bytes);
      },
      onData: () => { /* replaced by Ncp */ },
      onStateChange: s => {
        this.linkState_ = s;
        if (s === 'connected') {
          for (const r of this.linkReadyResolvers.splice(0)) r();
          this.transition('link-up');
        } else if (s === 'failed') {
          this.transition('failed');
          this.lastError = 'link handshake failed';
        }
      },
      onAdopt: () => {
        // Resumed a dead host's session — keep our channel ids clear of
        // the ones that session still holds on the device.
        this.ncp.randomizeClientChanBase();
      },
      onReset: () => {
        // The device tore the link down and re-handshaked mid-session,
        // so every NCP channel it had is gone. Drop our matching state,
        // abort any in-flight RFSV request immediately, and fall back to
        // 'link-up'; the next file op (or withRfsv's retry) re-opens
        // SYS$RFSV.* on the fresh link.
        this.linkGen++;
        this.ncp.resetChannels();
        this.rfsv?.reset();
        this.rfsv = null;
        this.rfsv16?.reset();
        this.rfsv16 = null;
        this.wprt?.reset();
        this.wprt = null;
        if (this.state_ === 'connected' || this.state_ === 'rfsv-opening') {
          this.transition('link-up');
        }
      },
      onPduIn: p => this.cfg.onPduIn?.(p),
      onPduOut: p => this.cfg.onPduOut?.(p),
    });
    this.ncp = new Ncp(
      this.link,
      cfg.protocol === 'rfsv16' ? NCP_VERSION.SIBO_NEW : NCP_VERSION.EPOC_ER3,
    );
  }

  get state(): ClientState { return this.state_; }
  get linkState(): LinkState { return this.linkState_; }

  // Begin the polling pump. Reads bytes from the bridge each tick,
  // feeds them into the decoder, and forwards every complete frame to
  // the LinkLayer state machine.
  start(): void {
    if (this.intervalId !== null) return;
    this.transition('attaching');
    activeSessions.add(this);
    const period = Math.max(1, Math.round(1000 / (this.cfg.pollHz ?? 200)));
    this.intervalId = setInterval(() => this.tick(), period);
    // Service the device's link-inactivity timer from inside the WASM pump
    // loop (which blocks the event loop and thus the wall-clock keepalive
    // below). See setPumpKeepAlive in wasmBridge.ts — without this the device
    // bounces mid drive-list wait and reaps the RFSV channel.
    setPumpKeepAlive(() => { if (this.linkState_ === 'connected') this.link.keepAlive(); });
  }

  stop(): void {
    activeSessions.delete(this);
    if (this.intervalId !== null) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
    setPumpKeepAlive(null);
  }

  // Stop driving the link WITHOUT tearing it down: no Disc_Pdu, and the
  // caller leaves the cable attached. The EPOC R5 link server keeps its
  // session alive indefinitely (it survives host silence and ignores
  // Req_Req while it exists), and once it HAS torn a session down — on
  // a Disc or a cable unplug — it never listens again until Remote Link
  // is toggled off/on on the device. So the recoverable end state is
  // "session parked": the next PlpClient resumes it via the adoption
  // probe (LinkLayer.probeAdopt). Used by the app-delivery flow and the
  // EPOC32 Remote Link dialog.
  release(): void {
    this.printWaitAborted = true;
    this.wprt?.reset();
    this.wprt = null;
    this.stop();
    setSerialPumpEnabled(false);
    this.rfsv = null;
    this.rfsv16 = null;
    this.transition('idle');
  }

  // Block until the link has gone quiet (no inbound bytes for ~400 ms,
  // bounded by maxWaitMs) while the poll loop keeps acking. Critical
  // before a state snapshot or a park: the device's last reply may still
  // be in the air, and a session captured (or parked) with an un-acked
  // frame burns its retransmit budget after restore and dies (terminally
  // — see release). Leaves the client RUNNING; callers that also want to
  // stop use releaseQuiesced.
  async quiesce(maxWaitMs = 2_000): Promise<void> {
    if (this.intervalId === null) return;   // not polling — nothing to flush
    const start = Date.now();
    while (Date.now() - start < maxWaitMs &&
           Date.now() - this.lastRxAt < 400) {
      await new Promise<void>(r => setTimeout(r, 100));
    }
  }

  // quiesce(), then release() — flush the device's last reply, then park
  // the session for the next client to adopt.
  async releaseQuiesced(maxWaitMs = 2_000): Promise<void> {
    await this.quiesce(maxWaitMs);
    this.release();
  }

  // Cleanly tear the link down: send Disc_Pdu so the device drops
  // its connection state, then stop the polling pump. The bridge
  // itself is detached by the dialog (after this call) so the
  // device sees DSR drop only after we've sent the Disc_Pdu.
  disconnect(): void {
    this.printWaitAborted = true;
    this.wprt?.reset();
    this.wprt = null;
    try { this.link.disconnect(); } catch { /* link may already be torn down */ }
    this.stop();
    setSerialPumpEnabled(false);
    setSimKeepAliveFrame(null);   // stop the worker's sim-time keepalive replay
    this.rfsv = null;
    this.rfsv16 = null;
    this.transition('idle');
  }

  // Wait until LinkLayer reaches 'connected', send our NCP Info, open
  // SYS$RFSV.*. Resolves once we have a working RfsvClient.
  //
  // The SA-1100 device re-handshakes its data-link aggressively while it's
  // busy (see link.ts) — a bounce mid-connect reaps the pending SYS$RFSV
  // channel and the raw connectServer() would reject ("NCP Connect …
  // rejected"). So retry the RFSV open in each fresh link window until the
  // overall deadline: a transient bounce then costs one short attempt
  // instead of failing the whole connection. On a quiet link (no bounce)
  // the first attempt succeeds and we return immediately.
  async connect(timeoutMs = 15_000): Promise<void> {
    const deadline = Date.now() + timeoutMs;
    await this.ensureLinkUp(timeoutMs);
    // Enable the serial pump NOW — before the NCP SYS$RFSV.* handshake, not
    // after it. The Connect round-trip needs the device's RemoteLinkServer
    // serial thread rescheduled promptly (the EKA1 Series 7 scheduler is slow
    // to wake it); the pump's serialPumpCycles + in-pump keepalive supply that
    // reschedule pressure in SIM-time, independent of the browser's slow,
    // irregular RAF. Without this the connect itself drags and the device
    // bounces before sending CONNECT_RESPONSE → "NCP Connect … rejected" (the
    // observed Series 7 browser failure; the netBook's faster server doesn't
    // need it). The link Req/Con handshake above runs pump-free as before —
    // the in-pump keepalive is gated to linkState 'connected' so it never
    // perturbs that. Measured ~10× faster drive-list locally.
    setSerialPumpEnabled(true);
    let lastErr: unknown;
    for (let attempt = 0; Date.now() < deadline; attempt++) {
      const remaining = deadline - Date.now();
      try {
        await this.ensureLinkUp(remaining);
        // Short per-attempt budget so a bounce fails fast and we retry in
        // the next clean window rather than burning the whole timeout.
        await this.openRfsv(Math.min(5_000, Math.max(1_000, remaining)));
        return;
      } catch (e) {
        lastErr = e;
        this.rfsv = null;
        // Drop any half-open channel state so the next attempt re-announces
        // cleanly on the (possibly freshly re-handshaked) link.
        try { this.ncp.resetChannels(); } catch { /* ignore */ }
        await new Promise<void>(r => setTimeout(r, 120));
      }
    }
    this.transition('failed');
    throw (lastErr instanceof Error ? lastErr
      : new Error('SYS$RFSV.* connect timed out'));
  }

  // Bring the data-link up if it isn't already. We proactively send our
  // own Req_Req_Pdu — on a fresh cable-plug the device sends one
  // spontaneously and the handshake races with ours (peer mode), but on
  // a subsequent reattach the device may not re-initiate, so without our
  // Req_Req_Pdu the await would hang until the timeout fires.
  private async ensureLinkUp(timeoutMs: number): Promise<void> {
    if (this.linkState_ === 'connected') return;
    // ER3/ER4 CL-PS711x (conSeq 2 — Series 5, Osaris): NEVER initiate. A
    // host-sent Req_Req_Pdu doesn't just go unanswered on these ROMs, it
    // actively wedges the device's link server — its own Req_Req retry burst
    // stops dead and nothing revives it short of a cable re-plug
    // (harness-verified: a lone host Req_Req at t=8.3 kills the 9.2s/10.4s
    // retries). The dialog's per-Connect re-plug guarantees a fresh
    // device-side burst, which handleFrame answers with the 0x22 Req_Con →
    // device Acks → connected. So in passive mode we just wait.
    const passive = this.cfg.conSeq === 2;
    if (passive) {
      await this.raceTimeout(
        new Promise<void>(resolve => this.linkReadyResolvers.push(resolve)),
        timeoutMs,
        'link handshake timed out',
      );
      return;
    }
    // Adoption first: if a parked session exists (a previous host
    // released or died with the link up), a Req_Req_Pdu would DESTROY
    // it — the R5 ROM only answers Req_Req when it has no session;
    // mid-session it tears the session down and its link server goes
    // terminally dormant (harness-verified against the live 5mx ROM).
    // So probe for the session and give the duplicate-ack a moment to
    // adopt it before risking a fresh handshake. Costs ~1 s on the
    // rare host-initiated path; the common fresh-connect path completes
    // via the device's own Req_Req burst, which handleFrame answers
    // regardless of this wait.
    if (!this.sibo) {
      const linkUp = () => this.linkState_ === 'connected';
      const probeDeadline = Math.min(Date.now() + 1_100, Date.now() + timeoutMs);
      this.link.probeAdopt();
      let reprobed = false;
      while (Date.now() < probeDeadline) {
        if (linkUp()) return;
        if (!reprobed && Date.now() > probeDeadline - 600) {
          reprobed = true;
          this.link.probeAdopt();
        }
        await new Promise<void>(r => setTimeout(r, 50));
      }
      if (linkUp()) return;
    }
    this.link.initiate();
    // Re-prompt periodically while we wait. The first Req_Req_Pdu can be lost:
    // on the web-worker path the host serial bridge attaches ASYNCHRONOUSLY
    // (serialAttachHost returns optimistically while the worker wires the UART
    // a round-trip later), so a Req_Req sent immediately may go out before the
    // device is reachable; it can also arrive before the device's serial driver
    // has serviced the cable-plug (modem-status) IRQ. A single attempt then
    // hangs until the timeout. The device answers EVERY Req_Req_Pdu with a
    // Req_Con_Pdu (an idempotent re-confirm — see handleFrame), so re-sending is
    // benign on every device and recovers a dropped first frame. Stops the
    // instant the link reaches 'connected'.
    const retry = setInterval(() => {
      if (this.linkState_ === 'connected') { clearInterval(retry); return; }
      this.link.initiate();
    }, 1000);
    try {
      await this.raceTimeout(
        new Promise<void>(resolve => this.linkReadyResolvers.push(resolve)),
        timeoutMs,
        'link handshake timed out',
      );
    } finally {
      clearInterval(retry);
    }
  }

  // Exchange NCP Info and open the SYS$RFSV.* channel, leaving us with a
  // working RfsvClient. Assumes the link is already up. Called by
  // connect() and re-called by ensureRfsv() after a mid-session reset
  // reaped the previous channel.
  private async openRfsv(timeoutMs: number): Promise<void> {
    // Send NCP Information frame. (The device sends its own unprompted;
    // sending ours stops the device from refusing subsequent Connect
    // requests with an immediate Disconnect, a quirk the harness
    // experiments revealed.)
    this.ncp.sendNcpInfo();

    // Give the device time to send its NCP Info.  On Windermere it
    // arrives spontaneously (peerInfoReceived already true by now). On
    // the SA-1100 netBook there's a round-trip delay — the device only
    // sends NCP Info AFTER receiving our Ack + NCP Info.  A short yield
    // lets the poll loop deliver any pending device frames before we
    // fire the RFSV Connect.  Non-blocking: we don't fail if the device
    // is slow; RFSV Connect has its own timeout.
    if (!this.ncp.peerInfoReceived) {
      for (let i = 0; i < 30 && !this.ncp.peerInfoReceived; i++) {
        await new Promise<void>(r => setTimeout(r, 100));
      }
    }

    // Open SYS$RFSV.*. Ncp.connectServer resolves with the assigned
    // server channel on success.
    this.transition('rfsv-opening');
    const serverChan = await this.raceTimeout(
      this.ncp.connectServer(NCP_SERVICE_RFSV),
      timeoutMs,
      'SYS$RFSV.* connect timed out',
    );
    void serverChan;

    // The clientChan Ncp allocated starts at 2 (first available after
    // 0=control and 1=LINK). Look it up from Ncp's bookkeeping —
    // safer than tracking a parallel counter here.
    const openChan = this.firstOpenClientChan();
    if (openChan === null) {
      this.transition('failed');
      this.lastError = 'no open client channel after connect';
      throw new Error(this.lastError);
    }
    if (this.sibo) {
      this.rfsv16 = new Rfsv16Client(this.ncp, openChan);
      if (this.cfg.requestTimeoutMs) this.rfsv16.setRequestTimeout(this.cfg.requestTimeoutMs);
    } else {
      this.rfsv = new RfsvClient(this.ncp, openChan);
      this.rfsv.setRequestTimeout(this.cfg.requestTimeoutMs ?? 30_000);
    }
    setSerialPumpEnabled(true);
    this.transition('connected');
  }

  // Guarantee a live RFSV channel before a file op, re-establishing it if
  // a mid-session link reset tore it down.
  private async ensureRfsv(timeoutMs: number): Promise<void> {
    await this.ensureLinkUp(timeoutMs);
    if (this.sibo ? !this.rfsv16 : !this.rfsv) await this.openRfsv(timeoutMs);
  }

  // Run an RFSV operation, transparently recovering from a data-link
  // reset that lands before or during it. The EPOC R5 netBook / Series 7
  // RemoteLinkServer bounces its link the moment the kernel is slow to
  // reschedule its serial thread; that reaps the RFSV channel, so the
  // raw request would otherwise time out (the "DRIVE_LIST failed: …
  // timed out after 10s" we see in the field). On a bounce we re-open
  // SYS$RFSV.* on the fresh link and retry the whole op. reset() makes
  // the in-flight request reject promptly with LinkResetError, so we
  // recover in tens of ms rather than waiting out the 10 s timeout.
  private async withRfsv<T>(fn: (c: RfsvClient) => Promise<T>, timeoutMs = 15_000): Promise<T> {
    for (let attempt = 0; ; attempt++) {
      await this.ensureRfsv(timeoutMs);
      const gen = this.linkGen;
      try {
        return await fn(this.rfsv!);
      } catch (e) {
        const bounced = e instanceof LinkResetError || this.linkGen !== gen;
        if (bounced && attempt < 4) {
          this.rfsv = null;                 // force re-open on the fresh link
          this.rfsv16 = null;
          await new Promise(r => setTimeout(r, 50));
          continue;
        }
        throw e;
      }
    }
  }

  async listDrives(): Promise<string[]> {
    if (this.sibo) return this.withRfsv16(c => c.getDriveList());
    return this.withRfsv(c => c.getDriveList());
  }

  async listDirectory(path: string): Promise<DirEntry[]> {
    if (this.sibo) return this.withRfsv16(c => c.listDirectory(path));
    return this.withRfsv(c => c.listDirectory(path));
  }

  // RFSV16 twin of withRfsv: same bounce-recovery semantics, EPOC16
  // client object.
  private async withRfsv16<T>(fn: (c: Rfsv16Client) => Promise<T>, timeoutMs = 15_000): Promise<T> {
    for (let attempt = 0; ; attempt++) {
      await this.ensureRfsv(timeoutMs);
      const gen = this.linkGen;
      try {
        return await fn(this.rfsv16!);
      } catch (e) {
        const bounced = e instanceof LinkResetError || this.linkGen !== gen;
        if (bounced && attempt < 4) {
          this.rfsv = null;
          this.rfsv16 = null;
          await new Promise(r => setTimeout(r, 50));
          continue;
        }
        throw e;
      }
    }
  }

  // Chunk size for the EPOC32 file read/write loops. Larger = fewer
  // request/reply round-trips, and each round-trip costs a poll tick
  // plus the device's serial servicing — the dominant transfer cost on
  // the emulated link. 1024 makes Ncp.sendOn split the request across
  // four ≤297-byte frames (the device's link layer reassembles them;
  // its replies fragment the same way and queuePartial reassembles
  // ours). The burst is bounded by the emulated UART's 4 KB host RX
  // FIFO (hardware.h kRxFifoCap): 1024 payload + frame overhead is
  // ~2.2 KB even at worst-case DLE stuffing, so nothing is dropped.
  // Tests with mocks that don't speak NCP fragmentation lower this
  // via setChunkSize().
  private chunkSize = 1024;
  setChunkSize(n: number) { this.chunkSize = Math.max(64, Math.min(n, 8192)); }

  // Download (FREAD) reply chunk cap — deliberately smaller than the upload
  // chunk. On a READ the DEVICE is the transmitter: it answers with up to
  // `chunkSize` bytes as one back-to-back link-frame burst. While it streams
  // that burst its serial thread is busy TX-ing and is slow to service the
  // inbound Acks we send for each frame, so its link layer keeps deciding the
  // window is unacknowledged and retransmits the WHOLE burst (go-back-N) on
  // every reply. At 1024 (four ~300-byte frames per reply) the retransmit
  // backlog outruns our Ack cadence under the web-worker serial bridge and the
  // device eventually exhausts its retransmit budget and goes dormant mid-file
  // (the "large download fails halfway" report). Halving the read burst to two
  // frames gives the device an RX-servicing gap between bursts, so our Acks
  // retire its window before it retransmits — validated end-to-end in
  // _plp_worker_repro (1024 wedges ~halfway; 512 completes a full round-trip).
  // Uploads are unaffected: there the host paces the burst and the device Acks
  // promptly, so they keep the larger chunk.
  private get readChunkSize(): number { return Math.min(this.chunkSize, 512); }

  // Download a file in chunks. Wrapped in withRfsv so a mid-transfer
  // link bounce re-opens the channel and restarts the download (reads
  // are idempotent).
  async downloadFile(path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array> {
    if (this.sibo) return this.withRfsv16(c => c.downloadFile(path, onProgress));
    return this.withRfsv(client => this.doDownload(client, path, onProgress));
  }

  // Bounded whole-transfer restarts for the chunked file loops. A READ /
  // WRITE that times out (a lost link frame) can't be safely resent on
  // the same handle — the device's file pointer may have already moved —
  // so instead we restart the whole transfer from a freshly opened handle
  // (offset 0 / truncated). Distinct from withRfsv's link-reset retry.
  private static readonly MAX_TRANSFER_RESTARTS = 3;

  private async doDownload(client: RfsvClient, path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array> {
    for (let restart = 0; ; restart++) {
      try {
        return await this.downloadOnce(client, path, onProgress);
      } catch (e) {
        // A link bounce is handled one level up (withRfsv re-opens the
        // channel on the fresh link). Any other failure here means a
        // non-idempotent READ timed out; restarting from a fresh OPEN
        // (offset 0) is the only way to guarantee we don't skip bytes.
        if (e instanceof LinkResetError) throw e;
        if (restart >= PlpClient.MAX_TRANSFER_RESTARTS) throw e;
      }
    }
  }

  private async downloadOnce(client: RfsvClient, path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array> {
    const mode = FILE_MODE.SHARE_READ;
    const reply = await client.send(buildOpenFile(client.nextOpId(), mode, path));
    if (reply.status !== 0) throw new Error(`OPEN_FILE(${path}) failed: ${reply.status}`);
    if (reply.data.length < 4) throw new Error('OPEN_FILE: no handle');
    const handle = u32leFromBytes(reply.data, 0);

    const chunks: Uint8Array[] = [];
    let total = 0;
    try {
      // Requests/replies larger than the 297-byte NCP-payload ceiling
      // fragment across multiple link frames (Ncp.sendOn outbound, the
      // device's link layer / our queuePartial inbound) — see the
      // Downloads use the smaller readChunkSize — see its comment for why
      // the device's reply burst must stay short.
      const CHUNK = this.readChunkSize;
      for (let i = 0; i < 64 * 1024; i++) {  // cap at 64 MiB
        // idempotent:false — a timed-out READ must NOT be blindly resent
        // on this handle (the device may have advanced its file pointer);
        // doDownload restarts the whole transfer instead.
        const r = await client.send(buildReadFile(client.nextOpId(), handle, CHUNK), false);
        // EpocErr.Eof (-25) is a legitimate end-of-file terminator, not a
        // failure; any other negative status is a real error.
        if (r.status !== 0 && r.status !== EpocErr.Eof) {
          throw new Error(`READ_FILE failed: ${r.status}`);
        }
        if (r.data.length > 0) {
          chunks.push(r.data);
          total += r.data.length;
          onProgress?.(total);
        }
        // EOF is signalled ONLY by a zero-length read (or an explicit Eof
        // status). A short read — fewer bytes than CHUNK with more to come
        // — is normal: a real EPOC32 RFSV server fills each FREAD only up
        // to its own buffer boundary, which can be well under CHUNK.
        // Treating a short read as EOF silently truncates the download
        // mid-file (the "large transfer fails halfway" field report).
        if (r.data.length === 0 || r.status === EpocErr.Eof) break;
      }
    } finally {
      try { await client.send(buildCloseHandle(client.nextOpId(), handle)); }
      catch { /* ignore */ }
    }

    const out = new Uint8Array(total);
    let off = 0;
    for (const c of chunks) { out.set(c, off); off += c.length; }
    return out;
  }

  // Upload a file in chunks. Wrapped in withRfsv so a mid-transfer link
  // bounce re-opens the channel and restarts the upload; REPLACE_FILE
  // truncates first, so a retry converges on the correct final contents.
  async uploadFile(path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void> {
    if (this.sibo) return this.withRfsv16(c => c.uploadFile(path, data, onProgress));
    return this.withRfsv(client => this.doUpload(client, path, data, onProgress));
  }

  private async doUpload(client: RfsvClient, path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void> {
    for (let restart = 0; ; restart++) {
      try {
        return await this.uploadOnce(client, path, data, onProgress);
      } catch (e) {
        // As in doDownload: a link bounce is handled by withRfsv; any
        // other failure means a non-idempotent WRITE timed out, so we
        // restart from a fresh REPLACE_FILE (which truncates), guaranteeing
        // we never double-append a chunk whose reply was merely lost.
        if (e instanceof LinkResetError) throw e;
        if (restart >= PlpClient.MAX_TRANSFER_RESTARTS) throw e;
      }
    }
  }

  private async uploadOnce(client: RfsvClient, path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void> {
    // REPLACE_FILE (opcode 0x2A): create-or-overwrite. Mode 0x0200 is
    // the read/write flag — without it WRITE_FILE refuses the handle.
    // Verified against the real Revo via harness: REPLACE_FILE +
    // WRITE_FILE + CLOSE_HANDLE all return status 0 for "C:\x.txt".
    const mode = FILE_MODE.SHARE_EXCLUSIVE | FILE_MODE.READ_WRITE;
    const reply = await client.send(buildReplaceFile(client.nextOpId(), mode, path));
    if (reply.status !== 0) throw new Error(`REPLACE_FILE(${path}) failed: ${reply.status}`);
    if (reply.data.length < 4) throw new Error('REPLACE_FILE: no handle');
    const handle = u32leFromBytes(reply.data, 0);

    try {
      // Requests/replies larger than the 297-byte NCP-payload ceiling
      // fragment across multiple link frames (Ncp.sendOn outbound, the
      // device's link layer / our queuePartial inbound) — see the
      // chunkSize comment for the sizing rationale.
      const CHUNK = this.chunkSize;
      for (let off = 0; off < data.length; off += CHUNK) {
        const chunk = data.subarray(off, Math.min(off + CHUNK, data.length));
        // idempotent:false — a timed-out WRITE must NOT be blindly resent
        // (it may have already been appended); doUpload restarts the whole
        // transfer from a truncating REPLACE_FILE instead.
        const r = await client.send(buildWriteFile(client.nextOpId(), handle, chunk), false);
        if (r.status !== 0) throw new Error(`WRITE_FILE @${off} failed: ${r.status}`);
        onProgress?.(off + chunk.length);
      }
    } finally {
      try { await client.send(buildCloseHandle(client.nextOpId(), handle)); }
      catch { /* ignore */ }
    }
  }

  async deleteFile(path: string): Promise<void> {
    if (this.sibo) return this.withRfsv16(c => c.deleteFile(path));
    return this.withRfsv(async client => {
      const r = await client.send(buildDelete(client.nextOpId(), path));
      if (r.status !== 0) throw new Error(`DELETE(${path}) failed: ${r.status}`);
      void decodeReply;  // keep imported for future status surfacing
      void ATTR;
    });
  }

  // ── Printer via PC (WPRT) ────────────────────────────────────────
  // The device's "Printer via PC" driver spools the rendered job and
  // the PC pulls it down over the SYS$WPRT service. PlpClient owns the
  // spec's connect dance (Connect → on failure LINK Register → Connect)
  // and the listen loop; the channel protocol lives in wprt-spec.ts.
  private wprt: WprtClient | null = null;
  private printWaitAborted = false;

  // Bring the link up and open the WPRT channel, with the same
  // bounce-retry envelope as connect().
  async connectPrint(timeoutMs = 20_000): Promise<void> {
    const deadline = Date.now() + timeoutMs;
    await this.ensureLinkUp(timeoutMs);
    setSerialPumpEnabled(true);
    let lastErr: unknown;
    for (let attempt = 0; Date.now() < deadline; attempt++) {
      const remaining = deadline - Date.now();
      try {
        await this.ensureLinkUp(remaining);
        await this.openWprt(Math.min(8_000, Math.max(1_000, remaining)));
        return;
      } catch (e) {
        lastErr = e;
        this.wprt = null;
        try { this.ncp.resetChannels(); } catch { /* ignore */ }
        await new Promise<void>(r => setTimeout(r, 120));
      }
    }
    this.transition('failed');
    throw (lastErr instanceof Error ? lastErr
      : new Error('SYS$WPRT.* connect timed out'));
  }

  private async openWprt(timeoutMs: number): Promise<void> {
    this.ncp.sendNcpInfo();
    if (!this.ncp.peerInfoReceived) {
      for (let i = 0; i < 30 && !this.ncp.peerInfoReceived; i++) {
        await new Promise<void>(r => setTimeout(r, 100));
      }
    }

    this.transition('rfsv-opening');
    // Spec sequence: try the Connect first; the server usually isn't
    // running yet, so on failure issue a LINK Register (which loads it)
    // and Connect again — using the name the Register reply suggests
    // when it carries a valid one.
    let connectName = NCP_SERVICE_WPRT;
    try {
      await this.raceTimeout(
        this.ncp.connectServer(connectName),
        Math.min(3_000, timeoutMs),
        'SYS$WPRT.* connect timed out',
      );
    } catch {
      const reg = await this.ncp.linkRegister(NCP_REGISTER_WPRT, timeoutMs);
      if (reg.serverName) connectName = reg.serverName;
      await this.raceTimeout(
        this.ncp.connectServer(connectName),
        timeoutMs,
        `${connectName} connect timed out after Register`,
      );
    }

    const openChan = this.firstOpenClientChan();
    if (openChan === null) {
      this.lastError = 'no open client channel after WPRT connect';
      throw new Error(this.lastError);
    }
    const wprt = new WprtClient(this.ncp, openChan);
    const status = await wprt.exchangeLevel(2, 0, timeoutMs);
    if (status !== 0) throw new Error(`WPRT_LEVEL rejected: status ${status}`);
    this.wprt = wprt;
    setSerialPumpEnabled(true);
    this.transition('connected');
  }

  // Listen until a complete print job arrives. Blocks indefinitely
  // (printing is user-initiated on the device), surviving link bounces
  // by re-opening the WPRT channel; a bounce mid-job discards the
  // partial job and goes back to listening. abortPrintWait() (or
  // disconnect()) makes this reject promptly.
  async waitForPrintJob(
    onProgress?: (bytesReceived: number, pagesReceived: number) => void,
  ): Promise<WprtJob> {
    this.printWaitAborted = false;
    let assembler = new WprtJobAssembler();
    let bytes = 0;
    let pages = 0;
    for (;;) {
      if (this.printWaitAborted) throw new Error('print capture stopped');
      const gen = this.linkGen;
      try {
        await this.ensureLinkUp(15_000);
        if (!this.wprt) await this.openWprt(15_000);
        const reply = await this.wprt!.readData(0);
        bytes += reply.length;
        const job = assembler.feed(reply);
        if (job) { onProgress?.(bytes, job.pages.length); return job; }
        pages = assembler.pagesSoFar;
        onProgress?.(bytes, pages);
      } catch (e) {
        if (this.printWaitAborted) throw new Error('print capture stopped');
        const bounced = e instanceof LinkResetError || this.linkGen !== gen;
        if (!bounced) throw e;
        // Link bounce: the channel (and any partial job) is gone.
        assembler = new WprtJobAssembler();
        bytes = 0;
        pages = 0;
        this.wprt = null;
        await new Promise<void>(r => setTimeout(r, 150));
      }
    }
  }

  // Stop a waitForPrintJob() listen without tearing the link down.
  abortPrintWait(): void {
    this.printWaitAborted = true;
    this.wprt?.reset();
    this.wprt = null;
  }

  // ── internals ──────────────────────────────────────────────────
  private tick(): void {
    // Keepalive while the link is up: re-Ack roughly twice a second so the
    // device's RemoteLinkServer serial thread keeps getting rescheduled and
    // its link-inactivity timer never fires during a slow RFSV reply. This
    // runs before the early-return below so it still fires across the quiet
    // multi-second drive-list wait when no device bytes are arriving. See
    // LinkLayer.keepAlive() / docs/series7-plp-drivelist-2026-06-06.md.
    if (this.linkState_ === 'connected') {
      const now = Date.now();
      if (now - this.lastKeepAliveAt >= KEEPALIVE_INTERVAL_MS) {
        this.lastKeepAliveAt = now;
        this.link.keepAlive();
        // Hand the worker the latest keepalive frame so it can replay it in
        // sim-time (no-op in main-thread mode). Keeps it fresh as seqRx moves.
        setSimKeepAliveFrame(this.link.keepAliveFrame());
      }
      // Resend any host→device Data_Pdu the device hasn't acked. This is what
      // recovers an upload after the worker serial bridge drops bytes (the
      // device retransmits its own data, but our outbound frames had no such
      // safety net). See LinkLayer.pumpRetransmit().
      this.link.pumpRetransmit();
    }
    const chunk = this.readBytes();
    if (chunk.length === 0) return;
    this.lastRxAt = Date.now();
    // Any inbound bytes — a reply fragment OR just a link-level Ack of one
    // of our frames — prove the device is alive, so push back the in-flight
    // RFSV request's inactivity timeout. This is what lets a transfer
    // survive the device being kept busy (e.g. the user tapping icons): the
    // RemoteLinkServer thread is slow to produce the RFSV reply, but the
    // link layer keeps Acking our frames, and those Acks hold the timer off.
    this.rfsv?.noteActivity();
    this.rfsv16?.noteActivity();
    this.cfg.onRawRx?.(chunk);
    const frames = this.decoder.feed(chunk);
    for (const f of frames) this.link.handleFrame(f.payload);
    // One cumulative Ack for the whole batch (R5 download path) — see
    // LinkLayer.flushAck() / the PDU_CONT_DATA handler.
    this.link.flushAck();
    // Refresh the worker's sim-time keepalive frame after every inbound batch.
    // During a download seqRx advances several frames per reply, far faster
    // than the 150 ms keepalive timer above — and the worker REPLAYS the last
    // frame it was handed 64×/s. A stale snapshot means the device is flooded
    // with an Ack for an old Seq while it streams new frames; its link layer
    // reads the regressed Ack as "everything after that Seq is unacknowledged"
    // and retransmits the whole window over and over, eventually exhausting its
    // retransmit budget and wedging the download. Re-handing the worker an Ack
    // for the CURRENT seqRx on each inbound batch keeps the replayed keepalive
    // honest (no-op on the main thread, where there is no worker replay).
    if (this.linkState_ === 'connected') setSimKeepAliveFrame(this.link.keepAliveFrame());
  }

  private transition(next: ClientState): void {
    if (next === this.state_) return;
    this.state_ = next;
    this.cfg.onState?.(next);
  }

  private firstOpenClientChan(): number | null {
    // Probe Ncp's internal byClient map. We don't expose an iterator
    // because the dialog should never enumerate channels directly,
    // but PlpClient is the trusted internal consumer.
    const map = (this.ncp as unknown as { byClient: Map<number, { state: string }> }).byClient;
    for (const [chan, info] of map) {
      if (info.state === 'open') return chan;
    }
    return null;
  }

  private raceTimeout<T>(p: Promise<T>, ms: number, msg: string): Promise<T> {
    // If the timeout wins the race, p keeps running; a later rejection of it
    // (e.g. resetChannels reaping a dangling pending connect on the next
    // connect() retry) must not surface as an unhandled rejection.
    p.catch(() => { /* settled late, ignored */ });
    return Promise.race([
      p,
      new Promise<T>((_, reject) => setTimeout(() => reject(new Error(msg)), ms)),
    ]);
  }
}
