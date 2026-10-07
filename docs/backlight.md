# LCD backlight

Every Psion here with a backlight has it the way the real machine does: the
emulated OS switches it, from the machine's own key, and the frontend only
shows what the hardware is doing. There is no frontend-side light switch, so
the OS's own behaviour — its auto-off timer, switching the lamp off at
power-down, the Series 7's brightness steps — all comes through unaided.

## Which machines, which output, which key

| Machine | Output the ROM drives | Key |
|---|---|---|
| Series 5mx, 5mx Pro, Ericsson MC218 | Windermere port D bit 4 | Fn+Space |
| Revo (Conan), both ROM builds — a blue panel | Windermere port D bit 4 | Fn+Space |
| Series 5 | CL-PS7110 port C bit 4 | Fn+Space |
| Osaris | CL-PS7111 port E bit 2 | Fn+Space |
| Geofox One | CL-PS7110 port D bit 1 | its own Backlight key (scancode 0x97) |
| Series 7, netBook | ASIC14 register 0x18 — a 5-bit brightness | Fn+Space steps it 0, 4, 8 … 28, 31, then wraps |
| Series 3mx | ASIC9 port C bit 5 | Psion+Space |
| Workabout | ASIC9 port C bit 0 | its own Backlight key (matrix col 7, 0x040) |
| WorkaboutMX | bit 6 of the board latch at I/O 0x100 | its own Backlight key (col 7, 0x040) |
| HC120 | ASIC2 Control3 bit 7 | its own Backlight key (col 6, 0x40) |

Every row was found the same way: boot the machine under the harness, press
the key, and see which output moved — then, on the SIBO machines, read the
ROM's backlight service to confirm it. That service has the same shape in
every EPOC16 ROM that has one (AL = 0 off, 1 on, 2 toggle, 3 query), which is
also how the machines *without* a backlight were ruled out: the Series 3, 3a,
Siena, MC200 and MC400 ROMs have no such service at all.

Two machines drive the pin with nothing on the end of it: the Revo runs the
5mx's code and flips port D bit 4 on Fn+Space, and the Series 3c runs the
3mx's and flips port C bit 5 on Psion+Space. Neither board has a lamp, so
both report no backlight. The Conan runs the same code on the Revo's board
but does have one, lit blue, so it reports the pin like the 5mx does. The netpad's colour panel is lit whenever the
machine is on, and its OS has nothing to switch.

The OS's own backlight timers show through: a 5mx puts its light out a minute
after it was switched on, a 3mx two minutes.

On the Series 7 the key's modifiers matter. Fn+Space steps the brightness up;
Ctrl+Fn+Space steps it down; Ctrl+Shift+Fn+Space jumps to full.

## What had to be fixed in the hardware models

- **ASIC9 GPIO read-back.** Port A/B and port C/D data registers read 0 for
  every pin the guest was driving. The 3mx toggles its backlight by a
  read-modify-write of port C, so every press read "off" and wrote "on": the
  light came on and could never be put out. Driven pins now read back the
  level they are driven to (port C/D's DDR bits are 1 for an *input* — the
  Siena's locale straps confirm the polarity; port A/B's are 1 for an output).
- **WorkaboutMX configuration EEPROM.** The MX keeps its factory configuration
  in a 93C46 serial EEPROM, bit-banged on ASIC9 port B (CS B3, SK B0, DI B1,
  DO B2), and its backlight service refuses every call unless word 1 bit 4
  says a backlight is fitted. With nothing answering on DO, all three words
  read 0 — which passes the kernel's checksum and describes a machine with no
  backlight (and the Backlight key fell back to nudging the contrast). It is
  now a real Microwire EEPROM model (`core/microwire_eeprom.h`) programmed
  with the kernel's own defaults. Port B's read-modify-writes are also what
  the A/B read-back fix above was needed for.
- **WorkaboutMX board latch.** I/O 0x100 on the MX is a write-only control
  latch (bit 6 = backlight). It was being routed to the Condor UART model the
  3a/3c/Siena have at that address, so the MX's latch writes landed in a UART
  transmit register.
- **Series 7 / netBook brightness.** The panel is transmissive, so the lamp is
  all the light it has: `readLCDIntoBuffer` now scales the panel output by the
  ASIC14 brightness, exactly as before at the OS's own boot level (8 of 31),
  falling to a quarter at 0 and up to a third brighter at 31. The netBook's
  bootloader sets 2 before the OS takes over, so its splash is dimmer than the
  desktop that follows.

## In the frontend

The core reports `hasBacklight`, `isBacklightDimmable`, `getBacklightLevel`
(0–100) and `getBacklightKey` (EmuBase, `core/emubase.h`), passed through
`getDeviceInfo` / `getBacklightLevel` in the WASM build. `useBacklight` builds
everything from those, so a machine gets the controls exactly when its
hardware has a backlight:

- a header bulb and a **Light** button in the control bar, both pressing the
  machine's own key — for the Series 7 / netBook they show the brightness;
- the **Ctrl+Shift+L** host shortcut. Fn+Space's host equivalent is
  Meta+Space, which macOS (Spotlight) and most Windows and Linux desktops
  (input switching) never let through to a page. The shortcut releases the
  host's held Ctrl and Shift and waits 250 ms before pressing the machine's
  key, because the Series 7's keyboard driver takes ~0.1 s to register a
  modifier going up — any sooner and it reads Ctrl+Fn+Space, "dimmer". The
  machines' own chords still work wherever the host passes them (Alt+Space is
  the 3mx's Psion+Space);
- the lit EL panel: the blit path (main thread and worker alike) re-lights
  the LCD — paper takes on the EL colour, ink stays dark — and a glow is
  cast onto the case. The page goes "lights out" around it (not in an
  embed, or the desktop app);
- one fade for the whole switch. When the UI sees the machine's output
  change it starts everything at once and over the same 400 ms
  (`BACKLIGHT_FADE_MS`): the LCD blends between its unlit and lit renderings
  frame by frame in the blit path (smoothstep), and the glow, the header
  bulb and every page colour transition together (`.psion-lights-fading`,
  ease-in-out) — rather than the LCD snapping on its own frame while the
  chrome caught up at its own pace.

The EL colours (`lib/backlight.ts`) are eyeballed from photos of lit machines
(the Conan's blue is set as a blue rather than matched to a photo);
everything else on this page is measured.

## Tests

- `tests/integration/test-backlight.sh` — every backlit machine pressed with
  its own key (`harness/run --press-backlight`), both Conan builds included,
  checked on and off (Series 7:
  brightness 8 → 12 → 16), plus the Revo and 3c checked to stay dark. Add
  `--slow` for the netBook, which boots from its OS card.
- `frontend/src/lib/__tests__/backlight.test.mts` (`npm run test:backlight`) —
  the lit-panel colours, the worker's copy of the lookup matching the
  module's, and the host shortcut.
