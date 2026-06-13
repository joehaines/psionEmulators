// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA receive end-to-end test: drive the REAL IrSendClient (the host beaming
// a file to a device) straight into the REAL IrReceiveClient (the host
// receiving a file the device beamed). Both are production stacks — IrLAP,
// IrLMP, IAS, Tiny TP and the EPOC Eikon-IR protocol — wired back-to-back over
// two byte queues. If they interoperate here, the receive responder speaks the
// exact wire format our verified sender expects (and, by construction, the
// format the real EPOC device's sender produces: the sender is validated
// against captured device bytes in irda.e2e.test.mts).
//
// In this pairing the IrSendClient plays the role of the beaming EPOC device
// (IrLAP primary) and the IrReceiveClient plays the host that accepts the file.
//
// Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/irda.receive.e2e.test.mts

import { IrSendClient } from '../irda/index.ts';
import { IrReceiveClient, IrReceiveCanceledError } from '../irda/index.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) {
    console.error(`FAIL: ${msg}`);
    failures++;
  }
}

// ── A bidirectional byte pipe between the two stacks ──────────────────
// "sender" is the IrSendClient (primary / device role); "receiver" is the
// IrReceiveClient (host accepting the file).
function makePair(receiverCfg = {}, senderCfg = {}) {
  const toReceiver: number[] = []; // bytes sender → receiver
  const toSender: number[] = []; // bytes receiver → sender

  const drain = (q: number[]): Uint8Array => {
    if (q.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(q);
    q.length = 0;
    return out;
  };

  const receiver = new IrReceiveClient(
    () => drain(toReceiver),
    (data) => {
      for (let i = 0; i < data.length; i++) toSender.push(data[i]);
      return data.length;
    },
    { pollHz: 1000, ...receiverCfg },
  );
  const sender = new IrSendClient(
    () => drain(toSender),
    (data) => {
      for (let i = 0; i < data.length; i++) toReceiver.push(data[i]);
      return data.length;
    },
    { pollHz: 1000, discoveryTimeoutMs: 1000, requestTimeoutMs: 4000, completionTimeoutMs: 2000, ...senderCfg },
  );
  return { receiver, sender };
}

// A pipe whose receiver→sender direction can be told to DROP the next N whole
// frames, modelling the half-duplex line losing (or the device missing in its
// turnaround window) a response we sent. Each IrTransport.sendFrame is exactly
// one writeBytes call, so a per-call counter drops whole frames cleanly. Used to
// prove the responder's stop-and-wait retransmission recovers a lost reply
// instead of stranding the handshake ("found but never sends").
function makeLossyPair(receiverCfg = {}, senderCfg = {}) {
  const toReceiver: number[] = [];
  const toSender: number[] = [];
  const ctl = { dropReceiverFrames: 0, dropped: 0 };

  const drain = (q: number[]): Uint8Array => {
    if (q.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(q);
    q.length = 0;
    return out;
  };

  const receiver = new IrReceiveClient(
    () => drain(toReceiver),
    (data) => {
      if (ctl.dropReceiverFrames > 0) {
        ctl.dropReceiverFrames--;
        ctl.dropped++;
        return data.length; // swallow the frame; pretend it went out
      }
      for (let i = 0; i < data.length; i++) toSender.push(data[i]);
      return data.length;
    },
    { pollHz: 1000, ...receiverCfg },
  );
  const sender = new IrSendClient(
    () => drain(toSender),
    (data) => {
      for (let i = 0; i < data.length; i++) toReceiver.push(data[i]);
      return data.length;
    },
    { pollHz: 1000, discoveryTimeoutMs: 1000, requestTimeoutMs: 6000, completionTimeoutMs: 6000, ...senderCfg },
  );
  return { receiver, sender, ctl };
}

function makePayload(n: number): Uint8Array {
  return new Uint8Array(n).map((_, i) => (i * 31 + 7) & 0xff);
}

async function main() {
  // 1. Happy path: the device beams a 1 234-byte file (spans many Tiny TP
  //    segments) and the host receives every byte intact, with the name and
  //    progress callbacks firing.
  {
    const { receiver, sender } = makePair();
    receiver.start();
    sender.start();
    const payload = makePayload(1234);

    let infoName = '';
    let infoSize = -1;
    let lastProgress = -1;
    const recvPromise = receiver.receive({
      onFileInfo: (name, size) => {
        infoName = name;
        infoSize = size;
      },
      onProgress: (n) => {
        lastProgress = n;
      },
    });

    let received: { name: string; data: Uint8Array } | null = null;
    try {
      await sender.discoverAndConnect();
      await sender.sendFile('Report.wrd', payload);
      received = await recvPromise;
    } finally {
      sender.stop();
      receiver.stop();
    }

    check(received !== null, 'host received a file');
    if (received) {
      check(received.name === 'Report.wrd', `received name = "${received.name}" (want Report.wrd)`);
      check(received.data.length === payload.length, `received length ${received.data.length} (want ${payload.length})`);
      let same = received.data.length === payload.length;
      for (let i = 0; same && i < payload.length; i++) same = received.data[i] === payload[i];
      check(same, 'received bytes match the beamed bytes');
    }
    check(infoName === 'Report.wrd', `onFileInfo name = "${infoName}"`);
    check(infoSize === payload.length, `onFileInfo size = ${infoSize} (want ${payload.length})`);
    check(lastProgress === payload.length, `final progress = ${lastProgress} (want ${payload.length})`);
  }

  // 2. Empty file still delivers (numToRecv 0 completes the moment FILE is
  //    ACKed; the host then gracefully disconnects).
  {
    const { receiver, sender } = makePair();
    receiver.start();
    sender.start();
    const recvPromise = receiver.receive();
    let received: { name: string; data: Uint8Array } | null = null;
    try {
      await sender.discoverAndConnect();
      await sender.sendFile('Empty.txt', new Uint8Array(0));
      received = await recvPromise;
    } finally {
      sender.stop();
      receiver.stop();
    }
    check(received !== null && received.data.length === 0, 'empty file delivered with zero-length body');
    check(received?.name === 'Empty.txt', 'empty file name preserved');
  }

  // 3. A file name containing spaces survives (the receiver parses everything
  //    after the 4th space as the name).
  {
    const { receiver, sender } = makePair();
    receiver.start();
    sender.start();
    const recvPromise = receiver.receive();
    const payload = makePayload(64);
    let received: { name: string; data: Uint8Array } | null = null;
    try {
      await sender.discoverAndConnect();
      await sender.sendFile('My Holiday Notes.txt', payload);
      received = await recvPromise;
    } finally {
      sender.stop();
      receiver.stop();
    }
    check(received?.name === 'My Holiday Notes.txt', `name with spaces preserved (got "${received?.name}")`);
    check(received?.data.length === payload.length, 'spaced-name file body length matches');
  }

  // 4. Device not beaming (receiver never put into receive mode) → the device's
  //    discovery finds nobody. We model "the host is not listening" by NOT
  //    calling receive(): the IrReceiveClient ignores XID, so the sender's
  //    discovery times out exactly as it would against a silent peer.
  {
    const { receiver, sender } = makePair({}, { discoveryTimeoutMs: 200 });
    receiver.start();
    sender.start();
    let err: unknown = null;
    try {
      await sender.discoverAndConnect();
    } catch (e) {
      err = e;
    } finally {
      sender.stop();
      receiver.stop();
    }
    check(err !== null && /not|listening|responding/i.test(String((err as Error)?.message ?? err)),
      `silent host (not in receive mode) fails discovery (got ${err})`);
  }

  // 5b. Lossy link: a response we send gets dropped during the handshake AND
  //     during the body. Pre-hardening this stranded the transfer ("another
  //     device was found but it never started to send the file") because the
  //     responder answered the device's re-poll with a bare RR that re-delivered
  //     nothing. With secondary-side stop-and-wait ARQ the device's re-poll
  //     draws a retransmission of the exact reply and the file still arrives.
  {
    const { receiver, sender, ctl } = makeLossyPair();
    receiver.start();
    sender.start();
    const payload = makePayload(300); // a few body segments
    let received: { name: string; data: Uint8Array } | null = null;
    // Listen first (so discovery sees us), then once the FILE header lands drop
    // one mid-body reply (a credit grant) to exercise body-phase recovery too.
    const recvPromise = receiver.receive({
      onFileInfo: () => {
        ctl.dropReceiverFrames = 1;
      },
    });
    try {
      await sender.discoverAndConnect();
      // The link is up; the very next reply we send is the IAS GetValueByClass
      // response. Drop it — exactly the "found + connected but never sends" hole.
      ctl.dropReceiverFrames = 1;
      await sender.sendFile('Lossy.txt', payload);
      received = await recvPromise;
    } finally {
      sender.stop();
      receiver.stop();
    }
    check(ctl.dropped >= 2, `lossy pipe actually dropped responses (dropped ${ctl.dropped})`);
    check(received !== null, 'file received despite dropped responses');
    let same = received != null && received.data.length === payload.length;
    for (let i = 0; same && i < payload.length; i++) same = received!.data[i] === payload[i];
    check(same, 'recovered file bytes match the beamed bytes');
    check(received?.name === 'Lossy.txt', 'recovered file name preserved');
  }

  // 5. Cancel before any device beams: receive() rejects with the cancel error.
  {
    const { receiver } = makePair();
    receiver.start();
    const recvPromise = receiver.receive();
    receiver.cancel();
    let err: unknown = null;
    try {
      await recvPromise;
    } catch (e) {
      err = e;
    } finally {
      receiver.stop();
    }
    check(err instanceof IrReceiveCanceledError, `cancel rejects with IrReceiveCanceledError (got ${err})`);
  }
}

main()
  .then(() => {
    if (failures > 0) {
      console.error(`\n${failures} test failure(s)`);
      process.exit(1);
    }
    console.log('OK — all IrDA receive end-to-end tests passed');
    process.exit(0);
  })
  .catch((e) => {
    console.error('FAIL: unexpected error', e);
    process.exit(1);
  });
