// web_touch.mjs - play the browser build with touch controls and with
// keyboard and mouse (WOLF-47).
//
//   PLAYWRIGHT=/path/to/node_modules/playwright/index.mjs \
//     node scripts/web_touch.mjs [BUILD_DIR] [OUT_DIR] [--only pixel7|iphone14|desktop]
//
// BUILD_DIR defaults to build-web (with the shareware data bundled), OUT_DIR
// to web-touch.  Chromium only: Playwright's mobile emulation (Pixel 7 and
// iPhone 14 viewports, user agents and touch) is not a real phone, so iOS
// Safari and Android Chrome themselves are not covered.  Touches are real
// touch events (CDP Input.dispatchTouchEvent), several fingers at once.
//
// Phones: title -> New Game -> E1M1 by taps and swipes, then move with the
// stick while firing, turn by dragging, open the first door with USE,
// change weapon, pause and resume, save in slot 1 with the phone keyboard
// (the Y/N quit prompt on the way must not hang the page), load it.
// Desktop (E1M1 via -tedlevel): no touch controls, keyboard play, pointer
// lock, the mouse turns and fires, leaving pointer lock pauses, and
// ?touch=1 forces the overlay.  It fails on any page
// or console error.  Page shots show the controls; canvas shots the game.

import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';

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
const only = flag('--only', null);
const buildDir = resolve(args[0] || 'build-web');
const outDir = resolve(args[1] || 'web-touch');
mkdirSync(outDir, { recursive: true });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const port = 8900 + Math.floor(Math.random() * 300);
const server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1',
  '-d', buildDir], { stdio: 'ignore' });
const base = `http://127.0.0.1:${port}/raylib_wolf3d.html`;

const log = [];
const errors = [];
const result = { checks: {} };
let failed = false;
function check(name, ok, detail) {
  result.checks[name] = { ok: !!ok, detail };
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${detail ? ': ' + detail : ''}`);
  if (!ok)
    failed = true;
}

const browser = await pw.chromium.launch({ headless: true });
result.chromium = browser.version();

// A small greyscale copy of the game picture.  The WebGL canvas cannot be
// read back after it is shown, so this decodes a screenshot in the page.
async function canvasSig(page) {
  const png = await retry(() => page.locator('#canvas').screenshot());
  return page.evaluate(async (b64) => {
    const bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
    const img = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
    const c = document.createElement('canvas');
    c.width = 160; c.height = 100;
    const g = c.getContext('2d');
    g.drawImage(img, 0, 0, 160, 100);
    const d = g.getImageData(0, 0, 160, 100).data, out = [];
    for (let i = 0; i < d.length; i += 4)
      out.push((d[i] + d[i + 1] + d[i + 2]) / 3);
    return out;
  }, png.toString('base64'));
}
function diff(a, b) {
  let d = 0;
  for (let i = 0; i < a.length; i++)
    d += Math.abs(a[i] - b[i]);
  return d / a.length;
}

async function openPage(context, name, query) {
  const page = await context.newPage();
  page.on('console', (m) => {
    const line = `[${name}] ${m.type()}: ${m.text()}`;
    log.push(line);
    if (m.type() === 'error')
      errors.push(line);
  });
  page.on('pageerror', (e) => {
    log.push(`[${name}] pageerror: ${e.message}`);
    errors.push(`[${name}] ${e.message}`);
  });
  for (let i = 0; i < 50; i++) {
    try { await fetch(base); break; } catch { await sleep(100); }
  }
  await page.goto(base + (query ? '?' + query : ''));
  await page.waitForSelector('#play:not([disabled])', { timeout: 30000 });
  return page;
}

// Fingers through CDP: every event carries all fingers still down.
class Fingers {
  constructor(cdp) {
    // A touch that is not handled in 10 s means the page hangs.
    this.cdp = { send: (m, p) => Promise.race([cdp.send(m, p), sleep(10000).then(() => {
      throw new Error('page did not take a touch event in 10 s (hung?)');
    })]) };
    this.down = new Map();
    this.next = 1;
  }
  points() { return [...this.down.entries()].map(([id, p]) => ({ x: p.x, y: p.y, id })); }
  async start(x, y) {
    const id = this.next++;
    this.down.set(id, { x, y });
    await this.cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: this.points() });
    return id;
  }
  async move(id, x, y) {
    this.down.set(id, { x, y });
    await this.cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: this.points() });
  }
  async end(id) {
    this.down.delete(id);
    await this.cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: this.points() });
  }
  // A drag in `steps` moves over `ms`.
  async drag(id, x0, y0, x1, y1, steps, ms) {
    for (let i = 1; i <= steps; i++) {
      await this.move(id, x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps);
      await sleep(ms / steps);
    }
  }
  async tap(x, y, ms = 120) {
    const id = await this.start(x, y);
    await sleep(ms);
    await this.end(id);
  }
}

// Headless Chromium now and then fails a capture; try again once.
async function retry(fn) {
  try { return await fn(); } catch { await sleep(700); return fn(); }
}

async function center(page, sel) {
  const b = await page.locator(sel).boundingBox();
  return [b.x + b.width / 2, b.y + b.height / 2];
}

async function phone(deviceName, tag) {
  const dev = pw.devices[deviceName];
  const context = await browser.newContext({ ...dev });
  const page = await openPage(context, tag, '');
  const cdp = await context.newCDPSession(page);
  const f = new Fingers(cdp);
  const vp = page.viewportSize();
  const shotPage = (n) => retry(() => page.screenshot({ path: join(outDir, `${tag}-${n}.png`) }));
  const shot = (n) => retry(() => page.locator('#canvas').screenshot({ path: join(outDir, `${tag}-${n}.png`) }));
  const tapPic = async (ms) => { await f.tap(vp.width / 2, vp.height / 2, ms); await sleep(400); };
  const tapBtn = async (sel, ms = 150) => { const [x, y] = await center(page, sel); await f.tap(x, y, ms); await sleep(350); };
  const playing = () => page.evaluate(() => Module.wolfPlaying | 0);
  const r = result[tag] = { device: deviceName, viewport: vp };

  r.coarse = await page.evaluate(() => matchMedia('(pointer: coarse)').matches);
  r.touchHelpShown = await page.locator('#panel .touchhelp').isVisible();
  check(`${tag}: coarse pointer, touch help on the start page`, r.coarse && r.touchHelpShown);
  await shotPage('0-start');

  await page.tap('#play');
  await sleep(2500);
  check(`${tag}: touch controls shown`, await page.locator('#touch').isVisible());
  check(`${tag}: no pointer-lock hint on a phone`, !(await page.locator('#hint').isVisible()));
  await tapPic();                       // signon: press a key
  await sleep(1200);
  await tapPic();                       // PG-13
  await sleep(1500);
  await tapPic();                       // title -> main menu
  await sleep(1500);
  await shotPage('1-menu');
  check(`${tag}: menu layout in menus`, await page.locator('#tok').isVisible() &&
        !(await page.locator('#tfire').isVisible()));
  // The shareware menu starts on "Read This!".  Swipe up one item and
  // back down, then go up to New Game with the D-pad (Save Game is
  // greyed out and skipped).
  {
    const before = await canvasSig(page);
    const id = await f.start(vp.width / 2, vp.height * 0.6);
    await f.drag(id, vp.width / 2, vp.height * 0.6, vp.width / 2, vp.height * 0.6 - 50, 5, 200);
    await f.end(id);
    await sleep(600);
    const moved = diff(before, await canvasSig(page));
    await shot('2-menu-swiped-up');
    const id2 = await f.start(vp.width / 2, vp.height * 0.3);
    await f.drag(id2, vp.width / 2, vp.height * 0.3, vp.width / 2, vp.height * 0.3 + 50, 5, 200);
    await f.end(id2);
    await sleep(600);
    const back = diff(before, await canvasSig(page));
    await shot('2-menu-swiped-back');
    check(`${tag}: swipe moves the menu cursor`, moved > 0.3 && back < moved,
      `up ${moved.toFixed(2)}, back ${back.toFixed(2)}`);
    for (let i = 0; i < 5; i++)
      await tapBtn('#tup', 120);
    await sleep(300);
    await shot('2-menu-newgame');
  }
  await tapPic();                       // New Game
  await sleep(800);
  await tapBtn('#tok');                 // episode 1 with the OK button
  await sleep(600);
  await tapBtn('#tup');                 // difficulty: "Can I play, Daddy?"
  await tapBtn('#tup');
  await tapPic();
  await page.waitForFunction(() => Module.wolfPlaying === 1, null, { timeout: 15000 })
    .catch(() => {});
  await sleep(1500);                    // fade-in
  r.playing = await playing();
  await shotPage('3-e1m1-controls');
  await shot('3-e1m1');
  check(`${tag}: E1M1 reached, play layout`, r.playing === 1 &&
        await page.locator('#tfire').isVisible() && !(await page.locator('#tok').isVisible()));

  // Move forward with the stick while holding FIRE (two fingers).
  {
    const before = await canvasSig(page);
    const sx = vp.width * 0.18, sy = vp.height * 0.7;
    const stick = await f.start(sx, sy);
    await f.drag(stick, sx, sy, sx, sy - 70, 4, 150);
    const [fx, fy] = await center(page, '#tfire');
    const fire = await f.start(fx, fy);
    await sleep(250);
    await page.screenshot({ path: join(outDir, `${tag}-4-move-and-fire.png`) });
    await sleep(500);
    await f.end(fire);
    await sleep(300);
    await f.end(stick);
    await sleep(400);
    const d = diff(before, await canvasSig(page));
    await shot('4-moved');
    check(`${tag}: stick moves while FIRE is held (multi-touch)`, d > 2, `picture change ${d.toFixed(2)}`);
  }
  // Walk to the door ahead and open it with USE.
  {
    const sx = vp.width * 0.18, sy = vp.height * 0.7;
    const st = await f.start(sx, sy);
    await f.drag(st, sx, sy, sx, sy - 80, 3, 100);
    await sleep(1200);
    await f.end(st);
    await sleep(300);
    await shot('6-at-door');
    const before = await canvasSig(page);
    await tapBtn('#tuse', 200);
    await sleep(1500);
    const d = diff(before, await canvasSig(page));
    await shot('7-door-open');
    r.doorChange = d;
    check(`${tag}: USE changes the view (door opens)`, d > 2, `picture change ${d.toFixed(2)}`);
  }
  // Turn by dragging on the right half.
  {
    const before = await canvasSig(page);
    const tx = vp.width * 0.6, ty = vp.height * 0.4;
    const id = await f.start(tx, ty);
    await f.drag(id, tx, ty, tx + 120, ty, 12, 500);
    await f.end(id);
    await sleep(300);
    const d = diff(before, await canvasSig(page));
    await shot('5-turned');
    check(`${tag}: drag on the right half turns`, d > 2, `picture change ${d.toFixed(2)}`);
    // and back with the stick (x turns)
    const sx = vp.width * 0.18, sy = vp.height * 0.7;
    const st = await f.start(sx, sy);
    await f.drag(st, sx, sy, sx - 60, sy, 3, 100);
    await sleep(350);
    await f.end(st);
    await sleep(300);
  }
  // Weapon: previous (knife) and next (pistol) - the status bar changes.
  {
    const before = await canvasSig(page);
    await tapBtn('#tprev');
    await sleep(400);
    await shot('8-knife');
    const d = diff(before, await canvasSig(page));
    await tapBtn('#tnext');
    await sleep(400);
    const back = diff(before, await canvasSig(page));
    check(`${tag}: weapon previous/next`, d > 0.2 && back < d,
      `knife ${d.toFixed(2)}, back to pistol ${back.toFixed(2)}`);
  }
  // RUN toggles.
  await tapBtn('#trun');
  check(`${tag}: RUN toggles on`, await page.locator('#trun.on').count() === 1);
  await tapBtn('#trun');
  // Pause and resume.
  {
    await tapBtn('#ttop button[data-bit="65536"]');
    await sleep(700);
    await shotPage('9-paused');
    const a = await canvasSig(page);
    await sleep(600);
    const still = diff(a, await canvasSig(page));
    const menuLayout = await page.locator('#tok').isVisible();
    await tapPic();                     // any key resumes
    await sleep(800);
    r.playingAfterPause = await playing();
    check(`${tag}: PAUSE pauses (menu layout, picture still) and a tap resumes`,
      menuLayout && still < 0.5 && r.playingAfterPause === 1, `still ${still.toFixed(2)}`);
  }
  // Save in slot 1 through the menu with the phone keyboard, then load it.
  {
    await tapBtn('#ttop button[data-bit="256"]');  // MENU
    await sleep(1200);
    await shotPage('10-menu-ingame');
    // main menu: New Game, Sound, Control, Load, Save ... -> Save is 4 down
    for (let i = 0; i < 4; i++)
      await tapBtn('#tdown', 120);
    await tapBtn('#tok');
    await sleep(1000);
    await tapBtn('#tok');               // slot 1
    await sleep(500);
    await tapBtn('#tkbd');
    r.keyboardFocused = await page.evaluate(() => document.activeElement.id === 'tinput');
    // touch players get "Save 1" as the name; replace it
    for (let i = 0; i < 6; i++) {
      await page.keyboard.press('Backspace');
      await sleep(80);
    }
    await page.keyboard.insertText('touch');
    await sleep(800);
    await shotPage('11-save-name');
    await page.keyboard.press('Enter');
    await sleep(1500);
    check(`${tag}: on-screen keyboard input focused for the save name`, r.keyboardFocused);
    await shot('12-saved');
    r.saves = await page.evaluate(() => {
      try {
        return Module.FS.readdir('/wolf3d/config/shareware/raylib_wolf3d')
          .filter((n) => /^SAVEGAM/.test(n));
      } catch (e) { return [String(e)]; }
    });
    // BACK (Esc) on the main menu asks to quit: the Y/N prompt must not
    // hang the page (WOLF-42 made Confirm's loop yield), and N stays.
    // the save starts with its 32-byte name
    r.saveName = await page.evaluate(() => {
      try {
        const b = Module.FS.readFile('/wolf3d/config/shareware/raylib_wolf3d/SAVEGAM0.WL1');
        return String.fromCharCode(...b.slice(0, 32)).replace(/\0.*$/s, '');
      } catch (e) { return String(e); }
    });
    check(`${tag}: save named with the phone keyboard`, r.saveName === 'touch', r.saveName);
    await tapBtn('#tback');
    await sleep(800);
    await shotPage('12b-quit-prompt');
    await tapBtn('#tno');
    await sleep(800);
    r.aliveAfterPrompt = await Promise.race([page.evaluate(() => 1), sleep(5000).then(() => 0)]);
    check(`${tag}: quit prompt answered with N, page responsive`, r.aliveAfterPrompt === 1);
    for (let i = 0; i < 4; i++)         // Save Game -> Back to Game
      await tapBtn('#tdown', 120);
    await tapBtn('#tok');
    await page.waitForFunction(() => Module.wolfPlaying === 1, null, { timeout: 8000 })
      .catch(() => {});
    r.playingAfterSave = await playing();
    // walk away, then load slot 1 from the menu
    const sx = vp.width * 0.18, sy = vp.height * 0.7;
    const st = await f.start(sx, sy);
    await f.drag(st, sx, sy, sx + 60, sy, 3, 100);
    await sleep(700);
    await f.end(st);
    await sleep(300);
    const away = await canvasSig(page);
    await tapBtn('#ttop button[data-bit="256"]');
    await sleep(1200);
    // the cursor is on Back to Game: Load Game is five up
    for (let i = 0; i < 5; i++)
      await tapBtn('#tup', 120);
    await tapBtn('#tok');
    await sleep(1000);
    await shot('13-load-menu');
    const names = r.saves;
    await tapBtn('#tok');               // slot 1
    await page.waitForFunction(() => Module.wolfPlaying === 1, null, { timeout: 10000 })
      .catch(() => {});
    await sleep(800);
    await shot('14-loaded');
    const d = diff(away, await canvasSig(page));
    // and it plays on: turn with the stick
    const before = await canvasSig(page);
    const st2 = await f.start(sx, sy);
    await f.drag(st2, sx, sy, sx - 60, sy, 3, 100);
    await sleep(700);
    await f.end(st2);
    await sleep(300);
    r.turnAfterLoad = diff(before, await canvasSig(page));
    r.playingAfterLoad = await playing();
    await shot('14b-after-load-turned');
    check(`${tag}: saved in slot 1 and loaded it back`, names.length >= 1 &&
      r.playingAfterSave === 1 && r.playingAfterLoad === 1 && d > 2 && r.turnAfterLoad > 2,
      `saves ${names.join(',')}, view change after load ${d.toFixed(2)}, ` +
      `turn after load ${r.turnAfterLoad.toFixed(2)}`);
  }
  // Portrait: controls stay usable, rotate hint shows.
  await page.setViewportSize({ width: vp.height, height: vp.width });
  await sleep(800);
  r.rotateHint = await page.locator('#rotate').isVisible();
  await shotPage('15-portrait');
  check(`${tag}: portrait shows the rotate hint`, r.rotateHint);
  r.scroll = await page.evaluate(() => [window.scrollX, window.scrollY]);
  check(`${tag}: page never scrolled`, r.scroll[0] === 0 && r.scroll[1] === 0);
  await context.close();
}

async function desktop() {
  const tag = 'desktop';
  const context = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const page = await openPage(context, tag, 'args=-tedlevel%200');
  const r = result[tag] = {};
  const shot = (n) => retry(() => page.locator('#canvas').screenshot({ path: join(outDir, `${tag}-${n}.png`) }));
  const tap = async (key, ms = 120) => {
    await page.keyboard.down(key); await sleep(ms); await page.keyboard.up(key); await sleep(250);
  };
  r.coarse = await page.evaluate(() => matchMedia('(pointer: coarse)').matches);
  check(`${tag}: keyboard help, no touch help`, await page.locator('#panel .keyhelp').isVisible() &&
        !(await page.locator('#panel .touchhelp').isVisible()));
  await page.click('#play');
  await sleep(2500);
  check(`${tag}: no touch controls`, !(await page.locator('#touch').isVisible()));
  // The phones go through the menus; here -tedlevel 0 starts E1M1 after
  // the signon screen, so the keyboard and mouse checks do not race the
  // title screen.
  await tap('Enter');
  await page.waitForFunction(() => Module.wolfPlaying === 1, null, { timeout: 15000 })
    .catch(() => {});
  await sleep(1500);
  await shot('1-e1m1');
  let a = await canvasSig(page);
  await tap('ArrowUp', 1000);
  await tap('ArrowLeft', 400);
  check(`${tag}: arrows move and turn`, diff(a, await canvasSig(page)) > 2);
  await tap(' ', 150);                  // open (nothing may be in reach)
  a = await canvasSig(page);
  await tap('2', 150); await tap('1', 150); await sleep(300);
  check(`${tag}: number keys change weapon`, diff(a, await canvasSig(page)) > 0.2);
  await tap('2', 150);
  await tap('Control', 300);            // fire
  r.scroll = await page.evaluate(() => [window.scrollX, window.scrollY]);
  check(`${tag}: game keys do not scroll`, r.scroll[0] === 0 && r.scroll[1] === 0);
  // Firefox-style quick find keys and Tab are kept from the browser
  r.prevented = await page.evaluate(() => ['/', "'", 'Tab', 'F3', 'F5', 'a'].map((k) => {
    const e = new KeyboardEvent('keydown', { key: k, code: 'KeyZ', cancelable: true, bubbles: true });
    window.dispatchEvent(e);
    window.dispatchEvent(new KeyboardEvent('keyup', { key: k, code: 'KeyZ', bubbles: true }));
    return e.defaultPrevented;
  }));
  r.f12 = await page.evaluate(() => {
    const e = new KeyboardEvent('keydown', { key: 'F12', code: 'F12', cancelable: true, bubbles: true });
    window.dispatchEvent(e);
    window.dispatchEvent(new KeyboardEvent('keyup', { key: 'F12', code: 'F12', bubbles: true }));
    return e.defaultPrevented;
  });
  check(`${tag}: browser shortcuts kept from game keys, F12 left alone`,
    r.prevented.every(Boolean) && !r.f12, JSON.stringify(r.prevented));

  const box = await page.locator('#canvas').boundingBox();
  await page.mouse.click(box.x + box.width / 2, box.y + box.height / 2);
  await sleep(500);
  r.locked = await page.evaluate(() => document.pointerLockElement === Module.canvas);
  check(`${tag}: pointer lock after a click`, r.locked);
  // Headless Chromium's movementX under pointer lock is erratic (a +300 px
  // drag can add up to -600), so: real moves must reach the page's
  // accumulator and be taken by the game, and a known amount must turn.
  await page.evaluate(() => {
    window.wolfMoves = 0;
    Module.canvas.addEventListener('mousemove', () => { window.wolfMoves++; });
  });
  for (let i = 0; i < 20; i++) {
    await page.mouse.move(box.x + box.width / 2 + (i + 1) * 15, box.y + box.height / 2);
    await sleep(30);
  }
  await sleep(400);
  r.mouseEvents = await page.evaluate(() => window.wolfMoves);
  r.mouseLeft = await page.evaluate(() => Module.wolfMouseDX);
  a = await canvasSig(page);
  await page.evaluate(() => { Module.wolfMouseDX += 400; });
  await sleep(500);
  r.mouseTurn = diff(a, await canvasSig(page));
  check(`${tag}: mouse motion reaches the game and turns`,
    r.mouseEvents > 0 && r.mouseLeft === 0 && r.mouseTurn > 2,
    `${r.mouseEvents} moves taken, picture change ${r.mouseTurn.toFixed(2)} for 400 counts`);
  await page.mouse.down(); await sleep(250); await page.mouse.up();
  await sleep(500);
  await shot('2-mouse');
  // Leaving pointer lock (as Esc does) pauses.
  await page.evaluate(() => document.exitPointerLock());
  await sleep(800);
  a = await canvasSig(page);
  await sleep(500);
  r.pausedStill = diff(a, await canvasSig(page));
  r.playingWhilePaused = await page.evaluate(() => Module.wolfPlaying | 0);
  await shot('3-pointer-released-paused');
  check(`${tag}: leaving pointer lock pauses`, r.playingWhilePaused === 0 && r.pausedStill < 0.5);
  await tap('Enter');
  await sleep(600);
  check(`${tag}: a key resumes`, await page.evaluate(() => Module.wolfPlaying | 0) === 1);
  await context.close();

  // ?touch=1 on a desktop: overlay on, keyboard still works.
  const c2 = await browser.newContext({ viewport: { width: 1280, height: 800 } });
  const p2 = await openPage(c2, 'desktop-touch1', 'touch=1');
  await p2.click('#play');
  await sleep(2500);
  r.forcedOverlay = await p2.locator('#touch').isVisible();
  for (const k of ['Enter', 'Enter', 'Enter']) {
    await p2.keyboard.down(k); await sleep(120); await p2.keyboard.up(k); await sleep(1500);
  }
  await p2.screenshot({ path: join(outDir, 'desktop-4-touch1-menu.png') });
  r.forcedMenuLayout = await p2.locator('#tok').isVisible();
  check(`${tag}: ?touch=1 forces the overlay; keyboard still drives the menu`,
    r.forcedOverlay && r.forcedMenuLayout);
  await c2.close();
}

try {
  if (!only || only === 'pixel7')
    await phone('Pixel 7 landscape', 'pixel7');
  if (!only || only === 'iphone14')
    await phone('iPhone 14 landscape', 'iphone14');
  if (!only || only === 'desktop')
    await desktop();
  check('no page errors or console errors', errors.length === 0, errors.slice(0, 5).join(' | '));
} catch (e) {
  check('script ran to the end', false, String(e && e.stack || e));
} finally {
  await browser.close();
  server.kill();
  writeFileSync(join(outDir, 'console.log'), log.join('\n') + '\n');
  writeFileSync(join(outDir, 'result.json'), JSON.stringify(result, null, 2) + '\n');
}
process.exit(failed ? 1 : 0);
