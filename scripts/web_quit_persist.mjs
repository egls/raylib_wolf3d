// web_quit_persist.mjs - files written as the game quits reach IndexedDB
// (WOLF-97).
//
//   PLAYWRIGHT=/path/to/node_modules/playwright/index.mjs \
//     node scripts/web_quit_persist.mjs [BUILD_DIR] [--browser chromium|firefox]
//
// In a fresh browser profile: start E1M1 (-tedlevel 0), save in slot 1
// (F2), quit (F10, Y), wait for "Thanks for playing." and reload the page.
// CONFIG.WL1 (written by Quit()) and SAVEGAM0.WL1 must then be in
// IndexedDB, byte for byte and with the timestamps they had at exit, and
// the reloaded page must read them back.  It fails on any page error or
// console error, such as "The database connection is closing".

import { spawn } from 'node:child_process';
import { resolve } from 'node:path';

const pw = await import(process.env.PLAYWRIGHT || 'playwright');
const args = process.argv.slice(2);
const bi = args.indexOf('--browser');
const browserName = bi < 0 ? 'chromium' : args.splice(bi, 2)[1];
const buildDir = resolve(args[0] || 'build-web');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const port = 8900 + Math.floor(Math.random() * 300);
const server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1',
  '-d', buildDir], { stdio: 'ignore' });
const url = `http://127.0.0.1:${port}/raylib_wolf3d.html?args=-tedlevel%200`;
const DIR = '/wolf3d/config/shareware/raylib_wolf3d';
const FILES = ['CONFIG.WL1', 'SAVEGAM0.WL1'];

const errors = [];
let failed = false;
function check(name, ok, detail) {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${detail ? ': ' + detail : ''}`);
  if (!ok)
    failed = true;
}

// Firefox has no WebGL when headless on some hosts; HEADLESS=0 under
// xvfb-run gives it one.
const browser = await pw[browserName].launch({ headless: process.env.HEADLESS !== '0' });
console.log(`Browser: ${browserName} ${browser.version()}`);
try {
  for (let i = 0; i < 50; i++) {
    try { await fetch(url); break; } catch { await sleep(100); }
  }
  const context = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const page = await context.newPage();
  page.on('console', (m) => {
    console.log(`  ${m.type()}: ${m.text()}`);
    if (m.type() === 'error' || /could not save|connection is closing/.test(m.text()))
      errors.push(m.text());
  });
  page.on('pageerror', (e) => {
    console.log(`  pageerror: ${e.message}`);
    errors.push(e.message);
  });

  async function tap(key, ms = 120) {
    await page.keyboard.down(key);
    await sleep(ms);
    await page.keyboard.up(key);
    await sleep(250);
  }
  const playing = () => page.evaluate(() => !!Module.wolfWantsPointer);
  async function until(want, poke) {
    for (let i = 0; i < 40; i++) {
      if (await playing() === want)
        return true;
      if (poke)
        await tap(poke);
      await sleep(1000);
    }
    return false;
  }
  // The files as the page has them: bytes and modification time.
  const memfs = () => page.evaluate(({ dir, files }) => {
    const out = {};
    for (const f of files) {
      try {
        const p = dir + '/' + f;
        out[f] = { data: Array.from(Module.FS.readFile(p)),
                   time: Module.FS.stat(p).mtime.getTime() };
      } catch { out[f] = null; }
    }
    return out;
  }, { dir: DIR, files: FILES });
  // The files as IndexedDB has them, read without the game's help.
  const idb = () => page.evaluate(({ dir, files }) => new Promise((done, fail) => {
    const req = indexedDB.open('/wolf3d/config');
    req.onerror = () => fail(String(req.error));
    req.onsuccess = () => {
      const db = req.result, out = {};
      const store = db.transaction(['FILE_DATA'], 'readonly').objectStore('FILE_DATA');
      let left = files.length;
      for (const f of files) {
        const get = store.get(dir + '/' + f);
        get.onsuccess = () => {
          const e = get.result;
          out[f] = e ? { data: Array.from(new Uint8Array(e.contents.buffer, e.contents.byteOffset, e.contents.byteLength)), time: e.timestamp.getTime() } : null;
          if (!--left) {
            db.close();
            done(out);
          }
        };
      }
    };
  }), { dir: DIR, files: FILES });
  const same = (a, b) => !!a && !!b && a.time === b.time &&
    a.data.length === b.data.length && a.data.every((v, i) => v === b.data[i]);
  const desc = (e) => e ? `${e.data.length} bytes @${e.time}` : 'missing';

  await page.goto(url);
  await page.waitForSelector('#play:not([disabled])', { timeout: 30000 });
  // Trace the page's IndexedDB writes and the exit, to check the order.
  await page.evaluate(() => {
    const fs = Module.FS, real = fs.syncfs, onExit = Module.onExit;
    const trace = window.wolfTrace = [];
    let n = 0;
    fs.syncfs = (populate, cb) => {
      const k = ++n;
      trace.push(`start ${k}`);
      return real.call(fs, populate, (e) => { trace.push(`end ${k} ${e || 'ok'}`); cb(e); });
    };
    Module.onExit = (status) => { trace.push('exit'); onExit(status); };
  });
  await page.click('#play');
  check('E1M1 starts', await until(true, 'Enter'));
  await sleep(1500);

  await tap('F2');                      // save in slot 1
  await until(false);
  await sleep(1000);
  await tap('Enter');
  await sleep(700);
  for (const ch of 'quittest')
    await tap(ch);
  await tap('Enter');
  await sleep(1500);
  await until(true, 'Escape');
  await sleep(1000);

  await tap('F10');                     // quit; Quit() writes CONFIG.WL1
  await sleep(1500);
  await tap('y');
  await page.waitForFunction(() => document.body.innerText.includes('Thanks for playing.'),
    null, { timeout: 30000 });
  const atExit = await memfs();
  // "Thanks for playing." must wait for a write that started after the exit.
  const trace = await page.evaluate(() => window.wolfTrace);
  const exitAt = trace.indexOf('exit');
  const after = trace.slice(exitAt).map((t) => t.match(/^start (\d+)/)).filter(Boolean)
    .map((m) => m[1]);
  check('the page stores the files after the exit, before it says goodbye',
    exitAt >= 0 && after.some((k) => trace.includes(`end ${k} ok`)), trace.join(', '));
  check('both files written by the game', FILES.every((f) => atExit[f]),
    FILES.map((f) => `${f} ${desc(atExit[f])}`).join(', '));

  // A player may reload as soon as the game says goodbye; give the old
  // page a moment anyway, so a failure here is not a race in this test.
  await sleep(2000);
  await page.goto(url);
  await page.waitForSelector('#play:not([disabled])', { timeout: 30000 });
  const stored = await idb();
  const loaded = await memfs();
  for (const f of FILES) {
    check(`${f} in IndexedDB as written at exit`, same(atExit[f], stored[f]),
      `exit ${desc(atExit[f])}, IndexedDB ${desc(stored[f])}`);
    check(`${f} read back after the reload`, same(atExit[f], loaded[f]),
      `reloaded ${desc(loaded[f])}`);
  }
  await context.close();
} catch (e) {
  check('test ran', false, e.message);
} finally {
  await browser.close();
  server.kill();
}
check('no page or console errors', errors.length === 0, errors.join(' | '));
console.log(failed ? 'FAILED' : 'PASSED');
process.exit(failed ? 1 : 0);
