// web_smoke.mjs - drive the browser build in headless Chromium (WOLF-41).
//
//   PLAYWRIGHT=/path/to/node_modules/playwright \
//     node scripts/web_smoke.mjs [BUILD_DIR] [OUT_DIR] [--browser chromium|firefox] [--no-demo]
//
// BUILD_DIR defaults to build-web (cmake --preset web, with the shareware
// data bundled); OUT_DIR (screenshots, console.log, result.json) to
// web-smoke.  The script serves BUILD_DIR on a local port, then:
//   1. -demotest all -viewsize 19: the four demos must match the native
//      hashes of tests/demo_sync.cmake (the wasm32 build plays them in sync)
//      and the time they take is the speed check;
//   2. a normal start: title screen, main menu, New Game into E1M1, walk
//      and turn with the keyboard, then turn with the mouse (pointer lock).
// It fails on any page error or console error.  Shots are of the canvas.

import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync, readFileSync } from 'node:fs';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const pw = await import(process.env.PLAYWRIGHT || 'playwright');
const args = process.argv.slice(2);
const flag = (name, def) => {
  const i = args.indexOf(name);
  if (i < 0)
    return def;
  const v = args[i + 1];
  args.splice(i, 2);
  return v;
};
const browserName = flag('--browser', 'chromium');
const noDemo = args.includes('--no-demo') && args.splice(args.indexOf('--no-demo'), 1);
const buildDir = resolve(args[0] || 'build-web');
const outDir = resolve(args[1] || 'web-smoke');
mkdirSync(outDir, { recursive: true });

const repo = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const expected = [...readFileSync(join(repo, 'tests/demo_sync.cmake'), 'utf8')
  .matchAll(/"(demotest \d: [^"]+)"/g)].map((m) => m[1]);

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const port = 8600 + Math.floor(Math.random() * 300);
const server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1',
  '-d', buildDir], { stdio: 'ignore' });
const url = `http://127.0.0.1:${port}/raylib_wolf3d.html`;

const log = [];
const errors = [];
const result = { browser: browserName, url, checks: {} };
let failed = false;
function check(name, ok, detail) {
  result.checks[name] = { ok, detail };
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${detail ? ': ' + detail : ''}`);
  if (!ok)
    failed = true;
}

const browser = await pw[browserName].launch({ headless: true });
result.version = browser.version();
try {
  for (let i = 0; i < 50; i++) {             // wait for the server
    try { await fetch(url); break; } catch { await sleep(100); }
  }

  async function open(query) {
    const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
    // Time the game spends suspended in emscripten_sleep (a setTimeout),
    // so the time it computes is the rest.
    await page.addInitScript(() => {
      const st = window.setTimeout;
      window.wolfIdleMs = 0;
      window.setTimeout = function (fn, ms, ...rest) {
        const t = performance.now();
        return st(function () { window.wolfIdleMs += performance.now() - t; fn(...rest); }, ms);
      };
    });
    page.on('console', (m) => {
      const line = `[${query || 'play'}] ${m.type()}: ${m.text()}`;
      log.push(line);
      if (m.type() === 'error')
        errors.push(line);
    });
    page.on('pageerror', (e) => {
      log.push(`[${query || 'play'}] pageerror: ${e.message}`);
      errors.push(e.message);
    });
    const t0 = Date.now();
    await page.goto(url + (query ? '?' + query : ''));
    await page.waitForSelector('#play:not([disabled])', { timeout: 30000 });
    result.loadMs = result.loadMs || Date.now() - t0;
    return page;
  }
  const shot = (page, name) => page.locator('#canvas').screenshot({ path: join(outDir, name) });
  // Synthetic key taps are held >= 100 ms (the game polls at 70 Hz).
  async function tap(page, key, ms = 120) {
    await page.keyboard.down(key);
    await sleep(ms);
    await page.keyboard.up(key);
    await sleep(250);
  }

  // 1. Demo sync and speed
  if (!noDemo) {
    const page = await open('args=-demotest%20all%20-viewsize%2019');
    const t0 = Date.now();
    await page.click('#play');
    await page.waitForFunction(() => /Thanks for playing|stopped|crashed/
      .test(document.getElementById('status').textContent), null, { timeout: 240000 });
    const ms = Date.now() - t0;
    result.demotestIdleMs = Math.round(await page.evaluate(() => window.wolfIdleMs));
    const out = log.filter((l) => l.includes('demotest')).join('\n');
    const missing = expected.filter((e) => !out.includes(e + ' '));
    check('demo_sync (4 demos match the native hashes)', missing.length === 0,
      missing.length ? 'missing ' + missing.join('; ') : `${expected.length} demos`);
    result.demotestMs = ms;
    console.log(`demotest wall time: ${ms} ms, suspended ${result.demotestIdleMs} ms`);
    await page.close();
  }

  // 2. Play
  {
    const page = await open('');
    await page.click('#play');
    await sleep(2500);
    await shot(page, '1-signon.png');
    await tap(page, 'Enter');                 // "Press a key" on the signon screen
    await sleep(1500);
    await shot(page, '2-pg13.png');
    await tap(page, 'Enter');                 // skip the PG-13 screen
    await sleep(2000);
    await shot(page, '3-title.png');
    await tap(page, 'Enter');                 // any key: main menu
    await sleep(2000);
    await shot(page, '4-menu.png');
    await tap(page, 'n');                     // first-letter jump: New Game
    await tap(page, 'Enter');
    await sleep(1000);
    await shot(page, '5-episode.png');
    await tap(page, 'Enter');                 // episode 1
    await sleep(1000);
    await tap(page, 'Enter');                 // difficulty
    await sleep(4000);                        // "Get Psyched!" and fade-in
    await shot(page, '6-e1m1-start.png');
    await tap(page, 'ArrowUp', 1200);         // walk forward
    await tap(page, 'ArrowLeft', 500);        // turn
    await sleep(500);
    await shot(page, '7-e1m1-moved.png');

    // Mouse: the click on the canvas asks for pointer lock.
    const box = await page.locator('#canvas').boundingBox();
    await page.mouse.click(box.x + box.width / 2, box.y + box.height / 2);
    await sleep(500);
    const locked = await page.evaluate(() => document.pointerLockElement === Module.canvas);
    result.pointerLocked = locked;
    for (let i = 0; i < 20; i++) {
      await page.mouse.move(box.x + box.width / 2 + (i + 1) * 15, box.y + box.height / 2);
      await sleep(30);
    }
    await sleep(500);
    await shot(page, '8-e1m1-mouse.png');
    check('pointer lock after a click', locked);

    result.canvas = await page.evaluate(() => {
      const c = Module.canvas;
      return { width: c.width, height: c.height, cssWidth: c.clientWidth, cssHeight: c.clientHeight,
               scrollY: window.scrollY };
    });
    check('no page scroll from game keys', result.canvas.scrollY === 0);

    // Responsive: the canvas follows the window, the game stays 4:3.
    await page.setViewportSize({ width: 700, height: 900 });
    await sleep(800);
    await shot(page, '9-e1m1-portrait.png');
    const resized = await page.evaluate(() => [Module.canvas.width, Module.canvas.height]);
    result.canvasResized = resized;
    check('canvas follows the window size', resized[0] === 700 && resized[1] === 900,
      resized.join('x'));
    await page.close();
  }
  check('no page errors or console errors', errors.length === 0, errors.slice(0, 5).join(' | '));
} finally {
  await browser.close();
  server.kill();
  writeFileSync(join(outDir, 'console.log'), log.join('\n') + '\n');
  writeFileSync(join(outDir, 'result.json'), JSON.stringify(result, null, 2) + '\n');
}
process.exit(failed ? 1 : 0);
