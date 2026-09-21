// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the shared-card projection.
//
// Run against the REAL filesystem writers, not mocks: the whole feature rests
// on fat16.ts and fefs.ts producing images those ROMs actually mount, and a
// mock would happily agree with a projection that a Psion would reject.
//
// The assertion that matters most is the round trip of NAMES. Both media are
// 8.3-only, so `Quarterly Report.wrd` becomes something like `QUARTE~1.WRD` on
// the card; if the mapping back is wrong, the user's document comes home
// renamed. That is data loss with no error message, so it is tested from
// several directions.
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/extdrive.project.test.mts

import * as fat16 from '../fat16.ts';
import * as fefs from '../fefs.ts';
import {
  familyFor, familySupportsProjection, projectFolder, readbackImage, shouldCompact,
  type HostFile,
} from '../extdrive/project.ts';
import {
  createIndex, deviceNameFor, hostNameFor, hostNameForNew, pruneIndex, toShort83,
} from '../extdrive/nameIndex.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq(actual: unknown, expected: unknown, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg} (got ${JSON.stringify(actual)}, want ${JSON.stringify(expected)})`);
    failures++;
  }
}
const enc = (s: string) => new TextEncoder().encode(s);
const dec = (b: Uint8Array) => new TextDecoder().decode(b);
function file(rel: string, body: string, mtimeMs = Date.UTC(2026, 5, 1, 12, 0, 0)): HostFile {
  return { rel, bytes: enc(body), mtimeMs };
}

// ── familyFor ────────────────────────────────────────────────────────────
{
  eq(familyFor({ hasCFSlot: true }), 'fat16-cf', 'a CF machine takes a FAT16 card');
  eq(familyFor({ hasMmcSlot: true }), 'fat16-mmc', 'the netpad takes an MMC');
  eq(familyFor({ ssdSlotCount: 2 }), 'fefs-ssd', 'a SIBO machine takes an SSD pack');
  eq(familyFor({ datapakSlotCount: 2 }), 'datapak', 'the Organiser II takes a Datapak');
  eq(familyFor({}), 'none', 'Revo and Geofox have no removable slot');
  eq(familyFor(null), 'none', 'no profile means no slot');
  // CF wins over SSD if a profile somehow claimed both; no shipping machine
  // does, but the order must be defined rather than incidental.
  eq(familyFor({ hasCFSlot: true, ssdSlotCount: 2 }), 'fat16-cf',
     'CF takes precedence when both are claimed');

  check(familySupportsProjection('fat16-cf'), 'CF can be projected onto');
  check(familySupportsProjection('fefs-ssd'), 'an SSD pack can be projected onto');
  check(!familySupportsProjection('datapak'),
        'Datapak cannot — no pack filesystem writer exists');
  check(!familySupportsProjection('none'), 'a machine with no slot cannot');
}

// ── 8.3 mangling ─────────────────────────────────────────────────────────
{
  eq(toShort83('report.wrd').stem, 'REPORT', 'a short name passes through');
  eq(toShort83('report.wrd').ext, 'WRD', 'and keeps its extension');
  eq(toShort83('Quarterly Report.wrd').stem, 'QUARTERL', 'a long stem truncates to eight');
  eq(toShort83('a.verylongext').ext, 'VER', 'a long extension truncates to three');
  eq(toShort83('no-extension').ext, '', 'no extension means no extension');
  // The legal 8.3 set is wider than it looks — !@#$%^&()_~- all pass through,
  // which is why '!!!' survives intact and only genuinely illegal characters
  // are substituted.
  eq(toShort83('!!!.txt').stem, '!!!', 'punctuation FAT allows passes through');
  eq(toShort83('a+b=c.txt').stem, 'A_B_C', 'characters FAT rejects become underscores');
  eq(toShort83('a b.txt').stem, 'A_B', 'spaces become underscores');
  // A leading dot is not an extension separator: '.hidden' is an eight-letter
  // name, not an empty name with an extension.
  eq(toShort83('.hidden').stem, '_HIDDEN', 'a dotfile keeps its whole name as the stem');
  eq(toShort83('').stem, 'FILE',
     'an empty segment still yields an openable stem (defensive — callers filter these out)');
}

// ── The name round trip ──────────────────────────────────────────────────
{
  const index = createIndex();
  const a = deviceNameFor(index, 'Quarterly Report.wrd');
  const b = deviceNameFor(index, 'Quarterly Review.wrd');
  eq(a, 'QUARTERL.WRD', 'the first long name takes the plain mangling');
  check(b !== a, 'a colliding name gets a different one');
  eq(b, 'QUARTE~1.WRD', 'the disambiguator eats into the stem to stay within 8.3');
  const stem = b.split('.')[0];
  check(stem.length <= 8, `the stem still fits eight characters (${stem})`);

  eq(hostNameFor(index, a), 'Quarterly Report.wrd', 'the first maps home exactly');
  eq(hostNameFor(index, b), 'Quarterly Review.wrd', 'and so does the disambiguated one');

  // Stability: asking again must not allocate a new name, or a re-projection
  // would shuffle every file on the card.
  eq(deviceNameFor(index, 'Quarterly Report.wrd'), a, 'a second ask returns the same name');

  // A third collision continues the sequence rather than reusing ~1.
  const c = deviceNameFor(index, 'Quarterly Results.wrd');
  check(c !== a && c !== b, `a third collision is distinct (${c})`);
}

// Directories are mangled per segment, and nesting survives.
{
  const index = createIndex();
  const dev = deviceNameFor(index, 'Work Documents/Big Project/notes file.txt');
  const segs = dev.split('/');
  eq(segs.length, 3, 'the nesting depth is preserved');
  check(segs.every((s) => s.split('.')[0].length <= 8),
        `every segment fits 8.3 (${dev})`);
  eq(hostNameFor(index, dev), 'Work Documents/Big Project/notes file.txt',
     'the whole nested path maps home');
}

// A file the DEVICE created has no index entry, so it needs a host name.
{
  const index = createIndex();
  eq(hostNameForNew(index, 'REPORT.WRD', { lowercase: true }), 'report.wrd',
     'a device-authored name is lower-cased — a folder of capitals reads as a fault');
  // The app-folder routing has to be undone, or the host folder slowly grows
  // EPOC's own directory layout.
  eq(hostNameForNew(index, 'WRD/LETTER.WRD',
                    { lowercase: true, stripDir: (d) => d === 'WRD' }),
     'letter.wrd', 'an app folder is stripped on the way back');
  eq(hostNameForNew(index, 'MyStuff/THING.TXT',
                    { lowercase: true, stripDir: (d) => d === 'WRD' }),
     'mystuff/thing.txt', "a folder the user made is NOT stripped");
  // Two device paths collapsing to one host name must both survive.
  const first = hostNameForNew(index, 'WRD/SAME.WRD',
                               { lowercase: true, stripDir: (d) => d === 'WRD' });
  const second = hostNameForNew(index, 'SPR/SAME.WRD',
                                { lowercase: true, stripDir: (d) => d === 'SPR' });
  check(first !== second,
        `two device files collapsing to one host name stay distinct (${first} / ${second})`);
}

// Pruning must forget deleted files, or their 8.3 names stay reserved forever
// and new files drift into ~7 territory for no reason.
{
  const index = createIndex();
  deviceNameFor(index, 'gone.txt');
  deviceNameFor(index, 'kept.txt');
  const pruned = pruneIndex(index, ['kept.txt']);
  eq(Object.keys(pruned.toDevice).length, 1, 'the deleted entry is dropped');
  eq(pruned.toDevice['kept.txt'] !== undefined, true, 'the live entry survives');
  // With the name freed, a new colliding file gets the plain form again.
  const fresh = deviceNameFor(pruned, 'gone.txt');
  eq(fresh, 'GONE.TXT', 'a freed name is available again');
}

// ── FAT16 projection round trip ──────────────────────────────────────────
{
  const files: HostFile[] = [
    file('Quarterly Report.wrd', 'the report body'),
    file('Quarterly Review.wrd', 'the review body'),
    file('notes.txt', 'plain notes'),
    file('Work Documents/deep/buried.txt', 'buried content'),
  ];
  const projected = projectFolder(files, 'fat16-cf', { capacityBytes: 4 * 1024 * 1024 });
  check(projected.image !== null, 'a CF image was produced');
  eq(projected.skipped.length, 0, 'nothing was skipped on a 4 MB card');
  const img = projected.image!;

  check(fat16.isFat16(img), 'the image is recognisably FAT16');
  const info = fat16.readInfo(img);
  eq(info.partitionStartSector, 32,
     'the partition starts at LBA 32, which EPOC’s pccd_ata fast-path expects');

  // Every file must be present and byte-identical under its mapped name.
  for (const f of files) {
    const devicePath = projected.index.toDevice[f.rel];
    check(!!devicePath, `${f.rel} has a device name`);
    const entry = fat16.findByPath(img, devicePath);
    check(!!entry, `${f.rel} is on the card at ${devicePath}`);
    if (entry) {
      eq(dec(fat16.readFileBytes(img, entry)), dec(f.bytes),
         `${f.rel} is byte-identical on the card`);
      check(Math.abs(entry.mtimeMs - f.mtimeMs) <= 2000,
            `${f.rel} carries its host mtime (±2s for FAT granularity)`);
    }
  }

  // Nesting became real directories, not a flattened name with slashes in it.
  const walked = fat16.walkAll(img);
  check(walked.some((e) => e.isDir), 'the card has directories');

  // Read-back with nothing changed must report nothing.
  const asProjected = files.map((f) => ({
    rel: f.rel, size: f.bytes.byteLength, mtimeMs: f.mtimeMs,
  }));
  const quiet = readbackImage(img, 'fat16-cf', projected.index, asProjected);
  eq(quiet.changed.length, 0, 'an untouched card reports no changes');
  eq(quiet.deleted.length, 0, 'and no deletions');

  // The guest edits a file: it must come back under the HOST name.
  const edited = new Uint8Array(img);
  const target = projected.index.toDevice['Quarterly Report.wrd'];
  fat16.writeFileAtPath(edited, target, enc('the report, revised on the device'),
                        { mtimeMs: Date.UTC(2026, 5, 2, 9, 0, 0) });
  const afterEdit = readbackImage(edited, 'fat16-cf', projected.index, asProjected);
  eq(afterEdit.changed.length, 1, 'one changed file is reported');
  eq(afterEdit.changed[0]?.rel, 'Quarterly Report.wrd',
     'and it comes home under its LONG host name, not the 8.3 one');
  eq(dec(afterEdit.changed[0]?.bytes ?? new Uint8Array()),
     'the report, revised on the device', 'with the bytes the guest wrote');
  eq(afterEdit.changed[0]?.isNew, false, 'and is not reported as a new file');

  // The guest creates a file: it needs a host name invented for it.
  const created = new Uint8Array(img);
  fat16.writeFileAtPath(created, 'FRESH.TXT', enc('made on the Psion'));
  const afterCreate = readbackImage(created, 'fat16-cf', projected.index, asProjected);
  const fresh = afterCreate.changed.find((c) => c.isNew);
  check(!!fresh, 'a device-created file is reported as new');
  eq(fresh?.rel, 'fresh.txt', 'under a lower-cased host name');

  // The guest deletes a file: reported, not acted on.
  const deleted = new Uint8Array(img);
  fat16.deleteAtPath(deleted, projected.index.toDevice['notes.txt']);
  const afterDelete = readbackImage(deleted, 'fat16-cf', projected.index, asProjected);
  check(afterDelete.deleted.includes('notes.txt'),
        'a file deleted on the device is reported as deleted');
}

// Re-projecting with the same index must give the same names — otherwise every
// sync cycle would churn the card and the read-back diff would be meaningless.
{
  const files = [file('Quarterly Report.wrd', 'a'), file('Quarterly Review.wrd', 'b')];
  const first = projectFolder(files, 'fat16-cf', { capacityBytes: 4 * 1024 * 1024 });
  const second = projectFolder(files, 'fat16-cf',
                               { capacityBytes: 4 * 1024 * 1024, index: first.index });
  eq(JSON.stringify(second.index.toDevice), JSON.stringify(first.index.toDevice),
     'a re-projection assigns identical device names');
}

// ── Capacity ─────────────────────────────────────────────────────────────
{
  // A 4 MB card and 5 MB of files: some must be reported, not dropped.
  const big = (n: number) => ({
    rel: `big-${n}.bin`,
    bytes: new Uint8Array(1_200_000).fill(n),
    mtimeMs: Date.UTC(2026, 5, 1),
  });
  const files = [big(1), big(2), big(3), big(4), big(5), file('tiny.txt', 'x')];
  const projected = projectFolder(files, 'fat16-cf', { capacityBytes: 4 * 1024 * 1024 });
  check(projected.skipped.length > 0, 'over-capacity files are reported');
  check(projected.skipped.every((s) => s.reason === 'no-space' || s.reason === 'too-big'),
        'with a space reason');
  // Smallest-first is what makes this true: the little file gets on the card
  // even though four large ones could not.
  const onCard = fat16.findByPath(projected.image!, projected.index.toDevice['tiny.txt']);
  check(!!onCard, 'a small file still fits when large ones do not (smallest-first)');
  check(projected.freeBytes >= 0, 'free space is reported');
}

// ── FEFS projection round trip ───────────────────────────────────────────
{
  const files: HostFile[] = [
    file('Letter to Bank.wrd', 'dear sir'),
    file('budget.spr', 'a,b,c'),
    file('readme.txt', 'no app folder for me'),
  ];
  const projected = projectFolder(files, 'fefs-ssd', { capacityBytes: 512 * 1024 });
  check(projected.image !== null, 'an SSD pack was produced');
  eq(projected.skipped.length, 0, 'nothing skipped on a 512K pack');
  const pack = projected.image!;
  eq(fefs.classifyPack(pack), 'flash', 'the pack classifies as FEFS flash');

  // The app-folder routing is the whole reason a .WRD is findable on a Series 3.
  const listed = fefs.listFiles(pack).map((f) => f.name);
  check(listed.some((n) => n.startsWith('WRD\\')),
        `a .wrd is routed into \\WRD\\ (${listed.join(', ')})`);
  check(listed.some((n) => n.startsWith('SPR\\')), 'a .spr is routed into \\SPR\\');
  check(listed.some((n) => !n.includes('\\')), 'an unrecognised type stays in the root');

  for (const f of files) {
    const devicePath = projected.index.toDevice[f.rel].replace(/\//g, '\\');
    eq(dec(fefs.readFileFromPack(pack, devicePath)), dec(f.bytes),
       `${f.rel} is byte-identical in the pack`);
  }

  // Read-back must undo the routing: a file from \WRD\ belongs at the host root.
  const asProjected = files.map((f) => ({
    rel: f.rel, size: f.bytes.byteLength, mtimeMs: f.mtimeMs, bytes: f.bytes,
  }));
  const quiet = readbackImage(pack, 'fefs-ssd', projected.index, asProjected);
  eq(quiet.changed.length, 0, 'an untouched pack reports no changes');

  const edited = fefs.addFileToPack(
    fefs.removeFileFromPack(pack, projected.index.toDevice['Letter to Bank.wrd'].replace(/\//g, '\\')),
    'LETTER~1.WRD', enc('dear madam'), 'WRD');
  // The name above is whatever the index assigned; re-read it to be exact.
  const devName = projected.index.toDevice['Letter to Bank.wrd'];
  const reEdited = fefs.addFileToPack(
    fefs.removeFileFromPack(pack, devName.replace(/\//g, '\\')),
    devName.slice(devName.lastIndexOf('/') + 1), enc('dear madam'), 'WRD');
  const afterEdit = readbackImage(reEdited, 'fefs-ssd', projected.index, asProjected);
  const back = afterEdit.changed.find((c) => c.rel === 'Letter to Bank.wrd');
  check(!!back, 'an edited pack file comes home under its long host name');
  eq(dec(back?.bytes ?? new Uint8Array()), 'dear madam', 'with the edited content');
  check(edited.length > 0, 'the intermediate pack was built');

  // A device-created file in an app folder lands at the host root, lower-cased.
  const withNew = fefs.addFileToPack(pack, 'DIARY.WRD', enc('written on the Psion'), 'WRD');
  const afterCreate = readbackImage(withNew, 'fefs-ssd', projected.index, asProjected);
  const created = afterCreate.changed.find((c) => c.isNew);
  check(!!created, 'a pack file the device created is reported as new');
  eq(created?.rel, 'diary.wrd',
     'at the host root, lower-cased, with the app folder stripped');
}

// ── Unsupported families ─────────────────────────────────────────────────
{
  const files = [file('a.txt', 'x')];
  for (const family of ['datapak', 'none'] as const) {
    const r = projectFolder(files, family);
    eq(r.image, null, `${family} produces no image`);
    eq(r.skipped.length, 1, `${family} reports the file as unprojectable`);
    eq(r.skipped[0].reason, 'unsupported-family', `${family} says why`);
    check(!!r.skipped[0].detail, `${family} explains what the machine has instead`);
  }
}

// ── Compaction ───────────────────────────────────────────────────────────
{
  // FEFS never reclaims: repeated add/remove marches the extent towards the
  // end of the pack whatever it actually holds. Without compaction a shared
  // folder edited daily would exhaust a small pack.
  let pack = fefs.createFlashPack(128 * 1024, 'CHURN');
  const payload = new Uint8Array(8 * 1024).fill(7);
  const freeAtStart = fefs.packFreeBytes(pack);
  for (let i = 0; i < 6; i++) {
    pack = fefs.addFileToPack(pack, 'CHURN.TXT', payload);
  }
  const freeAfterChurn = fefs.packFreeBytes(pack);
  check(freeAfterChurn < freeAtStart - 5 * payload.byteLength,
        `rewriting one file consumed the arena (${freeAtStart} -> ${freeAfterChurn})`);
  eq(fefs.listFiles(pack).length, 1, 'but only one file is live');

  const compacted = fefs.compactPack(pack);
  check(fefs.packFreeBytes(compacted) > freeAfterChurn,
        'compaction reclaims the space');
  eq(fefs.listFiles(compacted).length, 1, 'and keeps the live file');
  eq(dec(fefs.readFileFromPack(compacted, 'CHURN.TXT')), dec(payload),
     'with its contents intact');
  eq(fefs.classifyPack(compacted), 'flash', 'and still a valid FEFS pack');
  eq(fefs.readVolumeName(compacted), 'CHURN', 'preserving the volume name');

  check(shouldCompact(pack, freeAtStart), 'shouldCompact spots a pack needing a rebuild');
  check(!shouldCompact(compacted, 1024), 'and leaves a healthy one alone');
}

if (failures) {
  console.error(`\n${failures} assertion(s) failed`);
  process.exit(1);
}
console.log('extdrive.project.test.mts: all assertions passed');
