// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Real sockets for the emulated Psion LX's Wi-Fi: the socket side, with no
// Electron in it (net.ts wires it to IPC), so it can be tested under plain
// node. See net.ts for what the emulator asks of it.

import * as dgram from 'node:dgram';
import * as dns from 'node:dns';
import * as net from 'node:net';
import type { NetCommand, NetEvent } from '../ipc/contract';

const ROUTER_IP = 0x0a000202;
const DNS_IP = 0x0a000203;
const MAX_TCP = 256;
const MAX_UDP = 64;
const MAX_CHUNK = 1 << 20;

function ipString(ip: number): string {
  return [ip >>> 24, (ip >>> 16) & 255, (ip >>> 8) & 255, ip & 255].join('.');
}

function ipNumber(s: string): number | null {
  const m = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/.exec(s);
  if (!m) return null;
  return ((+m[1] << 24) | (+m[2] << 16) | (+m[3] << 8) | +m[4]) >>> 0;
}

/** Where the guest's 10.0.2.3:53 queries go: the first IPv4 system resolver. */
function resolverAddress(): string {
  try {
    for (const s of dns.getServers()) if (ipNumber(s) !== null) return s;
  } catch { /* fall through */ }
  return '1.1.1.1';
}

function isId(v: unknown): v is number {
  return typeof v === 'number' && Number.isInteger(v) && v >= 0 && v <= 0xffffffff;
}
function isPort(v: unknown): v is number {
  return typeof v === 'number' && Number.isInteger(v) && v > 0 && v <= 0xffff;
}
function isBytes(v: unknown): v is Uint8Array {
  return v instanceof Uint8Array && v.byteLength <= MAX_CHUNK;
}

export class NetHost {
  private tcp = new Map<number, net.Socket>();
  private udp = new Map<number, dgram.Socket>();
  private queue: NetEvent[] = [];
  private flushPending = false;
  private resolver = resolverAddress();

  private deliver: (events: NetEvent[]) => void;

  /** `deliver` gets each batch of events, in order. */
  constructor(deliver: (events: NetEvent[]) => void) {
    this.deliver = deliver;
  }

  private emit(e: NetEvent): void {
    this.queue.push(e);
    if (this.flushPending) return;
    this.flushPending = true;
    setImmediate(() => {
      this.flushPending = false;
      const batch = this.queue;
      this.queue = [];
      if (batch.length) this.deliver(batch);
    });
  }

  handle(c: NetCommand): void {
    switch (c.kind) {
      case 'tcp-connect': return this.connect(c.id, c.ip, c.port);
      case 'tcp-send': this.tcp.get(c.id)?.write(c.data); return;
      case 'tcp-shutdown': this.tcp.get(c.id)?.end(); return;
      case 'tcp-close': {
        const s = this.tcp.get(c.id);
        this.tcp.delete(c.id);
        s?.destroy();
        return;
      }
      case 'tcp-pause': this.tcp.get(c.id)?.pause(); return;
      case 'tcp-resume': this.tcp.get(c.id)?.resume(); return;
      case 'udp-send': return this.sendUdp(c.id, c.ip, c.port, c.data);
      case 'reset': return this.reset();
    }
  }

  private connect(id: number, ip: number, port: number): void {
    if (this.tcp.has(id) || this.tcp.size >= MAX_TCP) {
      this.emit({ kind: 'tcp-failed', id });
      return;
    }
    const host = ip === ROUTER_IP ? '127.0.0.1' : ipString(ip);
    const s = net.connect({ host, port });
    s.setNoDelay(true);
    this.tcp.set(id, s);
    let opened = false, ended = false;
    s.on('connect', () => { opened = true; this.emit({ kind: 'tcp-opened', id }); });
    s.on('data', (d: Buffer) => {
      this.emit({ kind: 'tcp-data', id, data: new Uint8Array(d.buffer, d.byteOffset, d.byteLength) });
    });
    s.on('end', () => { ended = true; this.emit({ kind: 'tcp-eof', id }); });
    s.on('error', () => { /* 'close' follows and reports it */ });
    s.on('close', () => {
      if (this.tcp.get(id) !== s) return;          // the guest closed it first
      this.tcp.delete(id);
      if (!opened) this.emit({ kind: 'tcp-failed', id });
      else if (!ended) this.emit({ kind: 'tcp-eof', id });
    });
  }

  private sendUdp(id: number, ip: number, port: number, data: Uint8Array): void {
    let s = this.udp.get(id);
    if (!s) {
      if (this.udp.size >= MAX_UDP) return;
      s = dgram.createSocket('udp4');
      s.on('error', () => { s?.close(); this.udp.delete(id); });
      s.on('message', (msg: Buffer, rinfo: dgram.RemoteInfo) => {
        let from = ipNumber(rinfo.address) ?? 0;
        let fromPort = rinfo.port;
        if (rinfo.address === this.resolver && rinfo.port === 53) { from = DNS_IP; fromPort = 53; }
        else if (rinfo.address === '127.0.0.1') from = ROUTER_IP;
        this.emit({ kind: 'udp-data', id, ip: from, port: fromPort,
                    data: new Uint8Array(msg.buffer, msg.byteOffset, msg.byteLength) });
      });
      this.udp.set(id, s);
    }
    const dnsQuery = ip === DNS_IP && port === 53;
    const host = dnsQuery ? this.resolver : ip === ROUTER_IP ? '127.0.0.1' : ipString(ip);
    s.send(data, port, host);
  }

  reset(): void {
    for (const s of this.tcp.values()) s.destroy();
    for (const s of this.udp.values()) s.close();
    this.tcp.clear();
    this.udp.clear();
    this.queue = [];
    this.resolver = resolverAddress();
  }
}

/** Validates one command off the wire; null drops it. */
export function parse(v: unknown): NetCommand | null {
  if (!v || typeof v !== 'object') return null;
  const c = v as Record<string, unknown>;
  switch (c.kind) {
    case 'tcp-connect':
      return isId(c.id) && isId(c.ip) && isPort(c.port) ? { kind: 'tcp-connect', id: c.id, ip: c.ip, port: c.port } : null;
    case 'tcp-send':
      return isId(c.id) && isBytes(c.data) ? { kind: 'tcp-send', id: c.id, data: c.data } : null;
    case 'tcp-shutdown': case 'tcp-close': case 'tcp-pause': case 'tcp-resume':
      return isId(c.id) ? { kind: c.kind, id: c.id } : null;
    case 'udp-send':
      return isId(c.id) && isId(c.ip) && isPort(c.port) && isBytes(c.data)
        ? { kind: 'udp-send', id: c.id, ip: c.ip, port: c.port, data: c.data } : null;
    case 'reset':
      return { kind: 'reset' };
    default:
      return null;
  }
}

