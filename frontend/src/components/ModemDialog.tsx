// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useEffect, useRef, useCallback } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import { useUartLease } from '../hooks/useUartLease';
import { ModemClient, type LinkPhase, type HttpEvent, type MailEvent } from '../lib/modem/client';
import type { DispatcherMode } from '../lib/modem/dispatcher';
import { getBrowserMailbox, type StoredMessage } from '../lib/inet/mailbox';

interface Props {
  controls: EmulatorControls;
  onClose: () => void;
}

// Simulated internet for the Psion 5mx family. Connect button attaches
// the host bridge to UART2, then a Hayes AT modem + PPP server
// negotiates LCP/PAP/IPCP so the device's Comms wizard sees a real
// dial-up connection. Phase 1 only takes the link as far as IPCP
// Opened and answers ICMP echoes — TCP/HTTP/SMTP/POP3 land in
// subsequent phases.
//
// Cable contention with Remote Link: this dialog only appears for
// Windermere devices that are NOT the Revo (the only device that
// currently surfaces Remote Link in EmulatorView). The two paths
// therefore never compete for UART2 at the UI level. If a future
// change exposes both buttons on the same device, the
// `controls.serialIsAttached(2)` precheck below still refuses the
// attach cleanly.

const UART_INDEX = 2;

function hexLine(bytes: Uint8Array, offset: number): string {
  const hex: string[] = [];
  const ascii: string[] = [];
  const end = Math.min(offset + 16, bytes.length);
  for (let i = offset; i < end; i++) {
    const b = bytes[i];
    hex.push(b.toString(16).padStart(2, '0'));
    ascii.push(b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : '.');
  }
  while (hex.length < 16) hex.push('  ');
  return `${offset.toString(16).padStart(6, '0')}  ${hex.join(' ')}  |${ascii.join('')}|`;
}
function formatHex(bytes: Uint8Array): string {
  if (bytes.length === 0) return '';
  const lines: string[] = [];
  for (let i = 0; i < bytes.length; i += 16) lines.push(hexLine(bytes, i));
  return lines.join('\n');
}

function pppName(p: number): string {
  switch (p) {
    case 0xC021: return 'LCP';
    case 0xC023: return 'PAP';
    case 0xC223: return 'CHAP';
    case 0x8021: return 'IPCP';
    case 0x0021: return 'IP';
    default: return `0x${p.toString(16).padStart(4, '0')}`;
  }
}

function ipProtoName(p: number): string {
  switch (p) {
    case 1:  return 'ICMP';
    case 6:  return 'TCP';
    case 17: return 'UDP';
    default: return `0x${p.toString(16).padStart(2, '0')}`;
  }
}

function ipStr(b: Uint8Array): string {
  return `${b[0]}.${b[1]}.${b[2]}.${b[3]}`;
}

interface AtTraceLine { index: number; line: string; result: string; }
interface PppTraceLine { index: number; dir: 'rx' | 'tx'; proto: string; len: number; preview: string; }
interface IpTraceLine  { index: number; dir: 'rx' | 'tx'; src: string; dst: string; proto: string; len: number; }
interface HttpTraceLine { index: number; summary: string; }

export default function ModemDialog({ controls, onClose }: Props) {
  // The desktop drive sync holds this port in the background; claiming it
  // here makes it yield for as long as this dialog is open.
  useUartLease(UART_INDEX, 'modem');

  // ── Connection state ─────────────────────────────────────────────
  const [phase, setPhase] = useState<LinkPhase>('idle');
  const [errorMsg, setErrorMsg] = useState<string>('');
  const clientRef = useRef<ModemClient | null>(null);

  // ── Diagnostics ──────────────────────────────────────────────────
  const [showDiag, setShowDiag] = useState<boolean>(false);
  const [rxLog, setRxLog] = useState<Uint8Array>(new Uint8Array(0));
  const [txLog, setTxLog] = useState<Uint8Array>(new Uint8Array(0));
  const rxBufRef = useRef<Uint8Array>(new Uint8Array(0));
  const txBufRef = useRef<Uint8Array>(new Uint8Array(0));
  const [atTrace, setAtTrace] = useState<AtTraceLine[]>([]);
  const [pppTrace, setPppTrace] = useState<PppTraceLine[]>([]);
  const [ipTrace,  setIpTrace]  = useState<IpTraceLine[]>([]);
  const [httpTrace, setHttpTrace] = useState<HttpTraceLine[]>([]);
  const [mode, setMode] = useState<DispatcherMode>('command');
  const atIdxRef   = useRef<number>(0);
  const pppIdxRef  = useRef<number>(0);
  const ipIdxRef   = useRef<number>(0);
  const httpIdxRef = useRef<number>(0);

  // ── Mailbox (visible regardless of connection state so users can
  //    seed inbox messages or inspect captured outbox before dialling).
  const [tab, setTab] = useState<'status' | 'outbox' | 'inbox'>('status');
  const [outbox, setOutbox] = useState<StoredMessage[]>([]);
  const [inbox,  setInbox]  = useState<StoredMessage[]>([]);
  const [mailToast, setMailToast] = useState<string>('');
  const [showCompose, setShowCompose] = useState<boolean>(false);
  const [composeFrom, setComposeFrom] = useState<string>('alice@example.com');
  const [composeTo,   setComposeTo]   = useState<string>('user@psion.local');
  const [composeSubj, setComposeSubj] = useState<string>('Hello from the simulated internet');
  const [composeBody, setComposeBody] = useState<string>('This is a test message you can retrieve via POP3.\n\n— alice');
  useEffect(() => {
    const mb = getBrowserMailbox();
    const refresh = () => { setOutbox(mb.listOutbox()); setInbox(mb.listInbox()); };
    refresh();
    const unsub = mb.onChange(refresh);
    return unsub;
  }, []);

  // Single source of truth for the Modem dialog's own session; the
  // detach side-effect deliberately doesn't live in a useEffect
  // cleanup so StrictMode's double-mount can't tear down a bridge
  // the Connect handler just stood up.
  useEffect(() => () => {
    if (clientRef.current) { clientRef.current.stop(); clientRef.current = null; }
  }, []);

  const appendRingBuffer = (prev: Uint8Array, chunk: Uint8Array, cap: number) => {
    if (prev.length + chunk.length <= cap) {
      const out = new Uint8Array(prev.length + chunk.length);
      out.set(prev, 0); out.set(chunk, prev.length);
      return out;
    }
    const keep = Math.max(0, cap - chunk.length);
    const out = new Uint8Array(Math.min(cap, keep + chunk.length));
    out.set(prev.subarray(prev.length - keep), 0);
    out.set(chunk, keep);
    return out;
  };

  const handleConnect = useCallback(() => {
    setErrorMsg('');
    if (clientRef.current) { clientRef.current.stop(); clientRef.current = null; }

    // Reset diagnostics.
    rxBufRef.current = new Uint8Array(0); setRxLog(rxBufRef.current);
    txBufRef.current = new Uint8Array(0); setTxLog(txBufRef.current);
    atIdxRef.current = 0; setAtTrace([]);
    pppIdxRef.current = 0; setPppTrace([]);
    ipIdxRef.current = 0; setIpTrace([]);
    httpIdxRef.current = 0; setHttpTrace([]);

    if (controls.serialIsAttached(UART_INDEX)) {
      setErrorMsg('Cable in use by Remote Link — disconnect there first.');
      return;
    }

    const client = new ModemClient({
      uartIndex: UART_INDEX,
      bridge: {
        serialAttachHost: controls.serialAttachHost,
        serialDetachHost: controls.serialDetachHost,
        serialIsAttached: controls.serialIsAttached,
        serialReadBytes:  controls.serialReadBytes,
        serialWriteBytes: controls.serialWriteBytes,
      },
      onPhaseChange: p => setPhase(p),
      onRawRx: chunk => {
        rxBufRef.current = appendRingBuffer(rxBufRef.current, chunk, 64 * 1024);
        setRxLog(rxBufRef.current);
      },
      onRawTx: chunk => {
        txBufRef.current = appendRingBuffer(txBufRef.current, chunk, 64 * 1024);
        setTxLog(txBufRef.current);
      },
      onAtLine: (line, result) => {
        const idx = atIdxRef.current++;
        const r = (() => {
          switch (result.kind) {
            case 'ok':        return 'OK';
            case 'error':     return 'ERROR';
            case 'connect':   return `CONNECT ${result.bps}`;
            case 'noCarrier': return 'NO CARRIER';
            case 'info':      return result.text;
          }
        })();
        setAtTrace(prev => {
          const next = [...prev, { index: idx, line, result: r }];
          return next.length > 80 ? next.slice(next.length - 80) : next;
        });
      },
      onModeChange: m => setMode(m),
      onPppFrame: (proto, info, dir) => {
        const idx = pppIdxRef.current++;
        const preview = Array.from(info.subarray(0, Math.min(24, info.length)))
          .map(b => b.toString(16).padStart(2, '0')).join(' ');
        setPppTrace(prev => {
          const next = [...prev, { index: idx, dir, proto: pppName(proto), len: info.length, preview }];
          return next.length > 80 ? next.slice(next.length - 80) : next;
        });
      },
      onIpPacket: (proto, src, dst, len, dir) => {
        const idx = ipIdxRef.current++;
        setIpTrace(prev => {
          const next = [...prev, { index: idx, dir, src: ipStr(src), dst: ipStr(dst), proto: ipProtoName(proto), len }];
          return next.length > 80 ? next.slice(next.length - 80) : next;
        });
      },
      onMail: (ev: MailEvent) => {
        const msg = ev.kind === 'smtp-received'
          ? 'Outbox: device sent a new message'
          : `Inbox: device retrieved ${ev.count} message${ev.count === 1 ? '' : 's'}`;
        setMailToast(msg);
        // Auto-dismiss after 4 s — guarded so a stale timer can't
        // clear a fresher message.
        const myMsg = msg;
        window.setTimeout(() => setMailToast(prev => prev === myMsg ? '' : prev), 4000);
      },
      onHttp: (kind, ev: HttpEvent) => {
        const idx = httpIdxRef.current++;
        const summary = kind === 'request'
          ? `→ ${ev.method ?? '?'} http://${ev.host}${ev.path ?? ''} [${ev.source}]`
          : `← ${ev.status ?? '?'}  ${ev.length ?? 0} B  ${ev.host}`;
        setHttpTrace(prev => {
          const next = [...prev, { index: idx, summary }];
          return next.length > 60 ? next.slice(next.length - 60) : next;
        });
      },
      onError: msg => setErrorMsg(msg),
    });
    clientRef.current = client;
    if (!client.start()) {
      clientRef.current = null;
    }
  }, [controls]);

  const handleDisconnect = useCallback(() => {
    if (clientRef.current) { clientRef.current.stop(); clientRef.current = null; }
    setPhase('idle');
    setMode('command');
    setErrorMsg('');
  }, []);

  const handleClose = useCallback(() => {
    if (clientRef.current) { clientRef.current.stop(); clientRef.current = null; }
    onClose();
  }, [onClose]);

  // ── Styling ─────────────────────────────────────────────────────
  const btn = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';

  const phaseLabel = (p: LinkPhase): string => {
    switch (p) {
      case 'idle':            return 'Idle';
      case 'attaching':       return 'Attaching…';
      case 'command':         return 'AT command mode (waiting for dial)';
      case 'lcp':             return 'Negotiating LCP…';
      case 'authenticating':  return 'Authenticating (PAP)…';
      case 'ipcp':            return 'Negotiating IPCP…';
      case 'online':          return 'Online (PPP up)';
      case 'hung-up':         return 'Hung up';
    }
  };

  return (
    <div className="w-full max-w-3xl max-h-[70vh] bg-psion-dark border border-psion-accent/40 rounded-lg overflow-hidden flex-shrink-0 mx-4 shadow-sm flex flex-col">
        <div className="flex items-center justify-between px-3 py-2 border-b border-psion-accent/40 bg-psion-mid">
          <span className="text-xs font-mono font-semibold text-gray-600">Modem (Simulated Internet)</span>
          <button
            onClick={handleClose}
            className="text-xs font-mono text-gray-400 hover:text-amber-700 transition-colors cursor-pointer"
          >
            Close
          </button>
        </div>

        <div className="flex flex-wrap gap-2 items-center px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/60">
          {phase === 'idle' && (
            <button onClick={handleConnect} className={btnPrimary}>Connect</button>
          )}
          {phase === 'attaching' && (
            <button disabled className={btnPrimary}>Attaching…</button>
          )}
          {phase !== 'idle' && phase !== 'attaching' && (
            <>
              <span className={`text-xs font-mono ${phase === 'online' ? 'text-green-700' : 'text-amber-700'}`}>
                ● {phaseLabel(phase)}
              </span>
              <button onClick={handleDisconnect} className={btn}>Disconnect</button>
              <span className="text-xs font-mono text-gray-500">Wire mode: {mode}</span>
            </>
          )}
        </div>

        {errorMsg && (
          <div className="px-4 py-2 border-b border-psion-accent/30 bg-red-50">
            <pre className="text-[11px] font-mono text-red-700 whitespace-pre-wrap">{errorMsg}</pre>
          </div>
        )}

        {mailToast && (
          <div className="px-4 py-2 border-b border-psion-accent/30 bg-emerald-50">
            <pre className="text-[11px] font-mono text-emerald-700 whitespace-pre-wrap">{mailToast}</pre>
          </div>
        )}

        {/* Tab bar — Status / Outbox / Inbox */}
        <div className="flex gap-1 px-4 pt-2 border-b border-psion-accent/30 bg-psion-mid/40">
          {(['status', 'outbox', 'inbox'] as const).map(t => {
            const count = t === 'outbox' ? outbox.length : t === 'inbox' ? inbox.length : null;
            return (
              <button
                key={t}
                onClick={() => setTab(t)}
                className={[
                  'px-3 py-1.5 text-xs font-mono rounded-t border-b-0 select-none cursor-pointer',
                  tab === t
                    ? 'bg-psion-mid border border-psion-accent/50 text-psion-charcoal'
                    : 'text-gray-500 hover:text-psion-charcoal',
                ].join(' ')}
              >
                {t === 'status' ? 'Status' : t === 'outbox' ? 'Outbox' : 'Inbox'}
                {count !== null && count > 0 && (
                  <span className="ml-1 inline-block px-1.5 rounded bg-psion-accent/20">{count}</span>
                )}
              </button>
            );
          })}
        </div>

        {/* Tab body */}
        {tab === 'status' && phase === 'idle' && (
          <div className="px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/30 text-[11px] font-mono text-psion-charcoal space-y-1">
            <p className="font-semibold">One-time setup on the device:</p>
            <ol className="list-decimal pl-5 space-y-0.5">
              <li>Control Panel → Modems → New: type <em>Standard / Hayes</em>, port <em>Serial 0</em>, speed 115200, flow control None, init string empty.</li>
              <li>Control Panel → Internet → New: connection <em>Standard dial-up</em>, phone any (ignored), user/pass anything, auth <em>PAP</em>, IP <em>Get automatically</em>, DNS <em>Get automatically</em>.</li>
              <li>Control Panel → Email → New POP3 account: incoming and outgoing both <code>mail.psion.local</code>, username/password anything.</li>
              <li>Click <em>Connect</em> above, then on the device choose your new Internet account and dial.</li>
            </ol>
            <p className="pt-1 text-gray-500">
              Once connected, the Web app can browse
              <code> http://canned.psion.local/</code> (curated) and
              any HTTP URL <code>window.fetch()</code> can reach
              (CORS-permissive). The Email app talks SMTP to the
              capture-only outbox below and POP3 to the inbox.
            </p>
          </div>
        )}

        {tab === 'outbox' && (
          <div className="flex-grow overflow-auto p-3 space-y-2 border-b border-psion-accent/30">
            {outbox.length === 0 ? (
              <p className="text-[11px] font-mono text-gray-500 italic px-2 py-1">
                (No outgoing mail captured yet. Connect, configure an SMTP account
                on the device pointing at <code>mail.psion.local</code>, then send
                a message — it will appear here.)
              </p>
            ) : (
              outbox.slice().reverse().map(m => (
                <MessageCard key={m.id} m={m} onDelete={() => getBrowserMailbox().deleteOutbox(m.id)} />
              ))
            )}
          </div>
        )}

        {tab === 'inbox' && (
          <div className="flex-grow overflow-auto p-3 space-y-2 border-b border-psion-accent/30">
            <div className="flex gap-2 items-center pb-2 border-b border-psion-accent/20">
              <button onClick={() => setShowCompose(s => !s)} className={btn}>
                {showCompose ? 'Cancel' : 'Inject test message…'}
              </button>
              <span className="text-[11px] font-mono text-gray-500">
                Messages here are delivered to the device via POP3.
              </span>
            </div>

            {showCompose && (
              <div className="space-y-1 p-2 border border-psion-accent/30 rounded bg-psion-mid/30">
                <label className="block text-[11px] font-mono">
                  From <input type="text" value={composeFrom} onChange={e => setComposeFrom(e.target.value)}
                    className="block w-full mt-0.5 text-[11px] font-mono bg-white border border-psion-accent/40 rounded px-1 py-0.5" />
                </label>
                <label className="block text-[11px] font-mono">
                  To <input type="text" value={composeTo} onChange={e => setComposeTo(e.target.value)}
                    className="block w-full mt-0.5 text-[11px] font-mono bg-white border border-psion-accent/40 rounded px-1 py-0.5" />
                </label>
                <label className="block text-[11px] font-mono">
                  Subject <input type="text" value={composeSubj} onChange={e => setComposeSubj(e.target.value)}
                    className="block w-full mt-0.5 text-[11px] font-mono bg-white border border-psion-accent/40 rounded px-1 py-0.5" />
                </label>
                <label className="block text-[11px] font-mono">
                  Body
                  <textarea value={composeBody} onChange={e => setComposeBody(e.target.value)}
                    rows={4}
                    className="block w-full mt-0.5 text-[11px] font-mono bg-white border border-psion-accent/40 rounded px-1 py-0.5" />
                </label>
                <button
                  onClick={() => {
                    const now = new Date();
                    const dateStr = now.toUTCString();
                    const raw =
                      `From: ${composeFrom}\r\n` +
                      `To: ${composeTo}\r\n` +
                      `Subject: ${composeSubj}\r\n` +
                      `Date: ${dateStr}\r\n` +
                      `Content-Type: text/plain; charset=utf-8\r\n` +
                      `\r\n` +
                      composeBody.replace(/\r?\n/g, '\r\n');
                    getBrowserMailbox().appendInbox({
                      from: composeFrom,
                      to:   [composeTo],
                      subject: composeSubj,
                      raw,
                    });
                    setShowCompose(false);
                  }}
                  className={btnPrimary}
                >
                  Add to inbox
                </button>
              </div>
            )}

            {inbox.length === 0 ? (
              <p className="text-[11px] font-mono text-gray-500 italic px-2 py-1">
                (No messages in inbox. Inject one above, then have the device
                check mail.)
              </p>
            ) : (
              inbox.slice().reverse().map(m => (
                <MessageCard key={m.id} m={m} onDelete={() => getBrowserMailbox().deleteInbox(m.id)} />
              ))
            )}
          </div>
        )}

        {/* Diagnostics */}
        <details
          className="border-t border-psion-accent/30 bg-psion-mid/20 flex-grow overflow-auto"
          open={showDiag}
          onToggle={e => setShowDiag((e.target as HTMLDetailsElement).open)}
        >
          <summary className="px-4 py-2 text-xs font-mono text-gray-500 cursor-pointer select-none">
            Diagnostics
          </summary>
          <div className="px-4 py-3 space-y-3">
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">RX from device ({rxLog.length})</div>
              <pre className="text-[11px] font-mono bg-black/40 text-green-300 p-2 rounded h-20 overflow-auto whitespace-pre">
                {rxLog.length === 0 ? '(no bytes yet)' : formatHex(rxLog)}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">TX to device ({txLog.length})</div>
              <pre className="text-[11px] font-mono bg-black/40 text-amber-200 p-2 rounded h-20 overflow-auto whitespace-pre">
                {txLog.length === 0 ? '(no bytes sent yet)' : formatHex(txLog)}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">AT trace ({atTrace.length})</div>
              <pre className="text-[11px] font-mono bg-black/40 text-cyan-200 p-2 rounded h-24 overflow-auto whitespace-pre">
                {atTrace.length === 0
                  ? '(no AT exchanges yet)'
                  : atTrace.map(l => `#${l.index.toString().padStart(3, '0')}  → ${l.line}\n      ← ${l.result}`).join('\n')}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">PPP frames ({pppTrace.length})</div>
              <pre className="text-[11px] font-mono bg-black/40 text-pink-200 p-2 rounded h-24 overflow-auto whitespace-pre">
                {pppTrace.length === 0
                  ? '(no PPP frames yet)'
                  : pppTrace.map(l => `#${l.index.toString().padStart(3, '0')}  ${l.dir === 'rx' ? '←' : '→'} ${l.proto.padEnd(5)} len=${l.len.toString().padStart(4)}  ${l.preview}`).join('\n')}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">IP packets ({ipTrace.length})</div>
              <pre className="text-[11px] font-mono bg-black/40 text-blue-200 p-2 rounded h-24 overflow-auto whitespace-pre">
                {ipTrace.length === 0
                  ? '(no IP packets yet)'
                  : ipTrace.map(l => `#${l.index.toString().padStart(3, '0')}  ${l.dir === 'rx' ? '←' : '→'} ${l.proto.padEnd(5)} ${l.src.padEnd(15)} → ${l.dst.padEnd(15)} len=${l.len}`).join('\n')}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">HTTP ({httpTrace.length})</div>
              <pre className="text-[11px] font-mono bg-black/40 text-orange-200 p-2 rounded h-24 overflow-auto whitespace-pre">
                {httpTrace.length === 0
                  ? '(no HTTP traffic yet)'
                  : httpTrace.map(l => `#${l.index.toString().padStart(3, '0')}  ${l.summary}`).join('\n')}
              </pre>
            </div>
          </div>
        </details>
    </div>
  );
}

function MessageCard({ m, onDelete }: { m: StoredMessage; onDelete: () => void }) {
  const [open, setOpen] = useState(false);
  const date = new Date(m.date).toLocaleString();
  return (
    <div className="border border-psion-accent/30 rounded bg-psion-mid/20 text-[11px] font-mono">
      <button
        onClick={() => setOpen(o => !o)}
        className="w-full text-left px-2 py-1.5 flex items-center gap-2 hover:bg-psion-accent/10"
      >
        <span className="text-psion-charcoal flex-grow truncate">
          <b>{m.subject || '(no subject)'}</b>
          <span className="text-gray-500"> — {m.from || '(unknown)'} → {m.to.join(', ') || '(unknown)'}</span>
        </span>
        <span className="text-gray-400 whitespace-nowrap">{date}</span>
        <span className="text-gray-400">{open ? '▾' : '▸'}</span>
      </button>
      {open && (
        <div className="border-t border-psion-accent/30 px-2 py-1.5 space-y-1">
          <pre className="bg-black/40 text-green-200 p-2 rounded max-h-64 overflow-auto whitespace-pre-wrap break-all">
            {m.raw}
          </pre>
          <div className="flex justify-end">
            <button
              onClick={onDelete}
              className="text-[11px] font-mono text-red-700 hover:underline cursor-pointer"
            >
              Delete
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
