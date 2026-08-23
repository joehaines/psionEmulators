// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The device registry is the source of truth for what machines exist, but
// two files outside the C++ have to list them by id as well, and neither
// failure is visible from the outside:
//
//   frontend/public/api/track.php  — $ALLOWED_DEVICES. A device missing
//     here has every analytics event rejected with a 400, and the client
//     swallows non-2xx responses by design, so the machine simply never
//     appears on the usage leaderboard and nothing anywhere says why.
//     (Caught exactly this way once: the Conan shipped without it.)
//
//   frontend/public/device-names.json — the id → display-name map the
//     leaderboard and the app library read. A device missing here is
//     listed by its raw id ("conan" rather than "Psion Revo (Conan)").
//
// Both are plain lists a new device is easy to forget, so assert them
// against the registry rather than trusting the comments that say to keep
// them in sync.
//
// Run:
//   node --experimental-strip-types tests/unit/device-lists-sync.mts

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (p: string) => readFileSync(join(ROOT, p), 'utf8');

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// Every profile in kProfiles opens with its id on a line of its own —
// `        "conan",` — which no other field in the struct looks like (the
// rest are either quoted strings with a dot in them, numbers, or C++
// identifiers). Hidden-from-picker profiles count too: they are reachable
// from the frontend and emit analytics like any other.
const registry = [...read('core/device_registry.cpp')
  .matchAll(/^\s+"([a-z0-9_]+)",$/gm)].map(m => m[1]);
check(registry.length > 15,
      `parsed ${registry.length} device ids out of the registry — the shape of ` +
      'kProfiles must have changed, so this test is no longer checking anything');

// track.php: the ids inside the $ALLOWED_DEVICES array literal.
const phpBlock = read('frontend/public/api/track.php')
  .split('$ALLOWED_DEVICES = [')[1]?.split('];')[0];
check(phpBlock !== undefined, 'found $ALLOWED_DEVICES in track.php');
const allowed = [...(phpBlock ?? '').matchAll(/'([a-z0-9_]+)'/g)].map(m => m[1]);

// device-names.json: the keys.
const names = JSON.parse(read('frontend/public/device-names.json')) as Record<string, string>;

for (const id of registry) {
  check(allowed.includes(id),
        `${id} is in the device registry but not in track.php's $ALLOWED_DEVICES ` +
        '— its analytics events would be rejected and it would never reach the leaderboard');
  check(id in names,
        `${id} is in the device registry but not in device-names.json ` +
        '— the leaderboard would list it by its raw id');
}

// The other direction: a stale entry is harmless at runtime but means one
// of these lists is describing a machine that no longer exists.
for (const id of allowed)
  check(registry.includes(id), `track.php allows '${id}', which is not a device in the registry`);
for (const id of Object.keys(names))
  check(registry.includes(id), `device-names.json names '${id}', which is not a device in the registry`);

if (failures) {
  console.error(`\n${failures} check(s) FAILED`);
  process.exit(1);
}
console.log(`PASS device-lists-sync (${registry.length} devices, tracked and named)`);
