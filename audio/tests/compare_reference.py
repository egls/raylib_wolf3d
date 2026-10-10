#!/usr/bin/env python3
"""Run actual Wolf4SDL routines with Nuked as their output adapter.

No SDL device or GPL reference code is shipped here. Extracted code stays in
a temporary directory. Pass a reference checkout at the documented SHA.
"""
import argparse
import csv
import hashlib
import pathlib
import re
import struct
import subprocess
import tempfile
import wave


def extract(source, name):
    match = re.search(r'(?m)^(?:static\s+)?(?:void|Sint16)\s+' + name + r'\(', source)
    if not match:
        raise ValueError('Reference function missing: ' + name)
    begin = match.start()
    brace = source.index('{', match.end())
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


PREFIX = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opl3.h"
typedef uint8_t byte; typedef uint16_t word; typedef uint32_t longword;
typedef uint8_t Uint8; typedef int16_t Sint16;
#pragma pack(push,1)
typedef struct { longword length; word priority; } SoundCommon;
typedef struct { byte mChar,cChar,mScale,cScale,mAttack,cAttack,mSus,cSus,mWave,cWave,nConn,voice,mode,unused[3]; } Instrument;
typedef struct { SoundCommon common; Instrument inst; byte block,data[1]; } AdLibSound;
#pragma pack(pop)
#define alChar 0x20
#define alScale 0x40
#define alAttack 0x60
#define alSus 0x80
#define alWave 0xe0
#define alFreqL 0xa0
#define alFreqH 0xb0
#define alFeedCon 0xc0
#define SQUARE_WAVE_AMP 0x2000
static opl3_chip chip;
static const int oplChip=0;
static FILE *tracefile;
static uint64_t frames;
static void YM3812Write(int ignored, unsigned reg, unsigned val) {
 (void)ignored; fprintf(tracefile,"%llu,%u,%u\n",(unsigned long long)frames,reg,val);
 OPL3_WriteReg(&chip,reg,val);
}
#define alOut(n,b) YM3812Write(oplChip,n,b)
static void YM3812UpdateOne(int ignored, int16_t *out, int n) {
 (void)ignored; for(int i=0;i<n;i++) OPL3_GenerateResampled(&chip,out+2*i); frames+=n;
}
#define Quit(msg) do { fprintf(stderr,"%s\n",msg); exit(1); } while(0)
static byte *alSound,alBlock,*curAlSound,*curAlSoundPtr;
static longword alLengthLeft,curAlLengthLeft,alTimeCount,sqHackTime;
static word *sqHack,*sqHackPtr;
static int sqHackLen,sqHackSeqLen,sqActive,SoundNumber,SoundPriority;
static int numreadysamples,soundTimeCounter=5,samplesPerMusicTick=63;
static byte *pcSound,pcLastSample=255;
static longword pcLengthLeft;
static int param_samplerate=44100;
'''

SUFFIX = r'''
int main(int argc,char **argv) {
 (void)argc;
 FILE *in=fopen(argv[2],"rb"),*out=fopen(argv[3],"wb");
 tracefile=fopen(argv[4],"w");
 if(!in || !out || !tracefile) return 1;
 fseek(in,0,SEEK_END); long size=ftell(in); rewind(in);
 byte *data=malloc(size); if(!data || fread(data,1,size,in)!=(size_t)size) return 1;
 OPL3_Reset(&chip,44100); OPL3_WriteReg(&chip,1,0x20);
 if(!strcmp(argv[1],"music")) {
  alOut(0xbd,0); for(int i=1;i<9;i++) alOut(0xb0+i,0);
  unsigned len=data[0] | data[1]<<8;
  sqHack=sqHackPtr=(word *)(void *)(len ? data+2 : data);
  sqHackLen=sqHackSeqLen=len ? len : size; sqActive=1;
 } else if(!strcmp(argv[1],"adlib")) SDL_ALPlaySound((AdLibSound *)(void *)data);
 else if(!strcmp(argv[1],"pc")) { pcLengthLeft=*(uint32_t *)(void *)data; pcSound=data+6; }
 unsigned seconds=(unsigned)atoi(argv[5]);
 if(!strcmp(argv[1],"pcm")) {
   int destsamples=(int)((float)size*44100.0f/7042.0f);
   for(unsigned i=0;i<seconds*44100;i++) {
     int16_t value=i<(unsigned)destsamples ? GetSample((float)size*(float)i/(float)destsamples,data,size) : 0;
     fwrite(&value,2,1,out); fwrite(&value,2,1,out);
   }
 } else for(unsigned i=0;i<seconds*700;i++) {
   int16_t buffer[126]={0};
   if(!strcmp(argv[1],"pc")) SDL_PCMixCallback(NULL,(Uint8 *)buffer,sizeof(buffer));
   else SDL_IMFMusicPlayer(NULL,(Uint8 *)buffer,sizeof(buffer));
   fwrite(buffer,2,126,out);
 }
 free(data); fclose(in); fclose(out); fclose(tracefile); return 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('reference', type=pathlib.Path)
    ap.add_argument('data', type=pathlib.Path)
    ap.add_argument('renderer', type=pathlib.Path)
    args = ap.parse_args()
    actual = subprocess.check_output(
        ['git', '-C', str(args.reference), 'rev-parse', 'HEAD'], text=True).strip()
    expected = 'dc8b250af35fb0ace68db5eb879490b50068c20e'
    if actual != expected:
        raise SystemExit('Unexpected Wolf4SDL revision: ' + actual)
    repo = pathlib.Path(__file__).resolve().parents[1]
    source = subprocess.check_output(
        ['git', '-C', str(args.reference), 'show', expected + ':id_sd.c'],
        text=True)
    functions = ['SDL_ALStopSound', 'SDL_AlSetFXInst', 'SDL_ALPlaySound',
                 'SDL_IMFMusicPlayer', 'SDL_PCMixCallback', 'GetSample']
    h = (args.data / 'AUDIOHED.WL1').read_bytes()
    d = (args.data / 'AUDIOT.WL1').read_bytes()
    v = (args.data / 'VSWAP.WL1').read_bytes()
    if len(h) != 1156:
        raise SystemExit('Expected v1.4 shareware')
    offsets = struct.unpack('<289I', h)
    n, _, base = struct.unpack_from('<HHH', v)
    vo = struct.unpack_from('<%dI' % n, v, 6)
    vl = struct.unpack_from('<%dH' % n, v, 6 + 4*n)
    with tempfile.TemporaryDirectory(prefix='wolf-audio-reference-') as tmp:
        tmp = pathlib.Path(tmp)
        (tmp / 'reference.c').write_text(
            PREFIX + '\n'.join(extract(source, f) for f in functions) + SUFFIX)
        subprocess.run(['cc', '-std=c11', '-O0', '-I', str(repo / 'vendor'),
                        str(tmp / 'reference.c'), str(repo / 'vendor/opl3.c'),
                        '-o', str(tmp / 'reference')], check=True)
        cases = [('music', 0, 12), ('adlib', 0, 2), ('adlib', 24, 2),
                 ('adlib', 26, 2), ('pc', 0, 2), ('pc', 24, 2),
                 ('pcm', 0, 2), ('pcm', 1, 2), ('pcm', 2, 4)]
        for kind, index, seconds in cases:
            if kind == 'pcm':
                start, length = struct.unpack_from('<HH', v, vo[-1] + 4*index)
                raw = b''.join(v[vo[p]:vo[p]+vl[p]]
                               for p in range(base+start, n-1))[:length]
            else:
                slot = index + {'music': 261, 'adlib': 87, 'pc': 0}[kind]
                raw = d[offsets[slot]:offsets[slot+1]]
            (tmp / 'chunk').write_bytes(raw)
            subprocess.run([str(tmp / 'reference'), kind, str(tmp / 'chunk'),
                            str(tmp / 'ref.raw'), str(tmp / 'ref.csv'),
                            str(seconds)], check=True)
            subprocess.run([str(args.renderer.resolve()), str(args.data), kind,
                            str(index), str(seconds), str(tmp / 'out.wav'),
                            '--trace', str(tmp / 'out.csv')], check=True,
                           stdout=subprocess.DEVNULL)
            with wave.open(str(tmp / 'out.wav'), 'rb') as wav:
                ours = wav.readframes(wav.getnframes())
            reference = (tmp / 'ref.raw').read_bytes()
            if ours != reference:
                diffs = sum(a != b for a, b in zip(
                    struct.unpack('<%dh' % (len(ours)//2), ours),
                    struct.unpack('<%dh' % (len(reference)//2), reference)))
                raise SystemExit(f'FAIL {kind} {index}: {diffs} samples differ')
            if kind in ('music', 'adlib'):
                with (tmp / 'out.csv').open() as f:
                    ours_trace = list(csv.reader(f))
                with (tmp / 'ref.csv').open() as f:
                    ref_trace = list(csv.reader(f))
                if ours_trace != ref_trace:
                    raise SystemExit(f'FAIL {kind} {index}: register traces differ')
            print(f'PASS {kind} {index}: {seconds*44100} stereo frames identical, '
                  f'sha256={hashlib.sha256(ours).hexdigest()}')
    print('PASS: actual Wolf4SDL IMF/AdLib/PC/PCM routines; '
          'same Nuked output adapter, no device capture')


if __name__ == '__main__':
    main()
