// WOLF-28/WOLF-42: real browser startup/shutdown preserves audio preferences.
import { spawn } from 'node:child_process';
import { resolve } from 'node:path';
const { chromium } = await import(process.env.PLAYWRIGHT || 'playwright');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const port = 30000 + Math.floor(Math.random() * 10000);
const server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1', '-d', resolve(process.argv[2] || 'build-web')], {stdio:'ignore'});
let browser;
try {
  await sleep(500);
  browser = await chromium.launch({headless:true, executablePath:process.env.CHROMIUM});
  console.log('Browser:', browser.version());
  const context = await browser.newContext({viewport:{width:640,height:480}});
  const config = '/wolf3d/home/raylib_wolf3d/CONFIG.WL1';
  let saved;
  async function run(silent, seed) {
    const page = await context.newPage();
    page.on('console', m => console.log(m.type(), m.text()));
    page.on('pageerror', e => console.log('pageerror', e.message));
    await page.goto(`http://127.0.0.1:${port}/raylib_wolf3d.html?args=${encodeURIComponent('tedlevel 0' + (silent ? ' -nosound' : ''))}`);
    await page.waitForSelector('#play:not([disabled])');
    if (saved) await page.evaluate(({config,seed,saved}) => {
      const b = new Uint8Array(saved);
      const v = new DataView(b.buffer, b.byteOffset, b.byteLength);
      if (seed) seed.forEach((x,i) => v.setUint16(472+i*2,x,true));
      Module.FS.mkdirTree(config.slice(0, config.lastIndexOf('/')));
      Module.FS.writeFile(config,b);
    }, {config,seed,saved});
    await page.click('#play');
    console.log('Started', silent ? 'silent' : 'normal');
    async function tap(key) {
      console.log('Sending', key);
      await page.keyboard.down(key); await sleep(150); await page.keyboard.up(key); await sleep(1500);
    }
    await sleep(8000);
    await tap('Enter'); // startup signon, before tedlevel enters GameLoop
    await sleep(10000);
    await tap('F10');
    await tap('y');
    await page.waitForFunction(() => document.body.innerText.includes('Thanks for playing.'), null, {timeout:30000});
    await sleep(1000);
    const modes = await page.evaluate(config => {
      const b = Module.FS.readFile(config);
      const v = new DataView(b.buffer,b.byteOffset,b.byteLength);
      return [0,1,2].map(i => v.getUint16(472+i*2,true));
    }, config);
    // Carry the file written by WriteConfig into the next startup. IndexedDB
    // persistence is tested separately by WOLF-43; this checks audio policy.
    saved = await page.evaluate(config => Array.from(Module.FS.readFile(config)), config);
    await page.close();
    console.log(silent ? 'silent' : 'normal', JSON.stringify(modes));
    return modes;
  }
  const defaults = await run(true);
  if (JSON.stringify(defaults) !== '[2,1,3]') throw Error('Fresh silent defaults changed');
  const expected = [1,1,3];
  for (const silent of [true,false]) {
    const modes = await run(silent, silent ? expected : null);
    if (JSON.stringify(modes) !== JSON.stringify(expected)) throw Error('Saved preferences changed');
  }
  console.log('PASS: silent then normal browser runs preserve PC / AdLib music / Sound Blaster preferences');
} finally {
  if (browser) await browser.close();
  server.kill();
}
