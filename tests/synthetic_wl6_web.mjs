// PLAYWRIGHT=/path/to/playwright/index.mjs node tests/synthetic_wl6_web.mjs build-web build/synthetic-web-data
import { createServer } from 'node:http';
import { readFileSync, readdirSync } from 'node:fs';
import { resolve, join, extname } from 'node:path';
const pw = await import(process.env.PLAYWRIGHT || 'playwright');
const [build, data] = process.argv.slice(2).map(p => resolve(p));
const files = Object.fromEntries(readdirSync(data).map(n => [n, [...readFileSync(join(data, n))]]));
const server = createServer((req, res) => {
  if (req.url === '/') {
    res.setHeader('Content-Type', 'text/html');
    res.end('<!doctype html><canvas id="canvas"></canvas>');
    return;
  }
  const name = req.url.slice(1);
  if (!['wolf3d-wl6.js', 'wolf3d-wl6.wasm'].includes(name)) {
    res.writeHead(404).end(); return;
  }
  res.setHeader('Content-Type', extname(name) === '.wasm' ? 'application/wasm' : 'text/javascript');
  res.end(readFileSync(join(build, name)));
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
let browser;
try {
  browser = await pw.chromium.launch({ headless: true });
  console.log('Chromium', browser.version());
  for (const broken of [false, true]) {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.evaluate(({ files, broken }) => {
      window.result = { lines: [], status: null };
      window.Module = {
        canvas: document.querySelector('canvas'),
        noInitialRun: true,
        print: s => result.lines.push(s),
        printErr: s => result.lines.push(s),
        onAbort: s => { result.abort = String(s); },
        onExit: status => { result.status = status; },
        preRun: [() => {
          Module.FS.mkdirTree('/synthetic');
          for (const [name, bytes] of Object.entries(files))
            Module.FS.writeFile('/synthetic/' + name, new Uint8Array(bytes));
          if (broken) Module.FS.writeFile('/synthetic/VGAHEAD.WL6', new Uint8Array(3));
        }],
        onRuntimeInitialized: () => Module.callMain(['-data', '/synthetic', '-datacheck', '-nosound'])
      };
    }, { files, broken });
    await page.addScriptTag({ url: '/wolf3d-wl6.js' });
    await page.waitForFunction(() => result.status !== null || result.abort, null, { timeout: 30000 });
    const result = await page.evaluate(() => window.result);
    if (errors.length || result.abort || result.status !== (broken ? 1 : 0))
      throw Error(JSON.stringify({ errors, result }));
    const expected = broken ? 'invalid, mismatched or truncated WL6' : 'PASS: WL6';
    if (!result.lines.some(s => s.includes(expected))) throw Error(JSON.stringify(result));
    console.log('PASS:', broken ? 'wasm rejected mismatched WL6 header' : 'wasm loaded all synthetic WL6 chunks');
    await page.close();
  }
} finally {
  if (browser) await browser.close();
  await new Promise(r => server.close(r));
}
