// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useRef, useEffect, useCallback } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import { useUartLease } from '../hooks/useUartLease';
import { IrSendClient, IrReceiveClient, IrNotListeningError, IrReceiveCanceledError } from '../lib/irda';
import { IrSiboSendClient, IrSiboBeamError } from '../lib/irda/sibosend';
import { setSerialPumpEnabled } from '../lib/wasmBridge';

interface Props {
  controls: EmulatorControls;
  // SoC UART the device's IrDA port is wired to. Windermere (5mx /
  // 5mxpro / mc218 / Revo), the CL-PS7111 (Osaris) and the CL-PS7110
  // (Series 5) drive infrared through UART1 in SIR mode; the SA-1100
  // (Series 7 / netBook) uses its ICP on UART2; SIBO2 devices (Series
  // 3c / Siena) share the Condor bridge at uartIndex 0.
  uartIndex: number;
  // Beam dialect: 'eikon' (EPOC32 Eikon-IR, default) or 'sibo' (the
  // EPOC16 "Psion IRLink" beam — Series 3c / Siena). From
  // DeviceProfile.irProtocol.
  protocol?: 'eikon' | 'sibo';
  onClose: () => void;
}

// Destination path for a host file beamed to a SIBO device. The
// System screen only lists files inside the per-app directories, so
// route by extension (defaulting to the Word column) and clamp to the
// EPOC16 8.3 character set.
function siboDestPath(hostName: string): string {
  const dot = hostName.lastIndexOf('.');
  let base = (dot > 0 ? hostName.slice(0, dot) : hostName)
    .toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 8) || 'BEAMED';
  let ext = (dot > 0 ? hostName.slice(dot + 1) : 'WRD')
    .toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 3) || 'WRD';
  const dirByExt: Record<string, string> = {
    WRD: 'WRD', DBF: 'DAT', AGN: 'AGN', SPR: 'SPR', WLD: 'WLD',
    OPL: 'OPL', WVE: 'WVE',
  };
  const dir = dirByExt[ext] ?? 'WRD';
  return `M:\\${dir}\\${base}.${ext}`;
}

type Phase = 'idle' | 'sending' | 'receiving' | 'done' | 'error';

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

// Trigger a browser download of `bytes` as `filename` — the way the device's
// beamed file is handed back to the user. Mirrors the CF/Remote-Link panels.
function downloadBlob(bytes: Uint8Array, filename: string): void {
  const buf = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buf).set(bytes);
  const url = URL.createObjectURL(new Blob([buf], { type: 'application/octet-stream' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = filename || 'infrared-file';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// Infrared (IrDA) transfers, inline panel. Two directions:
//   • Send    — beam a host file to the device (device in Infrared receive).
//   • Receive — wait for the device to beam a file to us (device in Infrared
//               send); the file is offered as a browser download on arrival.
// Both run the full EPOC Eikon-IR stack; only the Series 7 needs the emulated-
// cycle pump during the one-way body phase (see notes inline).
export default function InfraredDialog({ controls, uartIndex, protocol, onClose }: Props) {
  // The desktop drive sync holds this port in the background; claiming it
  // here makes it yield for as long as this dialog is open.
  useUartLease(uartIndex, 'infrared');

  const sibo = protocol === 'sibo';
  const [phase, setPhase] = useState<Phase>('idle');
  const [status, setStatus] = useState<string>('');
  const [error, setError] = useState<string>('');
  const [progress, setProgress] = useState<number>(0);
  const [total, setTotal] = useState<number>(0);
  const [fileName, setFileName] = useState<string>('');
  const clientRef = useRef<IrSendClient | IrReceiveClient | IrSiboSendClient | null>(null);
  const canceledRef = useRef<boolean>(false);
  // Reject of the in-flight send/receive race. Cancel calls this so the await
  // settles IMMEDIATELY, instead of limping on until an internal wall-clock
  // protocol timeout (discover/connect/IAS use setTimeout, and stop() freezes
  // the tick loop that would otherwise drive them) — which is why the Cancel
  // button used to appear to do nothing for several seconds.
  const cancelRef = useRef<(() => void) | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Only the Series 7 needs the emulated-cycle pump during the body phase (it
  // runs fast enough to starve on rAF timing); it actively breaks the slower
  // devices' handshakes, so gate on it. Same rule as the original send dialog.
  const pumpCycles = controls.currentDeviceId === 'series7';

  // Tear down the client + host bridge on unmount so a stale infrared session
  // never lingers across panel open/close. MUST be unmount-only (empty deps,
  // matching RemoteLinkDialog): `controls` is a fresh object every parent
  // render, so listing it as a dep made this cleanup fire on ANY re-render of
  // the app mid-transfer (a log line or status push was enough) — stopping the
  // client and detaching the UART under a live beam. That was the "infrared
  // sometimes just dies" flakiness. Latest props are read through refs.
  const teardownRef = useRef({ controls, uartIndex, pumpCycles });
  teardownRef.current = { controls, uartIndex, pumpCycles };
  useEffect(() => () => {
    const t = teardownRef.current;
    if (t.pumpCycles) setSerialPumpEnabled(false);
    if (clientRef.current) {
      clientRef.current.stop();
      clientRef.current = null;
    }
    if (t.controls.serialIsAttached(t.uartIndex)) t.controls.serialDetachHost(t.uartIndex);
  }, []);

  // ── Send: beam a host file to the device ──────────────────────────────
  const handleSend = useCallback(async (file: File) => {
    setError('');
    setStatus('');
    setProgress(0);
    setFileName(file.name);
    setTotal(file.size);
    canceledRef.current = false;

    if (!controls.serialIsAttached(uartIndex) && !controls.serialAttachHost(uartIndex)) {
      setPhase('error');
      setError('Could not access the infrared port. Make sure the emulator is running.');
      return;
    }

    const bytes = new Uint8Array(await file.arrayBuffer());
    if (sibo) {
      // EPOC16 "Psion IRLink" beam. The device must be armed first:
      // Infrared screen (Psion+Tab on the 3c) → Receive.
      const client = new IrSiboSendClient(
        () => controls.serialReadBytes(uartIndex),
        (data) => controls.serialWriteBytes(uartIndex, data),
        // Per-ROM stream framing, pinned from each device's own sender
        // (see docs/sibo-remote-link.md): the Siena uses a 132-byte
        // header, the 3c-family 136 bytes.
        controls.currentDeviceId === 'siena'
          ? { headerLen: 132 }
          : { headerLen: 136 },
      );
      clientRef.current = client;
      setPhase('sending');
      const dest = siboDestPath(file.name);
      try {
        setStatus('Looking for the Psion…');
        await client.sendFile(dest, bytes, {
          onPhase: p => { if (p === 'transfer') setStatus(`Sending ${file.name} → ${dest}…`); },
          onProgress: n => setProgress(n),
        });
        setPhase('done');
        setStatus(`Sent ${file.name} to ${dest} (${formatBytes(bytes.length)})`);
      } catch (e) {
        if (canceledRef.current) {
          setPhase('idle');
          setStatus('');
        } else {
          setPhase('error');
          setError(e instanceof IrSiboBeamError ? e.message
            : (e instanceof Error ? e.message : String(e)));
        }
      } finally {
        clientRef.current = null;
        if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
      }
      return;
    }
    const client = new IrSendClient(
      () => controls.serialReadBytes(uartIndex),
      (data) => controls.serialWriteBytes(uartIndex, data),
      { pollHz: 100 },   // 10 ms polls: keeps host replies inside IrLAP slot windows over the worker bridge
    );
    client.start();
    clientRef.current = client;

    setPhase('sending');
    // Race the transfer against a cancel signal so the Cancel button settles
    // the await at once (see cancelRef). The losing branch's late rejection is
    // swallowed so it never surfaces as an unhandled rejection.
    const cancelPromise = new Promise<never>((_, reject) => {
      cancelRef.current = () => reject(new Error('Infrared transfer canceled'));
    });
    try {
      const op = (async () => {
        setStatus('Looking for an infrared device…');
        await client.discoverAndConnect();
        // Series 7 ONLY: pump emulated cycles during the data + completion phase
        // so the device processes each request inside the host poll tick instead
        // of waiting for the next requestAnimationFrame. Other devices complete
        // unpumped (pumping starves their multi-round-trip handshakes).
        if (pumpCycles) setSerialPumpEnabled(true);
        setStatus(`Sending ${file.name}…`);
        await client.sendFile(file.name, bytes, (n) => setProgress(n));
      })();
      void op.catch(() => {});
      await Promise.race([op, cancelPromise]);
      setPhase('done');
      setStatus(`Sent ${file.name} (${formatBytes(bytes.length)})`);
    } catch (e) {
      if (canceledRef.current) {
        setPhase('idle');
        setStatus('');
      } else if (e instanceof IrNotListeningError) {
        setPhase('error');
        setError(
          'No infrared device responding. On the Psion, open the System screen ' +
            'and choose Infrared → Receive, then try sending again.',
        );
      } else {
        setPhase('error');
        setError(e instanceof Error ? e.message : String(e));
      }
    } finally {
      cancelRef.current = null;
      if (pumpCycles) setSerialPumpEnabled(false);
      client.stop();
      clientRef.current = null;
      if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
    }
  }, [controls, uartIndex, pumpCycles, sibo]);

  // ── Receive: accept a file the device beams to us ─────────────────────
  const handleReceive = useCallback(async () => {
    setError('');
    setStatus('');
    setProgress(0);
    setTotal(0);
    setFileName('');
    canceledRef.current = false;

    if (!controls.serialIsAttached(uartIndex) && !controls.serialAttachHost(uartIndex)) {
      setPhase('error');
      setError('Could not access the infrared port. Make sure the emulator is running.');
      return;
    }

    const client = new IrReceiveClient(
      () => controls.serialReadBytes(uartIndex),
      (data) => controls.serialWriteBytes(uartIndex, data),
      { pollHz: 100 },   // 10 ms polls: keeps host replies inside IrLAP slot windows over the worker bridge
    );
    client.start();
    clientRef.current = client;

    setPhase('receiving');
    setStatus(sibo
      ? 'Waiting — on the Psion, highlight a file, press Psion+Tab and choose Send…'
      : 'Waiting for the device to beam a file…');
    const cancelPromise = new Promise<never>((_, reject) => {
      cancelRef.current = () => reject(new Error('Infrared transfer canceled'));
    });
    try {
      const op = client.receive({
        onFileInfo: (name, size) => {
          setFileName(name);
          setTotal(size);
          setProgress(0);
          setStatus(`Receiving ${name}…`);
          // Series 7 ONLY: pump cycles now the one-way body phase has begun, so
          // the device streams the body inside the host poll tick (matches the
          // send path's gating). Other devices stream unpumped.
          if (pumpCycles) setSerialPumpEnabled(true);
        },
        onProgress: (n) => setProgress(n),
      });
      void op.catch(() => {});
      const received = await Promise.race([op, cancelPromise]);
      downloadBlob(received.data, received.name);
      setPhase('done');
      setStatus(`Received ${received.name} (${formatBytes(received.data.length)})`);
    } catch (e) {
      if (e instanceof IrReceiveCanceledError || canceledRef.current) {
        setPhase('idle');
        setStatus('');
      } else {
        setPhase('error');
        setError(e instanceof Error ? e.message : String(e));
      }
    } finally {
      cancelRef.current = null;
      if (pumpCycles) setSerialPumpEnabled(false);
      client.stop();
      clientRef.current = null;
      if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
    }
  }, [controls, uartIndex, pumpCycles, sibo]);

  // ── Cancel an in-progress transfer ────────────────────────────────────
  const handleCancel = useCallback(() => {
    canceledRef.current = true;
    // Settle the in-flight await immediately (don't wait for an internal
    // protocol timeout); the handler's catch sees canceledRef and goes idle,
    // and its finally stops the client + drops the host bridge.
    cancelRef.current?.();
    const client = clientRef.current;
    if (client instanceof IrReceiveClient) {
      // Politely settle the receive; the handler's finally drops the bridge,
      // which the device's beam sees as a lost link and aborts.
      client.cancel();
    } else if (client instanceof IrSiboSendClient) {
      client.cancel();
    } else if (client) {
      // Sending: stopping the link aborts the in-flight beam (its awaited
      // promise rejects and is swallowed via canceledRef).
      client.stop();
    }
  }, []);

  const busy = phase === 'sending' || phase === 'receiving';
  const triggerSend = useCallback(() => fileInputRef.current?.click(), []);

  // Press one of the Siena's dedicated IR keys (matrix COL1/COL2 bit
  // 0x100 → EpocKey PageUp = IR Send, PageDown = IR Receive — see
  // core/series3c.cpp). Held for ~600 ms: the kernel samples those
  // columns on its 32 Hz tick, so the 2-frame pulse pressEpocKey sends
  // can fall between scans (the harness scripts hold them 32 frames
  // for the same reason).
  const pressSienaIrKey = useCallback((code: number) => {
    controls.sendEpocKey(code, true);
    window.setTimeout(() => controls.sendEpocKey(code, false), 600);
  }, [controls]);

  // Inline-panel styling, matching the Remote Link / CF Card panels.
  const btn =
    'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const btnPrimary =
    'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';

  const showBar = busy && total > 0;
  const pct = total > 0 ? Math.min(100, Math.round((progress / total) * 100)) : 0;

  return (
    <div className="w-full max-w-3xl bg-psion-dark border border-psion-accent/40 rounded-lg overflow-hidden flex-shrink-0 mx-4 shadow-sm flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-3 border-b border-psion-accent/40 bg-psion-mid">
        <h2 className="text-sm font-mono font-semibold text-psion-charcoal">Infrared</h2>
        <button onClick={onClose} disabled={busy} className={btn}>Close</button>
      </div>

      {/* Action bar: Send / Receive (idle) or Cancel (busy). */}
      <div className="flex flex-wrap gap-2 items-center px-4 py-3 bg-psion-mid/60">
        <button onClick={triggerSend} disabled={busy} className={btnPrimary}
                title={sibo
                  ? 'On the Psion: open the Infrared screen (Psion+Tab) and choose Receive, then pick a file here'
                  : 'On the Psion: open Infrared receive, then pick a file here'}>Send</button>
        <button onClick={() => void handleReceive()} disabled={busy} className={btnPrimary}
                title={sibo
                  ? 'On the Psion: highlight a non-running file, press Psion+Tab and choose Send'
                  : 'On the Psion: select a file and beam it via Infrared send'}>Receive</button>
        {sibo && !busy && controls.currentDeviceId === 'siena' && (
          <>
            <button onClick={() => pressSienaIrKey(10)} className={btn}
                    title="Press the Siena's dedicated IR Send key — opens its Infrared send screen">
              IR Send key</button>
            <button onClick={() => pressSienaIrKey(11)} className={btn}
                    title="Press the Siena's dedicated IR Receive key — arms its Infrared receive screen">
              IR Receive key</button>
          </>
        )}
        {sibo && !busy && (
          <span className="text-[10px] font-mono text-psion-charcoal/60">
            {controls.currentDeviceId === 'siena'
              ? 'The IR keys open the Infrared screens on the Siena'
              : 'Psion+Tab opens the Infrared screen on the device'}
          </span>
        )}
        {busy && (
          <button onClick={handleCancel} className={btn}>Cancel</button>
        )}
        <input
          ref={fileInputRef}
          type="file"
          style={{ display: 'none' }}
          onChange={(e) => {
            const f = e.target.files?.[0];
            if (f) void handleSend(f);
            e.target.value = '';
          }}
        />
      </div>

      {/* Inline progress bar — shown once a transfer with a known size is in
          flight (the receive header arrives a moment after "Receive"). */}
      {(showBar || status || error) && (
        <div className="px-4 py-3 border-t border-psion-accent/30 bg-psion-mid/30 flex flex-col gap-2">
          {showBar && (
            <div className="w-full">
              <div className="h-2 bg-psion-dark/60 rounded overflow-hidden border border-psion-accent/30">
                <div
                  className="h-full bg-psion-accent transition-all"
                  style={{ width: `${pct}%` }}
                />
              </div>
              <div className="text-xs text-psion-charcoal/70 mt-1 font-mono">
                {fileName ? `${fileName} — ` : ''}{formatBytes(progress)} / {formatBytes(total)}
              </div>
            </div>
          )}

          {status && !error && (
            <div className={`text-xs font-mono ${phase === 'done' ? 'text-green-700' : 'text-psion-charcoal/80'}`}>
              {status}
            </div>
          )}

          {error && (
            <div className="text-xs font-mono text-red-700 bg-red-50 border border-red-200 rounded px-3 py-2">
              {error}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
