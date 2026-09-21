// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import { useUartLease } from '../hooks/useUartLease';
import { PrinterCapture } from '../lib/printer/capture';
import { decodePrintJob } from '../lib/printer/decode';
import { textPagesToPdf } from '../lib/printer/pdf';
import { parsePage, extractPageText, renderJobToPdf, type ParsedPage } from '../lib/printer/wprt-render';
import { PlpClient } from '../lib/plp/client-spec';
import { isJobSentinelPage } from '../lib/plp/wprt-spec';
import { trackFeature } from '../lib/analytics';

interface Props {
  controls: EmulatorControls;
  uartIndex: number;       // profile.remoteLinkUart — the cable UART
  sibo: boolean;           // EPOC16 device → SIBO-flavoured serial instructions
  viaPcAvailable: boolean; // EPOC32 Remote Link devices get the WPRT path
  conSeq?: number;         // Req_Con flavour for the link (see RemoteLinkDialog)
  onClose: () => void;
}

// Printer dialog, two capture paths:
//
//  - "Via PC" (default where available): plays PsiWin's print spooler.
//    The device prints with its built-in "Printer via PC" driver — no
//    printer setup needed beyond picking that driver — and we pull the
//    rendered job over the PLP link (LINK Register SYS$WPRT → Connect →
//    WPRT_DATA polls; see lib/plp/wprt-spec.ts) and re-render it to PDF.
//  - "Serial capture": raw byte capture of a serial printer driver
//    (General / Epson / HP), kept for SIBO machines and as a fallback.
//
// Both paths own the cable UART while active, contending with Remote
// Link / Modem exactly like ModemDialog does (attach refused with a
// readable error while the port is held elsewhere).

interface PcJob {
  id: number;
  texts: string[];        // per real page, for preview / .txt
  parsed: ParsedPage[];   // for the positioned PDF
  raw: Uint8Array[];      // raw page primitive streams
  errors: string[];
}

type PcPhase = 'idle' | 'connecting' | 'listening' | 'receiving';

export default function PrinterDialog({ controls, uartIndex, sibo, viaPcAvailable, conSeq, onClose }: Props) {
  // The desktop drive sync holds this port in the background; claiming it
  // here makes it yield for as long as this dialog is open.
  useUartLease(uartIndex, 'printer');

  const [mode, setMode] = useState<'viapc' | 'serial'>(viaPcAvailable ? 'viapc' : 'serial');
  const [errorMsg, setErrorMsg] = useState('');

  // ── Serial capture state ──────────────────────────────────────────
  const [listening, setListening] = useState(false);
  const [captured, setCaptured] = useState<Uint8Array>(new Uint8Array(0));
  const captureRef = useRef<PrinterCapture | null>(null);
  const bufRef = useRef<Uint8Array>(new Uint8Array(0));

  // ── Via-PC state ──────────────────────────────────────────────────
  const [pcPhase, setPcPhase] = useState<PcPhase>('idle');
  const [pcProgress, setPcProgress] = useState<{ bytes: number; pages: number } | null>(null);
  const [pcJobs, setPcJobs] = useState<PcJob[]>([]);
  const plpRef = useRef<PlpClient | null>(null);
  const jobIdRef = useRef(1);

  const stopSerial = useCallback(() => {
    if (captureRef.current) { captureRef.current.stop(); captureRef.current = null; }
    setCaptured(bufRef.current);  // pick up the final drain
    setListening(false);
  }, []);

  const stopViaPc = useCallback(() => {
    const client = plpRef.current;
    plpRef.current = null;
    if (client) {
      client.abortPrintWait();
      client.disconnect();
    }
    if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
    setPcPhase('idle');
    setPcProgress(null);
  }, [controls, uartIndex]);

  // Same StrictMode-safe teardown pattern as ModemDialog: the detach
  // lives in an unmount-only cleanup so a double-mount can't tear down
  // a session the Start handler just stood up.
  useEffect(() => () => {
    if (captureRef.current) { captureRef.current.stop(); captureRef.current = null; }
    const client = plpRef.current;
    plpRef.current = null;
    if (client) { client.abortPrintWait(); client.disconnect(); }
  }, []);

  // ── Serial capture handlers ───────────────────────────────────────
  const handleSerialStart = useCallback(() => {
    setErrorMsg('');
    if (captureRef.current) { captureRef.current.stop(); captureRef.current = null; }
    const capture = new PrinterCapture(
      {
        serialAttachHost: controls.serialAttachHost,
        serialDetachHost: controls.serialDetachHost,
        serialIsAttached: controls.serialIsAttached,
        serialReadBytes:  controls.serialReadBytes,
      },
      uartIndex,
      {
        onData: chunk => {
          const next = new Uint8Array(bufRef.current.length + chunk.length);
          next.set(bufRef.current, 0);
          next.set(chunk, bufRef.current.length);
          bufRef.current = next;
          setCaptured(next);
        },
        onError: msg => setErrorMsg(msg),
      },
    );
    if (capture.start()) {
      captureRef.current = capture;
      setListening(true);
      trackFeature(controls.currentDeviceId, 'printer_capture');
    }
  }, [controls, uartIndex]);

  const handleSerialClear = useCallback(() => {
    bufRef.current = new Uint8Array(0);
    setCaptured(bufRef.current);
  }, []);

  // ── Via-PC handlers ───────────────────────────────────────────────
  const handlePcStart = useCallback(() => {
    setErrorMsg('');
    if (plpRef.current) return;
    if (controls.serialIsAttached(uartIndex)) {
      setErrorMsg('Serial port is in use — disconnect Remote Link / Modem first.');
      return;
    }
    if (!controls.serialAttachHost(uartIndex)) {
      setErrorMsg(`Could not attach host to UART${uartIndex}. The emulator may not be running.`);
      return;
    }
    const client = new PlpClient(
      () => controls.serialReadBytes(uartIndex),
      data => controls.serialWriteBytes(uartIndex, data),
      { conSeq },
    );
    plpRef.current = client;
    client.start();
    setPcPhase('connecting');
    trackFeature(controls.currentDeviceId, 'printer_via_pc');

    void (async () => {
      try {
        await client.connectPrint(20_000);
        // Listen for jobs until stopped; each completed job re-arms.
        for (;;) {
          if (plpRef.current !== client) return;
          setPcPhase('listening');
          setPcProgress(null);
          const job = await client.waitForPrintJob((bytes, pages) => {
            setPcPhase('receiving');
            setPcProgress({ bytes, pages });
          });
          const rawPages = job.pages.filter(p => !isJobSentinelPage(p));
          const parsed = rawPages.map(parsePage);
          setPcJobs(prev => [...prev, {
            id: jobIdRef.current++,
            texts: parsed.map(extractPageText),
            parsed,
            raw: rawPages,
            errors: parsed.flatMap(p => p.errors),
          }]);
        }
      } catch (e) {
        // Stopped by the user (abortPrintWait / disconnect) → quiet.
        if (plpRef.current !== client) return;
        setErrorMsg(e instanceof Error ? e.message : String(e));
        plpRef.current = null;
        client.disconnect();
        if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
        setPcPhase('idle');
        setPcProgress(null);
      }
    })();
  }, [controls, uartIndex, conSeq]);

  const handleClose = useCallback(() => {
    stopSerial();
    stopViaPc();
    onClose();
  }, [stopSerial, stopViaPc, onClose]);

  const switchMode = useCallback((m: 'viapc' | 'serial') => {
    if (m === mode) return;
    setErrorMsg('');
    stopSerial();
    stopViaPc();
    setMode(m);
  }, [mode, stopSerial, stopViaPc]);

  // ── Downloads ─────────────────────────────────────────────────────
  const decoded = useMemo(() => decodePrintJob(captured), [captured]);

  const download = useCallback((data: Uint8Array, ext: string, mime: string) => {
    const url = URL.createObjectURL(new Blob([data as BlobPart], { type: mime }));
    const a = document.createElement('a');
    a.href = url;
    const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').slice(0, 19);
    a.download = `${controls.currentDeviceId ?? 'psion'}-print-${ts}.${ext}`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, [controls.currentDeviceId]);

  const concatRaw = (parts: Uint8Array[]): Uint8Array => {
    let total = 0;
    for (const p of parts) total += p.length;
    const out = new Uint8Array(total);
    let off = 0;
    for (const p of parts) { out.set(p, off); off += p.length; }
    return out;
  };

  // ── Styling (matches ModemDialog) ─────────────────────────────────
  const btn = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';

  return (
    <div className="w-full max-w-3xl max-h-[70vh] bg-psion-dark border border-psion-accent/40 rounded-lg overflow-hidden flex-shrink-0 mx-4 shadow-sm flex flex-col">
      <div className="flex items-center justify-between px-3 py-2 border-b border-psion-accent/40 bg-psion-mid">
        <span className="text-xs font-mono font-semibold text-gray-600">Printer</span>
        <button
          onClick={handleClose}
          className="text-xs font-mono text-gray-400 hover:text-amber-700 transition-colors cursor-pointer"
        >
          Close
        </button>
      </div>

      {/* Mode tabs */}
      {viaPcAvailable && (
        <div className="flex gap-1 px-4 pt-2 border-b border-psion-accent/30 bg-psion-mid/40">
          {([['viapc', 'Via PC'], ['serial', 'Serial capture']] as const).map(([m, label]) => (
            <button
              key={m}
              onClick={() => switchMode(m)}
              className={[
                'px-3 py-1.5 text-xs font-mono rounded-t border-b-0 select-none cursor-pointer',
                mode === m
                  ? 'bg-psion-mid border border-psion-accent/50 text-psion-charcoal'
                  : 'text-gray-500 hover:text-psion-charcoal',
              ].join(' ')}
            >
              {label}
            </button>
          ))}
        </div>
      )}

      {errorMsg && (
        <div className="px-4 py-2 border-b border-psion-accent/30 bg-red-50">
          <pre className="text-[11px] font-mono text-red-700 whitespace-pre-wrap">{errorMsg}</pre>
        </div>
      )}

      {/* ── Via PC mode ── */}
      {mode === 'viapc' && (
        <>
          <div className="flex flex-wrap gap-2 items-center px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/60">
            {pcPhase === 'idle' && (
              <button onClick={handlePcStart} className={btnPrimary}>Start listening</button>
            )}
            {pcPhase === 'connecting' && (
              <span className="text-xs font-mono text-amber-700">● Connecting to the device…</span>
            )}
            {pcPhase === 'listening' && (
              <span className="text-xs font-mono text-green-700">● Listening — print from the device now</span>
            )}
            {pcPhase === 'receiving' && (
              <span className="text-xs font-mono text-green-700">
                ● Receiving job… {pcProgress ? `${pcProgress.bytes} bytes, ${pcProgress.pages} page${pcProgress.pages === 1 ? '' : 's'}` : ''}
              </span>
            )}
            {pcPhase !== 'idle' && (
              <button onClick={stopViaPc} className={btn}>Stop</button>
            )}
          </div>

          {pcJobs.length === 0 && (
            <div className="px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/30 text-[11px] font-mono text-psion-charcoal space-y-1">
              <p className="font-semibold">No printer setup needed on the device — just:</p>
              <ol className="list-decimal pl-5 space-y-0.5">
                <li>Make sure the printer is set to <em>Printer via PC</em> (it's the default: <em>Control Panel → Printer → Print to</em>).</li>
                <li>Click <em>Start listening</em> above.</li>
                <li>Print from any app (<em>File → Printing → Print</em>) — the job appears below, ready to save as PDF or text.</li>
              </ol>
              <p className="pt-1 text-gray-500">
                This speaks PsiWin's print-spooler protocol (SYS$WPRT) over
                the Remote Link cable, so the fonts, positioning and graphics
                of the printed page are preserved in the PDF.
              </p>
            </div>
          )}

          {pcJobs.length > 0 && (
            <div className="flex-grow overflow-auto p-3 space-y-2 border-b border-psion-accent/30">
              {pcJobs.slice().reverse().map(job => (
                <div key={job.id} className="border border-psion-accent/30 rounded bg-psion-mid/20">
                  <div className="flex flex-wrap gap-2 items-center px-2 py-1.5 border-b border-psion-accent/20">
                    <span className="text-[11px] font-mono text-psion-charcoal font-semibold flex-grow">
                      Print job #{job.id} — {job.raw.length} page{job.raw.length === 1 ? '' : 's'}
                      {job.errors.length > 0 ? ` (${job.errors.length} parse warning${job.errors.length === 1 ? '' : 's'})` : ''}
                    </span>
                    <button onClick={() => download(renderJobToPdf(job.parsed), 'pdf', 'application/pdf')}
                            className={btn} title="Positioned rendering with fonts and graphics">Save as PDF</button>
                    <button onClick={() => download(new TextEncoder().encode(job.texts.join('\n\n--- page break ---\n\n')), 'txt', 'text/plain')}
                            className={btn} title="Plain-text extraction">Save as text</button>
                    <button onClick={() => download(concatRaw(job.raw), 'wprt', 'application/octet-stream')}
                            className={btn} title="Raw WPRT primitive stream, for debugging">Save raw</button>
                    <button onClick={() => setPcJobs(prev => prev.filter(j => j.id !== job.id))}
                            className={btn}>Discard</button>
                  </div>
                  <pre className="text-[11px] font-mono bg-white text-psion-charcoal p-3 m-2 rounded shadow-inner whitespace-pre-wrap max-h-48 overflow-auto">
                    {job.texts.map((t, i) => (
                      i === 0 ? t : `\n────────── page break ──────────\n${t}`
                    )).join('') || '(no text on this page — try the PDF)'}
                  </pre>
                </div>
              ))}
            </div>
          )}
        </>
      )}

      {/* ── Serial capture mode ── */}
      {mode === 'serial' && (
        <>
          <div className="flex flex-wrap gap-2 items-center px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/60">
            {!listening ? (
              <button onClick={handleSerialStart} className={btnPrimary}>Start capture</button>
            ) : (
              <>
                <span className="text-xs font-mono text-green-700">● Listening — print from the device now</span>
                <button onClick={stopSerial} className={btn}>Stop</button>
              </>
            )}
            <span className="text-xs font-mono text-gray-500">
              {captured.length > 0
                ? `${captured.length} bytes · ${decoded.pages.length} page${decoded.pages.length === 1 ? '' : 's'}`
                : '(nothing captured yet)'}
            </span>
          </div>

          {captured.length === 0 && (
            <div className="px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/30 text-[11px] font-mono text-psion-charcoal space-y-1">
              <p className="font-semibold">One-time setup on the device:</p>
              {sibo ? (
                <ol className="list-decimal pl-5 space-y-0.5">
                  <li>In the app (e.g. Word), open <em>Special → Print setup</em> (or the System screen's printer settings).</li>
                  <li>Set the printer <em>device</em> to <em>Serial</em> and the printer <em>model</em> to <em>General</em>.</li>
                  <li>Click <em>Start capture</em> above, then print — the output appears below.</li>
                </ol>
              ) : (
                <ol className="list-decimal pl-5 space-y-0.5">
                  <li>Pick the printer driver: in <em>Control Panel → Printer</em> (or the app's <em>File → Printing → Print setup</em>), set <em>Print to</em> to <em>General</em> — the plain-text driver. (The list of drivers varies by device.)</li>
                  <li>Set the connection to the serial port: in the Print / Print setup dialog, set <em>Print via</em> to <em>Serial</em> — not <em>Printer via PC</em>, <em>Infrared</em> or <em>Fax</em>.</li>
                  <li>Click <em>Start capture</em> above, then print from the app.</li>
                </ol>
              )}
              <p className="pt-1 text-gray-500">
                The <em>General</em> driver gives clean text for the .txt / .pdf
                downloads. The other drivers (Canon / Citizen / Epson / HP) also
                work, but their output is graphics and printer codes — save it
                with <em>Save raw</em> (.prn) instead.
              </p>
            </div>
          )}

          {captured.length > 0 && (
            <div className="flex-grow overflow-auto p-3 border-b border-psion-accent/30">
              <pre className="text-[11px] font-mono bg-white text-psion-charcoal p-3 rounded shadow-inner whitespace-pre-wrap min-h-24">
                {decoded.pages.map((p, i) => (
                  i === 0 ? p : `\n────────── page break ──────────\n${p}`
                )).join('')}
              </pre>
            </div>
          )}

          <div className="flex flex-wrap gap-2 items-center px-4 py-3 bg-psion-mid/40">
            <button onClick={() => download(new TextEncoder().encode(decoded.pages.join('\n\n--- page break ---\n\n')), 'txt', 'text/plain')}
                    disabled={captured.length === 0} className={btn}
                    title="Save the decoded text">Save as text</button>
            <button onClick={() => download(textPagesToPdf(decoded.pages.length > 0 ? decoded.pages : ['']), 'pdf', 'application/pdf')}
                    disabled={captured.length === 0} className={btn}
                    title="Save the decoded text as a paginated PDF (Courier, A4)">Save as PDF</button>
            <button onClick={() => download(captured, 'prn', 'application/octet-stream')}
                    disabled={captured.length === 0} className={btn}
                    title="Save the raw captured bytes, escape codes and all">Save raw</button>
            <button onClick={handleSerialClear} disabled={captured.length === 0} className={btn}
                    title="Discard the captured bytes and start fresh">Clear</button>
          </div>
        </>
      )}
    </div>
  );
}
