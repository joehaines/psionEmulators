# 3D device view (experimental)

A third way to show the running machine, alongside the flat LCD and the photo
"device view": a 3D model you can turn, zoom and pan, with the live screen on
it, a working touchscreen and keyboard, and a lid that opens and shuts.

It is behind **Experimental features** (Settings). With that on, machines that
have a model get a red-ringed 3D button next to the device-view button in the
header. Only the Series 5mx has a model so far.

## Using it

- **Turn:** drag with one finger or the left mouse button.
- **Zoom:** pinch, or scroll the wheel (it zooms towards the pointer).
- **Move:** two fingers, or drag with the right mouse button.
- **Use the machine:** a press that starts on the screen is the stylus, and a
  press on a key types it. The printed icon strips beside and below the LCD
  work too. Shift, Ctrl and Fn latch when tapped, so on a touchscreen you tap
  Shift and then the letter. The keyboard and screen silkscreen are UK
  English, though the scanned machine was French; see below.
- **Lid:** use *Close lid* / *Open lid* in the view's toolbar or the control
  bar, or click the shut lid. The machine keeps running with the lid shut, but
  the screen, digitiser and keyboard can't be reached, as in the 2D lid.
- **Views:** ⟲ resets the camera. Front, Top, Side and Back swing round to
  those angles. Double-clicking the background also resets.
- **Screenshot** (debug bar): with *Include surround* ticked, it saves the 3D
  view from the current angle at twice the on-screen resolution, on the same
  backdrop. Unticked, it saves the bare LCD as before.

## How it's built

The 5mx is a photogrammetry scan of a real machine (`tools/psion-5-mx.zip`, an
open French 5mx), not a model drawn by hand. The scan supplies the shape; its
texture is repainted, so the colours and printing are clean and UK.

| File | Role |
| --- | --- |
| `tools/scan3d/build_5mx_scan.py` | Turns the scan into the web assets below, and holds the UK key table. Run it from `tools/scan3d/`; it needs numpy, scipy, Pillow and `npx gltfpack`, and takes about a minute and a half. |
| `tools/scan3d/texclean.py` | Repaints the scan's texture (see below). |
| `tools/scan3d/texmap.py` | Works out where each texel of the texture sits on the model. |
| `tools/scan3d/fonts/` | Jost (SIL Open Font License), for the key legends. |
| `tools/scan3d/5mx-shut-top-photo.jpg` | A real 5mx photographed shut from above, for the part of the lid the scan never saw. |
| `frontend/public/device-3d/5mx-scan.glb` | The scan split into its three moving parts (meshopt-compressed). |
| `frontend/public/device-3d/5mx-scan-{8k,4k}.jpg` | The repainted colour texture. The 8k one loads where the GPU supports it. |
| `frontend/public/device-3d/5mx-scan-orm.png` | Roughness and metalness per texel. |
| `frontend/public/device-3d/5mx-scan-lidtop.png` | The lid-top decal, made from the photo. |
| `frontend/src/lib/device3d/psion5mx-scan.ts` | Generated: screen plane, LCD and glass edges, hinge, the UK key zones and codes. |
| `frontend/src/lib/device3d/psion5mx.ts` | Loads the scan, lays the live LCD over the scanned glass, maps taps and keys, and moves the parts. |
| `frontend/src/components/Device3DView.tsx` | Renderer, lights, camera controls, input routing, lid animation, capture. Lazy-loaded, so three.js only downloads when the view is opened. |
| `frontend/src/lib/device3d/index.ts` | Which device ids have a model, and how to load each one. |
| `frontend/src/lib/device3d/types.ts` | The contract between a model and the view. |

What the script does:

1. **Aligns the scan.** It levels the base's underside, takes the width from
   the screen's plane, and scales the scan to Psion's published 170 mm width.
   The other dimensions then come out right too: the base is 90 mm deep.
2. **Measures the screen.** It finds the LCD and digitiser-glass edges from the
   screen face flattened into its own plane. The live LCD is laid exactly over
   the scanned glass. A tap is mapped piecewise through those edges to the
   digitiser's coordinates, so the printed strips land on the same zones as in
   the 2D view.
3. **Splits the shell** into the lid, the keyboard tray and the base. It uses
   the geometry, and the texture's islands where the two faces meet at the
   hinge.
4. **Works out the mechanism.** Shut, the keyboard slides back about 31 mm and
   tips flat, and the lid lies face down on the keys. Following the patent
   (US 5,742,475), the lid is hinged to the keyboard; the hinge is the one
   point the open and shut poses agree on, and it comes out at the foot of the
   screen. As the lid closes, the keyboard slides home with it, so the two
   never come apart. The raised strip behind the screen, which the open
   screen leans on, settles flush with the lid as it shuts: the back of a shut
   5mx is flat, but the scan only shows that strip raised. Where the open
   screen rests on that strip, the scan runs from the lid's back onto the
   block and down its sides, closing the wedge under the screen. That piece
   of the scan rides on the lid, shown as scanned while the lid is open, and
   folds flat into the lid's back as the lid starts to close. So there is no
   gap behind the open screen, and nothing sticks up from the shut lid.
5. **Repaints the texture.** A scan's texture is a photograph, with the room's
   light, reflections and shadows baked in, and those are wrong under the 3D
   view's lights and upside down once the lid shuts. Each texel is repainted
   by what it is on the machine:
   - case surfaces get one clean colour per material (warm grey paint, black
     plastic), keeping only detail finer than about 1.5 mm from the scan:
     seams, grilles, ports, printing. Which material is decided by position
     where the photos are unambiguous (black battery barrel, grey lower shell,
     grey lid) and by the scan's colour warmth elsewhere;
   - the screen face is the UK device photo (`device-skins/5mx.png`), mapped
     through the measured LCD and glass edges, so the silkscreen is English and
     lines up with the touch mapping;
   - key tops are clean keycap black with UK legends drawn from the table in
     `build_5mx_scan.py`, transcribed from photos of a UK 5mx.
6. **Fills what an open-pose scan can't see.** When the machine is open, the
   hinge half of the lid's outside faces the rear block, so the scan has
   nothing there, the top of the badge included. That half is filled from the
   photo, rectified onto the lid and colour-matched to the scan, fading into
   the scan past the badge. Two small flat patches in the scanned paint colour
   cover the stretch of base the open lid stood in.

The LCD is the emulator's canvas, copied into a texture every frame. In worker
mode that canvas is an OffscreenCanvas placeholder, and the emulator can only
hand it over once. So the 2D frame stays mounted, out of sight, while the 3D
view is showing.

## Adding another machine

The best source is a 3D scan of the real machine, plus a photo of anything the
scan couldn't see.

1. Write a converter like `tools/scan3d/build_5mx_scan.py`: align and scale
   the scan, measure the screen, and split it into its moving parts.
2. Write `lib/device3d/<machine>.ts` exporting an async builder that returns a
   `Device3DModel` (see `types.ts`). Work in millimetres, with y up, +z
   towards the user and the base on y = 0.
3. Register it in `LOADERS` in `lib/device3d/index.ts`.
4. Put its chunk name in the PWA `globIgnores` in `vite.config.ts` next to
   `psion5mx-*.js`, so the service worker doesn't precache it for everyone.
   `public/device-3d/` is already excluded.

A scan only captures one pose. Anything hidden in that pose (for the 5mx, the
back of the open lid) has to come from a real photo, not a guess.
