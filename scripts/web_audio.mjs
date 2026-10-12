// WOLF-42: real-time WebAudio output capture and browser policy checks.
// Build with -DWOLF3D_WEB_AUDIO_TRACE=ON for accepted sound-event timing.
// PLAYWRIGHT=/path/to/playwright/index.mjs node scripts/web_audio.mjs build-web out
import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve, join } from 'node:path';
const pw = await import(process.env.PLAYWRIGHT || 'playwright');
const build = resolve(process.argv[2] || 'build-web');
const out = resolve(process.argv[3] || 'web-audio');
mkdirSync(out, { recursive: true });
let server;
const port=30000+Math.floor(Math.random()*10000);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const browser = await pw.chromium.launch({headless:true, executablePath:process.env.CHROMIUM, args:['--autoplay-policy=user-gesture-required']});
const result = {browser:browser.version(), viewport:{width:640,height:480}, checks:{}};
let failed = false;
const check = (name, ok, detail) => {
  result.checks[name]={ok, detail}; failed ||= !ok;
  console.log(`${ok?'PASS':'FAIL'} ${name}: ${JSON.stringify(detail)}`);
};
function wav(samples, rate, name) {
  const b=Buffer.alloc(44+samples.length);
  b.write('RIFF'); b.writeUInt32LE(b.length-8,4); b.write('WAVEfmt ',8);
  b.writeUInt32LE(16,16); b.writeUInt16LE(1,20); b.writeUInt16LE(2,22);
  b.writeUInt32LE(rate,24); b.writeUInt32LE(rate*4,28); b.writeUInt16LE(4,32);
  b.writeUInt16LE(16,34); b.write('data',36); b.writeUInt32LE(b.length-44,40);
  samples.copy(b,44);
  writeFileSync(join(out,name),b);
}
try {
  server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1', '-d', build], {stdio:'ignore'});
  await sleep(500);
  const page=await browser.newPage({viewport:result.viewport});
  page.on('pageerror',e=>{failed=true; console.log(e);});
  await page.addInitScript(()=>{
    window.audioCapture={enabled:false, blocks:[], events:[]};
    const original=AudioContext.prototype.createScriptProcessor;
    AudioContext.prototype.createScriptProcessor=function(...args) {
      const node=original.apply(this,args);
      const ctx=this;
      Object.defineProperty(node,'onaudioprocess',{set(fn) {
        node.addEventListener('audioprocess',function(e) {
          fn(e);
          if (!window.audioCapture.enabled) return;
          const l=e.outputBuffer.getChannelData(0), r=e.outputBuffer.getChannelData(1);
          const data=new Float32Array(l.length*2); for(let i=0;i<l.length;i++) {data[i*2]=l[i];data[i*2+1]=r[i];}
          window.audioCapture.blocks.push({data, now:performance.now(), playback:e.playbackTime,
            contextTime:ctx.currentTime, rate:ctx.sampleRate});
        });
      }});
      return node;
    };
    for(const type of ['keydown','keyup']) document.addEventListener(type,e=>window.audioCapture.events.push({type,key:e.key,now:performance.now()}));
  });
  await page.goto(`http://127.0.0.1:${port}/raylib_wolf3d.html`);
  await page.waitForSelector('#play:not([disabled])');
  await page.click('#play');
  await page.waitForFunction(()=>Module.wolfAudioStats);
  const stats=()=>page.evaluate(()=>({...Module.wolfAudioStats, context:Module.wolfAudio.context.state}));
  async function tap(key,ms=120) {await page.keyboard.down(key); await sleep(ms); await page.keyboard.up(key); await sleep(800);}
  async function capture(name,action) {
    await page.evaluate(()=>{audioCapture.blocks=[];audioCapture.events=[];audioCapture.enabled=true;Module.wolfAudioTrace=[];});
    const initial=await stats();
    await action();
    const capture=await page.evaluate(()=>{
      audioCapture.enabled=false;
      const endStats={...Module.wolfAudioStats};
      const soundEvents=Module.wolfAudioTrace;Module.wolfAudioTrace=null;
      const frames=audioCapture.blocks.reduce((n,b)=>n+b.data.length/2,0);
      const pcm=new Uint8Array(frames*4);const view=new DataView(pcm.buffer);
      let offset=0,peak=0;
      const blocks=audioCapture.blocks.map(b=>{
        let first=-1;
        for(let i=0;i<b.data.length;i++) {
          const x=b.data[i];peak=Math.max(peak,Math.abs(x));
          if(first<0&&Math.abs(x)>0.001) first=i;
          view.setInt16(offset,Math.round(Math.max(-1,Math.min(1,x))*32767),true);offset+=2;
        }
        return {first,now:b.now,playback:b.playback,contextTime:b.contextTime,rate:b.rate};
      });
      let binary='';for(let i=0;i<pcm.length;i+=8192) binary+=String.fromCharCode(...pcm.subarray(i,i+8192));
      return {blocks,events:audioCapture.events,soundEvents,endStats,frames,peak,pcm:btoa(binary)};
    });
    wav(Buffer.from(capture.pcm,'base64'),capture.blocks[0]?.rate||48000,name+'.wav');
    result[name]={blocks:capture.blocks.length, frames:capture.frames,
      underrunDelta:capture.endStats.underruns-initial.underruns,
      peak:capture.peak, stats:capture.endStats};
    return capture;
  }
  await sleep(2500); await tap('Enter'); await sleep(1500); await tap('Enter'); await sleep(2000);
  const title=await capture('title',()=>sleep(5000));
  check('title music nonzero',title.blocks.some(b=>b.first>=0),result.title);
  // A context suspended after startup must freeze synthesis, and a key unlocks it.
  await page.evaluate(()=>Module.wolfAudio.context.suspend()); await sleep(500);
  const before=await stats(); await sleep(1000); const after=await stats();
  check('suspended context has no backlog',after.queuedFrames===0&&after.generatedFrames===before.generatedFrames,after);
  await tap('Shift'); await sleep(500);
  check('keyboard unlock', (await stats()).context==='running',await stats());
  // Deterministic visibility event exercises the real handler under headless Chromium.
  await page.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,get:()=>true});document.dispatchEvent(new Event('visibilitychange'));});
  await sleep(1500); check('hidden tab suspended', (await stats()).context==='suspended',await stats());
  await page.evaluate(()=>{delete document.hidden;document.dispatchEvent(new Event('visibilitychange'));});
  await sleep(500); check('visible tab rebased', (await stats()).queuedFrames<=3840,await stats());
  const prior=(await stats()).resyncs;
  await page.evaluate(()=>{const until=performance.now()+400;while(performance.now()<until) {}});
  await sleep(500);
  check('long main-thread stall rebases queue',(await stats()).resyncs>prior&&(await stats()).queuedFrames<=1536,await stats());
  await sleep(1500);
  await tap('s'); await tap('Enter'); await sleep(700);
  await page.locator('#canvas').screenshot({path:join(out,'sound-menu.png')});
  async function move(key,n) {for(let i=0;i<n;i++) await tap(key);}
  async function select(name,kind) {
    await sleep(900);
    const c=await capture(name,async()=>{await tap('Enter');await sleep(800);});
    check(name+' produces sound',c.blocks.some(b=>b.first>=0),result[name]);
    check(name+' accepted expected type',c.soundEvents.some(e=>e.kind===kind),c.soundEvents);
    const event=c.soundEvents[0];
    let latency=null;
    for(const b of c.blocks) {
      const i=b.first;
      if(i>=0 && event) {const candidate=b.now+(b.playback-b.contextTime)*1000+Math.floor(i/2)/b.rate*1000-event.now;if(candidate>=0) {latency=candidate;break;}}
    }
    result[name].latencyMs=latency;result[name].soundEvents=c.soundEvents;
    check(name+' sound event to WebAudio about100 ms (110 ms tolerance)',latency!==null&&latency>=0&&latency<=110,latency);
  }
  // Active rows: FX none/PC/AdLib, digi none/SB, music none/AdLib.
  await move('ArrowDown',5); await tap('Enter'); // music off
  await move('ArrowUp',2); await tap('Enter');   // digitized off
  await move('ArrowUp',2); await select('pc','pc');  // PC speaker gun
  await tap('ArrowUp'); await tap('Enter'); await sleep(1200); // FX none
  const none=await capture('none',()=>sleep(1000));
  check('all modes off silent',none.blocks.every(b=>b.first<0),result.none);
  await tap('ArrowDown'); await select('pc-again','pc');
  await tap('ArrowDown'); await select('adlib','adlib');
  await move('ArrowDown',2); await select('digitized-on','adlib');
  await move('ArrowDown',2); await select('music-on','music');
  await tap('ArrowUp'); await tap('Enter');
  // Test in-game PCM in silence for event-to-output latency.
  // Leave sound menu, start E1M1, fire the pistol and open the starting door.
  await tap('Escape'); await sleep(1000); await tap('n'); await tap('Enter'); await sleep(800);
  await tap('Enter'); await sleep(800); await tap('Enter');
  await page.waitForFunction(()=>Module.wolfWantsPointer===1,null,{timeout:45000});
  await sleep(1500);
  await page.locator('#canvas').screenshot({path:join(out,'game.png')});
  const effects=await capture('effects',async()=>{await sleep(1000);await tap('Control',500); await tap('Space'); await sleep(2500);});
  check('effects capture nonzero',result.effects.peak>0.001,result.effects);
  result.final=await stats();
  check('in-game digitized PCM accepted',effects.soundEvents.some(e=>e.kind==='pcm'),effects.soundEvents);
  result.effectEvents=effects.events;result.effectSoundEvents=effects.soundEvents;
  const press=effects.events.find(e=>e.key==='Control'&&e.type==='keydown');
  const event=effects.soundEvents.find(e=>e.kind==='pcm'&&e.now>=press.now);
  for(const b of effects.blocks) {
    const i=b.first;
    if(i>=0&&event) {
      const latency=b.now+(b.playback-b.contextTime)*1000+Math.floor(i/2)/b.rate*1000-event.now;
      if(latency>=0) {result.fireLatencyMs=latency;break;}
    }
  }
  check('fire sound event to WebAudio <=100 ms',result.fireLatencyMs>=0&&result.fireLatencyMs<=100,result.fireLatencyMs);
  result.audioDevice=await page.evaluate(()=>({sampleRate:Module.wolfAudio.context.sampleRate,
    baseLatency:Module.wolfAudio.context.baseLatency, outputLatency:Module.wolfAudio.context.outputLatency,
    scriptFrames:window.miniaudio.devices.find(d=>d).scriptNode.bufferSize,
    shared:typeof SharedArrayBuffer!=='undefined' && Module.HEAPF32.buffer instanceof SharedArrayBuffer}));
  check('no shared memory',!result.audioDevice.shared,result.audioDevice);
} finally {
  await browser.close(); server?.kill();
  writeFileSync(join(out,'result.json'),JSON.stringify(result,null,2)+'\n');
}
process.exit(failed?1:0);
