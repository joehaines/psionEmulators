// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The Psion LX Wi-Fi's socket host (src/main/netHost.ts): the main-process
// half of the emulated router. Drives it against a TCP echo server and a
// UDP echo server on loopback — which the guest knows as 10.0.2.2 — and
// checks the event stream the emulator would receive: opened, data in
// order, end of stream, a refused connection, datagrams with the router as
// their source, and validation of whatever the renderer sends.
//
// netHost.ts imports nothing from electron, which is what lets this run
// under plain node.
//
// Run: node --experimental-strip-types desktop/test/net.test.mts

import * as dgram from 'node:dgram';
import * as net from 'node:net';
import type { NetEvent } from '../src/ipc/contract.ts';
import { NetHost, parse } from '../src/main/netHost.ts';

const ROUTER = 0x0a000202;
let failures = 0;
function check(cond: unknown, msg: string) {
  console.log(`${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
}

const events: NetEvent[] = [];
const host = new NetHost((batch) => events.push(...batch));
async function waitFor(pred: () => boolean, ms = 3000): Promise<boolean> {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    if (pred()) return true;
    await new Promise((r) => setTimeout(r, 10));
  }
  return pred();
}

// A TCP echo server that closes after the client half-closes.
const tcp = net.createServer((s) => {
  s.on('data', (d) => s.write(d));
  s.on('end', () => s.end());
});
await new Promise<void>((r) => tcp.listen(0, '127.0.0.1', r));
const tcpPort = (tcp.address() as net.AddressInfo).port;

host.handle({ kind: 'tcp-connect', id: 7, ip: ROUTER, port: tcpPort });
check(await waitFor(() => events.some((e) => e.kind === 'tcp-opened' && e.id === 7)),
      '10.0.2.2 connects to this machine\'s loopback');
host.handle({ kind: 'tcp-send', id: 7, data: new TextEncoder().encode('hello ') });
host.handle({ kind: 'tcp-send', id: 7, data: new TextEncoder().encode('psion') });
const got = () => new TextDecoder().decode(new Uint8Array(events.flatMap((e) =>
  e.kind === 'tcp-data' && e.id === 7 ? [...e.data] : [])));
check(await waitFor(() => got() === 'hello psion'), 'bytes come back in order');
host.handle({ kind: 'tcp-shutdown', id: 7 });
check(await waitFor(() => events.some((e) => e.kind === 'tcp-eof' && e.id === 7)),
      'a half-close is answered with end of stream');
check(events.filter((e) => e.kind === 'tcp-eof' && e.id === 7).length === 1, 'end of stream is reported once');

// A port nothing listens on.
const closed = net.createServer();
await new Promise<void>((r) => closed.listen(0, '127.0.0.1', r));
const closedPort = (closed.address() as net.AddressInfo).port;
await new Promise<void>((r) => closed.close(() => r()));
host.handle({ kind: 'tcp-connect', id: 8, ip: ROUTER, port: closedPort });
check(await waitFor(() => events.some((e) => e.kind === 'tcp-failed' && e.id === 8)),
      'a refused connection reports tcp-failed');

// UDP: an echo server on loopback; replies come from the router.
const udp = dgram.createSocket('udp4');
udp.on('message', (m, r) => udp.send(m, r.port, r.address));
await new Promise<void>((r) => udp.bind(0, '127.0.0.1', () => r()));
const udpPort = udp.address().port;
host.handle({ kind: 'udp-send', id: 1234, ip: ROUTER, port: udpPort, data: new Uint8Array([1, 2, 3]) });
check(await waitFor(() => events.some((e) => e.kind === 'udp-data')), 'a datagram is answered');
const reply = events.find((e) => e.kind === 'udp-data');
check(reply && reply.kind === 'udp-data' && reply.id === 1234 && reply.ip === ROUTER && reply.port === udpPort
      && reply.data.length === 3 && reply.data[2] === 3,
      'the reply arrives on the guest\'s port, from 10.0.2.2, unaltered');

// Validation of what crosses the IPC boundary.
check(parse({ kind: 'tcp-connect', id: 1, ip: ROUTER, port: 0 }) === null, 'port 0 is refused');
check(parse({ kind: 'tcp-connect', id: -1, ip: ROUTER, port: 80 }) === null, 'a negative id is refused');
check(parse({ kind: 'tcp-send', id: 1, data: 'not bytes' }) === null, 'non-binary data is refused');
check(parse({ kind: 'bogus' }) === null, 'unknown commands are refused');
check(parse({ kind: 'udp-send', id: 1, ip: ROUTER, port: 53, data: new Uint8Array(4) }) !== null, 'a good command passes');

host.reset();
tcp.close();
udp.close();
if (failures) { console.error(`\n${failures} check(s) failed`); process.exit(1); }
console.log('\nnet host ok');
