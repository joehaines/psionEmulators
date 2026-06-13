// Generates the PWA / iOS home-screen icons and the social-share image
// from the Psion logo artwork in frontend/public/favicon.svg.
//
// Run from frontend/ (where @resvg/resvg-js is a devDependency):
//   npm run icons
//
// Outputs into frontend/public/:
//   apple-touch-icon.png   180x180  iOS "Add to Home Screen"
//   icon-192.png           192x192  manifest icon
//   icon-512.png           512x512  manifest icon
//   icon-maskable-512.png  512x512  manifest maskable icon (logo inside
//                                   the safe zone so Android's circle /
//                                   squircle masks don't clip it)
//   og-image.png          1200x630  Open Graph / Twitter card image

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const frontendDir = join(dirname(fileURLToPath(import.meta.url)), '..', 'frontend');
const publicDir = join(frontendDir, 'public');
// @resvg/resvg-js lives in frontend/node_modules, so resolve it from there.
const { Resvg } = createRequire(join(frontendDir, 'package.json'))('@resvg/resvg-js');

// Pull the logo paths out of the favicon, dropping its full-bleed white
// background rect so we can place the artwork on our own background.
const favicon = readFileSync(join(publicDir, 'favicon.svg'), 'utf8');
const inner = favicon.match(/<g[^>]*>(.*)<\/g>/s)[1];
const logoPaths = inner.replace(/^<path d="M0 0h192\.756v192\.756H0V0z" fill="#fff"\/>/, '');

// Bounding box of the logo band within the favicon's coordinate space.
const LOGO = { x: 2.834, y: 54.29, w: 187.088, h: 84.177 };
const YELLOW = '#fadf3c'; // Psion brand yellow

// Square icon: logo centred on a full-bleed yellow tile. `coverage` is
// the fraction of the tile width the logo band spans.
function iconSvg(size, coverage) {
  const scale = (size * coverage) / LOGO.w;
  const tx = (size - LOGO.w * scale) / 2 - LOGO.x * scale;
  const ty = (size - LOGO.h * scale) / 2 - LOGO.y * scale;
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${size} ${size}">` +
    `<rect width="${size}" height="${size}" fill="${YELLOW}"/>` +
    `<g clip-rule="evenodd" fill-rule="evenodd" transform="translate(${tx} ${ty}) scale(${scale})">${logoPaths}</g>` +
    `</svg>`;
}

// Wide social-card image: logo centred on yellow.
function ogSvg(w, h) {
  const scale = (h * 0.52) / LOGO.h;
  const tx = (w - LOGO.w * scale) / 2 - LOGO.x * scale;
  const ty = (h - LOGO.h * scale) / 2 - LOGO.y * scale;
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}">` +
    `<rect width="${w}" height="${h}" fill="${YELLOW}"/>` +
    `<g clip-rule="evenodd" fill-rule="evenodd" transform="translate(${tx} ${ty}) scale(${scale})">${logoPaths}</g>` +
    `</svg>`;
}

function render(svg, width, file) {
  const png = new Resvg(svg, { fitTo: { mode: 'width', value: width } }).render().asPng();
  writeFileSync(join(publicDir, file), png);
  console.log(`wrote ${file} (${width}px, ${png.length} bytes)`);
}

render(iconSvg(512, 0.8), 180, 'apple-touch-icon.png');
render(iconSvg(512, 0.8), 192, 'icon-192.png');
render(iconSvg(512, 0.8), 512, 'icon-512.png');
// Maskable safe zone is a circle of radius 40% of the icon size, so keep
// the logo band inside ~62% of the width.
render(iconSvg(512, 0.62), 512, 'icon-maskable-512.png');
render(ogSvg(1200, 630), 1200, 'og-image.png');
