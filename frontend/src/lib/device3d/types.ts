// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The contract between a 3D device model (one builder per machine, e.g.
// psion5mx.ts) and the view that renders it (components/Device3DView.tsx).
//
// A model is plain three.js scene graph plus a handful of hooks: the view
// owns the renderer, camera, lights and input; the model owns geometry,
// materials, the lid mechanism and the mapping from a hit on its surface
// back to something the emulator understands (a digitiser coordinate or an
// EPOC key code). Builders are async: a model may load assets (the 5mx is
// a scanned mesh and its texture). Everything is in millimetres, with y up,
// x to the right and +z towards the person using the machine, the base
// resting on y = 0.

import type * as THREE from 'three';

// Where the LCD and the touch-sensitive pad sit within the device's photo
// skin (fractions of the photo's width / height) — the same numbers the 2D
// device view positions its canvas and touch overlay with, so a tap lands on
// the same digitiser coordinate in both views.
export interface PhotoScreenLayout {
  screenLeft: number;
  screenTop: number;
  screenWidth: number;
  screenHeight: number;
  padLeft: number;
  padTop: number;
  padWidth: number;
  padHeight: number;
  digitiserWidth: number;
  digitiserHeight: number;
}

// A hardware key (or case button) the user can press on the model.
export interface ModelKey {
  id: string;
  code: number;
  // Modifier keys latch on a tap (Shift, Ctrl, Fn) so a touch user can
  // chord them with the next key, the way a phone keyboard's Shift does.
  modifier?: boolean;
}

export interface Device3DModel {
  root: THREE.Group;
  // 0 = shut, 1 = fully open. Called every frame while the lid animates.
  setOpen(t: number): void;
  // The live LCD: the view repaints this canvas from the emulator each frame
  // and flags the texture for upload.
  lcdCanvas: HTMLCanvasElement;
  lcdTexture: THREE.CanvasTexture;
  // Integer upscale of the emulator framebuffer into lcdCanvas — keeps the
  // pixels square and crisp once the texture is filtered at an angle.
  lcdScale: number;
  // Unlit LCD colour, painted under the emulator's partly transparent pixels
  // (the photo skin's glass plays this part in the 2D view).
  lcdBaseColour: string;
  setBacklight(on: boolean, colour?: string): void;
  // The mesh carrying the touch-sensitive face. A ray hit on it, taken
  // into the mesh's frame, is on the digitiser when isOnDigitiser says so,
  // and converts to a digitiser coordinate through toDigitiser.
  digitiserMesh: THREE.Mesh;
  isOnDigitiser(localPoint: THREE.Vector3): boolean;
  // The digitiser's plane in the same frame, for following a pen drag
  // past the edge of the glass.
  digitiserPlane: { point: THREE.Vector3; normal: THREE.Vector3 };
  toDigitiser(localPoint: THREE.Vector3): { x: number; y: number };
  // The mesh carrying the keyboard. A ray hit on it, in the mesh's frame,
  // resolves to the key under it (null between keys or off the deck).
  keyMesh: THREE.Mesh;
  keyAt(localPoint: THREE.Vector3): ModelKey | null;
  // Press feedback for a key id (from the model or a host keyboard event).
  setKeyDown(id: string, down: boolean): void;
  // Host KeyboardEvent.code → model key id, for animating physical typing.
  keyIdForCode(code: string): string | null;
  // Meshes that, clicked while the lid is shut, open it.
  lidOuterMeshes: THREE.Object3D[];
  // Per-frame animation (key travel easing etc.). dt in seconds.
  update(dt: number): void;
  // Camera framing: the point to orbit about and a comfortable distance,
  // for the open and the shut machine.
  focus: { open: THREE.Vector3; closed: THREE.Vector3 };
  dispose(): void;
}

export interface Device3DBuildOptions {
  baseUrl: string;
  layout: PhotoScreenLayout;
  lcdWidth: number;
  lcdHeight: number;
  // Max texture anisotropy and texture size the renderer supports.
  anisotropy: number;
  maxTextureSize: number;
}
