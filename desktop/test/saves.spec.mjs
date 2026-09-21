// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Phase 2 gate: save generations on disk, exercised through the real bridge.
//
// The policy that decides WHEN to save and WHAT to keep is pure and tested in
// frontend/src/lib/__tests__/desktopAutoSave.test.mts. What needs a running
// app is the store itself: that bytes survive a round trip intact, that the
// manifest cannot end up describing a file that is not there, and that the
// guards hold — because everything here takes a device id that becomes a path
// and a filename the renderer supplies.
//
//   node test/saves.spec.mjs

import { createRequire } from 'node:module';
import { existsSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const DESKTOP = resolve(HERE, '..');
const require = createRequire(import.meta.url);
const { _electron } = require(join(DESKTOP, '..', 'frontend', 'node_modules', 'playwright'));

const ELECTRON_BIN = join(DESKTOP, 'node_modules', 'electron', 'dist',
                          process.platform === 'win32' ? 'electron.exe' : 'electron');

let failures = 0;
const check = (cond, msg) => {
  console.log(`${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
};

if (!existsSync(join(DESKTOP, 'build', 'renderer', 'index.html'))) {
  console.error('✗ renderer not built — run `npm run build:desktop` in frontend/');
  process.exit(1);
}

const userData = mkdtempSync(join(tmpdir(), 'psion-saves-test-'));
const savesRoot = join(userData, 'saves');

const app = await _electron.launch({
  executablePath: ELECTRON_BIN,
  args: [DESKTOP, '--no-sandbox', `--user-data-dir=${userData}`],
  env: { ...process.env, ELECTRON_DISABLE_SECURITY_WARNINGS: '1' },
});
const page = await app.firstWindow();
await page.waitForLoadState('domcontentloaded');

// A minimal well-formed PSIONST1 bundle. The store validates the magic before
// it will offer something to the user as restorable, so the shape matters.
const makeBundle = await page.evaluate(() => {
  window.__makeBundle = (marker) => {
    const header = JSON.stringify({ bundleVersion: 2, savedAt: new Date().toISOString(), devices: [] });
    const headerBytes = new TextEncoder().encode(header);
    const magic = new TextEncoder().encode('PSIONST1');
    const out = new Uint8Array(magic.length + 4 + headerBytes.length + 4);
    out.set(magic, 0);
    new DataView(out.buffer).setUint32(magic.length, headerBytes.length, true);
    out.set(headerBytes, magic.length + 4);
    // A recognisable tail, so a round trip can be checked byte for byte.
    out.set([marker, marker, marker, marker], magic.length + 4 + headerBytes.length);
    return out;
  };
  return true;
});
check(makeBundle === true, 'test bundle builder installed');

try {
  // ── Round trip ───────────────────────────────────────────────────────
  const written = await page.evaluate(async () => {
    const h = window.psionHost;
    if (!h.saves) return { error: 'host.saves is missing — phase 2 not wired' };
    const bundle = window.__makeBundle(0x41);
    await h.saves.write('5mx', bundle, { savedAt: Date.UTC(2026, 8, 19, 14, 2, 33), schemaVersion: 12 });
    const list = await h.saves.list('5mx');
    const back = await h.saves.read('5mx', list[0].file);
    return {
      list,
      identical: back.length === bundle.length && back.every((b, i) => b === bundle[i]),
      length: back.length,
    };
  });
  check(!written.error, written.error ?? 'host.saves is exposed');
  check(written.list?.length === 1, `one generation recorded (got ${written.list?.length})`);
  check(written.list?.[0].file === '20260919-140233.psionst1',
        `the filename is the UTC timestamp (got ${written.list?.[0].file})`);
  check(written.list?.[0].schemaVersion === 12, 'the schema version round-trips');
  check(written.identical, 'the bundle comes back byte for byte');

  // Landed in the right place, outside the renderer's reach.
  const onDisk = readdirSync(join(savesRoot, '5mx'));
  check(onDisk.includes('20260919-140233.psionst1'), 'the generation file is on disk');
  check(onDisk.includes('manifest.json'), 'a manifest was written beside it');

  // ── Several generations, newest first ────────────────────────────────
  const many = await page.evaluate(async () => {
    const h = window.psionHost;
    for (const [i, day] of [[1, 18], [2, 17], [3, 16]].entries()) {
      await h.saves.write('5mx', window.__makeBundle(0x50 + i),
        { savedAt: Date.UTC(2026, 8, day[1], 10, 0, 0), schemaVersion: 12 });
    }
    return h.saves.list('5mx');
  });
  check(many.length === 4, `four generations now (got ${many.length})`);
  const savedAts = many.map((g) => g.savedAt);
  check(savedAts.every((v, i) => i === 0 || savedAts[i - 1] >= v),
        'the list is newest-first');

  // ── Pruning ─────────────────────────────────────────────────────────
  const pruned = await page.evaluate(async (files) => {
    const h = window.psionHost;
    await h.saves.prune('5mx', files);
    return h.saves.list('5mx');
  }, many.slice(2).map((g) => g.file));
  check(pruned.length === 2, `pruning removed the two oldest (got ${pruned.length})`);
  check(pruned[0].file === many[0].file, 'the newest survived');

  // The store refuses to leave a device with nothing, whatever it is asked.
  const protectedNewest = await page.evaluate(async () => {
    const h = window.psionHost;
    const all = await h.saves.list('5mx');
    await h.saves.prune('5mx', all.map((g) => g.file));
    return h.saves.list('5mx');
  });
  check(protectedNewest.length === 1,
        `asked to delete everything, the newest is still kept (got ${protectedNewest.length})`);

  // ── Guards ──────────────────────────────────────────────────────────
  // Both arguments cross the bridge from the renderer and both reach a path.
  const guards = await page.evaluate(async () => {
    const h = window.psionHost;
    const out = {};
    const refuses = async (name, fn) => {
      try { await fn(); out[name] = false; } catch { out[name] = true; }
    };
    await refuses('traversalDeviceId', () =>
      h.saves.write('../../../etc/evil', window.__makeBundle(1),
                    { savedAt: Date.now(), schemaVersion: 12 }));
    await refuses('absoluteDeviceId', () =>
      h.saves.write('/etc/passwd', window.__makeBundle(1),
                    { savedAt: Date.now(), schemaVersion: 12 }));
    await refuses('badFilename', () => h.saves.read('5mx', '../manifest.json'));
    await refuses('emptyBundle', () =>
      h.saves.write('5mx', new Uint8Array(0), { savedAt: Date.now(), schemaVersion: 12 }));
    await refuses('notABundle', () =>
      h.saves.write('5mx', new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9]),
                    { savedAt: Date.now(), schemaVersion: 12 }));
    await refuses('missingSavedAt', () =>
      h.saves.write('5mx', window.__makeBundle(1), { schemaVersion: 12 }));
    await refuses('pruneBadFilename', () => h.saves.prune('5mx', ['../../evil']));
    return out;
  });
  check(guards.traversalDeviceId, 'a traversal in the device id is refused');
  check(guards.absoluteDeviceId, 'an absolute path as a device id is refused');
  check(guards.badFilename, 'a traversal in the generation filename is refused');
  check(guards.emptyBundle, 'an empty bundle is refused');
  check(guards.notABundle, 'bytes that are not a PSIONST1 bundle are refused');
  check(guards.missingSavedAt, 'a write without savedAt is refused');
  check(guards.pruneBadFilename, 'prune refuses a bad filename');
  check(!existsSync(join(userData, '..', 'etc')), 'nothing was written outside userData');

  // ── The manifest never outlives its files ───────────────────────────
  // The dangerous drift direction: a manifest offering a restore that cannot
  // happen. Delete a generation behind the app's back and the next list must
  // drop it rather than report it.
  const remaining = readdirSync(join(savesRoot, '5mx'))
    .filter((f) => f.endsWith('.psionst1'));
  rmSync(join(savesRoot, '5mx', remaining[0]));
  const afterExternalDelete = await page.evaluate(() => window.psionHost.saves.list('5mx'));
  check(!afterExternalDelete.some((g) => g.file === remaining[0]),
        'a generation deleted behind the app is not reported as restorable');

  // And the other direction: a file the manifest never knew about is adopted
  // rather than ignored, which is what an interrupted write leaves behind.
  const orphan = '20260101-000000.psionst1';
  await page.evaluate(async () => {
    // Re-seed so there is a manifest to be out of date.
    await window.psionHost.saves.write('5mx', window.__makeBundle(0x60),
      { savedAt: Date.UTC(2026, 8, 20, 9, 0, 0), schemaVersion: 12 });
  });
  writeFileSync(join(savesRoot, '5mx', orphan), Buffer.from('PSIONST1padding'));
  const adopted = await page.evaluate(() => window.psionHost.saves.list('5mx'));
  check(adopted.some((g) => g.file === orphan),
        'a generation file the manifest never recorded is adopted');

  console.log('');
  if (failures) {
    console.error(`✗ ${failures} assertion(s) failed`);
    process.exit(1);
  }
  console.log('✓ Phase 2 save-generation gate passed\n');
} finally {
  await app.close().catch(() => {});
  rmSync(userData, { recursive: true, force: true });
}
