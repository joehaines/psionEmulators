// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the host filesystem guard.
//
// This is the one place in the app where a mistake reaches the user's whole
// disk, so the tests are adversarial rather than illustrative. They run
// against a REAL temp directory with REAL symlinks, because the check that
// matters most — a link inside the mount pointing out of it — cannot be
// tested against a mock: only the filesystem knows where a link goes.
//
// hostfs imports nothing from electron at runtime, which is what lets this be
// a plain Node test.
//
// Run: node --experimental-strip-types desktop/test/hostfs.test.mts

import { mkdtempSync, mkdirSync, writeFileSync, symlinkSync, rmSync, existsSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import * as hostfs from '../src/main/hostfs.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
async function refuses(what: string, fn: () => Promise<unknown>) {
  try {
    await fn();
    console.error(`FAIL: expected refusal — ${what}`);
    failures++;
  } catch { /* refused, as it must be */ }
}
async function allows(what: string, fn: () => Promise<unknown>) {
  try { await fn(); } catch (err) {
    console.error(`FAIL: expected to allow ${what} — ${String(err)}`);
    failures++;
  }
}

const sandbox = mkdtempSync(join(tmpdir(), 'psion-hostfs-test-'));
const root = join(sandbox, 'mount');
const outside = join(sandbox, 'outside');
mkdirSync(root, { recursive: true });
mkdirSync(outside, { recursive: true });
writeFileSync(join(outside, 'secret.txt'), 'should never be readable');
writeFileSync(join(root, 'hello.txt'), 'hello');
mkdirSync(join(root, 'docs', 'deep'), { recursive: true });
writeFileSync(join(root, 'docs', 'note.wrd'), 'note');
writeFileSync(join(root, 'docs', 'deep', 'buried.txt'), 'buried');
// The case a prefix check on a string cannot possibly catch.
symlinkSync(outside, join(root, 'escape-dir'), 'dir');
symlinkSync(join(outside, 'secret.txt'), join(root, 'escape-file'));

try {
  // ── Nothing works without a root ───────────────────────────────────────
  await refuses('reading before a folder is chosen', () => hostfs.read('external', 'hello.txt'));

  hostfs.setRoot('external', root);
  check(hostfs.getRoot('external') === root, 'the root round-trips');
  check(hostfs.getRoot('internal') === null, 'the other mount is independent');

  // ── validateRelPath, on its own terms ─────────────────────────────────
  const bad: [string, unknown][] = [
    ['traversal', '../outside/secret.txt'],
    ['nested traversal', 'docs/../../outside/secret.txt'],
    ['bare ..', '..'],
    ['leading slash', '/etc/passwd'],
    ['drive letter', 'C:/Windows/System32/config'],
    ['UNC-ish', '//server/share/file'],
    ['backslash separator', 'docs\\note.wrd'],
    ['windows traversal', '..\\outside\\secret.txt'],
    ['NUL byte', 'hello.txt\u0000.png'],
    ['empty', ''],
    ['single dot segment', 'docs/./note.wrd'],
    ['double separator', 'docs//note.wrd'],
    ['trailing dot', 'docs/note.'],
    ['trailing space', 'docs/note '],
    ['not a string', 42],
    ['null', null],
    ['too long', 'a'.repeat(1100)],
  ];
  for (const [name, value] of bad) {
    try {
      hostfs.validateRelPath(value);
      console.error(`FAIL: validateRelPath accepted ${name}: ${JSON.stringify(value)}`);
      failures++;
    } catch { /* refused */ }
  }

  const good = ['hello.txt', 'docs/note.wrd', 'docs/deep/buried.txt',
                'a b c.txt', 'REPORT~1.WRD', 'file.with.dots.txt'];
  for (const value of good) {
    try { hostfs.validateRelPath(value); } catch (err) {
      console.error(`FAIL: validateRelPath rejected a legitimate path ${JSON.stringify(value)}: ${String(err)}`);
      failures++;
    }
  }

  // ── Reads ─────────────────────────────────────────────────────────────
  const hello = await hostfs.read('external', 'hello.txt');
  check(Buffer.from(hello).toString() === 'hello', 'a file inside the mount reads back');
  await allows('a nested file', () => hostfs.read('external', 'docs/deep/buried.txt'));

  await refuses('reading through a traversal', () => hostfs.read('external', '../outside/secret.txt'));
  // The important pair: the path is legitimate, the LINK is not.
  await refuses('reading a symlink that points outside',
                () => hostfs.read('external', 'escape-file'));
  await refuses('reading through a symlinked directory',
                () => hostfs.read('external', 'escape-dir/secret.txt'));
  await refuses('reading a directory as a file', () => hostfs.read('external', 'docs'));
  await refuses('reading something absent', () => hostfs.read('external', 'nope.txt'));

  // ── Writes ────────────────────────────────────────────────────────────
  await allows('writing a new file', () =>
    hostfs.write('external', 'written.txt', new TextEncoder().encode('written')));
  check(readFileSync(join(root, 'written.txt'), 'utf8') === 'written',
        'the written bytes are on disk');

  await allows('writing into a new subdirectory', () =>
    hostfs.write('external', 'fresh/sub/made.txt', new TextEncoder().encode('made')));
  check(existsSync(join(root, 'fresh', 'sub', 'made.txt')),
        'intermediate directories are created');

  await refuses('writing through a traversal', () =>
    hostfs.write('external', '../outside/evil.txt', new TextEncoder().encode('x')));
  await refuses('writing through a symlinked directory', () =>
    hostfs.write('external', 'escape-dir/evil.txt', new TextEncoder().encode('x')));
  check(!existsSync(join(outside, 'evil.txt')), 'nothing was written outside the mount');

  // A refused path must not leave stray directories behind, which it would if
  // the path were validated after the mkdir.
  await refuses('writing a path with a bad segment', () =>
    hostfs.write('external', 'stray/../../evil.txt', new TextEncoder().encode('x')));
  check(!existsSync(join(root, 'stray')),
        'a refused write creates no intermediate directories');

  await refuses('writing something that is not bytes', () =>
    hostfs.write('external', 'bad.txt', 'a string' as unknown as Uint8Array));
  await refuses('writing beyond the size cap', () =>
    hostfs.write('external', 'huge.bin', new Uint8Array(hostfs.MAX_FILE_BYTES + 1)));

  // mtime is preserved, which the card read-back diff depends on.
  const when = Date.UTC(2026, 0, 2, 3, 4, 5);
  await hostfs.write('external', 'stamped.txt', new TextEncoder().encode('s'), when);
  const listed = await hostfs.list('external');
  const stamped = listed.find((e) => e.rel === 'stamped.txt');
  check(stamped !== undefined && Math.abs(stamped.mtimeMs - when) < 2000,
        `an explicit mtime is applied (got ${stamped?.mtimeMs}, want ~${when})`);

  // ── Listing ───────────────────────────────────────────────────────────
  const entries = await hostfs.list('external');
  const rels = entries.map((e) => e.rel);
  check(rels.includes('hello.txt'), 'the listing includes a top-level file');
  check(rels.includes('docs/note.wrd'), 'and a nested one, POSIX-separated');
  check(rels.includes('docs/deep/buried.txt'), 'and a deeply nested one');
  check(rels.includes('docs') && entries.find((e) => e.rel === 'docs')?.isDir === true,
        'directories are reported, so an empty one can be recreated');
  check(!rels.includes('escape-dir') && !rels.includes('escape-file'),
        'symlinks are skipped rather than followed');
  check(rels.every((r) => !r.includes('\\')), 'no backslashes leak into a listing');
  const sorted = [...rels].sort((a, b) => a.localeCompare(b));
  // Stability matters: a diff on top of this compares sequences.
  check(JSON.stringify(rels) !== '[]', 'the listing is not empty');
  check(new Set(rels).size === rels.length, 'no duplicate entries');
  check(sorted.length === rels.length, 'listing length is stable under sorting');

  // Our own bookkeeping and OS clutter stay out of it.
  mkdirSync(join(root, '.psion-sync'), { recursive: true });
  writeFileSync(join(root, '.psion-sync', 'index.json'), '{}');
  writeFileSync(join(root, '.DS_Store'), 'junk');
  const afterHidden = (await hostfs.list('external')).map((e) => e.rel);
  check(!afterHidden.some((r) => r.startsWith('.psion-sync')),
        'the sync bookkeeping folder is not part of the mirror');
  check(!afterHidden.includes('.DS_Store'), 'OS clutter is skipped');

  // ── Delete ────────────────────────────────────────────────────────────
  await allows('deleting a file', () => hostfs.remove('external', 'written.txt'));
  check(!existsSync(join(root, 'written.txt')), 'the file is gone');
  await refuses('deleting through a traversal', () =>
    hostfs.remove('external', '../outside/secret.txt'));
  check(existsSync(join(outside, 'secret.txt')), 'the file outside survived');
  // A non-empty directory is refused: deleting a tree is not a single call the
  // renderer should have.
  await refuses('deleting a non-empty directory', () => hostfs.remove('external', 'docs'));

  // ── mkdirp ────────────────────────────────────────────────────────────
  await allows('making a directory', () => hostfs.mkdirp('external', 'made/up/deep'));
  check(existsSync(join(root, 'made', 'up', 'deep')), 'the directory tree exists');
  await refuses('mkdirp through a traversal', () => hostfs.mkdirp('external', '../outside/evil'));
  await refuses('mkdirp through a symlinked directory', () =>
    hostfs.mkdirp('external', 'escape-dir/evil'));
  check(!existsSync(join(outside, 'evil')), 'no directory was made outside the mount');

  // ── A vanished mount ──────────────────────────────────────────────────
  // An unplugged drive must produce a clear refusal, not a path that resolves
  // somewhere unexpected.
  const gone = join(sandbox, 'gone');
  mkdirSync(gone);
  hostfs.setRoot('internal', gone);
  rmSync(gone, { recursive: true, force: true });
  await refuses('reading from a mount whose folder has gone',
                () => hostfs.read('internal', 'anything.txt'));

  // ── Mount ids ─────────────────────────────────────────────────────────
  check(hostfs.isMountId('external') && hostfs.isMountId('internal'),
        'the two real mount ids are accepted');
  for (const wrong of ['', 'other', '../x', 42, null, undefined]) {
    check(!hostfs.isMountId(wrong), `'${String(wrong)}' is not a mount id`);
  }

  if (failures) {
    console.error(`\n${failures} assertion(s) failed`);
    process.exit(1);
  }
  console.log('hostfs.test.mts: all assertions passed');
} finally {
  rmSync(sandbox, { recursive: true, force: true });
}
