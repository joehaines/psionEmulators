// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Top-level orchestrator for the modem path. Owns the UART port,
// transport poll loop, mode dispatcher, and the PPP/IP state.
//
// Lifecycle: caller constructs, calls start() to attach the UART and
// start polling, then waits for events (status, frames, packets) to
// arrive on the supplied callbacks. stop() detaches.
//
// Phase 1 brings the link to IPCP-Opened and answers ICMP echoes;
// TCP/UDP/HTTP/SMTP/POP3 join in phase 2/3 by plugging more handlers
// into the dispatcher's onPppFrame and into the IP demux.

import { ModemPort, type SerialBridge } from './port.ts';
import { ModemTransport } from './transport.ts';
import { ModeDispatcher, type DispatcherMode } from './dispatcher.ts';
import { Lcp } from '../ppp/lcp.ts';
import { recvPap } from '../ppp/pap.ts';
import { Ipcp, HOST_IP, DEVICE_IP } from '../ppp/ipcp.ts';
import { PROTO_LCP, PROTO_PAP, PROTO_IPCP, PROTO_IP } from '../ppp/demux.ts';
import { parseIpv4, buildIpv4 } from '../inet/ip.ts';
import { handleIcmp } from '../inet/icmp.ts';
import { parseUdp, buildUdp } from '../inet/udp.ts';
import { handleDnsQuery } from '../inet/dns.ts';
import { Tcp } from '../inet/tcp.ts';
import { createHttpProxyListener, type FetchFn } from '../inet/http-proxy.ts';
import { createSmtpListener } from '../inet/smtp.ts';
import { createPop3Listener } from '../inet/pop3.ts';
import { Mailbox, getBrowserMailbox } from '../inet/mailbox.ts';
import { IP_PROTO_ICMP, IP_PROTO_UDP, IP_PROTO_TCP } from '../inet/types.ts';
import type { AtResult } from './at.ts';

export type LinkPhase =
  | 'idle'
  | 'attaching'
  | 'command'        // attached, modem in AT command mode (post-CONNECT goes online)
  | 'lcp'            // PPP starting, LCP negotiating
  | 'authenticating' // PAP in flight
  | 'ipcp'           // LCP open, IPCP negotiating
  | 'online'         // IPCP open — device has its IP, IP traffic flows
  | 'hung-up';

export interface HttpEvent {
  host: string;
  method?: string;       // present on request events
  path?: string;
  source?: 'curated' | 'fetch';
  status?: number;       // present on response events
  length?: number;
}

export type MailEvent =
  | { kind: 'smtp-received'; id: string }
  | { kind: 'pop3-delivered'; count: number };

export interface ModemClientOptions {
  uartIndex: number;
  bridge: SerialBridge;
  onPhaseChange?: (phase: LinkPhase) => void;
  onRawRx?: (bytes: Uint8Array) => void;
  onRawTx?: (bytes: Uint8Array) => void;
  onAtLine?: (line: string, result: AtResult) => void;
  onModeChange?: (mode: DispatcherMode) => void;
  onPppFrame?: (protocol: number, info: Uint8Array, dir: 'rx' | 'tx') => void;
  onIpPacket?: (proto: number, src: Uint8Array, dst: Uint8Array, len: number, dir: 'rx' | 'tx') => void;
  onHttp?: (kind: 'request' | 'response', ev: HttpEvent) => void;
  onMail?: (ev: MailEvent) => void;
  onError?: (message: string) => void;
  // Test hook: replace window.fetch in the HTTP proxy.
  fetchImpl?: FetchFn;
  // Test hook: inject a non-default mailbox (in-memory for tests; the
  // browser path uses the shared IndexedDB-backed singleton).
  mailbox?: Mailbox;
}

export class ModemClient {
  readonly hostIp:   readonly number[] = HOST_IP;
  readonly deviceIp: readonly number[] = DEVICE_IP;

  private opts: ModemClientOptions;
  private port: ModemPort;
  private transport: ModemTransport;
  private dispatcher: ModeDispatcher;
  private lcp: Lcp = new Lcp();
  private ipcp: Ipcp = new Ipcp();
  private tcp: Tcp;
  private mailbox: Mailbox;
  private phase: LinkPhase = 'idle';
  private echoTimer: ReturnType<typeof setInterval> | null = null;

  constructor(opts: ModemClientOptions) {
    this.opts = opts;
    this.port = new ModemPort(opts.bridge, opts.uartIndex);
    this.transport = new ModemTransport(this.port, {
      onRawRx: bytes => opts.onRawRx?.(bytes),
      onRawTx: bytes => opts.onRawTx?.(bytes),
    });
    this.dispatcher = new ModeDispatcher(
      bytes => this.transport.send(bytes),
      {
        onAtLine: (line, result) => opts.onAtLine?.(line, result),
        onModeChange: mode => {
          opts.onModeChange?.(mode);
          if (mode === 'online') this.startPpp();
          else if (mode === 'command') this.setPhase('command');
        },
        onPppFrame: (proto, info) => this.handleIncomingPpp(proto, info),
      },
    );
    this.transport.setRxSink(b => this.dispatcher.feed(b));
    // TCP engine — bound to our host IP, emits packets back through the
    // same IP path that handles ICMP. Register listeners as the
    // application layer grows (port 80 for HTTP today; 25/110 for
    // SMTP/POP3 in phase 3).
    const hostArr = Uint8Array.from(this.hostIp as readonly number[]);
    this.tcp = new Tcp(hostArr, packet => {
      this.opts.onIpPacket?.(packet[9], packet.subarray(12, 16), packet.subarray(16, 20), packet.length, 'tx');
      this.dispatcher.sendIp(packet);
    });
    this.tcp.listen(80, createHttpProxyListener({
      fetch: opts.fetchImpl,
      onRequest: (host, method, path, source) => opts.onHttp?.('request', { host, method, path, source }),
      onResponse: (host, status, length) => opts.onHttp?.('response', { host, status, length }),
    }));
    // Mailbox-backed SMTP + POP3 — port 25 and 110.
    this.mailbox = opts.mailbox ?? getBrowserMailbox();
    this.tcp.listen(25, createSmtpListener({
      mailbox: this.mailbox,
      onMessageReceived: id => opts.onMail?.({ kind: 'smtp-received', id }),
    }));
    this.tcp.listen(110, createPop3Listener({
      mailbox: this.mailbox,
      onMessagesDelivered: count => opts.onMail?.({ kind: 'pop3-delivered', count }),
    }));
  }

  // Mailbox accessor for the dialog (Outbox/Inbox tabs).
  getMailbox(): Mailbox { return this.mailbox; }

  start(): boolean {
    if (this.port.isAttached()) {
      this.opts.onError?.('UART is already attached — disconnect there first.');
      return false;
    }
    this.setPhase('attaching');
    if (!this.port.attach()) {
      this.setPhase('idle');
      this.opts.onError?.('Could not attach host to UART.');
      return false;
    }
    this.transport.start();
    this.setPhase('command');
    return true;
  }

  stop(): void {
    if (this.echoTimer !== null) { clearInterval(this.echoTimer); this.echoTimer = null; }
    this.transport.stop();
    this.port.detach();
    this.setPhase('idle');
  }

  isAttached(): boolean { return this.port.isAttached(); }
  getPhase():   LinkPhase { return this.phase; }

  // Test-only: pump one transport tick synchronously. Production
  // wiring uses the transport's own setInterval started by start().
  pumpForTest(): void { this.transport.tick(); }

  // ── PPP/IP plumbing ───────────────────────────────────────────
  private startPpp(): void {
    this.setPhase('lcp');
    this.lcp = new Lcp();
    this.ipcp = new Ipcp();
    // Send our initial Configure-Request.
    const req = this.lcp.open();
    this.txPpp(PROTO_LCP, req);
  }

  private handleIncomingPpp(proto: number, info: Uint8Array): void {
    this.opts.onPppFrame?.(proto, info, 'rx');
    switch (proto) {
      case PROTO_LCP: {
        const events = this.lcp.recv(info);
        for (const ev of events) {
          if (ev.send) this.txPpp(PROTO_LCP, ev.send);
          if (ev.stateChanged === 'opened') {
            if (this.lcp.authProto === 0xC023) {
              this.setPhase('authenticating');
              // Wait for the peer's Authenticate-Request. EPOC always
              // initiates PAP — we don't need to start it.
            } else {
              // No auth negotiated; jump straight to IPCP.
              this.startIpcp();
            }
          }
        }
        break;
      }
      case PROTO_PAP: {
        const events = recvPap(info);
        for (const ev of events) {
          if (ev.send) this.txPpp(PROTO_PAP, ev.send);
          if (ev.authenticated) this.startIpcp();
        }
        break;
      }
      case PROTO_IPCP: {
        const events = this.ipcp.recv(info);
        for (const ev of events) {
          if (ev.send) this.txPpp(PROTO_IPCP, ev.send);
          if (ev.stateChanged === 'opened') {
            this.setPhase('online');
            this.startEchoKeepalive();
          }
        }
        break;
      }
      case PROTO_IP: {
        this.handleIp(info);
        break;
      }
      default:
        // Unknown protocol — RFC 1661 would have us send a
        // Protocol-Reject in an LCP message; that's a phase-2 nicety.
        break;
    }
  }

  private startIpcp(): void {
    this.setPhase('ipcp');
    const req = this.ipcp.open();
    this.txPpp(PROTO_IPCP, req);
  }

  private startEchoKeepalive(): void {
    if (this.echoTimer !== null) clearInterval(this.echoTimer);
    this.echoTimer = setInterval(() => {
      if (this.phase !== 'online') return;
      const echo = this.lcp.buildEchoRequest();
      this.txPpp(PROTO_LCP, echo);
    }, 30_000);
  }

  private txPpp(proto: number, info: Uint8Array): void {
    this.opts.onPppFrame?.(proto, info, 'tx');
    this.dispatcher.sendPpp(proto, info);
  }

  private handleIp(info: Uint8Array): void {
    const pkt = parseIpv4(info);
    if (!pkt) return;
    this.opts.onIpPacket?.(pkt.header.protocol, pkt.header.src, pkt.header.dst, pkt.header.totalLength, 'rx');
    const hostArr = Uint8Array.from(this.hostIp as readonly number[]);
    switch (pkt.header.protocol) {
      case IP_PROTO_ICMP: {
        const reply = handleIcmp(pkt);
        if (reply) {
          const out = buildIpv4(IP_PROTO_ICMP, hostArr, pkt.header.src, reply);
          this.opts.onIpPacket?.(IP_PROTO_ICMP, hostArr, pkt.header.src, out.length, 'tx');
          this.dispatcher.sendIp(out);
        }
        break;
      }
      case IP_PROTO_UDP: {
        const udp = parseUdp(pkt.payload);
        if (!udp) break;
        // Phase 2 only has the DNS server on port 53. Anything else
        // we silently drop; the inet stack on the device will retry.
        if (udp.dstPort === 53) {
          const reply = handleDnsQuery(udp.payload);
          if (reply) {
            const seg = buildUdp(hostArr, pkt.header.src, {
              srcPort: 53, dstPort: udp.srcPort, payload: reply,
            });
            const out = buildIpv4(IP_PROTO_UDP, hostArr, pkt.header.src, seg);
            this.opts.onIpPacket?.(IP_PROTO_UDP, hostArr, pkt.header.src, out.length, 'tx');
            this.dispatcher.sendIp(out);
          }
        }
        break;
      }
      case IP_PROTO_TCP: {
        // Hand off to the TCP engine; it emits outbound segments via
        // the constructor-supplied callback (which calls
        // dispatcher.sendIp).
        this.tcp.handleSegment(pkt.header.src, pkt.header.dst, pkt.payload);
        break;
      }
      default: break;
    }
  }

  private setPhase(p: LinkPhase): void {
    if (this.phase === p) return;
    this.phase = p;
    this.opts.onPhaseChange?.(p);
  }
}
