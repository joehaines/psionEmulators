#!/usr/bin/env bash
# Psion LX peripherals: the CompactFlash socket, sound output and Wi-Fi,
# checked from inside the guest the way a user at its shell would.
#
# Boots the LX (OS image + factory NAND) with a FAT16 CompactFlash card and
# the Wi-Fi PC Card, logs in on the serial console (the image runs a getty
# on ttySA0 with a password-less root) and, in one boot:
#
#   CF     the card is identified by ide-cs and mounted at /media/cf by the
#          image's own cardmgr scripts; a file the host put on it reads back;
#          a file the guest writes survives an eject and re-insert.
#   sound  a 1 kHz tone from the card, played into /dev/dsp, reaches the host
#          speaker stream at the tone's pitch (scaled by whatever rate the
#          image's sound server left the DAC at).
#   Wi-Fi  orinoco_cs binds the Prism2 card, which joins "PsionNet"; the
#          guest gets 10.0.2.15 by DHCP and fetches a file over HTTP from a
#          server on this machine's loopback — through the core's TCP
#          endpoint and the harness's BSD-socket host, the same path the
#          desktop app takes with Node's sockets.
#
#   bash tests/integration/test-lx-peripherals.sh

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/lx-harness"
ROM="$REPO_ROOT/roms/netbook_LX_prototype/LX_boot/nBkProOs.img"
NAND="$REPO_ROOT/roms/netbook_LX_prototype/LX_boot/lx-nand.img.gz"

if [ ! -f "$ROM" ]; then echo "SKIP: $ROM not present" >&2; exit 0; fi
for tool in node python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "SKIP: needs $tool" >&2; exit 0; }
done
if [ ! -f "$NAND" ]; then
    (cd "$REPO_ROOT" && node --experimental-strip-types scripts/build-lx-nand.mts) >&2 || exit 1
fi
if [ ! -x "$HARNESS" ]; then
    echo "lx-harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2 || exit 1
fi

WORK="$(mktemp -d)"
SERVER_PID=""
cleanup() { [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null; rm -rf "$WORK"; }
trap cleanup EXIT

# The card: a hello file and two seconds of a 1 kHz stereo tone at 48 kHz.
python3 - "$WORK/tone.raw" <<'EOF'
import math, struct, sys
with open(sys.argv[1], 'wb') as f:
    for i in range(96000):
        v = int(12000 * math.sin(2 * math.pi * 1000 * i / 48000))
        f.write(struct.pack('<hh', v, v))
EOF
node --experimental-strip-types - "$WORK/cf.img" "$WORK/tone.raw" <<EOF || exit 1
import { createBlankImage, addFile } from '$REPO_ROOT/frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'node:fs';
const [out, tone] = process.argv.slice(2);
const img = createBlankImage(16 * 1024 * 1024);
for (const [name, data] of [['HELLO.TXT', new TextEncoder().encode('hello from the host\n')],
                            ['TONE.RAW', new Uint8Array(readFileSync(tone))]] as const) {
  const r = addFile(img, name, data, 0);
  if (!r.ok) { console.error('addFile failed', name, r); process.exit(1); }
}
writeFileSync(out, img);
EOF

# A web server on loopback, which the guest reaches as 10.0.2.2.
mkdir -p "$WORK/www"
echo "hello over the virtual wifi" > "$WORK/www/hello.txt"
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
(cd "$WORK/www" && exec python3 -m http.server "$PORT" --bind 127.0.0.1 >/dev/null 2>&1) &
SERVER_PID=$!

cat > "$WORK/script" <<EOF
wait login: 400
send root
sleep 3
# CompactFlash: mounted by cardmgr's ide script, readable, writable, hot-pluggable.
mark
send while [ ! -f /media/cf/tone.raw ]; do sleep 1; done; cat /media/cf/hello.txt; echo CF\$((1+1))
wait CF2 120
expect hello from the host
send echo written-by-lx > /media/cf/fromlx.txt; sync; echo CF\$((1+2))
wait CF3 60
send cardctl eject 0; sleep 2; echo CF\$((1+3))
wait CF4 60
eject-cf
sleep 2
insert-cf
sleep 8
mark
send while [ ! -f /media/cf/fromlx.txt ]; do sleep 1; done; cat /media/cf/fromlx.txt; echo CF\$((1+4))
wait CF5 60
expect written-by-lx
# Sound: the image's sound server holds /dev/dsp, so stop it first.
send killall -9 gpe-soundserver esd 2>/dev/null; sleep 1; cat /media/cf/tone.raw > /dev/dsp && echo SND\$((1+1))
wait SND2 90
# Wi-Fi: associated, addressed, and through the router to the host.
mark
send iwconfig eth0 | head -1; ifconfig eth0 | grep 'inet addr'; echo NET\$((1+1))
wait NET2 30
expect ESSID:"PsionNet"
expect inet addr:10.0.2.15
mark
send wget -O - http://10.0.2.2:$PORT/hello.txt; echo NET\$((1+2))
wait NET3 60
expect hello over the virtual wifi
EOF

echo "=== Psion LX peripherals ===" >&2
"$HARNESS" "$ROM" "$NAND" --cf "$WORK/cf.img" --wav "$WORK/out.wav" --net "$WORK/script" \
    >"$WORK/console.log" 2>"$WORK/harness.log"
status=$?
if [ $status -ne 0 ]; then
    echo "FAIL: guest checks" >&2
    cat "$WORK/harness.log" >&2
    exit 1
fi

# The tone at the host speaker: about two seconds of it, at 1 kHz scaled by
# the DAC rate the sound server left behind (48 or 44.1 kHz).
python3 - "$WORK/out.wav" <<'EOF' || exit 1
import struct, sys
d = open(sys.argv[1], 'rb').read()[44:]
a = struct.unpack('<%dh' % (len(d) // 2), d)
loud = [i for i, v in enumerate(a) if abs(v) > 1000]
if not loud:
    print('FAIL: no audio reached the host speaker', file=sys.stderr); sys.exit(1)
secs = (loud[-1] - loud[0]) / 48000
seg = a[loud[0] + 4800: loud[0] + 9600]
hz = sum(1 for i in range(1, len(seg)) if seg[i - 1] < 0 <= seg[i]) * 10
print(f'tone: {secs:.2f} s at ~{hz} Hz, peak {max(seg)}', file=sys.stderr)
if not (1.9 <= secs <= 2.4 and 880 <= hz <= 1020):
    print('FAIL: tone length or pitch is wrong', file=sys.stderr); sys.exit(1)
EOF

echo "PASS: CompactFlash, sound and Wi-Fi" >&2
