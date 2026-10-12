// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The Psion Series 5mx, from a photogrammetry scan of a real machine.
//
// tools/psion-5-mx.zip holds the scan (an open, French-keyboard 5mx). The
// script tools/scan3d/build_5mx_scan.py levels and scales it to Psion's
// 170 mm width, splits its single shell into the three rigid parts of the
// real mechanism (lid, keyboard tray, base), repaints its texture (clean
// case colours, the UK screen silkscreen and UK key legends — see
// tools/scan3d/texclean.py) and writes:
//   public/device-3d/5mx-scan.glb          the meshes (meshopt)
//   public/device-3d/5mx-scan-{8k,4k}.jpg  the colour texture
//   public/device-3d/5mx-scan-orm.png      roughness / metalness
//   lib/device3d/psion5mx-scan.ts          measurements and the UK keys
//
// Everything here is in the scan's open pose, in millimetres, y up and z
// towards the typist.
//
// The live screen is a plane laid exactly over the scanned LCD glass. A
// tap on the lid is mapped back through the scan's measured LCD and
// digitiser-glass edges to the digitiser's own coordinates, piecewise, so
// the printed icon strips beside and below the LCD land on the same
// silkscreen zones as in the 2D device view.
//
// Closing the lid follows US 5,742,475: the screen leaf is hinged to the
// keyboard leaf (SCAN_5MX.lidHinge, solved from the open and shut poses),
// and as it swings down the keyboard slides back along the base, tipping
// flat; the two stay joined at the hinge the whole way.

import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { MeshoptDecoder } from 'three/examples/jsm/libs/meshopt_decoder.module.js';
import type { Device3DBuildOptions, Device3DModel, ModelKey } from './types';
import { SCAN_5MX as S } from './psion5mx-scan';

// ── Keys ──────────────────────────────────────────────────────────────────
// UK layout, positions measured off the scanned keyboard (see uk_keys() in
// tools/scan3d/build_5mx_scan.py): x across, d along the deck from the
// hinge towards the typist, both mm.
const KEYS: ReadonlyArray<{ id: string; code: number; x0: number; x1: number; d0: number; d1: number; modifier?: boolean }> = S.keys;

// Host KeyboardEvent.code → key id, purely so physical typing visibly
// presses the model's keys. Letters go by the letter (the host's layout
// decides which one was typed); the rest by position.
const HOST_CODE_TO_ID: Record<string, string> = {
  Escape: 'Esc', Backspace: 'Del', Delete: 'Del', Enter: 'Enter', NumpadEnter: 'Enter',
  Tab: 'Tab', CapsLock: 'Tab', ShiftLeft: 'ShiftL', ShiftRight: 'ShiftR',
  ControlLeft: 'Ctrl', ControlRight: 'Ctrl', MetaLeft: 'Fn', MetaRight: 'Fn',
  AltLeft: 'Menu', AltRight: 'Menu', ContextMenu: 'Menu', Space: 'Space',
  Comma: 'Comma', Period: 'Stop', Slash: 'Comma', Quote: 'Quote', Semicolon: 'Quote',
  Home: 'Left', End: 'Right', PageUp: 'Up', PageDown: 'Down',
  ArrowLeft: 'Left', ArrowRight: 'Right', ArrowUp: 'Up', ArrowDown: 'Down',
};

// The deck plane: through the hinge knuckle, tipped forward by the tray's
// tilt. Key tops sit just under it.
const DECK_ORIGIN = new THREE.Vector3(...S.deck.origin);
const DECK_ALONG = new THREE.Vector3(...S.deck.along);
const DECK_UP = new THREE.Vector3(...S.deck.up);
const KEY_TOP_OFFSET = -0.45;   // key tops, below the deck line, mm

// ── Screen frame ──────────────────────────────────────────────────────────
const SCREEN_O = new THREE.Vector3(...S.screenOrigin);
const SCREEN_N = new THREE.Vector3(...S.screenNormal);
const SCREEN_W = new THREE.Vector3(...S.screenUp);
const X_AXIS = new THREE.Vector3(1, 0, 0);

const ease = (t: number) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);

export async function buildPsion5mx(opts: Device3DBuildOptions): Promise<Device3DModel> {
  const { layout, anisotropy } = opts;
  const disposables: Array<{ dispose(): void }> = [];
  const track = <T extends { dispose(): void }>(x: T): T => { disposables.push(x); return x; };

  // ── Load the scan ──
  const loader = new GLTFLoader().setMeshoptDecoder(MeshoptDecoder);
  const texFile = opts.maxTextureSize >= 8192 ? '5mx-scan-8k.jpg' : '5mx-scan-4k.jpg';
  const texLoader = new THREE.TextureLoader();
  const [gltf, tex, ormTex, lidTopTex] = await Promise.all([
    loader.loadAsync(`${opts.baseUrl}device-3d/5mx-scan.glb`),
    texLoader.loadAsync(`${opts.baseUrl}device-3d/${texFile}`),
    texLoader.loadAsync(`${opts.baseUrl}device-3d/5mx-scan-orm.png`),
    texLoader.loadAsync(`${opts.baseUrl}device-3d/5mx-scan-lidtop.png`),
  ]);
  ormTex.flipY = false;
  track(ormTex);
  tex.flipY = false;   // glTF UVs
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = anisotropy;
  track(tex);
  // The texture is the scan's, repainted (tools/scan3d/texclean.py): clean
  // paint and plastic albedo with the room's light taken out, so the
  // scene's lights do the shading. Roughness and metalness come per texel
  // (paint is a metallic finish, plastic and keycaps are not).
  // Double-sided: shut, the parts show slivers of each other's insides
  // where the open-pose scan never saw a surface; the inside of the shell
  // reads as a surface there rather than a hole onto the background.
  const scanMat = track(new THREE.MeshStandardMaterial({
    map: tex, roughnessMap: ormTex, metalnessMap: ormTex, roughness: 1, metalness: 1,
    envMapIntensity: 0.8, side: THREE.DoubleSide,
  }));
  const part = (name: string, material: THREE.Material = scanMat) => {
    let mesh: THREE.Mesh | null = null;
    gltf.scene.getObjectByName(name)?.traverse(c => {
      if (!mesh && (c as THREE.Mesh).isMesh) mesh = c as THREE.Mesh;
    });
    if (!mesh) throw new Error(`5mx scan: no ${name} mesh`);
    const m: THREE.Mesh = mesh;
    (m.material as THREE.Material).dispose();
    m.material = material;
    m.castShadow = m.receiveShadow = true;
    track(m.geometry);
    // Bake gltfpack's dequantisation transform into the geometry so the
    // part can be re-parented under its pivot.
    m.updateWorldMatrix(true, false);
    m.geometry.applyMatrix4(m.matrixWorld);
    m.position.set(0, 0, 0);
    m.quaternion.identity();
    m.scale.set(1, 1, 1);
    m.removeFromParent();
    return m;
  };
  const baseMesh = part('base');
  const trayMesh = part('tray');
  const lidMesh = part('lid');
  // Where the open lid rests on the rear block the scan runs from the lid's
  // back onto the block and down its sides, closing the wedge under the
  // screen. It rides on the lid, as scanned while open, and folds flat into
  // the lid's back as the lid closes (setOpen below).
  const rimMesh = part('rim');
  rimMesh.castShadow = false;
  rimMesh.raycast = () => {};
  // The half of the lid's outside the scan never saw (it faces the rear
  // block while open), from a photograph of a real 5mx shut: a decal over
  // the lid that fades into the scan past the badge.
  lidTopTex.flipY = false;
  lidTopTex.colorSpace = THREE.SRGBColorSpace;
  lidTopTex.anisotropy = anisotropy;
  track(lidTopTex);
  const lidTop = part('lidtop', track(new THREE.MeshStandardMaterial({
    map: lidTopTex, transparent: true, depthWrite: false,
    roughness: 0.6, metalness: 0.1, envMapIntensity: 0.3,
    polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2,
  })));
  lidTop.castShadow = false;
  lidTop.raycast = () => {};

  const root = new THREE.Group();
  root.name = 'psion-5mx';
  root.add(baseMesh);

  // Tray: pivots about its contact with the base's front edge.
  const contact = new THREE.Vector3(0, S.trayContact[0], S.trayContact[1]);
  const trayPivot = new THREE.Group();
  trayPivot.position.copy(contact);
  root.add(trayPivot);
  const tray = new THREE.Group();
  tray.position.copy(contact).negate();
  trayPivot.add(tray);
  tray.add(trayMesh);

  // Lid: hinged to the keyboard tray, so it rides with it.
  const hinge = new THREE.Vector3(0, S.lidHinge[0], S.lidHinge[1]);
  const lidPivot = new THREE.Group();
  lidPivot.position.copy(hinge);
  tray.add(lidPivot);
  const lid = new THREE.Group();
  lid.position.copy(hinge).negate();
  lidPivot.add(lid);
  lid.add(lidMesh, lidTop, rimMesh);

  // ── Live LCD over the scanned glass ──
  const lcdScale = 2;
  const lcdCanvas = document.createElement('canvas');
  lcdCanvas.width = opts.lcdWidth * lcdScale;
  lcdCanvas.height = opts.lcdHeight * lcdScale;
  const lcdTexture = track(new THREE.CanvasTexture(lcdCanvas));
  lcdTexture.colorSpace = THREE.SRGBColorSpace;
  lcdTexture.anisotropy = anisotropy;
  lcdTexture.generateMipmaps = true;
  lcdTexture.minFilter = THREE.LinearMipmapLinearFilter;
  const lcdMat = track(new THREE.MeshStandardMaterial({
    map: lcdTexture, emissiveMap: lcdTexture, emissive: '#ffffff', emissiveIntensity: 0.22,
    roughness: 0.35, metalness: 0,
    polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2,
  }));
  // A point on the screen plane from (x, w) as measured on the scan.
  const onScreen = (x: number, w: number, lift: number) => SCREEN_O.clone()
    .addScaledVector(X_AXIS, x - SCREEN_O.x).addScaledVector(SCREEN_W, w).addScaledVector(SCREEN_N, lift);
  const screenBasis = new THREE.Matrix4().makeBasis(X_AXIS, SCREEN_W, SCREEN_N);
  const screenPlane = (r: { x0: number; x1: number; w0: number; w1: number }, lift: number,
                       mat: THREE.Material) => {
    const g = track(new THREE.PlaneGeometry(r.x1 - r.x0, r.w1 - r.w0));
    g.applyMatrix4(screenBasis);
    const m = new THREE.Mesh(g, mat);
    m.position.copy(onScreen((r.x0 + r.x1) / 2, (r.w0 + r.w1) / 2, lift));
    m.raycast = () => {};
    lid.add(m);
    return m;
  };
  screenPlane(S.lcd, 0.35, lcdMat);
  // Glass: a faint glossy sheet over the digitiser, for reflections.
  screenPlane(S.pad, 0.5, track(new THREE.MeshPhysicalMaterial({
    color: '#ffffff', metalness: 0, roughness: 0.08, transparent: true, opacity: 0.07,
    clearcoat: 1, clearcoatRoughness: 0.06, depthWrite: false,
  })));

  // Digitiser coordinates of the LCD's edges, from the photo layout the 2D
  // view uses — the anchors for the piecewise mapping below.
  const dw = layout.digitiserWidth, dh = layout.digitiserHeight;
  const lcdDx0 = ((layout.screenLeft - layout.padLeft) / layout.padWidth) * dw;
  const lcdDx1 = ((layout.screenLeft + layout.screenWidth - layout.padLeft) / layout.padWidth) * dw;
  const lcdDy0 = ((layout.screenTop - layout.padTop) / layout.padHeight) * dh;
  const lcdDy1 = ((layout.screenTop + layout.screenHeight - layout.padTop) / layout.padHeight) * dh;
  // a0 < a1 < a2 < a3 (scan mm) → b0..b3 (digitiser), linear per segment.
  const piece = (v: number, a: number[], b: number[]) => {
    const i = v < a[1] ? 0 : v <= a[2] ? 1 : 2;
    return b[i] + ((v - a[i]) / (a[i + 1] - a[i])) * (b[i + 1] - b[i]);
  };
  // `p` in the lid's frame (= the scan's open pose).
  const toDigitiser = (p: THREE.Vector3) => {
    const w = p.clone().sub(SCREEN_O).dot(SCREEN_W);
    return {
      x: Math.round(piece(p.x, [S.pad.x0, S.lcd.x0, S.lcd.x1, S.pad.x1], [0, lcdDx0, lcdDx1, dw])),
      // w runs up the lid; digitiser y runs down the screen.
      y: Math.round(piece(-w, [-S.pad.w1, -S.lcd.w1, -S.lcd.w0, -S.pad.w0], [0, lcdDy0, lcdDy1, dh])),
    };
  };
  const isOnDigitiser = (p: THREE.Vector3) => {
    const d = p.clone().sub(SCREEN_O);
    const w = d.dot(SCREEN_W);
    return Math.abs(d.dot(SCREEN_N)) < 2.5 && p.x >= S.pad.x0 && p.x <= S.pad.x1
      && w >= S.pad.w0 && w <= S.pad.w1;
  };

  // ── Keys: hit zones on the deck, and a press shadow per key ──
  const deckBasis = new THREE.Matrix4().makeBasis(X_AXIS, DECK_ALONG.clone().negate(), DECK_UP);
  const shadows = new Map<string, Array<{ mesh: THREE.Mesh; mat: THREE.MeshBasicMaterial; level: number }>>();
  {
    for (const z of KEYS) {
      const g = track(new THREE.PlaneGeometry(z.x1 - z.x0 - 1.2, z.d1 - z.d0 - 1.2));
      g.applyMatrix4(deckBasis);
      const mat = track(new THREE.MeshBasicMaterial({
        color: '#000000', transparent: true, opacity: 0, depthWrite: false,
        polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -4,
      }));
      const m = new THREE.Mesh(g, mat);
      m.position.copy(DECK_ORIGIN)
        .addScaledVector(DECK_ALONG, (z.d0 + z.d1) / 2)
        .addScaledVector(DECK_UP, KEY_TOP_OFFSET + 0.15);
      m.position.x = (z.x0 + z.x1) / 2;
      m.visible = false;
      m.raycast = () => {};
      tray.add(m);
      const list = shadows.get(z.id) ?? [];
      list.push({ mesh: m, mat, level: 0 });
      shadows.set(z.id, list);
    }
  }
  // `p` in the tray's frame (= the scan's open pose).
  const keyAt = (p: THREE.Vector3): ModelKey | null => {
    const rel = p.clone().sub(DECK_ORIGIN);
    if (rel.dot(DECK_UP) < -6) return null;   // the tray's flanks and underside
    const d = rel.dot(DECK_ALONG);
    for (const z of KEYS) {
      if (d >= z.d0 && d <= z.d1 && p.x >= z.x0 && p.x <= z.x1) {
        return { id: z.id, code: z.code, modifier: z.modifier };
      }
    }
    return null;
  };

  // ── Lid / tray motion ──
  // One parameter: the lid's angle on its hinge. The keyboard's travel
  // follows the lid's angle (the linkage of the patent): it starts to slide
  // back as soon as the lid lifts, and is fully home as the lid lands, so
  // the screen's foot never digs into the rear block and its free edge
  // comes down onto the keys from above.
  //
  // The raised strip behind the screen, which the open screen leans on, is
  // flush with the lid on the shut machine (the back of a shut 5mx is flat).
  // The scan only shows it raised, so as the lid closes the base above
  // SETTLE_FLOOR at the back is lowered, proportionally, until its top meets
  // the shut lid's.
  const SETTLE_FLOOR = 17, SETTLE_BACK_OF = -20;
  // The rim folds away over the first part of the keyboard's travel; folded,
  // it lies RIM_INSET mm inside the lid's back, within the cap's outline.
  const RIM_FOLD = 0.25, RIM_INSET = 0.3, RIM_X = 84, RIM_A0 = -33.5, RIM_A1 = 8.5;
  const basePos = baseMesh.geometry.attributes.position as THREE.BufferAttribute;
  const settleIdx: number[] = [];
  const settleY0: number[] = [];
  let rearTop = 0;
  for (let i = 0; i < basePos.count; i++) {
    const y = basePos.getY(i);
    if (basePos.getZ(i) < SETTLE_BACK_OF && y > SETTLE_FLOOR) {
      settleIdx.push(i);
      settleY0.push(y);
      rearTop = Math.max(rearTop, y);
    }
  }
  const setPose = (a: number) => {
    lidPivot.rotation.x = S.lidHingeSwing * a;
    const f = Math.sin(a * Math.PI / 2);   // keyboard travel leads the lid
    trayPivot.rotation.x = S.trayShutAngle * f;
    trayPivot.position.z = contact.z - S.traySlide * f;
    return f;
  };
  // Where the shut lid's top is: the settled strip's target.
  setPose(1);
  root.updateMatrixWorld(true);
  const lidPos = lidMesh.geometry.attributes.position as THREE.BufferAttribute;
  const ys: number[] = [];
  const v = new THREE.Vector3();
  for (let i = 0; i < lidPos.count; i += 3) ys.push(v.fromBufferAttribute(lidPos, i).applyMatrix4(lidMesh.matrixWorld).y);
  ys.sort((p, q) => p - q);
  const shutLidTop = ys[Math.floor(ys.length * 0.995)];
  const settledScale = (shutLidTop - SETTLE_FLOOR) / (rearTop - SETTLE_FLOOR);
  // The rim, folded: pressed into the plane just inside the lid's back
  // cap and kept within its outline, so the shut lid hides it.
  const rimPos = rimMesh.geometry.attributes.position as THREE.BufferAttribute;
  const rimOpen = Float32Array.from(rimPos.array as ArrayLike<number>);
  const rimFolded = new Float32Array(rimOpen.length);
  for (let i = 0; i < rimPos.count; i++) {
    const d = v.fromArray(rimOpen, i * 3).sub(SCREEN_O);
    const x = THREE.MathUtils.clamp(rimOpen[i * 3], -RIM_X, RIM_X);
    const a = THREE.MathUtils.clamp(d.dot(SCREEN_W), RIM_A0, RIM_A1);
    const s = Math.max(d.dot(SCREEN_N), S.lidBack + RIM_INSET);
    onScreen(x, a, s).toArray(rimFolded, i * 3);
  }
  rimMesh.frustumCulled = false;   // its bounds are the open pose's
  let lastSettle = -1;
  const setOpen = (t: number) => {
    const f = setPose(1 - ease(t));        // 0 open … 1 shut
    const k = 1 + (settledScale - 1) * f;
    if (Math.abs(k - lastSettle) < 1e-4) return;
    lastSettle = k;
    for (let j = 0; j < settleIdx.length; j++) {
      basePos.setY(settleIdx[j], SETTLE_FLOOR + (settleY0[j] - SETTLE_FLOOR) * k);
    }
    basePos.needsUpdate = true;
    const g = THREE.MathUtils.smoothstep(f, 0, RIM_FOLD);
    const arr = rimPos.array as Float32Array;
    for (let i = 0; i < arr.length; i++) arr[i] = rimOpen[i] + (rimFolded[i] - rimOpen[i]) * g;
    rimPos.needsUpdate = true;
  };
  setOpen(1);

  const keyDown = new Set<string>();
  const update = (dt: number) => {
    const a = 1 - Math.exp(-dt * 30);
    for (const [id, list] of shadows) {
      const target = keyDown.has(id) ? 0.4 : 0;
      for (const s of list) {
        s.level += (target - s.level) * a;
        s.mat.opacity = s.level;
        s.mesh.visible = s.level > 0.005;
      }
    }
  };

  return {
    root,
    setOpen,
    lcdCanvas,
    lcdTexture,
    lcdScale,
    lcdBaseColour: '#a7ab8c',
    setBacklight: (on: boolean) => { lcdMat.emissiveIntensity = on ? 0.95 : 0.22; },
    digitiserMesh: lidMesh,
    isOnDigitiser,
    digitiserPlane: { point: SCREEN_O.clone(), normal: SCREEN_N.clone() },
    toDigitiser,
    keyMesh: trayMesh,
    keyAt,
    setKeyDown: (id, down) => { if (down) keyDown.add(id); else keyDown.delete(id); },
    keyIdForCode: code => {
      if (/^Key[A-Z]$/.test(code)) return code.slice(3);
      if (/^Digit[0-9]$/.test(code)) return code.slice(5);
      return HOST_CODE_TO_ID[code] ?? null;
    },
    lidOuterMeshes: [lidMesh],
    update,
    focus: { open: new THREE.Vector3(0, 24, 6), closed: new THREE.Vector3(0, 12, 2) },
    dispose: () => { for (const d of disposables) d.dispose(); },
  };
}
