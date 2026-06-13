// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Hayes AT command parser + responder.
//
// Scope is "enough to pass ER5's stock dial-up script": echo on/off,
// verbose/numeric result codes, ATZ reset, ATE/Q/V/X family, an
// S-register store, ATDT (the dial), ATA, ATH, ATO, and AT&F/&D/&C/&K.
// Anything we don't recognise still returns OK so the script doesn't
// abort — modems were historically lenient about unknown ATxxx.

export type AtResult =
  | { kind: 'ok' }                   // → "OK"
  | { kind: 'error' }                // → "ERROR"
  | { kind: 'connect'; bps: number } // → "CONNECT <bps>" — switches to data mode
  | { kind: 'noCarrier' }            // → "NO CARRIER"
  | { kind: 'info'; text: string };  // → "<text>" (no trailing OK)

// Parser handles one line of AT input at a time. The dispatcher feeds
// it bytes until CR, then asks for the result.
export class AtCommandParser {
  // Modem state. All defaults match a typical Hayes "factory" config.
  echo: boolean = true;      // ATE0/E1 — echo input back to the host
  verbose: boolean = true;   // ATV0/V1 — text vs numeric result codes
  quiet: boolean = false;    // ATQ0/Q1 — suppress result codes entirely
  resultLevel: number = 4;   // ATX0..X4 — extended result codes
  sregs: Map<number, number> = new Map();

  // Buffer one line of input until we see CR (0x0D). LF is tolerated.
  private buf: string = '';

  // Feed raw RX bytes. Returns any complete commands found (one per CR).
  feed(bytes: Uint8Array): string[] {
    const out: string[] = [];
    for (const b of bytes) {
      if (b === 0x0A) continue;           // LF — ignore
      if (b === 0x08 || b === 0x7F) {     // BS / DEL
        if (this.buf.length > 0) this.buf = this.buf.slice(0, -1);
        continue;
      }
      if (b === 0x0D) {
        if (this.buf.length > 0) out.push(this.buf);
        this.buf = '';
        continue;
      }
      this.buf += String.fromCharCode(b);
    }
    return out;
  }

  // Synchronous execution of a single AT line. Returns a list of
  // results because compound commands (ATE0V1Q0) produce one logical
  // result for the whole line — but ATDT switches modes mid-line, so
  // we return whatever we computed so the dispatcher can act.
  execute(line: string): AtResult {
    const trimmed = line.trim();
    if (trimmed.length === 0) return { kind: 'ok' };
    // The "AT" prefix is mandatory. "A/" repeats the last command but
    // we don't bother — vanishingly rare in dial scripts.
    if (!/^at/i.test(trimmed)) return { kind: 'error' };

    let i = 2; // past "AT"
    const s = trimmed;
    while (i < s.length) {
      const c = s[i].toUpperCase();

      // ATD<modifiers><number> — dial. Consume the rest of the line
      // and report CONNECT. Modifiers (T/P/W/,/;/@) are ignored.
      if (c === 'D') {
        // Skip to end-of-line; the digits are irrelevant to us.
        return { kind: 'connect', bps: 115200 };
      }

      // ATE0 / ATE1
      if (c === 'E') {
        const arg = parseDigit(s, i + 1);
        this.echo = arg !== 0;
        i += 1 + (arg === null ? 0 : 1);
        continue;
      }
      if (c === 'V') { const a = parseDigit(s, i + 1); this.verbose = a !== 0; i += 1 + (a === null ? 0 : 1); continue; }
      if (c === 'Q') { const a = parseDigit(s, i + 1); this.quiet   = a === 1; i += 1 + (a === null ? 0 : 1); continue; }
      if (c === 'X') { const a = parseDigit(s, i + 1); if (a !== null) this.resultLevel = a; i += 1 + (a === null ? 0 : 1); continue; }

      // ATZ — reset to defaults. Treat as factory reset.
      if (c === 'Z') {
        this.echo = true; this.verbose = true; this.quiet = false; this.resultLevel = 4;
        this.sregs.clear();
        // Optional profile digit: ATZ0, ATZ1
        if (i + 1 < s.length && isDigit(s[i + 1])) i++;
        i += 1;
        continue;
      }

      // ATA — answer. We never answer in this design (originate only),
      // but accept it gracefully.
      if (c === 'A') { i += 1; continue; }

      // ATH — hang up. ATH0 / ATH1 both treated the same.
      if (c === 'H') {
        if (i + 1 < s.length && isDigit(s[i + 1])) i++;
        i += 1;
        return { kind: 'noCarrier' };
      }

      // ATO — return to online (data) mode after a +++ escape. Since
      // our dispatcher tracks mode separately, we just return CONNECT.
      if (c === 'O') {
        if (i + 1 < s.length && isDigit(s[i + 1])) i++;
        i += 1;
        return { kind: 'connect', bps: 115200 };
      }

      // ATI / ATI0..9 — identification strings.
      if (c === 'I') {
        if (i + 1 < s.length && isDigit(s[i + 1])) i++;
        i += 1;
        return { kind: 'info', text: 'Psion Emulator Modem' };
      }

      // S-register: ATSn=v or ATSn?
      if (c === 'S') {
        let j = i + 1;
        let nstr = '';
        while (j < s.length && isDigit(s[j])) { nstr += s[j]; j++; }
        if (nstr.length === 0) { i += 1; continue; }
        const n = parseInt(nstr, 10);
        if (s[j] === '=') {
          j++;
          let vstr = '';
          while (j < s.length && isDigit(s[j])) { vstr += s[j]; j++; }
          if (vstr.length > 0) this.sregs.set(n, parseInt(vstr, 10));
          i = j;
          continue;
        }
        if (s[j] === '?') {
          j++;
          const v = this.sregs.get(n) ?? 0;
          i = j;
          // Don't return yet — the line may have more commands. But
          // typical usage is "ATS0?" alone, so we report and stop.
          return { kind: 'info', text: v.toString().padStart(3, '0') };
        }
        i = j;
        continue;
      }

      // AT&F / AT&D / AT&C / AT&K — extended-config commands. We
      // accept and ignore the parameter.
      if (c === '&') {
        if (i + 1 < s.length) {
          i += 2;
          if (i < s.length && isDigit(s[i])) i++;
          continue;
        }
        i += 1; continue;
      }

      // AT+xxx — extended commands. Accept and skip to next semicolon
      // or end-of-line.
      if (c === '+') {
        while (i < s.length && s[i] !== ';') i++;
        if (s[i] === ';') i++;
        continue;
      }

      // Unknown character — skip silently. Real Hayes modems behaved
      // this way for forwards-compat.
      i += 1;
    }
    return { kind: 'ok' };
  }

  // Render an AtResult into the modem-output bytes the host should
  // send back to the device. CRLF wrapping per Hayes spec.
  renderResult(r: AtResult): Uint8Array {
    if (this.quiet) {
      // Even in quiet mode, "info" lines (like ATI's identification)
      // are sent; only the OK/ERROR/CONNECT/NO CARRIER lines are
      // suppressed.
      if (r.kind === 'info') return enc(`\r\n${r.text}\r\n`);
      return new Uint8Array(0);
    }
    if (this.verbose) {
      switch (r.kind) {
        case 'ok':        return enc('\r\nOK\r\n');
        case 'error':     return enc('\r\nERROR\r\n');
        case 'connect':   return enc(`\r\nCONNECT ${r.bps}\r\n`);
        case 'noCarrier': return enc('\r\nNO CARRIER\r\n');
        case 'info':      return enc(`\r\n${r.text}\r\nOK\r\n`);
      }
    }
    // Numeric Hayes codes
    switch (r.kind) {
      case 'ok':        return enc('0\r');
      case 'error':     return enc('4\r');
      case 'connect':   return enc('1\r'); // 1 = CONNECT (generic; speed-specific codes vary by modem)
      case 'noCarrier': return enc('3\r');
      case 'info':      return enc(`${r.text}\r0\r`);
    }
  }
}

function parseDigit(s: string, i: number): number | null {
  if (i >= s.length || !isDigit(s[i])) return null;
  return parseInt(s[i], 10);
}
function isDigit(ch: string): boolean { return ch >= '0' && ch <= '9'; }
function enc(s: string): Uint8Array {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xFF;
  return out;
}
