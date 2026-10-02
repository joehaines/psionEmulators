# Lemmings for the Psion Series 5

A faithful port of the *rules* of Lemmings to EPOC Release 1 — the Series
5 — as a native program. Nothing here is Psygnosis's: the engine, the
sprites and all twelve levels are new, written to play the way the
original does (see "What is and is not the same" below).

    bash tools/lemmings/build.sh          # -> tools/lemmings/LEMMINGS.EXE
    gcc ... host/test_host.c ...          # the rules, tested on a PC
    bash tests/integration/test-lemmings-series5.sh   # runs it on a Series 5
    bash tests/integration/test-lemmings-netpad.sh    # ... and on a netpad, in colour
    node --experimental-strip-types tools/lemmings/mkaif.mts   # LEMMINGS.AIF, the icon

## Playing

Pick a level on the title screen (arrows and Enter, or tap it) and get
the lemmings from the trapdoor to the exit. You cannot steer them; you can
only give them skills.

| Keys                 | What                                          |
|----------------------|-----------------------------------------------|
| arrows               | move the crosshair; at the screen's edge the level scrolls |
| Enter / Space        | give the selected skill to the lemming under the crosshair |
| Tab or 1-8           | choose a skill                                |
| + / -                | release rate up / down                        |
| P                    | pause                                         |
| F                    | fast forward                                  |
| N                    | nuke — press twice                            |
| R                    | restart the level                             |
| Esc                  | back to the title screen                      |

On the pen: tap a lemming to give it the selected skill, tap a panel
button, or tap and drag on the minimap to scroll.

The skills are the eight the original has in its first few worlds:
climber, floater, bomber, blocker, builder, basher, miner and digger.

## What a frame costs

A profile of the game on an emulated Series 5 (`--pc-sample-hz`) shows the
game's own code is under one percent of the time: what a frame costs is the
window server answering each filled-rectangle call, about half a
millisecond each. So the work is in keeping the count down:

* `src/present.c` turns "these rectangles of the screen buffer changed" into
  as few filled rectangles as will do. A rectangle may cover pixels that are
  already right, so runs and columns are merged across them; about eight
  rectangles per walking lemming per frame, a few hundred for a whole frame.
  `host/test_host.c` runs the same code and fails if an average frame costs
  more than 250 rectangles or a full repaint more than 8000.
* The art is made to be cheap to send: earth in flat bands rather than
  specks, a hard-edged sky rather than a dithered one. (Dithering the sky
  alone cost 7,000 rectangles a repaint.)
* The game ticks sixteen times a second but draws every second tick, eight
  frames a second, with the timer for the next frame started before the
  work on this one, so the frame's cost sits inside its period instead of
  being added to it.
* The panel is repainted only where a frame touched it.

A full repaint — the title, the start of a level — still costs several
thousand rectangles and some seconds on the emulated machine; scrolling is
a repaint of the view.

## Colour, greys and the icon

Everything is drawn in sixteen palette indices (`src/palette.h`), and the
platform decides what they look like. On a colour panel — the netpad —
they are the colours in `PAL_RGB`: blue sky with a dithered horizon and
clouds, earth with specks, green grass, steel with rivets, bricks with
mortar, a green-haired lemming in a blue robe. On a Series 5 or 5mx they
are the sixteen greys of `PAL_GREY`, chosen so that a lemming still shows
against earth and a button against the panel.

Nothing in the screen device says which it is (its display-mode word
reads the same on a 5mx and a netpad), so the program goes by the panel's
size in twips: the netpad's is wider than 8000, a Series 5's and 5mx's is
7620. That is a heuristic tied to the machines that have been seen, not a
query of the hardware.

`LEMMINGS.AIF` is the application information file — caption "Lemmings"
and a 48 x 48 four-grey icon with its mask — in the layout real ER5 AIFs
have. `mkaif.mts` writes it and reads it back through the decoder the app
library uses. A bare EXE has no place to show an icon on the device
itself (the System screen shows a plain document icon for it), so the
icon's home is the app library, where `applib/epocgames/Lemmings` carries
the EXE and the AIF and the library shows the icon.

## What is and is not the same

The same: a lemming walks at a fixed pace, steps up a ledge of six pixels
and down three, turns at a wall it cannot step, falls, and dies from a
long fall unless it is a floater. Climbers and floaters keep their skill
for life. A bomber's fuse runs while it keeps walking, and it goes off
with a shout. Builders lay twelve bricks, bashers and miners stop at
steel, blockers turn others around, and the trapdoor's release rate can be
raised but never lowered below where the level starts it.

Different: the sprites and levels are mine; the levels are shapes painted
into a 1280 x 160 map when the level starts, not bitmaps; the map is 4 bits
a pixel with 1-7 for earth and 8 and up for steel. There is no sound, no
save file, and no level codes.

## How it is put together

    src/        the game, portable C, no library, no division except through
                udivmod (an ARM710a has neither divide nor 64-bit multiply)
      game.c      terrain, lemmings, skills, panel, screens
      gfx.c       clipped drawing into a 640x240x4bpp frame buffer
      sprites.c   sprites as text
      levels.c    the twelve levels
    epoc/       the Series 5 end
      platform_epoc.c  window server session, a full-screen backed-up window
                       and a gc; paints only what changed
      start.S          entry, import thunks, two kernel calls
      lemmings.spec    the ordinals imported (Ws32, FbsCli, EUser)
    host/       test_host.c: the same game headless, plays scripted
                solutions of every level and checks that they win

Each frame the game repaints only the rectangles that changed and hands
them to `plat_present`, which compares them to what the window already
shows and sends one filled rectangle per run of pixels that differ. That
is the only way found to put pixels on the screen from a bare EXE here: a
bitmap made with CFbsBitmap::Create cannot be Duplicate()d by this
environment's font-and-bitmap server, so BitBlt is out.

The ordinals were read out of the ROM (Cone.dll is the best Rosetta
stone for Ws32), and are the same on the 5mx's ROM.

## Loading it: a quirk worth knowing

This emulator's loader silently refuses an image whose import names are
the wrong length: the DLL name, with its `[uid]` and extension, plus one
for the terminator, must be a multiple of four, or the load fails with no
message at all. `padname.py` pads the hex uid with leading zeros to make
it so (`WS32[1000017d].DLL` becomes `WS32[01000017d].DLL`). A real
machine may not care; the emulator does.

## What works and what does not

Works on an emulated Series 5 (R1 ROM): starts from the System screen,
title screen, level selection with keys and pen, play, the panel,
skills, all twelve levels solvable (checked on the host).

Runs on all three emulated machines that share this window server: the
Series 5 (R1 ROM) and 5mx (opened from the System screen) and the netpad
(opened from a card, which `mkcard.mts` makes). Colour only on the netpad.

Not done or not known: sound; a real Series 5 — nothing here
has run on hardware; speed: in the emulator the game runs at roughly 0.8 of its 16 ticks a second with a dozen lemmings out, and slower with twenty (see "What a frame costs"); real hardware is unmeasured.
