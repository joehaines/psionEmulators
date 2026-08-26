// End-to-end render check in a real browser.
//
// Runs the REAL emulator-worker.js against the REAL psion.js in headless
// Chromium, boots a device onto an OffscreenCanvas and confirms the canvas
// actually has varied content. The worker unit tests drive its RPC in a vm
// sandbox and never paint, so nothing else covers the rendering path — which
// matters for changes like pointing ImageData straight at the wasm heap.
//
// Usage:
//   cp roms/<a-rom>.bin frontend/public/testrom.bin
//   (cd frontend/public && npx http-server -p 8765 -s .) &
//   node scripts/browser-render-check.mjs
// End-to-end render check: run the REAL emulator-worker.js in Chromium against
// the REAL psion.js, boot a Series 7 onto an OffscreenCanvas, and confirm the
// canvas actually has varied content. Guards the ImageData-on-heap change,
// which no unit test paints through.
const pw = await import('/opt/node22/lib/node_modules/playwright/index.js');
const b = await pw.default.chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
const p = await b.newPage();
p.on('console', m => { const t=m.text(); if(/error|abort|Uncaught|detach/i.test(t)) console.log('  [page]', t.slice(0,180)); });
p.on('pageerror', e => console.log('  [pageerror]', String(e).slice(0,200)));
await p.goto('http://127.0.0.1:8765/');
const out = await p.evaluate(async () => {
  const cvs = document.createElement('canvas'); cvs.width = 640; cvs.height = 480;
  const off = cvs.transferControlToOffscreen();
  const w = new Worker('/emulator-worker.js');
  w.onerror = (e) => console.log('WORKER ERROR', e.message, e.filename, e.lineno);
  const wait = (pred) => new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('timeout')), 120000);
    w.addEventListener('message', function h(e) {
      if (pred(e.data)) { clearTimeout(t); w.removeEventListener('message', h); res(e.data); }
    });
  });
  w.postMessage({ type: 'init', baseUrl: '/' });
  await wait(d => d.type === 'ready' || d.type === 'initError');
  w.postMessage({ type: 'setCanvas', canvas: off }, [off]);
  w.postMessage({ type: 'rpc', rpc: 'loadDevice', id: 1,
                  args: { deviceId: 'series7', romUrl: '/testrom.bin', preroll: 400 } });
  await wait(d => d.type === 'rpcResult' && d.id === 1);
  await new Promise(r => setTimeout(r, 12000));            // let it run and paint
  document.body.appendChild(cvs);
  const cvs2 = document.createElement('canvas'); cvs2.width = 640; cvs2.height = 480;
  const ctx2 = cvs2.getContext('2d');
  ctx2.drawImage(cvs, 0, 0);
  const px = ctx2.getImageData(0, 0, 640, 480).data;
  const seen = new Set(); let nonBlack = 0;
  for (let i = 0; i < px.length; i += 4) {
    seen.add((px[i] << 16) | (px[i+1] << 8) | px[i+2]);
    if (px[i] || px[i+1] || px[i+2]) nonBlack++;
  }
  return { distinctColours: seen.size, nonBlackPct: (100 * nonBlack / (px.length/4)).toFixed(1) };
});
console.log(`canvas: ${out.distinctColours} distinct colours, ${out.nonBlackPct}% non-black pixels`);
console.log(out.distinctColours > 2 && Number(out.nonBlackPct) > 5 ? 'RENDER OK' : 'RENDER SUSPECT');
await b.close();
