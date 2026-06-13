// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal POP3 server (RFC 1939 subset) backed by the mailbox's inbox.
//
//   S: +OK psion-emu POP3 ready
//   C: USER alice
//   S: +OK
//   C: PASS bar
//   S: +OK 2 messages
//   C: STAT
//   S: +OK 2 5000
//   C: LIST
//   S: +OK 2 messages
//   S: 1 1000
//   S: 2 4000
//   S: .
//   C: RETR 1
//   S: +OK 1000 octets
//   S: <message body, dot-stuffed>
//   S: .
//   C: DELE 1
//   S: +OK
//   C: QUIT
//   S: +OK; <delete-list flushed to mailbox>
//
// Authentication is accept-all. We snapshot the inbox at USER time so
// message numbers stay stable across DELE/LIST during the session;
// commitments happen at QUIT.

import type { TcpListenerHandler, TcpConn } from './tcp.ts';
import { Mailbox, type StoredMessage } from './mailbox.ts';

interface Pop3Session {
  buf: string;
  state: 'auth' | 'transaction' | 'closed';
  haveUser: boolean;
  messages: StoredMessage[];    // 1-indexed via messages[n-1]
  toDelete: Set<string>;        // ids marked for deletion
}

export interface Pop3ListenerOptions {
  mailbox: Mailbox;
  onMessagesDelivered?: (count: number) => void;
}

export function createPop3Listener(opts: Pop3ListenerOptions): TcpListenerHandler {
  return {
    onConnect(conn: TcpConn): void {
      const s: Pop3Session = {
        buf: '', state: 'auth',
        haveUser: false, messages: [], toDelete: new Set(),
      };
      send(conn, '+OK psion-emu POP3 ready\r\n');

      conn.onData = chunk => {
        for (let i = 0; i < chunk.length; i++) s.buf += String.fromCharCode(chunk[i]);
        let nl = -1;
        while ((nl = s.buf.indexOf('\n')) >= 0) {
          const line = s.buf.slice(0, nl).replace(/\r$/, '');
          s.buf = s.buf.slice(nl + 1);
          handleLine(conn, s, line, opts);
          if (s.state === 'closed') return;
        }
      };

      conn.onClose = () => { /* peer hung up */ };
    },
  };
}

function handleLine(conn: TcpConn, s: Pop3Session, line: string, opts: Pop3ListenerOptions): void {
  const space = line.indexOf(' ');
  const cmd = (space < 0 ? line : line.slice(0, space)).toUpperCase();
  const arg = space < 0 ? '' : line.slice(space + 1).trim();

  if (s.state === 'auth') {
    switch (cmd) {
      case 'USER':
        s.haveUser = true;
        send(conn, '+OK\r\n');
        return;
      case 'PASS':
        if (!s.haveUser) { send(conn, '-ERR USER required first\r\n'); return; }
        s.messages = opts.mailbox.listInbox();
        s.state = 'transaction';
        send(conn, `+OK mailbox has ${s.messages.length} messages\r\n`);
        return;
      case 'APOP':
        s.messages = opts.mailbox.listInbox();
        s.state = 'transaction';
        send(conn, `+OK mailbox has ${s.messages.length} messages\r\n`);
        return;
      case 'CAPA':
        send(conn, '+OK\r\nUSER\r\nUIDL\r\n.\r\n');
        return;
      case 'QUIT':
        s.state = 'closed';
        send(conn, '+OK bye\r\n');
        conn.close();
        return;
      default:
        send(conn, '-ERR command not allowed in AUTHORIZATION state\r\n');
        return;
    }
  }

  // TRANSACTION state.
  switch (cmd) {
    case 'STAT': {
      const active = s.messages.filter(m => !s.toDelete.has(m.id));
      const octets = active.reduce((n, m) => n + m.raw.length, 0);
      send(conn, `+OK ${active.length} ${octets}\r\n`);
      return;
    }
    case 'LIST': {
      if (arg) {
        const n = parseInt(arg, 10);
        const m = s.messages[n - 1];
        if (!m || s.toDelete.has(m.id)) { send(conn, '-ERR no such message\r\n'); return; }
        send(conn, `+OK ${n} ${m.raw.length}\r\n`);
        return;
      }
      let out = '+OK\r\n';
      s.messages.forEach((m, i) => {
        if (!s.toDelete.has(m.id)) out += `${i + 1} ${m.raw.length}\r\n`;
      });
      out += '.\r\n';
      send(conn, out);
      return;
    }
    case 'UIDL': {
      if (arg) {
        const n = parseInt(arg, 10);
        const m = s.messages[n - 1];
        if (!m || s.toDelete.has(m.id)) { send(conn, '-ERR no such message\r\n'); return; }
        send(conn, `+OK ${n} ${m.id}\r\n`);
        return;
      }
      let out = '+OK\r\n';
      s.messages.forEach((m, i) => {
        if (!s.toDelete.has(m.id)) out += `${i + 1} ${m.id}\r\n`;
      });
      out += '.\r\n';
      send(conn, out);
      return;
    }
    case 'RETR': {
      const n = parseInt(arg, 10);
      const m = s.messages[n - 1];
      if (!m || s.toDelete.has(m.id)) { send(conn, '-ERR no such message\r\n'); return; }
      send(conn, `+OK ${m.raw.length} octets\r\n`);
      send(conn, stuff(m.raw));
      // The terminating "." must be on its own line. If raw ends with
      // \r\n that's fine; otherwise insert one.
      const tail = m.raw.endsWith('\r\n') ? '.\r\n' : '\r\n.\r\n';
      send(conn, tail);
      return;
    }
    case 'TOP': {
      // TOP <msg> <n-lines> — like RETR but only headers + first N body lines.
      const parts = arg.split(/\s+/);
      const n = parseInt(parts[0], 10);
      const lines = parseInt(parts[1], 10);
      const m = s.messages[n - 1];
      if (!m || s.toDelete.has(m.id) || isNaN(lines)) { send(conn, '-ERR\r\n'); return; }
      const headerEnd = (() => {
        const a = m.raw.indexOf('\r\n\r\n');
        const b = m.raw.indexOf('\n\n');
        if (a >= 0 && (b < 0 || a < b)) return a + 4;
        if (b >= 0) return b + 2;
        return m.raw.length;
      })();
      const headers = m.raw.slice(0, headerEnd);
      const body = m.raw.slice(headerEnd);
      const bodyLines = body.split(/\r?\n/).slice(0, lines).join('\r\n');
      send(conn, '+OK\r\n');
      send(conn, stuff(headers + bodyLines));
      send(conn, '\r\n.\r\n');
      return;
    }
    case 'DELE': {
      const n = parseInt(arg, 10);
      const m = s.messages[n - 1];
      if (!m) { send(conn, '-ERR no such message\r\n'); return; }
      s.toDelete.add(m.id);
      send(conn, `+OK message ${n} marked for deletion\r\n`);
      return;
    }
    case 'RSET':
      s.toDelete.clear();
      send(conn, '+OK\r\n');
      return;
    case 'NOOP':
      send(conn, '+OK\r\n');
      return;
    case 'QUIT': {
      const count = s.toDelete.size;
      opts.mailbox.deleteInboxBulk(s.toDelete);
      if (count > 0) opts.onMessagesDelivered?.(count);
      s.state = 'closed';
      send(conn, '+OK bye\r\n');
      conn.close();
      return;
    }
    default:
      send(conn, '-ERR Unrecognised command\r\n');
      return;
  }
}

function send(conn: TcpConn, str: string): void {
  const out = new Uint8Array(str.length);
  for (let i = 0; i < str.length; i++) out[i] = str.charCodeAt(i) & 0xFF;
  conn.send(out);
}

// RFC 1939 §3.5 dot-stuffing — lines starting with "." are sent as "..".
function stuff(s: string): string {
  return s.replace(/(^|\n)\./g, '$1..');
}
