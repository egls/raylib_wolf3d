// web_shell.mjs - check the web page's data and save handling (WOLF-43).
//
//   PLAYWRIGHT=/path/to/node_modules/playwright/index.mjs \
//     node scripts/web_shell.mjs [BUILD_DIR] [OUT_DIR] [--browser chromium|firefox] [--data DIR]
//
// BUILD_DIR defaults to build-web, OUT_DIR to web-shell.  --data is the
// shareware WL1 directory to copy for the "your own data" steps (default
// ~/wolf3d-data/shareware); the copies get lower-case names and stay in
// OUT_DIR.  One browser profile is kept across reloads, so IndexedDB
// survives as it would for a player.  Steps:
//   1. first load: bundled shareware, Play enabled;
//   2. rejected sets: incomplete, mixed .WL1/.WL6, .WL6 only, .SOD;
//   3. load the shareware copy through the file picker: the page reloads
//      with ?data=user and plays it;
//   4. New Game, save in slot 1, reload, the save is still there (IndexedDB),
//      load it from the menu, and it is not in the shareware set's saves;
//   5. "Use shareware data", "Forget my data";
//   6. without IndexedDB, your own data still plays for the visit.
// It fails on any page error or console error.  Shots are of the page.

import { spawn, spawnSync } from 'node:child_process';
import { mkdirSync, writeFileSync, readFileSync, readdirSync, copyFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { homedir } from 'node:os';

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
const dataDir = resolve(flag('--data', join(homedir(), 'wolf3d-data/shareware')));
const buildDir = resolve(args[0] || 'build-web');
const outDir = resolve(args[1] || 'web-shell');
mkdirSync(outDir, { recursive: true });

// Test sets, outside the repository.
const BASES = ['VSWAP', 'MAPHEAD', 'GAMEMAPS', 'VGAHEAD', 'VGADICT', 'VGAGRAPH', 'AUDIOHED', 'AUDIOT'];
const sets = join(outDir, 'sets');
function makeSet(name, files) {
  const dir = join(sets, name);
  mkdirSync(dir, { recursive: true });
  for (const [src, dst] of files)
    copyFileSync(join(dataDir, src), join(dir, dst));
  return readdirSync(dir).map((f) => join(dir, f));
}
const lower = makeSet('lower', BASES.map((b) => [b + '.WL1', b.toLowerCase() + '.wl1']));
const incomplete = makeSet('incomplete', BASES.filter((b) => b !== 'VGAGRAPH')
  .map((b) => [b + '.WL1', b + '.WL1']));
const mixed = makeSet('mixed', BASES.map((b, i) => [b + '.WL1', b + (i < 4 ? '.WL1' : '.WL6')]));
const wl6 = makeSet('wl6', BASES.map((b) => [b + '.WL1', b + '.WL6']));
const sod = makeSet('sod', BASES.map((b) => [b + '.WL1', b + '.SOD']));

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const port = 8600 + Math.floor(Math.random() * 300);
const server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1',
  '-d', buildDir], { stdio: 'ignore' });
// A sub-directory URL, like the /wolf3d/ deploy, is covered by the
// relative URLs; the local server serves the build at its root.
const base = `http://127.0.0.1:${port}/raylib_wolf3d.html`;

const log = [];
const errors = [];
const result = { browser: browserName, base, checks: {} };
let failed = false;
function check(name, ok, detail) {
  result.checks[name] = { ok, detail };
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${detail ? ': ' + detail : ''}`);
  if (!ok)
    failed = true;
}

const browser = await pw[browserName].launch({ headless: true });
result.version = browser.version();
const context = await browser.newContext({ viewport: { width: 1280, height: 800 } });
let shotn = 0;
try {
  for (let i = 0; i < 50; i++) {
    try { await fetch(base); break; } catch { await sleep(100); }
  }

  const page = await context.newPage();
  page.on('console', (m) => {
    const line = `${m.type()}: ${m.text()}`;
    log.push(line);
    if (m.type() === 'error')
      errors.push(line);
  });
  page.on('pageerror', (e) => {
    log.push(`pageerror: ${e.message}`);
    errors.push(e.message);
  });
  page.on('dialog', (d) => { log.push(`dialog: ${d.message()}`); d.accept(); });

  const shot = async (name) => {
    const file = `${String(++shotn).padStart(2, '0')}-${name}.png`;
    await page.screenshot({ path: join(outDir, file) });
    return file;
  };
  const ready = () => page.waitForSelector('#play:not([disabled])', { timeout: 30000 });
  async function tap(key, ms = 120) {
    await page.keyboard.down(key);
    await sleep(ms);
    await page.keyboard.up(key);
    await sleep(250);
  }
  async function type(text) {
    for (const ch of text)
      await tap(ch);
  }
  // From "Click to play" to the main menu (signon, PG-13, title).
  async function toMenu() {
    await page.click('#play');
    await sleep(2500);
    await tap('Enter');
    await sleep(1500);
    await tap('Enter');
    await sleep(2000);
    await tap('Enter');
    await sleep(3500);                  // the menu fades in
  }
  const fsList = (dir) => page.evaluate((d) => {
    try { return Module.FS.readdir(d).filter((n) => n[0] !== '.'); } catch { return null; }
  }, dir);
  const text = (sel) => page.locator(sel).innerText();

  // 1. First load
  await page.goto(base);
  await ready();
  const name1 = await text('#dataname');
  await shot('first-load');
  check('first load plays the bundled shareware', /shareware/.test(name1), name1);
  check('a stored set is offered only once there is one',
    await page.locator('#forget').isHidden());

  // 2. Rejected sets
  for (const [label, files, re] of [
    ['incomplete set', incomplete, /Missing VGAGRAPH\.WL1/],
    ['mixed .WL1/.WL6 set', mixed, /more than one set/],
    ['.WL6 set', wl6, /coming soon/],
    ['.SOD set', sod, /not supported/],
  ]) {
    await page.setInputFiles('#picker', files);
    await sleep(300);
    const msg = await text('#datamsg');
    check(`rejects an ${label}`, re.test(msg) && page.url() === base, msg);
    if (label === 'incomplete set')
      await shot('reject-incomplete');
  }

  // 3. Load your own data (the shareware files, lower-case names)
  await Promise.all([
    page.waitForURL(/data=user/, { timeout: 20000 }),
    page.setInputFiles('#picker', lower),
  ]);
  await ready();
  const name2 = await text('#dataname');
  await shot('own-data-loaded');
  check('loaded set restarts the page with ?data=user', /your own data \(wl1-[0-9a-f]{8}\)/.test(name2), name2);
  const userFiles = await fsList('/wolf3d/user');
  check('own data stored with engine names', BASES.every((b) => userFiles.includes(b + '.WL1')),
    userFiles.join(' '));
  const setId = name2.match(/wl1-[0-9a-f]{8}/)[0];
  const userConf = `/wolf3d/config/user-${setId}/raylib_wolf3d`;

  // 4. Save, reload, load.  -tedlevel 0 starts in E1M1 (no intro or menu
  // timing to depend on); F2 and F3 are the game's save and load keys.
  const playUrl = base + '?data=user&args=-tedlevel%200';
  // The game asks for the mouse (Module.wolfWantsPointer) only while it is
  // being played, so that tells gameplay from menus and title screens.
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
  const startGame = async () => {
    await page.click('#play');
    await until(true, 'Enter');         // signon "press a key", "Get Psyched!"
    await sleep(1500);                  // fade-in
  };
  await page.goto(playUrl);
  await ready();
  await startGame();
  await tap('ArrowUp', 900);            // walk and turn, away from the start
  await tap('ArrowLeft', 400);
  await sleep(500);
  const before = await shot('game-before-save');
  await tap('F2');
  await until(false);
  await sleep(1000);
  await tap('Enter');                   // slot 1
  await sleep(700);
  await type('webtest');
  await tap('Enter');
  await sleep(1500);
  await shot('saved');
  await until(true, 'Escape');          // back to the game
  const saved = await fsList(userConf);
  check('save written', saved && saved.includes('SAVEGAM0.WL1'), String(saved));
  await sleep(1500);                    // let FS.syncfs finish

  await page.goto(playUrl);
  await ready();
  const afterReload = await fsList(userConf);
  check('save still there after a reload (IndexedDB)', afterReload && afterReload.includes('SAVEGAM0.WL1'),
    String(afterReload));
  const shareSaves = await fsList('/wolf3d/config/shareware/raylib_wolf3d');
  check('shareware set does not see it', !shareSaves || !shareSaves.includes('SAVEGAM0.WL1'),
    String(shareSaves));
  await startGame();
  const fresh = await shot('game-fresh-start');
  await tap('F3');
  await until(false);
  await sleep(1000);
  await shot('load-menu');
  await tap('Enter');                   // slot 1

  // The 3D view (above the status bar) must become the saved one, not the
  // start ("Get Psyched!" comes first).
  const match = (a, b) => parseFloat(spawnSync('python3', ['-c', `
import sys
from PIL import Image, ImageChops
a, b = (Image.open(f).convert('RGB') for f in sys.argv[1:3])
box = (0, 0, a.width, int(a.height * 0.78))
d = ImageChops.difference(a.crop(box), b.crop(box)).convert('L')
print(sum(1 for v in d.getdata() if v < 8) / (d.width * d.height))
`, join(outDir, a), join(outDir, b)], { encoding: 'utf8' }).stdout);
  const after = `${String(++shotn).padStart(2, '0')}-game-after-load.png`;
  let same = 0;
  for (let i = 0; i < 30 && !(same > 0.97); i++) {
    await sleep(1000);
    await page.screenshot({ path: join(outDir, after) });
    same = match(before, after);
  }
  const differ = match(fresh, after);
  check('loaded game shows the saved view', same > 0.97 && differ < same,
    `${(same * 100).toFixed(1)}% of ${after} matches ${before}, ${(differ * 100).toFixed(1)}% matches ${fresh}`);
  result.shots = { before, fresh, after };

  // 5. Use shareware, then forget
  await page.goto(base);
  await ready();
  await Promise.all([page.waitForNavigation(), page.click('#useshare')]);
  await ready();
  const name3 = await text('#dataname');
  check('"Use shareware data" switches back', /shareware/.test(name3), name3);
  check('stored set kept after switching', await page.locator('#useuser').isVisible());
  await Promise.all([page.waitForNavigation(), page.click('#forget')]);
  await ready();
  const left = await fsList('/wolf3d/user');
  await shot('forgotten');
  check('"Forget my data" empties the store', left && left.length === 0, String(left));

  // 6. No IndexedDB (some private modes): own data still plays this visit.
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } });
    await ctx.addInitScript(() => { Object.defineProperty(window, 'indexedDB', { value: undefined }); });
    const p2 = await ctx.newPage();
    p2.on('pageerror', (e) => errors.push(e.message));
    p2.on('dialog', (d) => { log.push(`dialog: ${d.message()}`); d.accept(); });
    await p2.goto(base);
    await p2.waitForSelector('#play:not([disabled])', { timeout: 30000 });
    await p2.setInputFiles('#picker', lower);
    await p2.waitForFunction(() => /your own data/.test(document.getElementById('dataname').textContent),
      null, { timeout: 10000 }).catch(() => {});
    const name = await p2.locator('#dataname').innerText();
    const status = await p2.locator('#status').innerText();
    check('without IndexedDB, own data is used for the visit', /your own data/.test(name)
      && /cannot keep/.test(status) && p2.url() === base, `${name} / ${status}`);
    await ctx.close();
  }
  check('no page errors or console errors', errors.length === 0, errors.slice(0, 5).join(' | '));
} finally {
  await browser.close();
  server.kill();
  writeFileSync(join(outDir, 'console.log'), log.join('\n') + '\n');
  writeFileSync(join(outDir, 'result.json'), JSON.stringify(result, null, 2) + '\n');
}
process.exit(failed ? 1 : 0);
