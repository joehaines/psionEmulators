// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PLP data-link layer: PDU encoder/decoder, link state machine, and
// the Revo R5 connection handshake.
//
// Sits on top of frontend/src/lib/plp/framing.ts (which handles the
// byte-level SYN-DLE-STX framing, CRC, and DLE stuffing). This module
// owns the Cont/Seq byte and the PDU types the spec calls out:
// Req_Req_Pdu, Req_Con_Pdu, Ack_Pdu, Data_Pdu.
//
// Verified end-to-end against the real Revo via harness/run.cpp's
// --serial-auto-rule surface: see commit 194573e5's message for the
// exact byte-level transcripts of each handshake step.

import { encodeFrame, FrameDecoder } from './framing.ts';
import { PDU_CONT_ACK, PDU_CONT_DISC, PDU_CONT_REQ, PDU_CONT_DATA } from './types.ts';

// ── PDU types ───────────────────────────────────────────────────────
export interface Pdu {
  cont: number;        // high nibble of the Cont/Seq byte (0..15)
  seq: number;         // low nibble of the Cont/Seq byte (0..15)
  data: Uint8Array;    // payload bytes after the Cont/Seq byte
}

// Build the Cont/Seq byte. Seq > 7 needs the EPOC extended encoding
// (Cont, (Seq&7)|8, Seq>>3, …) — the Revo doesn't seem to exercise
// that path in our captures, but we handle it for completeness.
export function encodePdu(pdu: Pdu): Uint8Array {
  const { cont, seq, data } = pdu;
  if (seq <= 7) {
    const payload = new Uint8Array(1 + data.length);
    payload[0] = (cont << 4) | (seq & 0x7);
    payload.set(data, 1);
    return encodeFrame(payload);
  }
  // Extended encoding: two header bytes
  const payload = new Uint8Array(2 + data.length);
  payload[0] = (cont << 4) | ((seq & 0x7) | 0x8);
  payload[1] = (seq >> 3) & 0xFF;
  payload.set(data, 2);
  return encodeFrame(payload);
}

// Decode an already-de-framed payload into a Pdu. Returns null when
// the payload is empty.
export function decodePdu(payload: Uint8Array): Pdu | null {
  if (payload.length < 1) return null;
  const b = payload[0];
  const cont = (b >> 4) & 0xF;
  let seq = b & 0xF;
  let dataStart = 1;
  if (seq & 0x8) {
    // EPOC extended Seq encoding — the low 3 bits of seq are in `b`,
    // the upper bits in payload[1].
    if (payload.length < 2) return null;
    seq = (seq & 0x7) | (payload[1] << 3);
    dataStart = 2;
  }
  return { cont, seq, data: payload.subarray(dataStart) };
}

// ── Convenience PDU constructors ────────────────────────────────────
// The Revo's R5 ROM uses Cont=2 Seq=4 (= 0x24) for its Req_Con_Pdu,
// not the spec's Cont=4 Seq=4 (= 0x44). The behaviour is identical
// (carries a 4-byte magic, advances on Ack_Pdu Seq=0) but the byte
// differs. We emit the Revo-flavoured form so the same client also
// connects to a real device speaking R5 / R3 — both work.
export function reqReqPdu(seq: number = 1): Pdu {
  return { cont: PDU_CONT_REQ, seq, data: new Uint8Array(0) };
}
export function reqConPdu(magic: Uint8Array, seq: number = 4): Pdu {
  if (magic.length !== 4) throw new Error('reqConPdu: magic must be 4 bytes');
  return { cont: PDU_CONT_REQ, seq, data: magic };
}
export function ackPdu(seq: number = 0): Pdu {
  return { cont: PDU_CONT_ACK, seq, data: new Uint8Array(0) };
}
export function dataPdu(seq: number, data: Uint8Array): Pdu {
  return { cont: PDU_CONT_DATA, seq, data };
}
// Disc_Pdu: terminates the link. The spec says "no reply expected" —
// the receiver is just expected to drop its connection state. Without
// sending one, a device whose serial driver only sees DSR drop may
// leave its Remote Link state machine half-up and refuse to
// re-handshake on the next plug-in.
export function discPdu(): Pdu {
  return { cont: PDU_CONT_DISC, seq: 0, data: new Uint8Array(0) };
}

// ── Link state machine ──────────────────────────────────────────────
// Drives the EPOC peer-to-peer connection sequence and the per-PDU
// ack-tracking that data transfer needs. The caller supplies a
// `sendBytes` function (to push framed bytes onto the wire) and is
// expected to feed every received frame payload back into `handleFrame`.
// The state machine fires `onData` for each accepted Data_Pdu payload
// (the NCP layer above wires `onData` to its packet dispatcher).

export type LinkState =
  | 'idle'         // disconnected, waiting for either side to initiate
  | 'connecting'   // saw or sent Req_Req_Pdu; waiting for Req_Con_Pdu
  | 'confirming'   // sent Req_Con_Pdu; waiting for Ack_Pdu
  | 'connected'    // link is up, Data_Pdu flow allowed
  | 'failed';      // gave up trying

export interface LinkConfig {
  // Which flavour of the PLP data link the peer speaks. 'epoc'
  // (default) uses the Req_Req/Req_Con+magic handshake and mod-2048
  // sequence numbers; 'sibo' (Series 3c / 3mx / Siena EPOC16 ROMs)
  // uses the plain Req_Pdu/Req_Pdu/Ack handshake and mod-8 sequence
  // numbers. Verified against the live 3c v5.20f kernel: host
  // Req_Pdu(0x21) -> device Req_Pdu(0x20) -> host Ack(0x00) ->
  // device Ack + NCP Info (version 0x03).
  variant?: 'epoc' | 'sibo';
  // Sends raw framed bytes onto the wire. The caller wires this to the
  // WASM serialWriteFromHost helper.
  sendBytes(bytes: Uint8Array): void;
  // Called for every accepted Data_Pdu payload.
  onData(payload: Uint8Array): void;
  // Optional state-change observer for the UI.
  onStateChange?(state: LinkState): void;
  // Fired when an *established* link is torn down and re-handshaked by
  // the peer (a mid-session Req_Req_Pdu). The NCP layer above forgets
  // every channel on a reset, so PlpClient uses this to drop and re-open
  // its RFSV channel rather than stranding an in-flight request.
  onReset?(): void;
  // Fired when probeAdopt() resumed a dead host's session instead of
  // handshaking fresh — the NCP layer above moves its channel
  // allocation clear of the dead session's channels.
  onAdopt?(): void;
  // Optional traffic mirror for diagnostics (logs every PDU each way).
  onPduIn?(pdu: Pdu): void;
  onPduOut?(pdu: Pdu): void;
  // Seq nibble of the Req_Con_Pdu we send when the peer initiates. The R5
  // Windermere/SA-1100 ROMs accept the Revo-flavoured 4 (0x24, the default);
  // the ER3/ER4 CL-PS711x ROMs (Series 5, Osaris) silently DROP 0x24 — and
  // stop retrying their Req_Req, wedging the link — but complete the
  // handshake with 2 (0x22): they reply Ack_Pdu and proceed to NCP Info.
  // Established empirically via the harness reply matrix (see
  // scripts/test-remote-link.sh).
  conSeq?: number;
}

export class LinkLayer {
  private cfg: LinkConfig;
  private state_: LinkState = 'idle';
  // EPOC variant uses mod-2048 sequence numbers. We start at 0 and
  // increment-then-send.
  private seqTx = 0;
  // Last valid Data_Pdu Seq we received; this is what we put in
  // outgoing Ack_Pdu frames per the spec.
  private seqRx = 0;
  // Whether any Data_Pdu has arrived since the link came up — needed to
  // tell a retransmitted duplicate (seq == seqRx) from the first frame.
  private gotData = false;
  // Set while an adoption probe (see probeAdopt) is outstanding.
  private probeSent = false;
  // 4-byte pseudo-random local magic, sampled at construction time so
  // the same instance keeps a stable value across reconnect attempts.
  private magic: Uint8Array;

  constructor(cfg: LinkConfig) {
    this.cfg = cfg;
    this.magic = new Uint8Array(4);
    // Math.random is fine — the magic only needs to differ from the
    // remote's value to prevent self-connection detection.
    for (let i = 0; i < 4; i++) this.magic[i] = Math.floor(Math.random() * 256);
  }

  get state(): LinkState { return this.state_; }

  // Drives the handshake forward when the host attaches. The Revo
  // does send Req_Req_Pdu spontaneously on cable-plug, but only the
  // first time — on a subsequent reattach (e.g. dialog closed and
  // re-opened) its serial driver may not re-initiate, leaving the
  // host hanging. Calling initiate() makes us issue our own
  // Req_Req_Pdu, which the device answers with a Req_Con_Pdu in
  // peer-to-peer mode regardless of which side's link state machine
  // thinks the connection is fresh.
  initiate(): void {
    if (this.state_ === 'connected') return;  // already up — no-op
    this.transition('connecting');
    this.send(reqReqPdu(1));
  }

  // Gracefully tear the link down by sending a Disc_Pdu. The spec
  // says "no reply expected" — once we send it, the peer is expected
  // to drop its state. Caller still needs to detach the bridge after
  // calling this (Disc_Pdu doesn't unwire the byte transport).
  disconnect(): void {
    if (this.state_ === 'connected' || this.state_ === 'confirming' || this.state_ === 'connecting') {
      this.send(discPdu());
    }
    this.transition('idle');
    this.seqTx = 0;
    this.seqRx = 0;
    this.gotData = false;
    this.probeSent = false;
  }

  // Probe for a session a dead host left behind. The EPOC R5 link
  // server never tears an established session down on its own — not on
  // host silence, not on Disc_Pdu, not on a cable unplug/replug — and
  // while one exists it ignores Req_Req_Pdu entirely, so a fresh
  // handshake can never succeed (verified against the live 5mx ROM via
  // the harness socket bridge; on the physical device only toggling
  // Remote Link off/on clears it). What the dead session DOES still do
  // is duplicate-ack: any Data_Pdu we send is answered with an Ack
  // carrying the last Seq the device accepted from the old host. The
  // ACK branch in handleFrame uses that to ADOPT the session — resume
  // its Tx numbering and carry on — instead of waiting for a handshake
  // that will never come. The probe payload is a single byte: too
  // short to be a valid NCP packet, so even if the guessed Seq lands
  // in-window the device's NCP discards it harmlessly.
  probeAdopt(): void {
    if (this.state_ === 'connected') return;
    if (this.state_ === 'idle') this.transition('connecting');
    this.probeSent = true;
    this.send(dataPdu(5, new Uint8Array(1)));
  }

  // Link-level keepalive: re-Ack the last received Data_Pdu Seq while the
  // link is up. Benign on the wire (a duplicate Ack the peer already moved
  // past), but on the EPOC R5 device the inbound frame raises a serial-RX
  // interrupt, which the emulator turns into a synthetic OSMR1 scheduler
  // kick — rescheduling RemoteLinkServer's serial thread so its
  // link-inactivity timer is serviced and reset. Without this, a slow RFSV
  // reply (the SA-1100 drive-list takes several seconds of internal F32 +
  // scheduler work, during which the host is otherwise silent) lets that
  // timer fire, the device spontaneously re-handshakes (mid-session
  // Req_Req_Pdu), and the in-flight request is stranded on a channel the
  // device has just forgotten. See handleFrame()'s Req_Req_Pdu branch and
  // docs/series7-plp-drivelist-2026-06-06.md. No-op unless connected.
  keepAlive(): void {
    if (this.state_ !== 'connected') return;
    this.send(ackPdu(this.seqRx));
  }

  // Send a Data_Pdu carrying `payload`. Returns the assigned sequence
  // number — useful for matching against the next Ack_Pdu.
  sendData(payload: Uint8Array): number {
    if (this.state_ !== 'connected') {
      throw new Error(`sendData while link is ${this.state_}`);
    }
    // Tx sequence space: mod 2048 on the modern (R5) link, mod 8 —
    // INCLUDING seq 0 — on both 3-bit dialects: the SIBO EPOC16 link
    // (variant 'sibo': Series 3c / 3mx / Siena / Workabout MX) and the
    // ER3/ER4 CL-PS711x link (conSeq 2: Series 5, Osaris). The 3-bit
    // ROMs ignore any Data_Pdu outside their window and duplicate-ack
    // the last good seq forever; after seq 7 they expect seq 0, not 1
    // (live-traced on both families: literal seq=8 ignored; wrap-to-1
    // ignored; the full 0..7 cycle is the only progression they
    // accept — on the CL-PS711x this surfaced as READDIR stalling
    // ~43 s into RFSV retries after exactly 7 PDUs). The skip-0 rule
    // (Seq=0 reserved for the handshake) applies only to the R5 link.
    if (this.cfg.variant === 'sibo' || this.cfg.conSeq === 2) {
      this.seqTx = (this.seqTx + 1) & 0x7;
    } else {
      this.seqTx = (this.seqTx + 1) & 0x7FF;  // mod 2048 (R5 link)
      if (this.seqTx === 0) this.seqTx = 1;   // skip Seq=0 (reserved for handshake)
    }
    this.send(dataPdu(this.seqTx, payload));
    return this.seqTx;
  }

  // Feed a complete frame payload (output of FrameDecoder.feed) into
  // the state machine. Triggers cfg.sendBytes for outbound PDUs and
  // cfg.onData for accepted Data_Pdu payloads.
  handleFrame(payload: Uint8Array): void {
    const pdu = decodePdu(payload);
    if (!pdu) return;
    this.cfg.onPduIn?.(pdu);

    // Req_Pdu / Req_Req_Pdu / Req_Con_Pdu all share Cont=2 in the
    // Revo's R5 encoding. The data length disambiguates:
    //   no data → Req_Req_Pdu (we reply with Req_Con_Pdu + magic)
    //   4 bytes → Req_Con_Pdu (we reply with Ack_Pdu Seq=0)
    if (pdu.cont === PDU_CONT_REQ) {
      if (this.cfg.variant === 'sibo') {
        // SIBO connection sequence (PLP spec "SIBO Connection"): the
        // host sends Req_Pdu, the device accepts with its own Req_Pdu,
        // and an Ack(0) completes the handshake. There is no
        // Req_Con/magic stage. A Req_Pdu on an established link is the
        // device re-handshaking — reset our counters and re-accept.
        const wasEstablished = this.state_ === 'connected';
        this.seqTx = 0;
        this.seqRx = 0;
        this.gotData = false;
        this.send(ackPdu(0));
        this.transition('connected');
        if (wasEstablished) this.cfg.onReset?.();
        return;
      }
      if (pdu.data.length === 0) {
        // Peer requested a connection — confirm with our magic.
        //
        // A link handshake resets BOTH sequence counters to 0 (PLP spec).
        // If the peer re-initiates mid-session — the EPOC R5 netBook's
        // RemoteLinkServer does exactly this whenever its link-inactivity
        // timer fires before the kernel reschedules its serial thread — we
        // MUST restart our counters too. Otherwise we keep emitting
        // Data_Pdus with our stale, ever-climbing Tx Seq while the freshly
        // reset device expects Seq 1; it flags a sequence error and
        // re-handshakes again, looping forever and stranding every NCP
        // channel (an in-flight RFSV request just times out on a channel
        // the device has already forgotten). Observed live on Series 7 as
        // "DRIVE_LIST failed: ... timed out after 10s".
        const wasEstablished =
          this.state_ === 'connected' || this.state_ === 'confirming';
        this.seqTx = 0;
        this.seqRx = 0;
        this.transition('confirming');
        this.send(reqConPdu(this.magic, this.cfg.conSeq ?? 4));
        if (wasEstablished) this.cfg.onReset?.();
      } else if (pdu.data.length === 4) {
        // Peer's Req_Con_Pdu carrying its magic. Reject the rare
        // self-connection case where the magics match.
        let match = true;
        for (let i = 0; i < 4; i++) if (pdu.data[i] !== this.magic[i]) { match = false; break; }
        if (match) {
          this.transition('failed');
          return;
        }
        this.send(ackPdu(0));
        this.transition('connected');
      }
      return;
    }
    if (pdu.cont === PDU_CONT_ACK) {
      // During the handshake, an Ack closes out the peer's confirmation
      // of OUR Req_Con_Pdu — we're now connected. Once `connected`,
      // Acks just match outstanding Data_Pdu sequence numbers (we
      // don't gate on them yet; single-outstanding flow control would
      // tighten this). On the SIBO variant the device Acks our Req_Pdu
      // directly (peer-to-peer accept), so 'connecting' completes here
      // too.
      if (this.state_ === 'confirming' ||
          (this.cfg.variant === 'sibo' && this.state_ === 'connecting')) {
        this.probeSent = false;
        this.transition('connected');
        return;
      }
      // Duplicate-ack reply to an adoption probe (see probeAdopt): the
      // peer still holds a dead host's session, and the Ack's Seq is
      // the last Data_Pdu Seq it accepted on it. Resume from there.
      if (this.state_ === 'connecting' && this.probeSent &&
          this.cfg.variant !== 'sibo') {
        this.probeSent = false;
        this.seqTx = pdu.seq;
        this.seqRx = 0;
        this.gotData = false;
        this.cfg.onAdopt?.();
        this.transition('connected');
      }
      return;
    }
    if (pdu.cont === PDU_CONT_DATA) {
      // Data on a link we never brought up is a dead host's parked
      // session retransmitting. ACK it but do NOT deliver: the ack
      // keeps that session adoptable (un-acked frames exhaust the
      // device's retransmit budget within about a second, after which
      // its link server goes terminally dormant — only a Remote Link
      // off/on toggle revives it), while delivering would hand a stale
      // reply to a fresh NCP. probeAdopt then resumes the session.
      if (this.state_ !== 'connected') {
        this.send(ackPdu(pdu.seq));
        return;
      }
      // Per spec, ACK every Data_Pdu — including out-of-order ones —
      // with the last valid Seq we've seen. On the SIBO variant the
      // device retransmits a Data_Pdu until our Ack lands; a duplicate
      // (same Seq as the last delivered frame) must be re-acked but
      // NOT delivered again, or a stale reply gets matched against the
      // next in-flight request (observed live: a retransmitted FCLOSE
      // status was consumed as the following FOPEN's reply).
      const dup = this.cfg.variant === 'sibo' && this.gotData && pdu.seq === this.seqRx;
      this.seqRx = pdu.seq;
      this.gotData = true;
      this.send(ackPdu(this.seqRx));
      if (!dup) this.cfg.onData(pdu.data);
      return;
    }
    if (pdu.cont === PDU_CONT_DISC) {
      // Peer terminated the link. Reset to idle so the next
      // initiate() can rebuild the connection from scratch.
      this.transition('idle');
      this.seqTx = 0;
      this.seqRx = 0;
      return;
    }
  }

  private send(pdu: Pdu): void {
    this.cfg.onPduOut?.(pdu);
    this.cfg.sendBytes(encodePdu(pdu));
  }

  private transition(next: LinkState): void {
    if (next === this.state_) return;
    this.state_ = next;
    this.cfg.onStateChange?.(next);
  }
}

// ── Helper: build a streaming pump that combines FrameDecoder + LinkLayer ──
// Returns a function the caller invokes with each new chunk of bytes.
export function makeBytePump(decoder: FrameDecoder, link: LinkLayer) {
  return (chunk: Uint8Array) => {
    if (chunk.length === 0) return;
    const frames = decoder.feed(chunk);
    for (const f of frames) link.handleFrame(f.payload);
  };
}
