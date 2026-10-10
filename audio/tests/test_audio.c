#include "sd_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint8_t music[]={8,0,0x20,1,2,0,0x40,2,20,0};
static uint8_t adlib[27]={4,0,0,0,10,0, 1,1,0,0,0xf0,0xf0,0x77,0x77,0,0,0,0,0,0,0,0,4, 64,0,80,100};
static uint8_t pc[]={4,0,0,0,10,0,30,60,0,30};
static uint8_t pcm[]={128,255,0,128};
static uint64_t event_frame[100]; static unsigned event_reg[100],events;
static void hook(void *p,uint64_t frame,uint8_t r,uint8_t v) {
    (void)p; (void)v;
    CHECK(events<100); event_frame[events]=frame; event_reg[events++]=r;
}
static void timing(void) {
    sd_audio *s=sd_audio_create(44100); int16_t out[2000];
    CHECK(s); sd_audio_set_register_hook(s,hook,NULL);
    CHECK(sd_audio_music(s,music,sizeof(music),1)); events=0;
    sd_audio_mix(s,out,1000);
    CHECK(events>4); CHECK(event_frame[0]==0 && event_reg[0]==0x20);
    CHECK(event_frame[1]==(2*44100+sd_audio_music_hz()-1)/sd_audio_music_hz());
    /* Last delay ignored: loop begins on following tick. */
    CHECK(event_frame[2]==(3*44100+sd_audio_music_hz()-1)/sd_audio_music_hz());
    sd_audio_destroy(s);
    s=sd_audio_create(44100); sd_audio_set_register_hook(s,hook,NULL);
    CHECK(sd_audio_adlib(s,adlib,sizeof(adlib))); events=0;
    sd_audio_mix(s,out,1000);
    CHECK(events && event_reg[0]==0xa0);
    CHECK(event_frame[0]==((sd_audio_music_hz()/140-1)*44100+sd_audio_music_hz()-1)/sd_audio_music_hz());
    sd_audio_destroy(s);
}
static void contracts(void) {
    sd_audio *s=sd_audio_create(44100); int16_t out[4096]; uint8_t lower[27];
    CHECK(!sd_audio_create(0)); CHECK(!sd_audio_create(400000));
    CHECK(!sd_audio_music(s,music,3,1)); CHECK(!sd_audio_music(s,music,9,1));
    CHECK(!sd_audio_adlib(s,adlib,26)); CHECK(!sd_audio_pc(s,pc,9));
    CHECK(sd_audio_adlib(s,adlib,sizeof(adlib)));
    memcpy(lower,adlib,sizeof(lower)); lower[4]=9;
    CHECK(!sd_audio_adlib(s,lower,sizeof(lower)));
    sd_audio_mix(s,out,2048); CHECK(sd_audio_adlib(s,lower,sizeof(lower)));
    CHECK(!sd_audio_pcm(s,pcm,4,1,16,0)); CHECK(!sd_audio_pcm(s,pcm,4,1,15,15));
    CHECK(sd_audio_pcm(s,pcm,4,2,0,0)); CHECK(!sd_audio_pcm(s,pcm,4,1,0,0));
    sd_audio_stop_effect(s); sd_audio_stop_pcm(s); sd_audio_stop_music(s);
    sd_audio_destroy(s);
}
static void partition(void) {
    int16_t a[12000],b[12000]; sd_audio *x=sd_audio_create(48001),*y=sd_audio_create(48001);
    CHECK(sd_audio_music(x,music,sizeof(music),1)); CHECK(sd_audio_music(y,music,sizeof(music),1));
    CHECK(sd_audio_adlib(x,adlib,sizeof(adlib))); CHECK(sd_audio_adlib(y,adlib,sizeof(adlib)));
    CHECK(sd_audio_pcm(x,pcm,4,1,2,5)); CHECK(sd_audio_pcm(y,pcm,4,1,2,5));
    sd_audio_mix(x,a,6000);
    for (unsigned i=0;i<6000;) { unsigned n=(i%71)+1; if (n>6000-i) n=6000-i; sd_audio_mix(y,b+i*2,n); i+=n; }
    CHECK(!memcmp(a,b,sizeof(a)));
    CHECK(sd_audio_pc(x,pc,sizeof(pc))); CHECK(sd_audio_pc(y,pc,sizeof(pc)));
    sd_audio_mix(x,a,6000); for (unsigned i=0;i<6000;++i) sd_audio_mix(y,b+i*2,1);
    CHECK(!memcmp(a,b,sizeof(a))); sd_audio_destroy(x); sd_audio_destroy(y);
}
static void pcm_pc(void) {
    sd_audio *s=sd_audio_create(7042); int16_t out[16];
    CHECK(sd_audio_pcm(s,pcm,4,1,0,15)); sd_audio_mix(s,out,8);
    CHECK(out[0]==0 && out[2]==32512 && out[4]==-32768 && out[6]==0);
    for (unsigned i=0;i<8;++i) CHECK(out[i*2+1]==0);
    CHECK(sd_audio_pcm(s,pcm,4,2,0,0)); sd_audio_mix(s,out,4);
    CHECK(sd_audio_pcm(s,pcm,4,1,0,0)); /* priority released at final frame */
    sd_audio_destroy(s); s=sd_audio_create(44100);
    CHECK(sd_audio_pc(s,pc,sizeof(pc))); sd_audio_mix(s,out,8);
    CHECK(out[0]==8192 && out[1]==8192);
    sd_audio_stop_effect(s); sd_audio_mix(s,out,8); CHECK(out[0]==0);
    sd_audio_destroy(s);
}
int main(void) { timing(); contracts(); partition(); pcm_pc(); puts("PASS: tick timing, looping, malformed inputs, priorities, partitioning, PCM, PC"); return 0; }
