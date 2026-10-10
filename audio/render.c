/* Shareware-only, offline WAV harness. No device/window dependencies. */
#define _POSIX_C_SOURCE 200809L
#include "sd_audio.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

typedef struct { uint8_t *p; size_t n; } blob;
static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1]<<8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p+2)<<16; }
static void fail(const char *msg) { fprintf(stderr,"%s\n",msg); exit(1); }
static blob read_file(const char *dir, const char *name) {
    char path[4096]; FILE *f; long size; blob b;
    if (snprintf(path,sizeof(path),"%s/%s",dir,name)>=(int)sizeof(path)) fail("Data path too long");
    f=fopen(path,"rb");
    if (!f) { fprintf(stderr,"%s: %s\n",path,strerror(errno)); exit(1); }
    if (fseek(f,0,SEEK_END) || (size=ftell(f))<0 || size>64*1024*1024 || fseek(f,0,SEEK_SET)) fail("Invalid file size");
    b.n=(size_t)size; b.p=malloc(b.n ? b.n : 1);
    if (!b.p || fread(b.p,1,b.n,f)!=b.n) fail("Reading data failed");
    fclose(f); return b;
}
static int span(blob b, size_t off, size_t n) { return off<=b.n && n<=b.n-off; }
static blob chunk(blob h, blob d, unsigned index) {
    size_t start,end; blob b;
    if (h.n%4 || !span(h,(size_t)index*4,8)) fail("Audio index out of range");
    start=u32(h.p+index*4); end=u32(h.p+index*4+4);
    if (end<start || !span(d,start,end-start)) fail("Invalid audio offsets");
    b.p=d.p+start; b.n=end-start; return b;
}
static blob digi(blob v, unsigned index) {
    unsigned pages,base,start,len; size_t table,info,out=0; blob b;
    if (v.n<6) fail("Truncated VSWAP header");
    pages=u16(v.p); base=u16(v.p+4); table=6+(size_t)pages*4;
    if (!pages || base>=pages-1 || !span(v,6,(size_t)pages*6)) fail("Invalid VSWAP directory");
    info=u32(v.p+6+(pages-1)*4);
    if ((size_t)index*4+4>u16(v.p+table+(pages-1)*2) || !span(v,info+(size_t)index*4,4)) fail("Digitized index out of range");
    start=u16(v.p+info+index*4); len=u16(v.p+info+index*4+2);
    if (!len || start>=pages-1-base) fail("Invalid digitized entry");
    b.p=malloc(len); b.n=len;
    if (!b.p) fail("Out of memory");
    for (unsigned page=base+start; out<len; ++page) {
        size_t off,n;
        if (page>=pages-1) fail("Digitized sound extends beyond pages");
        off=u32(v.p+6+page*4); n=u16(v.p+table+page*2);
        if (!off || !n || !span(v,off,n)) fail("Invalid digitized page");
        if (n>len-out) n=len-out;
        memcpy(b.p+out,v.p+off,n); out+=n;
    }
    return b;
}
static void put16(FILE *f, unsigned v) { fputc(v&255,f); fputc((v>>8)&255,f); }
static void put32(FILE *f, uint32_t v) { put16(f,v&65535); put16(f,v>>16); }
static void trace(void *user, uint64_t frame, uint8_t reg, uint8_t value) {
    fprintf((FILE *)user,"%llu,%u,%u\n",(unsigned long long)frame,reg,value);
}
static unsigned number(const char *p) {
    char *end; unsigned long n;
    errno=0; n=strtoul(p,&end,10);
    if (errno || !*p || *p=='-' || *end || n>UINT_MAX) fail("Invalid numeric argument");
    return (unsigned)n;
}
int main(int argc, char **argv) {
    unsigned index,seconds,rate=44100,sounds; uint64_t frames,done=0;
    blob h={0},d={0},v={0},data={0}; sd_audio *s; FILE *wav,*log=NULL;
    int realtime=0,allocated=0; struct timespec start;
    if (argc<6) {
        fprintf(stderr,"Usage: %s DATA_DIR music|adlib|pc|pcm INDEX SECONDS OUT.wav [--rate HZ] [--trace OUT.csv] [--realtime]\n",argv[0]);
        return 1;
    }
    index=number(argv[3]); seconds=number(argv[4]);
    for (int i=6;i<argc;++i) {
        if (!strcmp(argv[i],"--realtime")) realtime=1;
        else if (!strcmp(argv[i],"--rate") && i+1<argc) rate=number(argv[++i]);
        else if (!strcmp(argv[i],"--trace") && i+1<argc) {
            log=fopen(argv[++i],"w"); if (!log) fail("Cannot create trace");
        } else fail("Unknown/missing option");
    }
    frames=(uint64_t)rate*seconds;
    if (!seconds || frames>(UINT32_MAX-36)/4) fail("Duration outside RIFF limits");
    s=sd_audio_create(rate); if (!s) fail("Unsupported sample rate (700..384000 Hz)");
    if (log) sd_audio_set_register_hook(s,trace,log);
    if (!strcmp(argv[2],"pcm")) {
        v=read_file(argv[1],"VSWAP.WL1"); data=digi(v,index); allocated=1;
        if (!sd_audio_pcm(s,data.p,data.n,1,0,0)) fail("Invalid PCM");
    } else {
        h=read_file(argv[1],"AUDIOHED.WL1"); d=read_file(argv[1],"AUDIOT.WL1");
        /* WL1 v1.4 has 288 chunks, earlier source data has 234. Reject other
         * layouts rather than accidentally treating registered data as WL1. */
        if (h.n==1156) sounds=87;
        else if (h.n==940) sounds=69;
        else fail("Unsupported WL1 AUDIOHED layout");
        if (!strcmp(argv[2],"music")) {
            if (index>=27) fail("Music index out of range");
            data=chunk(h,d,sounds*3+index);
            if (!sd_audio_music(s,data.p,data.n,1)) fail("Invalid IMF");
        } else {
            if (index>=sounds) fail("Sound index out of range");
            if (!strcmp(argv[2],"adlib")) {
                data=chunk(h,d,sounds+index);
                if (!sd_audio_adlib(s,data.p,data.n)) fail("Invalid AdLib sound");
            } else if (!strcmp(argv[2],"pc")) {
                data=chunk(h,d,index);
                if (!sd_audio_pc(s,data.p,data.n)) fail("Invalid PC sound");
            } else fail("Unknown sound kind");
        }
    }
    wav=fopen(argv[5],"wb"); if (!wav) fail("Cannot create WAV");
    fwrite("RIFF",1,4,wav); put32(wav,(uint32_t)(36+frames*4)); fwrite("WAVEfmt ",1,8,wav);
    put32(wav,16); put16(wav,1); put16(wav,2); put32(wav,rate); put32(wav,rate*4);
    put16(wav,4); put16(wav,16); fwrite("data",1,4,wav); put32(wav,(uint32_t)(frames*4));
    if (realtime && clock_gettime(CLOCK_MONOTONIC,&start)) fail("Cannot read wall clock");
    while (done<frames) {
        int16_t buffer[512*2]; size_t n=frames-done>512 ? 512 : (size_t)(frames-done);
        sd_audio_mix(s,buffer,n);
        for (size_t i=0;i<n*2;++i) put16(wav,(uint16_t)buffer[i]);
        done+=n;
        if (realtime) {
            uint64_t ns=done*1000000000/rate;
            struct timespec deadline=start;
            deadline.tv_sec+=(time_t)(ns/1000000000);
            deadline.tv_nsec+=(long)(ns%1000000000);
            if (deadline.tv_nsec>=1000000000) { ++deadline.tv_sec; deadline.tv_nsec-=1000000000; }
            int e;
            do { e=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&deadline,NULL); } while(e==EINTR);
            if (e) fail("Wall clock pacing failed");
        }
    }
    if (ferror(wav) || fclose(wav)) fail("Writing WAV failed");
    if (log && (ferror(log) || fclose(log))) fail("Writing trace failed");
    printf("Rendered %llu frames at %u Hz (%u Hz IMF), %s\n",(unsigned long long)frames,rate,sd_audio_music_hz(),realtime ? "wall-clock paced" : "offline sample clock");
    sd_audio_destroy(s); if (allocated) free(data.p);
    free(h.p); free(d.p); free(v.p); return 0;
}
