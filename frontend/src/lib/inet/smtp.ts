// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal SMTP server (RFC 5321 subset) for the simulated internet.
//
// Accepts any greeting, any AUTH, any sender. Captures the message
// body into the mailbox's outbox, so the user can see what the Email
// app actually transmitted. The connection lifecycle:
//
//   S: 220 mail.psion.local ESMTP psion-emu
//   C: EHLO somewhere
//   S: 250-mail.psion.local Hello
//   S: 250 AUTH PLAIN LOGIN
//   C: AUTH LOGIN
//   S: 334 VXNlcm5hbWU6
//   C: <base64 user>
//   S: 334 UGFzc3dvcmQ6
//   C: <base64 pass>
//   S: 235 Authentication successful
//   C: MAIL FROM:<a@b>
//   S: 250 OK
//   C: RCPT TO:<x@y>
//   S: 250 OK
//   C: DATA
//   S: 354 End data with <CR><LF>.<CR><LF>
//   C: (headers + body + CRLF.CRLF)
//   S: 250 OK queued
//   C: QUIT
//   S: 221 Bye
//
// All commands are case-insensitive. Bare-LF line endings are tolerated.

import type { TcpListenerHandler, TcpConn } from './tcp.ts';
import { Mailbox, parseHeaders } from './mailbox.ts';

type SmtpPhase = 'init' | 'greeted' | 'mailFrom' | 'rcptTo' | 'data' | 'done';

interface SmtpSession {
  phase: SmtpPhase;
  buf: string;             // line buffer
  mailFrom: string;
  rcptTo: string[];
  data: string;            // accumulated DATA bytes (de-stuffed)
  awaitingAuthUser: boolean;
  awaitingAuthPass: boolean;
}

export interface SmtpListenerOptions {
  mailbox: Mailbox;
  onMessageReceived?: (id: string) => void;
}

export function createSmtpListener(opts: SmtpListenerOptions): TcpListenerHandler {
  return {
    onConnect(conn: TcpConn): void {
      const session: SmtpSession = {
        phase: 'init', buf: '',
        mailFrom: '', rcptTo: [], data: '',
        awaitingAuthUser: false, awaitingAuthPass: false,
      };
      send(conn, '220 mail.psion.local ESMTP psion-emu\r\n');
      session.phase = 'greeted';

      conn.onData = chunk => {
        // Append decoded bytes to the line buffer; pull off complete lines.
        for (let i = 0; i < chunk.length; i++) session.buf += String.fromCharCode(chunk[i]);
        // In DATA mode, watch for end-of-data marker instead of single lines.
        if (session.phase === 'data') {
          handleDataBytes(conn, session, opts);
          return;
        }
        let nl = -1;
        while ((nl = findNl(session.buf)) >= 0) {
          const line = session.buf.slice(0, nl);
          session.buf = session.buf.slice(nl + (session.buf[nl - 1] === '\r' ? 0 : 0) + 1);
          // Strip a trailing \r if it was part of the CRLF.
          const cleaned = line.endsWith('\r') ? line.slice(0, -1) : line;
          handleLine(conn, session, cleaned);
          // handleLine may have mutated session.phase — read it back
          // through a widening cast so TS doesn't keep the narrowing
          // from the top-of-function check above.
          const after = session.phase as SmtpPhase;
          if (after === 'data') {
            // The buffer may already contain DATA bytes after the
            // DATA command — re-enter DATA-mode parsing.
            handleDataBytes(conn, session, opts);
            return;
          }
          if (after === 'done') return;
        }
      };

      conn.onClose = () => {
        // Peer hung up mid-session — nothing to do.
      };
    },
  };
}

function handleLine(conn: TcpConn, s: SmtpSession, line: string): void {
  // AUTH LOGIN substate: not a standard SMTP verb, just raw base64.
  if (s.awaitingAuthUser) { s.awaitingAuthUser = false; s.awaitingAuthPass = true; send(conn, '334 UGFzc3dvcmQ6\r\n'); return; }
  if (s.awaitingAuthPass) { s.awaitingAuthPass = false; send(conn, '235 Authentication successful\r\n'); return; }

  const upper = line.toUpperCase();
  if (upper.startsWith('HELO ') || upper === 'HELO') {
    send(conn, '250 mail.psion.local Hello\r\n');
    return;
  }
  if (upper.startsWith('EHLO ') || upper === 'EHLO') {
    send(conn,
      '250-mail.psion.local Hello\r\n' +
      '250-AUTH PLAIN LOGIN\r\n' +
      '250-8BITMIME\r\n' +
      '250 SIZE 10485760\r\n');
    return;
  }
  if (upper.startsWith('AUTH LOGIN')) {
    // Server prompts for username (base64 "Username:")
    s.awaitingAuthUser = true;
    send(conn, '334 VXNlcm5hbWU6\r\n');
    return;
  }
  if (upper.startsWith('AUTH PLAIN')) {
    // Single-step PLAIN — optional credentials on the same line.
    send(conn, '235 Authentication successful\r\n');
    return;
  }
  if (upper.startsWith('MAIL FROM')) {
    s.mailFrom = extractAddress(line);
    s.rcptTo = [];
    s.phase = 'mailFrom';
    send(conn, '250 OK\r\n');
    return;
  }
  if (upper.startsWith('RCPT TO')) {
    s.rcptTo.push(extractAddress(line));
    s.phase = 'rcptTo';
    send(conn, '250 OK\r\n');
    return;
  }
  if (upper === 'DATA') {
    if (s.phase !== 'rcptTo') {
      send(conn, '503 Bad sequence: need MAIL/RCPT first\r\n');
      return;
    }
    s.data = '';
    s.phase = 'data';
    send(conn, '354 End data with <CR><LF>.<CR><LF>\r\n');
    return;
  }
  if (upper === 'RSET') {
    s.mailFrom = ''; s.rcptTo = []; s.data = ''; s.phase = 'greeted';
    send(conn, '250 OK\r\n');
    return;
  }
  if (upper === 'NOOP') { send(conn, '250 OK\r\n'); return; }
  if (upper === 'QUIT') {
    s.phase = 'done';
    send(conn, '221 mail.psion.local closing connection\r\n');
    conn.close();
    return;
  }
  // Unknown verb — be polite.
  send(conn, '500 Unrecognised command\r\n');
}

function handleDataBytes(conn: TcpConn, s: SmtpSession, opts: SmtpListenerOptions): void {
  // Search for "\r\n.\r\n" or "\n.\n" terminator in s.buf. Anything
  // before that gets appended to s.data (with dot-stuffing reversal).
  // We use a simple state machine over s.buf because messages may
  // arrive in multiple TCP segments.
  while (s.buf.length > 0) {
    const idxCr = s.buf.indexOf('\r\n.\r\n');
    const idxLf = s.buf.indexOf('\n.\n');
    let term = -1;
    let termLen = 0;
    if (idxCr >= 0 && (idxLf < 0 || idxCr < idxLf)) { term = idxCr; termLen = 5; }
    else if (idxLf >= 0)                            { term = idxLf; termLen = 3; }
    if (term < 0) {
      // No terminator visible yet — consume everything except the
      // last 4 bytes (which might be a partial terminator) into data.
      if (s.buf.length > 4) {
        s.data += unstuff(s.buf.slice(0, s.buf.length - 4));
        s.buf = s.buf.slice(s.buf.length - 4);
      }
      return;
    }
    s.data += unstuff(s.buf.slice(0, term));
    s.buf = s.buf.slice(term + termLen);
    finaliseMessage(conn, s, opts);
    s.phase = 'greeted';
    return;
  }
}

function finaliseMessage(conn: TcpConn, s: SmtpSession, opts: SmtpListenerOptions): void {
  const hdr = parseHeaders(s.data);
  const stored = opts.mailbox.appendOutbox({
    from:    hdr.from    ?? s.mailFrom ?? '(unknown)',
    to:      hdr.to      ?? s.rcptTo,
    subject: hdr.subject ?? '(no subject)',
    date:    hdr.date,
    raw:     s.data,
  });
  opts.onMessageReceived?.(stored.id);
  send(conn, '250 OK message queued\r\n');
  // Don't auto-close — the client may send QUIT next.
}

function send(conn: TcpConn, str: string): void {
  const out = new Uint8Array(str.length);
  for (let i = 0; i < str.length; i++) out[i] = str.charCodeAt(i) & 0xFF;
  conn.send(out);
}

function findNl(buf: string): number {
  for (let i = 0; i < buf.length; i++) if (buf.charCodeAt(i) === 0x0A) return i;
  return -1;
}

function extractAddress(line: string): string {
  // Pull "<...>" or fall back to the bit after ":".
  const angle = line.match(/<([^>]*)>/);
  if (angle) return angle[1];
  const colon = line.indexOf(':');
  return colon >= 0 ? line.slice(colon + 1).trim() : '';
}

// RFC 5321 §4.5.2: lines beginning with "." in DATA are sent as "..".
// Reverse the stuffing here.
function unstuff(s: string): string {
  return s.replace(/(^|\n)\.\./g, '$1.');
}
