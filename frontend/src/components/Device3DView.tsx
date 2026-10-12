// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// 3D device view (experimental). Renders the running machine as a 3D model
// you can turn, zoom and pan, with the live LCD on its screen, a working
// touchscreen and keyboard, and a lid that opens and shuts.
//
// Loaded lazily from EmulatorView so three.js only downloads for people who
// switch the view on. The model for each machine lives in lib/device3d/.
//
// Input routing: a pointer that goes down on the screen or on a key is the
// machine's (pen or key press) and never reaches the camera controls;
// anything else — the case, the background — turns, zooms or pans the view.
// One finger / left button turns, two fingers / right button pans, pinch /
// wheel zooms.

import { forwardRef, useEffect, useImperativeHandle, useRef, useState } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/examples/jsm/environments/RoomEnvironment.js';
import { load3DModel, type Device3DModel, type PhotoScreenLayout } from '../lib/device3d';
import type { ModelKey } from '../lib/device3d/types';

export interface Device3DHandle {
  // Renders the current view at `scale` × the on-screen size, over the
  // same backdrop the page shows, and returns it as a 2D canvas.
  capture(scale?: number): HTMLCanvasElement | null;
}

interface Props {
  deviceId: string;
  baseUrl: string;
  layout: PhotoScreenLayout;
  lcdWidth: number;
  lcdHeight: number;
  // The emulator's own LCD canvas (an OffscreenCanvas placeholder in worker
  // mode), read every frame into the model's screen texture.
  getLcdSource: () => HTMLCanvasElement | null;
  lidOpen: boolean;
  onToggleLid(): void;
  backlightOn: boolean;
  backlightColor?: string;
  paused: boolean;
  onPenDown(x: number, y: number): void;
  onPenMove(x: number, y: number): void;
  onPenUp(): void;
  onKey(code: number, down: boolean): void;
  // Fills its parent rather than taking the page's default height.
  fill?: boolean;
}

// Studio backdrop, light and dark. Shared by the CSS behind the WebGL
// canvas and by capture(), so a screenshot shows what the page shows.
const BACKDROP = {
  light: ['#fbfaf7', '#e7e4dc', '#c9c4b8'],
  dark:  ['#47484d', '#2a2b2f', '#141416'],
};
const prefersDark = () =>
  typeof window !== 'undefined' && window.matchMedia?.('(prefers-color-scheme: dark)').matches;
const backdropCss = (dark: boolean) => {
  const [a, b, c] = dark ? BACKDROP.dark : BACKDROP.light;
  return `radial-gradient(120% 95% at 50% 36%, ${a} 0%, ${b} 52%, ${c} 100%)`;
};

// Camera presets: azimuth (degrees, 0 = straight on, negative = from the
// left) and elevation above the desk.
const VIEWS = {
  home:  { az: -16, el: 34, label: 'Reset view' },
  // The open screen lies back ~32° from the desk, so face-on is high up.
  front: { az: 0,   el: 52, label: 'Face on' },
  top:   { az: 0,   el: 82, label: 'From above' },
  side:  { az: 90,  el: 6,  label: 'From the side' },
  back:  { az: 160, el: 22, label: 'From behind' },
} as const;
type ViewName = keyof typeof VIEWS;

const LID_SECONDS = 1.15;
// Shortest a key is held down in the machine. EPOC scans the keyboard
// matrix on a timer, so a click whose up follows its down within the same
// millisecond is never seen; the on-screen SIBO buttons hold for 120 ms for
// the same reason.
const MIN_KEY_HOLD_MS = 120;

const Device3DView = forwardRef<Device3DHandle, Props>(function Device3DView(props, ref) {
  const wrapRef = useRef<HTMLDivElement>(null);
  const propsRef = useRef(props);
  propsRef.current = props;
  const [status, setStatus] = useState<'loading' | 'ready' | 'error'>('loading');
  const [errorText, setErrorText] = useState('');
  const [hintVisible, setHintVisible] = useState(true);
  const [dark, setDark] = useState(prefersDark);
  // Imperative bridges into the effect-owned scene.
  const apiRef = useRef<{
    capture(scale: number): HTMLCanvasElement | null;
    goTo(view: ViewName): void;
  } | null>(null);

  useEffect(() => {
    const mq = window.matchMedia?.('(prefers-color-scheme: dark)');
    if (!mq) return;
    const on = () => setDark(mq.matches);
    mq.addEventListener('change', on);
    return () => mq.removeEventListener('change', on);
  }, []);

  useImperativeHandle(ref, () => ({
    capture: (scale = 2) => apiRef.current?.capture(scale) ?? null,
  }), []);

  useEffect(() => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    let disposed = false;
    let cleanup = () => {};

    (async () => {
      const build = await load3DModel(props.deviceId);
      if (disposed) return;
      if (!build) { setStatus('error'); setErrorText('No 3D model for this machine yet.'); return; }

      let renderer: THREE.WebGLRenderer;
      try {
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
      } catch {
        setStatus('error');
        setErrorText('3D view needs WebGL, which this browser has turned off.');
        return;
      }
      renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
      renderer.outputColorSpace = THREE.SRGBColorSpace;
      renderer.toneMapping = THREE.ACESFilmicToneMapping;
      renderer.toneMappingExposure = 0.95;
      renderer.shadowMap.enabled = true;
      renderer.shadowMap.type = THREE.PCFSoftShadowMap;
      const canvas = renderer.domElement;
      canvas.style.display = 'block';
      canvas.style.width = '100%';
      canvas.style.height = '100%';
      canvas.style.touchAction = 'none';
      canvas.style.outline = 'none';
      wrap.prepend(canvas);

      const scene = new THREE.Scene();
      const pmrem = new THREE.PMREMGenerator(renderer);
      const room = new RoomEnvironment();
      const envTex = pmrem.fromScene(room, 0.04).texture;
      scene.environment = envTex;
      scene.environmentIntensity = 0.6;

      // Lights: a soft key from above front-left that casts the shadow, a
      // cooler rim from behind right to pick out the edges, a faint fill.
      const key = new THREE.DirectionalLight('#fff6ea', 1.7);
      key.position.set(-170, 380, 260);
      key.castShadow = true;
      key.shadow.mapSize.set(2048, 2048);
      Object.assign(key.shadow.camera, { left: -170, right: 170, top: 170, bottom: -170, near: 50, far: 1200 });
      key.shadow.bias = -0.0005;
      key.shadow.normalBias = 0.35;
      scene.add(key);
      const rim = new THREE.DirectionalLight('#dfe8ff', 0.9);
      rim.position.set(220, 160, -260);
      scene.add(rim);
      scene.add(new THREE.HemisphereLight('#ffffff', '#8a857a', 0.3));

      // Ground: a shadow catcher plus a soft contact blob, both invisible
      // apart from the darkening they add, so the device sits on the page's
      // backdrop rather than on a visible floor.
      const ground = new THREE.Group();
      const catcher = new THREE.Mesh(
        new THREE.PlaneGeometry(1600, 1600).rotateX(-Math.PI / 2),
        new THREE.ShadowMaterial({ opacity: 0.2 }),
      );
      catcher.receiveShadow = true;
      ground.add(catcher);
      const blobCanvas = document.createElement('canvas');
      blobCanvas.width = blobCanvas.height = 256;
      {
        const g = blobCanvas.getContext('2d')!;
        const grd = g.createRadialGradient(128, 128, 0, 128, 128, 128);
        grd.addColorStop(0, 'rgba(0,0,0,0.5)');
        grd.addColorStop(0.55, 'rgba(0,0,0,0.22)');
        grd.addColorStop(1, 'rgba(0,0,0,0)');
        g.fillStyle = grd;
        g.fillRect(0, 0, 256, 256);
      }
      const blobTex = new THREE.CanvasTexture(blobCanvas);
      const blob = new THREE.Mesh(
        new THREE.PlaneGeometry(240, 150).rotateX(-Math.PI / 2),
        new THREE.MeshBasicMaterial({ map: blobTex, transparent: true, depthWrite: false }),
      );
      blob.position.set(0, 0.05, 6);
      ground.add(blob);
      scene.add(ground);

      const p0 = propsRef.current;
      let model: Device3DModel;
      try {
        model = await build({
          baseUrl: p0.baseUrl,
          layout: p0.layout,
          lcdWidth: p0.lcdWidth,
          lcdHeight: p0.lcdHeight,
          anisotropy: renderer.capabilities.getMaxAnisotropy(),
          maxTextureSize: renderer.capabilities.maxTextureSize,
        });
      } catch (err) {
        renderer.dispose();
        canvas.remove();
        throw err;
      }
      if (disposed) {
        model.dispose();
        renderer.dispose();
        canvas.remove();
        return;
      }
      scene.add(model.root);
      const lcdCtx = model.lcdCanvas.getContext('2d')!;

      // ── Camera ──
      const camera = new THREE.PerspectiveCamera(30, 1, 5, 6000);
      const controls = new OrbitControls(camera, canvas);
      controls.enableDamping = true;
      controls.dampingFactor = 0.085;
      controls.rotateSpeed = 0.8;
      controls.zoomSpeed = 0.9;
      controls.panSpeed = 0.9;
      controls.screenSpacePanning = true;
      controls.zoomToCursor = true;
      controls.minDistance = 80;
      controls.maxDistance = 1400;
      controls.touches = { ONE: THREE.TOUCH.ROTATE, TWO: THREE.TOUCH.DOLLY_PAN };
      controls.mouseButtons = { LEFT: THREE.MOUSE.ROTATE, MIDDLE: THREE.MOUSE.DOLLY, RIGHT: THREE.MOUSE.PAN };
      controls.addEventListener('start', () => setHintVisible(false));

      let openT = p0.lidOpen ? 1 : 0;
      let lidOverride: number | null = null;   // dev hook only
      model.setOpen(openT);
      const focusAt = (t: number) => model.focus.closed.clone().lerp(model.focus.open, t);

      // Distance that frames the machine snugly: about 96 mm round its
      // centre vertically, and its 170 mm width across with room for the
      // near corner to loom in an angled view — the latter is what limits a
      // portrait phone.
      const fitDistance = () => {
        const vfov = THREE.MathUtils.degToRad(camera.fov);
        const hfov = 2 * Math.atan(Math.tan(vfov / 2) * camera.aspect);
        return Math.max(96 / Math.sin(vfov / 2), 122 / Math.tan(hfov / 2));
      };
      const viewPose = (name: ViewName) => {
        const v = VIEWS[name];
        const az = THREE.MathUtils.degToRad(v.az), el = THREE.MathUtils.degToRad(v.el);
        const target = focusAt(openT);
        const d = fitDistance();
        const pos = new THREE.Vector3(Math.sin(az) * Math.cos(el), Math.sin(el), Math.cos(az) * Math.cos(el))
          .multiplyScalar(d).add(target);
        return { pos, target };
      };
      let tween: { from: { pos: THREE.Vector3; target: THREE.Vector3 }; to: { pos: THREE.Vector3; target: THREE.Vector3 }; t: number } | null = null;
      const goTo = (name: ViewName) => {
        tween = { from: { pos: camera.position.clone(), target: controls.target.clone() }, to: viewPose(name), t: 0 };
      };

      const resize = () => {
        const w = wrap.clientWidth, h = wrap.clientHeight;
        if (w < 2 || h < 2) return;
        renderer.setSize(w, h, false);
        camera.aspect = w / h;
        camera.updateProjectionMatrix();
      };
      resize();
      {
        const pose = viewPose('home');
        camera.position.copy(pose.pos);
        controls.target.copy(pose.target);
        controls.update();
      }
      const ro = new ResizeObserver(resize);
      ro.observe(wrap);

      // ── Picking ──
      const raycaster = new THREE.Raycaster();
      const ndc = new THREE.Vector2();
      const setNdc = (e: PointerEvent | MouseEvent) => {
        const r = canvas.getBoundingClientRect();
        ndc.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
        raycaster.setFromCamera(ndc, camera);
      };
      type Hit = { kind: 'pen'; x: number; y: number } | { kind: 'key'; key: ModelKey } | { kind: 'lid' } | null;
      const pick = (e: PointerEvent | MouseEvent): Hit => {
        setNdc(e);
        const hits = raycaster.intersectObject(model.root, true);
        const first = hits[0];
        if (!first) return null;
        const open = openT >= 0.999;
        if (open && first.object === model.keyMesh) {
          const k = model.keyAt(model.keyMesh.worldToLocal(first.point.clone()));
          return k ? { kind: 'key', key: k } : null;
        }
        if (open && first.object === model.digitiserMesh) {
          const local = model.digitiserMesh.worldToLocal(first.point.clone());
          if (model.isOnDigitiser(local)) return { kind: 'pen', ...model.toDigitiser(local) };
          return null;
        }
        if (openT <= 0.001 && model.lidOuterMeshes.includes(first.object)) return { kind: 'lid' };
        return null;
      };
      // Pen drags keep tracking past the pad's edge, so moves are projected
      // onto the face's plane rather than re-picked.
      const facePlane = new THREE.Plane();
      const penPoint = (e: PointerEvent) => {
        setNdc(e);
        const m = model.digitiserMesh;
        m.updateWorldMatrix(true, false);
        const { point, normal } = model.digitiserPlane;
        facePlane.setFromNormalAndCoplanarPoint(
          normal.clone().transformDirection(m.matrixWorld), point.clone().applyMatrix4(m.matrixWorld));
        const p = new THREE.Vector3();
        if (!raycaster.ray.intersectPlane(facePlane, p)) return null;
        return model.toDigitiser(m.worldToLocal(p));
      };

      // Gesture state.
      let penId: number | null = null;
      let keyGesture: { id: number; key: ModelKey; at: number } | null = null;
      const pendingUps = new Map<number, ModelKey>();
      // Lets a key go, but not sooner than MIN_KEY_HOLD_MS after `downAt`.
      const releaseKey = (k: ModelKey, downAt: number) => {
        const wait = MIN_KEY_HOLD_MS - (performance.now() - downAt);
        const up = () => { propsRef.current.onKey(k.code, false); model.setKeyDown(k.id, false); };
        if (wait <= 0) { up(); return; }
        const t = window.setTimeout(() => { pendingUps.delete(t); up(); }, wait);
        pendingUps.set(t, k);
      };
      const latched = new Map<string, ModelKey & { at: number }>();
      let lidClick: { x: number; y: number; t: number } | null = null;
      // Latched modifiers go up together with the key they modified (never
      // before it, or the machine would see that key unmodified).
      const releaseLatched = (withKeyDownAt: number) => {
        for (const k of latched.values()) releaseKey(k, withKeyDownAt);
        latched.clear();
      };

      const onDown = (e: PointerEvent) => {
        if (e.target !== canvas) return;
        if (penId !== null || keyGesture) { e.stopPropagation(); return; }
        const hit = e.button === 0 || e.pointerType !== 'mouse' ? pick(e) : null;
        if (!hit) return;
        if (hit.kind === 'lid') { lidClick = { x: e.clientX, y: e.clientY, t: performance.now() }; return; }
        // The machine's, not the camera's.
        e.stopPropagation();
        e.preventDefault();
        canvas.setPointerCapture(e.pointerId);
        setHintVisible(false);
        if (hit.kind === 'pen') {
          penId = e.pointerId;
          propsRef.current.onPenDown(hit.x, hit.y);
        } else {
          const k = hit.key;
          const held = latched.get(k.id);
          if (held) {
            // Second tap on a latched modifier lets it go.
            latched.delete(k.id);
            releaseKey(held, held.at);
            return;
          }
          keyGesture = { id: e.pointerId, key: k, at: performance.now() };
          propsRef.current.onKey(k.code, true);
          model.setKeyDown(k.id, true);
        }
      };
      const onMove = (e: PointerEvent) => {
        if (penId === e.pointerId) {
          const d = penPoint(e);
          if (d) propsRef.current.onPenMove(d.x, d.y);
          return;
        }
        if (penId !== null || keyGesture || e.buttons !== 0) return;
        // Hover feedback.
        const hit = pick(e);
        canvas.style.cursor = !hit ? 'grab' : hit.kind === 'pen' ? 'crosshair' : 'pointer';
      };
      const onUp = (e: PointerEvent) => {
        if (penId === e.pointerId) {
          penId = null;
          propsRef.current.onPenUp();
          return;
        }
        if (keyGesture && keyGesture.id === e.pointerId) {
          const { key: k, at } = keyGesture;
          keyGesture = null;
          if (k.modifier && e.type === 'pointerup') {
            // Tapped: hold it for the next key, like a phone's Shift.
            latched.set(k.id, { ...k, at });
            return;
          }
          releaseKey(k, at);
          if (!k.modifier) releaseLatched(at);
          return;
        }
        if (lidClick && e.type === 'pointerup') {
          const moved = Math.hypot(e.clientX - lidClick.x, e.clientY - lidClick.y);
          if (moved < 6 && performance.now() - lidClick.t < 500) propsRef.current.onToggleLid();
        }
        lidClick = null;
      };
      const onDblClick = (e: MouseEvent) => {
        if (e.target !== canvas) return;
        if (!pick(e)) goTo('home');
      };
      const onContext = (e: Event) => e.preventDefault();
      wrap.addEventListener('pointerdown', onDown, { capture: true });
      canvas.addEventListener('pointermove', onMove);
      canvas.addEventListener('pointerup', onUp);
      canvas.addEventListener('pointercancel', onUp);
      canvas.addEventListener('dblclick', onDblClick);
      canvas.addEventListener('contextmenu', onContext);

      // Physical typing presses the model's keys too.
      const onKeyEvt = (e: KeyboardEvent) => {
        const id = model.keyIdForCode(e.code);
        if (id) model.setKeyDown(id, e.type === 'keydown');
      };
      const onBlur = () => { for (const r of ['ShiftL', 'ShiftR', 'Ctrl', 'Fn', 'Menu']) if (!latched.has(r)) model.setKeyDown(r, false); };
      window.addEventListener('keydown', onKeyEvt);
      window.addEventListener('keyup', onKeyEvt);
      window.addEventListener('blur', onBlur);

      // ── Frame loop ──
      let last = performance.now();
      let lastLcd = 0;
      let lastBacklight: boolean | null = null;
      const paintLcd = (now: number) => {
        if (now - lastLcd < 30) return;
        lastLcd = now;
        const p = propsRef.current;
        const w = model.lcdCanvas.width, h = model.lcdCanvas.height;
        const lit = p.backlightOn && !!p.backlightColor;
        lcdCtx.globalCompositeOperation = 'source-over';
        lcdCtx.fillStyle = lit ? p.backlightColor! : model.lcdBaseColour;
        lcdCtx.fillRect(0, 0, w, h);
        const src = p.getLcdSource();
        if (src && !p.paused) {
          lcdCtx.imageSmoothingEnabled = false;
          try { lcdCtx.drawImage(src, 0, 0, w, h); } catch { /* placeholder not ready */ }
        }
        model.lcdTexture.needsUpdate = true;
        if (lastBacklight !== lit) { lastBacklight = lit; model.setBacklight(lit, p.backlightColor); }
      };
      const renderFrame = (now: number) => {
        const dt = Math.min(0.1, (now - last) / 1000);
        last = now;
        const p = propsRef.current;
        // Lid travel. The orbit target rides along with the machine's centre
        // of interest (by the change only, so a pan the user made sticks).
        const want = p.lidOpen ? 1 : 0;
        if (lidOverride !== null) {
          openT = lidOverride;
          model.setOpen(openT);
        } else if (openT !== want) {
          const prevFocus = focusAt(openT);
          const step = dt / LID_SECONDS;
          openT = want > openT ? Math.min(want, openT + step) : Math.max(want, openT - step);
          model.setOpen(openT);
          const delta = focusAt(openT).sub(prevFocus);
          controls.target.add(delta);
          camera.position.add(delta);
        }
        if (tween) {
          tween.t = Math.min(1, tween.t + dt / 0.75);
          const k = tween.t < 0.5 ? 4 * tween.t ** 3 : 1 - (-2 * tween.t + 2) ** 3 / 2;
          // Swing round the target rather than cutting through the model.
          const a = tween.from.pos.clone().sub(tween.from.target);
          const b = tween.to.pos.clone().sub(tween.to.target);
          const dir = a.clone().normalize().lerp(b.clone().normalize(), k).normalize();
          const dist = THREE.MathUtils.lerp(a.length(), b.length(), k);
          controls.target.lerpVectors(tween.from.target, tween.to.target, k);
          camera.position.copy(controls.target).addScaledVector(dir, dist);
          if (tween.t >= 1) tween = null;
        }
        paintLcd(now);
        model.update(dt);
        controls.update();
        // From underneath, a shadow on a floor that isn't drawn just looks
        // like a smudge across the machine.
        ground.visible = camera.position.y > 0;
        renderer.render(scene, camera);
      };
      renderer.setAnimationLoop(renderFrame);

      // Dev-only automation hook: lets the Playwright checks frame the model
      // from any angle (azimuth / elevation in degrees, elevation may be
      // negative to look from underneath). Stripped from production builds.
      if (import.meta.env.DEV) {
        // Freeze the lid at a point of its travel (0 shut … 1 open), for
        // checking the mechanism frame by frame; null hands it back.
        (window as unknown as Record<string, unknown>).__lid3d = (t: number | null) => {
          lidOverride = t;
        };
        (window as unknown as Record<string, unknown>).__orbit3d = (az: number, el: number) => {
          const a = THREE.MathUtils.degToRad(az), e = THREE.MathUtils.degToRad(el);
          const target = focusAt(openT);
          camera.position.set(Math.sin(a) * Math.cos(e), Math.sin(e), Math.cos(a) * Math.cos(e))
            .multiplyScalar(fitDistance()).add(target);
          controls.target.copy(target);
          controls.update();
        };
      }

      apiRef.current = {
        goTo,
        capture: (scale: number) => {
          const w = wrap.clientWidth, h = wrap.clientHeight;
          const pr = renderer.getPixelRatio();
          renderer.setPixelRatio(scale);
          renderer.setSize(w, h, false);
          lastLcd = 0;
          paintLcd(performance.now());
          renderer.render(scene, camera);
          const out = document.createElement('canvas');
          out.width = canvas.width;
          out.height = canvas.height;
          const ctx = out.getContext('2d')!;
          const [a, b, c] = prefersDark() ? BACKDROP.dark : BACKDROP.light;
          // CSS radial-gradient(120% 95% at 50% 36%) — an ellipse; drawn as a
          // circle stretched vertically to match.
          ctx.save();
          const rx = out.width * 1.2, ry = out.height * 0.95;
          ctx.translate(out.width * 0.5, out.height * 0.36);
          ctx.scale(1, ry / rx);
          const grd = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
          grd.addColorStop(0, a); grd.addColorStop(0.52, b); grd.addColorStop(1, c);
          ctx.fillStyle = grd;
          ctx.fillRect(-out.width, -out.height * (rx / ry), out.width * 2, out.height * 2 * (rx / ry));
          ctx.restore();
          ctx.drawImage(canvas, 0, 0);
          renderer.setPixelRatio(pr);
          renderer.setSize(w, h, false);
          renderer.render(scene, camera);
          return out;
        },
      };
      setStatus('ready');

      cleanup = () => {
        renderer.setAnimationLoop(null);
        ro.disconnect();
        wrap.removeEventListener('pointerdown', onDown, { capture: true });
        canvas.removeEventListener('pointermove', onMove);
        canvas.removeEventListener('pointerup', onUp);
        canvas.removeEventListener('pointercancel', onUp);
        canvas.removeEventListener('dblclick', onDblClick);
        canvas.removeEventListener('contextmenu', onContext);
        window.removeEventListener('keydown', onKeyEvt);
        window.removeEventListener('keyup', onKeyEvt);
        window.removeEventListener('blur', onBlur);
        // Nothing may stay pressed in the machine once the view goes.
        if (penId !== null) propsRef.current.onPenUp();
        for (const [t, k] of pendingUps) { window.clearTimeout(t); propsRef.current.onKey(k.code, false); }
        if (keyGesture) propsRef.current.onKey(keyGesture.key.code, false);
        for (const k of latched.values()) propsRef.current.onKey(k.code, false);
        controls.dispose();
        model.dispose();
        scene.traverse(o => {
          const m = o as THREE.Mesh;
          if (m.isMesh && ground.children.includes(m)) {
            m.geometry.dispose();
            (m.material as THREE.Material).dispose();
          }
        });
        blobTex.dispose();
        envTex.dispose();
        pmrem.dispose();
        room.dispose?.();
        renderer.dispose();
        canvas.remove();
        apiRef.current = null;
        if (import.meta.env.DEV) {
          delete (window as unknown as Record<string, unknown>).__orbit3d;
          delete (window as unknown as Record<string, unknown>).__lid3d;
        }
      };
    })().catch(err => {
      if (disposed) return;
      setStatus('error');
      setErrorText(err instanceof Error ? err.message : String(err));
    });

    return () => { disposed = true; cleanup(); };
    // The scene is rebuilt only for a different machine or screen; every
    // other prop is read live through propsRef.
  }, [props.deviceId, props.lcdWidth, props.lcdHeight]); // eslint-disable-line react-hooks/exhaustive-deps

  const pill = [
    'px-2.5 py-1 rounded-full text-[11px] font-mono whitespace-nowrap select-none transition-colors',
    dark ? 'text-white/85 hover:bg-white/15' : 'text-psion-charcoal/80 hover:bg-black/10',
  ].join(' ');

  return (
    <div
      ref={wrapRef}
      className={[
        'relative overflow-hidden select-none',
        props.fill ? 'w-full h-full' : 'w-full rounded-xl shadow-inner',
      ].join(' ')}
      style={{
        background: backdropCss(dark),
        // Tall enough for the open machine, but on a narrow screen no
        // taller than its width calls for.
        ...(props.fill ? {} : { height: 'min(74vh, 880px, max(360px, 120vw))' }),
      }}
    >
      {status === 'loading' && (
        <div className="absolute inset-0 flex items-center justify-center text-xs font-mono text-psion-charcoal/60">
          Building the 3D model…
        </div>
      )}
      {status === 'error' && (
        <div className="absolute inset-0 flex items-center justify-center p-6 text-center text-xs font-mono text-red-600">
          {errorText}
        </div>
      )}

      {/* How to drive it — gone at the first interaction. */}
      {status === 'ready' && (
        <div
          className={[
            'pointer-events-none absolute top-3 left-1/2 -translate-x-1/2 px-3 py-1.5 rounded-full',
            'whitespace-nowrap text-[10px] md:text-[11px] font-mono backdrop-blur-md transition-opacity duration-700',
            dark ? 'bg-black/35 text-white/80' : 'bg-white/55 text-psion-charcoal/75',
            hintVisible ? 'opacity-100' : 'opacity-0',
          ].join(' ')}
        >
          <span className="hidden md:inline">Drag to turn · scroll to zoom · right-drag to move · use the screen and keys directly</span>
          <span className="md:hidden">Drag to turn · pinch to zoom · two fingers to move</span>
        </div>
      )}

      {/* Lid + camera presets. */}
      {status === 'ready' && (
        <div
          className={[
            'absolute bottom-3 left-1/2 -translate-x-1/2 flex items-center gap-0.5 p-1 rounded-full',
            'backdrop-blur-md shadow-lg ring-1',
            dark ? 'bg-black/40 ring-white/10' : 'bg-white/60 ring-black/5',
          ].join(' ')}
        >
          <button
            type="button"
            onClick={props.onToggleLid}
            className={`${pill} font-semibold`}
            title={props.lidOpen ? 'Shut the lid — the machine keeps running' : 'Open the lid'}
          >
            {props.lidOpen ? 'Close lid' : 'Open lid'}
          </button>
          <span className={`w-px h-4 mx-1 ${dark ? 'bg-white/20' : 'bg-black/15'}`} aria-hidden="true" />
          {(Object.keys(VIEWS) as ViewName[]).map(v => (
            <button
              key={v}
              type="button"
              className={pill}
              title={VIEWS[v].label}
              onClick={() => apiRef.current?.goTo(v)}
            >
              {v === 'home' ? '⟲' : v[0].toUpperCase() + v.slice(1)}
            </button>
          ))}
        </div>
      )}
    </div>
  );
});

export default Device3DView;
