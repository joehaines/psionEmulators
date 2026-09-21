// Phase 0: does the emulator's runtime substrate work under Electron over
// the app:// scheme? Each check mirrors something the real app depends on.
// Results go to main over the preload's bridge-free console channel.
const results = {};
const note = (k, ok, detail) => { results[k] = { ok: !!ok, detail: detail ?? null }; };

// 1. Secure context. The app:// scheme is registered `secure`, which is
//    what makes crypto.randomUUID and the async clipboard available —
//    analytics.ts and uniqueId.ts both reach for them.
note('secureContext', window.isSecureContext, `isSecureContext=${window.isSecureContext}`);
try { note('randomUUID', typeof crypto.randomUUID() === 'string'); }
catch (e) { note('randomUUID', false, String(e)); }

// 2. The WORKER_MODE support probe from App.tsx, verbatim.
note('workerModeSupported',
  typeof Worker !== 'undefined' && typeof OffscreenCanvas !== 'undefined'
  && typeof HTMLCanvasElement !== 'undefined'
  && typeof HTMLCanvasElement.prototype.transferControlToOffscreen === 'function');

// 3. WebAssembly at all: a 2-byte-module instantiate.
try {
  const bytes = new Uint8Array([0,97,115,109,1,0,0,0]);
  await WebAssembly.instantiate(bytes);
  note('wasm', true);
} catch (e) { note('wasm', false, String(e)); }

// 4. IndexedDB — where save states live.
try {
  const db = await new Promise((res, rej) => {
    const r = indexedDB.open('psion-probe', 1);
    r.onupgradeneeded = () => r.result.createObjectStore('s');
    r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error);
  });
  await new Promise((res, rej) => {
    const tx = db.transaction('s', 'readwrite');
    tx.objectStore('s').put(new Uint8Array([1,2,3]), 'k');
    tx.oncomplete = res; tx.onerror = () => rej(tx.error);
  });
  const got = await new Promise((res, rej) => {
    const tx = db.transaction('s', 'readonly');
    const q = tx.objectStore('s').get('k');
    q.onsuccess = () => res(q.result); q.onerror = () => rej(q.error);
  });
  note('indexedDB', got && got.length === 3);
} catch (e) { note('indexedDB', false, String(e)); }

// 5. A Worker constructed from app://, importScripts()-ing a sibling, and
//    drawing into a transferred OffscreenCanvas. This is THE Phase 0
//    question: file:// fails all three.
try {
  const w = new Worker(new URL('probe-worker.js', location.href).href);
  const ready = await new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('worker never signalled ready')), 8000);
    w.onmessage = (e) => { if (e.data.type === 'ready') { clearTimeout(t); res(true); } };
    w.onerror = (e) => { clearTimeout(t); rej(new Error(e.message || 'worker error')); };
  });
  note('workerStarts', ready);
  const canvas = document.getElementById('c').transferControlToOffscreen();
  const out = await new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('no result from worker')), 8000);
    w.onmessage = (e) => { if (e.data.type === 'result') { clearTimeout(t); res(e.data); } };
    w.postMessage({ type: 'canvas', canvas }, [canvas]);
  });
  note('workerImportScripts', out.importOk, out.importError);
  note('offscreenCanvasInWorker', out.drew, out.drawError);
} catch (e) {
  note('workerStarts', false, String(e));
}

// 6. Fetching a ROM the way the app does. useEmulator reads Content-Length
//    and streams resp.body.getReader() for its progress line, so those are
//    the two things that must hold — byte ranges are never requested.
try {
  const rom = new URLSearchParams(location.search).get('rom');
  const romUrl = new URL(`roms/${rom}`, location.href).href;
  const resp = await fetch(romUrl);
  const len = Number(resp.headers.get('Content-Length') || 0);
  note('romContentLength', len > 0, `Content-Length=${len || '(absent)'}`);

  // Drain it through a reader exactly as the loader does, and check the
  // streamed total agrees with the advertised length.
  let received = 0;
  let chunks = 0;
  const reader = resp.body.getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    received += value.length;
    chunks++;
  }
  note('romStreams', received > 0 && (len === 0 || received === len),
       `${received} bytes in ${chunks} chunk(s)`);
  note('romFetch', received > 0, `${received} bytes`);
} catch (e) { note('romFetch', false, String(e)); }

// 7. Traversal must be refused by the protocol handler.
try {
  const r = await fetch(new URL('../../../../etc/passwd', location.href).href);
  note('traversalRefused', !r.ok, `status=${r.status}`);
} catch { note('traversalRefused', true, 'threw'); }

console.log('PSION_PROBE_RESULT ' + JSON.stringify(results));
