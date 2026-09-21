# Desktop drive sync: what the cable can actually do

The desktop app can keep a host folder in step with a machine's own internal
drive, over the emulated Remote Link cable. This note records what that costs,
because the number decides how the feature should be described to users — and
it is much worse than it sounds like it should be.

## The measurement

`tests/integration/test-drive-sync.mts` drives the real `PlpClient` against a
real ROM in the native harness and reports throughput. On a Series 5mx
(`5mx_v1.05(260)_eng.bin`, Windermere, UART2, `conSeq` 4), with
`PSION_REALTIME=1` so sim time tracks the wall clock:

```
MEASURED: 2101 B in 2.5 s = 0.81 KB/s (0.8 s per file, listings included)
```

**Under one kilobyte per second.** For comparison, the wire is nominally 115200
baud — about 11.5 KB/s — so roughly 93% of the theoretical rate is lost before
any file data moves. It goes to:

- one RFSV round-trip per 1 KB chunk (2 KB on SA-1100 machines), each paying a
  full EPOC thread wake on the device side;
- DLE/ESC byte-stuffing in the PLP frame layer;
- PLP, NCP and RFSV header overhead per frame;
- an `OPEN_FILE` / `WRITE` / `CLOSE` triple per file, plus a directory listing
  per cycle, all of which are round-trips that carry no payload.

Practical consequences, which the UI is written around:

| Operation | Roughly |
|---|---|
| A 4 KB note | 5 seconds |
| A 100 KB document | 2 minutes |
| A 1 MB file | 20 minutes |
| Listing one directory | 0.3–1 second |

So this is presented as **"synced a moment ago"**, never as a mounted drive.
The default folder is small, the mirror warns past 64 files, and refuses past
512. For moving anything bulky, the shared card (which is projected host-side
and costs nothing) is the right road; the cable is for "drop a document in and
open it in Word", which it does well.

## Things a real machine does that a mock will not

Both of these were found by running against the ROM, and both changed the
design.

### `C:\Documents` is not empty, and some of it cannot be read

A freshly booted 5mx has stock `Word`, `Sheet`, `Data` and `Agenda` documents in
`C:\Documents` — real files, ~400–550 bytes, with the ARCHIVE attribute — and it
holds them **open**. RFSV answers `KErrInUse` (−14) to any attempt to open them:

```
"Word"   short=""          size=548  attr=0x0020 [ARCHIVE] isDir=false
"Sheet"  short=""          size=413  attr=0x0020 [ARCHIVE] isDir=false
```

They also appear over time rather than all at once, as EPOC settles.

A mirror that simply retried would spend a round-trip per file per cycle,
forever, on a link with none to spare. So a file that fails gets a backoff
record in the mirror state — one minute, doubling to an hour — cleared the
moment a transfer succeeds, so a document the user closes is picked up without
being told. The failure stays visible in the status line as "in use on the
device" rather than as a status code.

### An empty directory listing is not a deletion

RFSV's `OPEN_DIR` wants a trailing separator: `C:\Documents` lists nothing where
`C:\Documents\` lists the folder. Every pre-existing caller in the codebase
(`RemoteLinkDialog`, `appLibrary`) happens to pass one, so the requirement is
easy to miss.

The mirror passed a root without one, got an empty listing, concluded that every
file had been deleted on the device — and deleted them from the user's host
folder. Nothing about the plan looked wrong at any point.

Two changes came out of that:

1. `createDeviceFs` normalises the trailing separator.
2. `planMirror` refuses any plan that would delete more than half of what it is
   tracking (floor of 3), because bulk deletion is far more likely to be a link
   or path fault than something the user did. Being wrong in that direction
   costs one extra cycle; being wrong in the other costs their documents.

## Reproducing

```sh
bash harness/build.sh          # needs g++ only, no Emscripten
node --experimental-strip-types tests/integration/test-drive-sync.mts \
     --device 5mx --seconds 90
```

The `--seconds` budget is the harness's own `--serial-poll-until`; the checks
finish in about 15 seconds after an 8-second boot, so 90 is ample.
`--device netpad` exercises the SA-1100 flavour (UART3, 2 KB chunks).

The engine's decision table — which side wins, when a deletion propagates, what
happens when both sides changed — is unit-tested separately and much more
thoroughly in `frontend/src/lib/__tests__/hostsync.mirror.test.mts`. This test
exists to prove the parts only a real machine can: that the bytes actually
arrive, and how long they take.
