# Conan: why Remote Link can't connect

The Conan (`roms/conan_s2_2201.engbuild.IMG`, TRomHeader 0.01(22), built
2001-05-12) is the only supported EPOC machine whose ROM does **not** speak the
PLP data link `frontend/src/lib/plp` implements. Its profile in
`core/device_registry.cpp` therefore carries `linkProtocol = 0`, which hides the
Remote Link button, the printer dialog's "Via PC" tab and cable app installs,
while leaving the cable UART (2) and everything else the Revo profile gives it.

This is the same wall PsiWin hit on real hardware: this generation of machine
needed a new version of the connectivity software, not a newer cable.

## What the user sees without the fix

The Remote Link dialog attaches, sends its `Req_Req_Pdu` burst, and times out
with "link handshake timed out". Its raw-byte panel shows the device answering
with a repeating two-byte pattern and nothing else:

```
--- Raw bytes from device (6) ---
000000  19 23 19 23 19 23                                |.#.#.#|
```

Those six bytes are the whole story: they are not PLP frames.

## What the ROM actually speaks

### The data link moved and was rewritten

Every other supported EPOC ROM ships two protocol modules: `Plp.prt` (the PLP
transport) and **`PlpDL.prt`**, which carries the `PLP Link` data link — the
`SYN 0x16 | DLE STX | payload | DLE ETX | CRC-hi CRC-lo` framing our client
speaks. The Conan has no `PlpDL.prt` at all; its `Plp.prt` (ROM entry at
`0x502740b0`, 0x94b0 bytes) provides `PLP Link`, `PLP Transport` and
`PLP Loopback` itself.

```
Revo_v1.06(390)_eng.bin     PlpDL.prt present   SYS$RFSV.*  SYS$RPCS.*
5mx_v1.05(260)_eng.bin      PlpDL.prt present   SYS$RFSV.*  SYS$RPCS.*
conan_s2_2201.engbuild.IMG  PlpDL.prt absent    SYS$RFSVU.* SYS$RPCSU.*
```

The service names are the second half of the incompatibility: the Conan
registers only the Unicode servers, so even a completed handshake would have our
RFSV32 client sending an NCP `Connect` for `SYS$RFSV.*`, a name this ROM never
registers.

### The new transport: an escaped, XON/XOFF-safe byte stream

`Plp.prt` configures the cable port for software flow control — the
`TCommConfigV01` it fills in at `0x50275a40` sets `iXonChar = 0x11` and
`iXoffChar = 0x13` (the stores at `0x50275acc`) — and escapes the stream so
data can never look like a flow-control character. `ESC` is `0x19`:

| On the wire | Meaning              |
|-------------|----------------------|
| `19 20`     | literal `0x11` (XON) |
| `19 21`     | literal `0x13` (XOFF)|
| `19 19`     | literal `0x19` (ESC) |
| `19 23`     | ENQ — link probe     |
| `19 24`     | ACK — answer to ENQ  |

The encoder is the loop at `0x50275308` (`cmp #0x13 / cmp #0x11 / cmp #0x19`,
emitting `0x19` then `0x20` / `0x21` / `0x19`); the decoder is the jump table at
`0x50275648`, indexed by `byte - 0x19`, whose `0x23` entry queues an ACK and
whose `0x24` entry does nothing but clear the escape state. The ENQ retry state
machine lives at `0x5027503c`: first probe 2 s after inbound activity, then up
to three more at 2.75 s intervals, then the client request completes with
`-36` (`KErrDisconnected`).

Live behaviour matches the disassembly exactly:

* Send `19 23` and the device answers `19 24` within one poll — the only input
  that ever draws a reply. A sweep of all 256 `19 XX` sequences produced a
  response for `19 23` and nothing else.
* After any inbound byte the device emits its own `19 23`, then repeats at
  2.76 s. Answering with `19 24` (or with `19 23`) just restarts the timer: the
  ACK is accepted but does not unblock anything above.

No other supported ROM (Revo, 5mx, 5mx Pro, MC218, Series 5, Osaris, Series 7,
netBook, netpad) contains this code.

### It answers no PLP frame

Everything below was driven through `harness/run` against the live ROM with the
bridge attached on UART2. In every case the device stayed silent apart from its
own ENQ probes:

* Every link PDU we can form: `Req_Pdu (0x20)`, `Req_Req_Pdu (0x21)`,
  `Disc_Req (0x11)`, `Req_Con_Pdu (0x24 + magic)`, `Ack_Pdu (0x00)`, and an NCP
  Info `0x31` frame — before and after a completed ENQ/ACK exchange.
* Both framings: with the `SYN 0x16` prefix and without it, plus bare and
  length-prefixed candidates (`21`, `01 21`, `01 00 21`, `02 21 00`, …).
* **All 65536 CRC trailers** on `16 10 02 21 10 03 XX YY` and on
  `10 02 21 10 03 XX YY` — so "same framing, different CRC parameters" is ruled
  out, not just guessed at.

## Reproducing

```sh
bash harness/build.sh
# The documented behaviour: an ESC ENQ probe, no PLP frames.
bash tests/integration/test-remote-link.sh conan
```

Raw, if you want to watch it yourself:

```sh
./harness/run roms/conan_s2_2201.engbuild.IMG --device conan --quiet-logs \
    --serial-attach 2 8 \
    --serial-tx 2 10 "19,23" \
    --serial-tx-framed 2 14 "21" \
    --serial-poll-until 24
```

`19 24` comes back immediately after the ENQ at t=10; the framed `Req_Req_Pdu`
at t=14 is answered only by the next ENQ probe.

## What supporting it would take

A second link client, not a tweak to the existing one:

1. An escaping layer under `frontend/src/lib/plp/framing.ts` (encode/decode ESC
   sequences, answer ENQ with ACK, probe with ENQ).
2. Whatever framing `Plp.prt`'s own `PLP Link` uses above that escaped stream —
   still unknown. It is not the `SYN`/`DLE` framing with any CRC trailer, and
   the device gives nothing away: it never initiates, so no specimen frame of
   its own has been captured. Reading `Plp.prt` far enough to recover the frame
   format is the gating task.
3. The Unicode service names (`SYS$RFSVU.*`, `SYS$RPCSU.*`, `SYS$WPRTU*`) and
   whatever else the ER5u RFSV differs in, once a channel can be opened.
