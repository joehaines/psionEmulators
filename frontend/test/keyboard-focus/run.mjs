// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Regression test for "the Machine ID field won't let me type".
//
// EmulatorView listens for keydown on `window` and preventDefaults every key
// the device keymap covers, so a keystroke aimed at one of the host UI's own
// text fields (the Machine ID hex box, the Modem dialog's compose fields) was
// cancelled before the browser could insert the character: the field looked
// frozen and the letter went to EPOC instead. lib/keymap.ts's isHostTextEntry
// is the guard that fixes it, and it has to keep excluding the emulator's own
// hidden textarea (data-psion-input) so mobile soft-keyboard input still
// reaches the guest.
//
// This needs a real browser: the behaviour under test IS the DOM's
// "preventDefault on keydown suppresses text insertion" rule and real event
// propagation to `window`, neither of which a hand-rolled mock would prove.
// The REAL isHostTextEntry is bundled out of lib/keymap.ts and driven against
// real elements and real keystrokes.
//
// Requires `playwright` (dev dependency) and a Chromium it can find — set
// PW_EXE, or rely on PLAYWRIGHT_BROWSERS_PATH / the default download location.
//
// Usage (from frontend/):
//   node test/keyboard-focus/run.mjs

import { chromium } from 'playwright';
import * as esbuild from 'esbuild';

let failures = 0;
const check = (cond, msg) => {
  if (cond) console.log(`ok: ${msg}`);
  else { console.error(`FAIL: ${msg}`); failures++; }
};

// Bundle the real module so the browser runs the shipping implementation.
const { outputFiles } = await esbuild.build({
  entryPoints: [new URL('../../src/lib/keymap.ts', import.meta.url).pathname],
  bundle: true, format: 'iife', globalName: 'keymapModule',
  write: false, logLevel: 'silent',
});
const keymapBundle = outputFiles[0].text;

// Mirrors EmulatorView's window listener + the app's DOM shape: a dialog with
// text inputs, and the emulator's marked hidden textarea.
const PAGE = `<!doctype html><body>
<div id="root">
  <div id="dialog">
    <label>ID <input id="hex" type="text"></label>
    <div id="editable" contenteditable="true"></div>
    <select id="picker"><option value="a">a</option><option value="b">b</option></select>
  </div>
  <textarea id="psion" data-psion-input="1"></textarea>
</div>
<script>
window.sentToEpoc = [];
window.installListener = () => {
  window.addEventListener('keydown', e => {
    if (window.guardOn && keymapModule.isHostTextEntry(e.target)) return;
    // Stand-in for handleKeyDown's "key is in the device keymap" branch.
    if (e.key.length !== 1) return;
    e.preventDefault();
    window.sentToEpoc.push(e.key);
  });
};
</script></body>`;

const browser = await chromium.launch({ executablePath: process.env.PW_EXE || undefined });
const page = await browser.newPage();
await page.setContent(PAGE);
await page.addScriptTag({ content: keymapBundle });
await page.evaluate(() => window.installListener());

const typeInto = async (selector, text) => {
  await page.evaluate(() => { window.sentToEpoc = []; });
  await page.click(selector);
  await page.keyboard.type(text);
  return {
    value: await page.evaluate(sel => {
      const el = document.querySelector(sel);
      return el.isContentEditable ? el.textContent : el.value;
    }, selector),
    forwarded: await page.evaluate(() => window.sentToEpoc.join('')),
  };
};

// ── 1. Guard off: the original bug, so the test proves the mechanism ──
await page.evaluate(() => { window.guardOn = false; });
{
  const r = await typeInto('#hex', 'CAFE');
  check(r.value === '', `unguarded: hex field stays empty (got "${r.value}") — the reported bug`);
  check(r.forwarded === 'CAFE', 'unguarded: the keystrokes went to the emulator instead');
}

// ── 2. Guard on: host fields type normally, nothing leaks to the guest ──
await page.evaluate(() => { window.guardOn = true; });
{
  const r = await typeInto('#hex', 'CAFE');
  check(r.value === 'CAFE', `guarded: hex field accepts text (got "${r.value}")`);
  check(r.forwarded === '', `guarded: nothing forwarded to the emulator (got "${r.forwarded}")`);
}
{
  // contenteditable is covered too — the app has no such field today, but the
  // guard claims it, so hold it to that.
  const r = await typeInto('#editable', 'xy');
  check(r.value === 'xy', `guarded: contenteditable accepts text (got "${r.value}")`);
  check(r.forwarded === '', 'guarded: contenteditable leaked nothing to the emulator');
}
{
  // A <select> uses letter keys to jump between options; forwarding them would
  // also fire EPOC keys behind the dropdown.
  await page.evaluate(() => { window.sentToEpoc = []; });
  await page.click('#picker');
  await page.keyboard.type('b');
  const forwarded = await page.evaluate(() => window.sentToEpoc.join(''));
  check(forwarded === '', `guarded: select keystrokes not forwarded (got "${forwarded}")`);
}

// ── 3. The emulator's own hidden textarea must still feed the guest ──
{
  const r = await typeInto('#psion', 'hi');
  check(r.forwarded === 'hi',
        `guarded: hidden psion textarea still forwards to the emulator (got "${r.forwarded}")`);
}

// ── 4. Clicks on the device frame / buttons keep reaching EPOC ──
{
  await page.evaluate(() => { window.sentToEpoc = []; });
  await page.evaluate(() => document.getElementById('root').focus?.());
  await page.keyboard.type('z');
  const forwarded = await page.evaluate(() => window.sentToEpoc.join(''));
  check(forwarded === 'z', `guarded: typing outside any field reaches the emulator (got "${forwarded}")`);
}

await browser.close();
if (failures) {
  console.error(`\n${failures} check(s) failed`);
  process.exit(1);
}
console.log('\nkeyboard-focus tests passed');
