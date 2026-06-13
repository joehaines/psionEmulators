// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Persistent mail store for the simulated SMTP / POP3 servers.
//
// Two queues — outbox (messages the device sent, captured by the SMTP
// server) and inbox (messages the user injected, served to the device
// by the POP3 server). Persists to IndexedDB so messages survive page
// reloads; in-memory cache makes reads synchronous which keeps the
// SMTP/POP3 server code simple.
//
// Tests inject the in-memory backend directly.

export interface StoredMessage {
  id: string;
  from: string;
  to: string[];
  subject: string;
  date: number;     // unix ms
  raw: string;      // full RFC 822 message body (including headers)
}

export interface NewMessage {
  from: string;
  to: string[];
  subject: string;
  date?: number;
  raw: string;
}

export interface MailStorageBackend {
  load(): Promise<{ outbox: StoredMessage[]; inbox: StoredMessage[] }>;
  saveOutbox(msgs: StoredMessage[]): Promise<void>;
  saveInbox(msgs: StoredMessage[]): Promise<void>;
}

export class MemoryBackend implements MailStorageBackend {
  private outbox: StoredMessage[] = [];
  private inbox: StoredMessage[] = [];
  async load() { return { outbox: [...this.outbox], inbox: [...this.inbox] }; }
  async saveOutbox(m: StoredMessage[]) { this.outbox = [...m]; }
  async saveInbox(m: StoredMessage[])  { this.inbox  = [...m]; }
}

export class IndexedDbBackend implements MailStorageBackend {
  private static readonly DB_NAME = 'psion-modem-mail';
  private static readonly STORE   = 'queues';
  private dbPromise: Promise<IDBDatabase> | null = null;

  private getDb(): Promise<IDBDatabase> {
    if (this.dbPromise) return this.dbPromise;
    this.dbPromise = new Promise((resolve, reject) => {
      const req = indexedDB.open(IndexedDbBackend.DB_NAME, 1);
      req.onupgradeneeded = () => {
        req.result.createObjectStore(IndexedDbBackend.STORE);
      };
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error ?? new Error('IndexedDB open failed'));
    });
    return this.dbPromise;
  }

  async load(): Promise<{ outbox: StoredMessage[]; inbox: StoredMessage[] }> {
    const db = await this.getDb();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(IndexedDbBackend.STORE, 'readonly');
      const store = tx.objectStore(IndexedDbBackend.STORE);
      const out = store.get('outbox');
      const inb = store.get('inbox');
      tx.oncomplete = () => resolve({
        outbox: (out.result as StoredMessage[] | undefined) ?? [],
        inbox:  (inb.result as StoredMessage[] | undefined) ?? [],
      });
      tx.onerror = () => reject(tx.error ?? new Error('IndexedDB load failed'));
    });
  }

  async saveOutbox(msgs: StoredMessage[]): Promise<void> {
    return this.saveKey('outbox', msgs);
  }
  async saveInbox(msgs: StoredMessage[]): Promise<void> {
    return this.saveKey('inbox', msgs);
  }

  private async saveKey(key: 'outbox' | 'inbox', msgs: StoredMessage[]): Promise<void> {
    const db = await this.getDb();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(IndexedDbBackend.STORE, 'readwrite');
      tx.objectStore(IndexedDbBackend.STORE).put(msgs, key);
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(tx.error ?? new Error('IndexedDB save failed'));
    });
  }
}

export class Mailbox {
  private outbox: StoredMessage[] = [];
  private inbox: StoredMessage[] = [];
  private readonly backend: MailStorageBackend;
  private listeners: Set<() => void> = new Set();
  private loaded: boolean = false;

  constructor(backend: MailStorageBackend) {
    this.backend = backend;
  }

  async init(): Promise<void> {
    if (this.loaded) return;
    try {
      const data = await this.backend.load();
      this.outbox = data.outbox;
      this.inbox  = data.inbox;
    } catch {
      // First-run with no DB yet — fall through with empty queues.
    }
    this.loaded = true;
    this.emit();
  }

  listOutbox(): StoredMessage[] { return [...this.outbox]; }
  listInbox():  StoredMessage[] { return [...this.inbox]; }

  appendOutbox(msg: NewMessage): StoredMessage {
    const stored = this.normalise(msg);
    this.outbox.push(stored);
    void this.backend.saveOutbox(this.outbox);
    this.emit();
    return stored;
  }

  appendInbox(msg: NewMessage): StoredMessage {
    const stored = this.normalise(msg);
    this.inbox.push(stored);
    void this.backend.saveInbox(this.inbox);
    this.emit();
    return stored;
  }

  deleteOutbox(id: string): void {
    const before = this.outbox.length;
    this.outbox = this.outbox.filter(m => m.id !== id);
    if (this.outbox.length !== before) {
      void this.backend.saveOutbox(this.outbox);
      this.emit();
    }
  }

  deleteInbox(id: string): void {
    const before = this.inbox.length;
    this.inbox = this.inbox.filter(m => m.id !== id);
    if (this.inbox.length !== before) {
      void this.backend.saveInbox(this.inbox);
      this.emit();
    }
  }

  // POP3 commits its DELE list at QUIT time; do it in one save.
  deleteInboxBulk(ids: Set<string>): void {
    if (ids.size === 0) return;
    const before = this.inbox.length;
    this.inbox = this.inbox.filter(m => !ids.has(m.id));
    if (this.inbox.length !== before) {
      void this.backend.saveInbox(this.inbox);
      this.emit();
    }
  }

  onChange(cb: () => void): () => void {
    this.listeners.add(cb);
    return () => this.listeners.delete(cb);
  }

  private normalise(m: NewMessage): StoredMessage {
    return {
      id: makeId(),
      from: m.from,
      to: [...m.to],
      subject: m.subject,
      date: m.date ?? Date.now(),
      raw: m.raw,
    };
  }

  private emit(): void {
    for (const l of this.listeners) {
      try { l(); } catch { /* listener errors are not our concern */ }
    }
  }
}

let browserMailbox: Mailbox | null = null;

// Browser-side singleton — survives across dialog open/close cycles
// and across modem connect/disconnect.
export function getBrowserMailbox(): Mailbox {
  if (browserMailbox) return browserMailbox;
  const hasIdb = typeof indexedDB !== 'undefined';
  const backend = hasIdb ? new IndexedDbBackend() : new MemoryBackend();
  browserMailbox = new Mailbox(backend);
  void browserMailbox.init();
  return browserMailbox;
}

function makeId(): string {
  return `${Date.now().toString(36)}-${Math.floor(Math.random() * 0xFFFFFF).toString(16).padStart(6, '0')}`;
}

// Parse a raw RFC 822 message body to pull out the headers we display
// in the mailbox UI. Tolerates either CRLF or LF, ignores headers we
// don't care about.
export function parseHeaders(raw: string): { from?: string; to?: string[]; subject?: string; date?: number } {
  const headerEnd = (() => {
    const idx = raw.indexOf('\r\n\r\n');
    if (idx >= 0) return idx;
    return raw.indexOf('\n\n');
  })();
  const headerStr = headerEnd >= 0 ? raw.slice(0, headerEnd) : raw;
  const lines = headerStr.split(/\r?\n/);
  // Unfold continuations (line starting with whitespace = continuation
  // of previous header).
  const unfolded: string[] = [];
  for (const l of lines) {
    if ((l.startsWith(' ') || l.startsWith('\t')) && unfolded.length > 0) {
      unfolded[unfolded.length - 1] += ' ' + l.trim();
    } else {
      unfolded.push(l);
    }
  }
  let from: string | undefined;
  let to: string[] | undefined;
  let subject: string | undefined;
  let date: number | undefined;
  for (const line of unfolded) {
    const colon = line.indexOf(':');
    if (colon < 0) continue;
    const key = line.slice(0, colon).trim().toLowerCase();
    const val = line.slice(colon + 1).trim();
    if (key === 'from')     from = val;
    else if (key === 'to')  to = val.split(',').map(s => s.trim()).filter(Boolean);
    else if (key === 'subject') subject = val;
    else if (key === 'date') {
      const t = Date.parse(val);
      if (!Number.isNaN(t)) date = t;
    }
  }
  return { from, to, subject, date };
}
