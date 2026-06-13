// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// FAT16 directory-support test. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/fat16.test.mts
// or `bash scripts/test-fat16.sh`.
//
// Covers the cluster-chain directory operations added on top of the
// original root-only API: listing sub-directories, creating directories
// (including nested), adding files into a sub-dir, recursive delete, and
// the legacy `addFile` still targeting the root.

import {
  createBlankImage, isFat16,
  listRoot, listDirectory,
  addFile, addFileToDirectory,
  createDirectory, deleteEntryRecursive,
  readFileBytes, freeSpace, readInfo, setVolumeLabel,
  ROOT_DIR_CLUSTER,
} from '../fat16.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq<T>(actual: T, expected: T, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg}: got ${String(actual)}, want ${String(expected)}`);
    failures++;
  }
}

// 1. Blank image is valid FAT16 and starts empty.
const img = createBlankImage(8 * 1024 * 1024);
check(isFat16(img), 'fresh image is FAT16');
eq(listRoot(img).length, 0, 'fresh root is empty');

// 2. Create a sub-directory in the root.
const mk = createDirectory(img, ROOT_DIR_CLUSTER, 'DOCS');
check(mk.ok, `createDirectory(DOCS) ok: ${mk.reason ?? ''}`);
check(typeof mk.cluster === 'number' && mk.cluster! >= 2, 'DOCS got a real cluster');

const root1 = listRoot(img);
eq(root1.length, 1, 'root now has one entry');
eq(root1[0].name, 'DOCS', 'root entry is DOCS');
check(root1[0].isDirectory, 'DOCS is a directory');

// 3. New directory is empty when listed (the "."/".." pair is filtered out).
const docsCluster = mk.cluster!;
const inDocs0 = listDirectory(img, docsCluster);
eq(inDocs0.length, 0, 'DOCS starts empty (./ .. filtered)');

// 4. Add a file inside DOCS. Verify both that it appears via listDirectory
//    AND that the root listing is untouched.
const enc = new TextEncoder();
const hello = enc.encode('hello from a subdir');
const addRes = addFileToDirectory(img, docsCluster, 'NOTE.TXT', hello);
check(addRes.ok, `addFileToDirectory NOTE.TXT ok: ${addRes.reason ?? ''}`);

const inDocs1 = listDirectory(img, docsCluster);
eq(inDocs1.length, 1, 'DOCS has one entry after add');
eq(inDocs1[0].name, 'NOTE.TXT', 'sub-dir entry name');
eq(inDocs1[0].size, hello.byteLength, 'sub-dir entry size');
check(!inDocs1[0].isDirectory, 'NOTE.TXT is a file');

const readBack = readFileBytes(img, inDocs1[0]);
eq(readBack.byteLength, hello.byteLength, 'read-back length');
let bytewise = true;
for (let i = 0; i < hello.byteLength; i++) if (readBack[i] !== hello[i]) bytewise = false;
check(bytewise, 'read-back bytes match');

// Root still has only DOCS, not NOTE.TXT.
const root2 = listRoot(img);
eq(root2.length, 1, 'adding inside DOCS does not pollute root');

// 5. Nested directory: DOCS/SUB
const mkSub = createDirectory(img, docsCluster, 'SUB');
check(mkSub.ok, `createDirectory(DOCS/SUB) ok: ${mkSub.reason ?? ''}`);
const subCluster = mkSub.cluster!;

const inDocs2 = listDirectory(img, docsCluster);
eq(inDocs2.length, 2, 'DOCS has two entries (NOTE.TXT + SUB)');
const subEntry = inDocs2.find(e => e.name === 'SUB');
check(!!subEntry && subEntry.isDirectory, 'SUB is a directory under DOCS');

// File inside the nested directory.
const deep = enc.encode('deep');
const deepRes = addFileToDirectory(img, subCluster, 'DEEP.TXT', deep);
check(deepRes.ok, `add DEEP.TXT into DOCS/SUB ok: ${deepRes.reason ?? ''}`);
eq(listDirectory(img, subCluster).length, 1, 'DOCS/SUB has DEEP.TXT');

// 6. Legacy addFile still targets the root.
const rootFile = enc.encode('root');
const rootAdd = addFile(img, 'README.TXT', rootFile);
check(rootAdd.ok, `addFile(README.TXT) ok: ${rootAdd.reason ?? ''}`);
const root3 = listRoot(img);
eq(root3.length, 2, 'root now has DOCS + README.TXT');
const readme = root3.find(e => e.name === 'README.TXT');
check(!!readme && !readme!.isDirectory, 'README.TXT is a file in root');

// 7. Duplicate names rejected at create-dir + add-file.
const dup1 = createDirectory(img, ROOT_DIR_CLUSTER, 'DOCS');
check(!dup1.ok, 'duplicate folder rejected');
const dup2 = addFileToDirectory(img, docsCluster, 'NOTE.TXT', hello);
check(!dup2.ok, 'duplicate file in sub-dir rejected');

// 8. Recursive delete of DOCS frees DOCS itself, NOTE.TXT, SUB, and DEEP.TXT,
//    and the cluster accounting reflects that.
const before = freeSpace(img);
const docsEntry = listRoot(img).find(e => e.name === 'DOCS')!;
deleteEntryRecursive(img, docsEntry);

const root4 = listRoot(img);
eq(root4.length, 1, 'root has just README.TXT after recursive delete');
eq(root4[0].name, 'README.TXT', 'survivor is README.TXT');

const after = freeSpace(img);
check(after.free > before.free, 'free space increased after recursive delete');
// At minimum we freed the DOCS dir cluster, the SUB dir cluster, NOTE.TXT's
// cluster, and DEEP.TXT's cluster — i.e. ≥ 4 clusters of free space recovered.
const info = (await import('../fat16.ts')).readInfo(img);
check(
  after.free - before.free >= 4 * info.clusterBytes,
  `freed at least 4 clusters (got ${after.free - before.free} bytes)`,
);

// 9. Volume label rename: BPB label updates, a root volume-label entry is
//    created (and stays hidden from normal listings), and the rename survives
//    a re-read of the image.
const lbl = createBlankImage(8 * 1024 * 1024);
eq(readInfo(lbl).volumeLabel, 'NO NAME', 'fresh card label is NO NAME');
const rn = setVolumeLabel(lbl, 'My Card');
check(rn.ok, `setVolumeLabel ok: ${rn.reason ?? ''}`);
eq(readInfo(lbl).volumeLabel, 'MY CARD', 'label upper-cased in BPB');
eq(listRoot(lbl).length, 0, 'volume-label entry is hidden from root listing');
// A second rename replaces (not duplicates) the existing entry.
check(setVolumeLabel(lbl, 'SECOND').ok, 'second rename ok');
eq(readInfo(lbl).volumeLabel, 'SECOND', 'second rename took effect');
eq(listRoot(lbl).length, 0, 'still no stray entries after re-rename');
// Adding a real file alongside the label still lists just that file.
check(addFile(lbl, 'A.TXT', enc.encode('x')).ok, 'add file alongside label');
const lblRoot = listRoot(lbl);
eq(lblRoot.length, 1, 'root lists the file but not the volume label');
eq(lblRoot[0].name, 'A.TXT', 'listed entry is the file');
// Clearing the label blanks the BPB and drops the root entry.
check(setVolumeLabel(lbl, '').ok, 'clear label ok');
eq(readInfo(lbl).volumeLabel, '', 'label cleared in BPB');
// Validation: over-long and illegal labels are rejected without mutating.
check(!setVolumeLabel(lbl, 'WAY TOO LONG LABEL').ok, 'over-long label rejected');
check(!setVolumeLabel(lbl, 'BAD/NAME').ok, 'illegal-char label rejected');

if (failures === 0) {
  console.log('PASS fat16 directory support (list/create/add/recursive-delete)');
  process.exit(0);
} else {
  console.error(`FAIL: ${failures} assertion(s)`);
  process.exit(1);
}
