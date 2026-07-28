// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Playwright runner for the real-browser Remote Link transfer repro
// (transfer-test.html). Boots the actual emulator-worker.js in real Chromium
// and round-trips a file over the real PlpClient — the two-thread environment
// the Node repro can't model. Use it to reproduce/diagnose large-file transfer
// wedges that only show with a real worker thread + real OffscreenCanvas blit.
//
// Requires `playwright` (dev dependency) and a Chromium it can find — set
// PW_EXE, or rely on PLAYWRIGHT_BROWSERS_PATH / the default download location.
//
// Usage (from frontend/):
//   node test/plp-browser/run.mjs                       # 250 KB synthetic round-trip
//   OPTS='{"size":989000}' node test/plp-browser/run.mjs
//   OPTS='{"realFile":"/applib/epocutil/nconvert/nConvert.sis"}' node test/plp-browser/run.mjs
//
// /applib/* is served from the repo's top-level applib/ so realFile can point at
// any real app without copying it into public/.
import { createServer } from 'vite';
import { chromium } from 'playwright';
import * as fs from 'node:fs';
import * as path from 'node:path';

const opts = JSON.parse(process.env.OPTS || '{}');
const PORT = Number(process.env.PORT || 5199);
const repoRoot = path.resolve(process.cwd(), '..');
const applibDir = path.join(repoRoot, 'applib');

const server = await createServer({
  root: process.cwd(),
  base: '/',                          // override the app's '/psion/' base
  logLevel: 'warn',
  server: { port: PORT, strictPort: true },
  // Serve the repo's applib/ at /applib so realFile can fetch real app bytes.
  plugins: [{
    name: 'serve-applib',
    configureServer(s) {
      s.middlewares.use('/applib', (req, res, next) => {
        const rel = decodeURIComponent((req.url || '').split('?')[0]);
        const fp = path.join(applibDir, rel);
        if (!fp.startsWith(applibDir) || !fs.existsSync(fp) || fs.statSync(fp).isDirectory()) return next();
        res.setHeader('content-type', 'application/octet-stream');
        fs.createReadStream(fp).pipe(res);
      });
    },
  }],
});
await server.listen();
const url = `http://localhost:${PORT}/test/plp-browser/transfer-test.html`;
console.log('serving', url);

const browser = await chromium.launch({
  executablePath: process.env.PW_EXE || undefined,
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage();
page.on('console', m => console.log('[page]', m.text()));
page.on('pageerror', e => console.log('[pageerror]', e.message));

let code = 1;
try {
  await page.goto(url, { timeout: 30000 });
  await page.waitForFunction('window.__plpReady === true', { timeout: 30000 });
  const result = await page.evaluate(o => window.runPlpTest(o), opts);
  console.log('RESULT_JSON ' + JSON.stringify(result));
  code = result && result.result === 'PASS' ? 0 : 1;
} catch (e) {
  console.log('RUNNER_ERROR ' + (e && e.message || e));
} finally {
  await browser.close();
  await server.close();
  process.exit(code);
}
