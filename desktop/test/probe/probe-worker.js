// Phase 0 probe worker. Mirrors what frontend/public/emulator-worker.js
// actually does: it is a CLASSIC worker that importScripts() a sibling
// script and paints to a transferred OffscreenCanvas.
let importOk = false;
let importError = null;
try {
  importScripts(new URL('probe-import.js', self.location.href).href);
  importOk = self.__psionImportWorked === true;
} catch (err) {
  importError = String(err);
}

self.onmessage = (e) => {
  const msg = e.data;
  if (msg.type === 'canvas') {
    let drew = false;
    let drawError = null;
    try {
      const ctx = msg.canvas.getContext('2d');
      ctx.fillStyle = '#123456';
      ctx.fillRect(0, 0, 8, 8);
      drew = ctx.getImageData(1, 1, 1, 1).data[2] === 0x56;
    } catch (err) {
      drawError = String(err);
    }
    self.postMessage({ type: 'result', importOk, importError, drew, drawError });
  }
};
self.postMessage({ type: 'ready' });
