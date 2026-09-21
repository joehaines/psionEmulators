// Generates the desktop app icon from the same Psion logo artwork the web
// icons come from (frontend/public/favicon.svg).
//
// Run from the repo root (needs frontend's devDependencies installed):
//   node scripts/generate-desktop-icon.mjs
//
// Outputs desktop/resources/icon.png at 1024x1024. That single file is all
// electron-builder needs: it derives the .icns for macOS and the .ico for
// Windows from it, at every size each platform wants. Committing one PNG rather
// than three hand-built formats means the icon cannot drift between platforms,
// and there is nothing to rebuild on a machine without iconutil.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = join(HERE, '..');
const require = createRequire(join(REPO, 'frontend', 'package.json'));
const { Resvg } = require('@resvg/resvg-js');

const SVG = join(REPO, 'frontend', 'public', 'favicon.svg');
const OUT_DIR = join(REPO, 'desktop', 'resources');
const SIZE = 1024;

const svg = readFileSync(SVG, 'utf8');
const resvg = new Resvg(svg, {
  fitTo: { mode: 'width', value: SIZE },
  // The app icon sits on whatever the OS puts behind it — a dock, a taskbar, a
  // dark or light theme — so it keeps the artwork's own background rather than
  // being rendered transparent and becoming invisible on one of them.
  background: '#c9c9bd',
});
mkdirSync(OUT_DIR, { recursive: true });
const png = resvg.render().asPng();
writeFileSync(join(OUT_DIR, 'icon.png'), png);
console.log(`desktop/resources/icon.png — ${SIZE}x${SIZE}, ${png.length} bytes`);
