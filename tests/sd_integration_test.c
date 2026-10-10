/* Exercises the real cache/page manager -> ID_SD -> engine contract.
 * Device build also supplies wall-clock-paced AudioStream evidence without
 * depending on unfinished game-loop adapters. Uses only external WL1 data. */
#include "WL_DEF.H"
#include "id_raylib.h"
#include "sd_audio.h"
#include "wl_data.h"
#include <assert.h>
#include <time.h>
pictabletype *pictable;
void VL_Plot(int x,int y,int c) { (void)x; (void)y; (void)c; }
int VW_MarkUpdateBlock(int x,int y,int xx,int yy) { (void)x; (void)y; (void)xx; (void)yy; return 0; }
int _argc;
char **_argv;
static void (*mixer)(short *,unsigned);
static short output[9600*2];
#ifndef SD_DEVICE
int RL_AudioOpen(void (*f)(short *,unsigned)) { mixer=f; return 1; }
void RL_AudioClose(void) { mixer=NULL; }
void RL_AudioPump(void) { }
void RL_PumpEvents(void) { }
#endif
void WL_Idle(void) { struct timespec t={0,1000000}; nanosleep(&t,NULL); }
void Quit(char *msg) { fprintf(stderr,"%s\n",msg); exit(1); }
long filelength(int fd) { struct stat st; return fstat(fd,&st) ? -1 : st.st_size; }
static void pump(double seconds) {
#ifdef SD_DEVICE
    struct timespec start,now,t={0,1000000};
    clock_gettime(CLOCK_MONOTONIC,&start);
    do {
        SD_Poll(); nanosleep(&t,NULL);
        clock_gettime(CLOCK_MONOTONIC,&now);
    } while ((now.tv_sec-start.tv_sec)+(now.tv_nsec-start.tv_nsec)*1e-9<seconds);
#else
    while (seconds>0) {
        unsigned n=seconds*48000;
        if (n>9600) n=9600;
        if (!n) break;
        mixer(output,n); seconds-=(double)n/48000;
    }
#endif
}
#ifndef SD_DEVICE
static void compare_effect(unsigned sound, int pc) {
    FILE *h=fopen("AUDIOHED.WL1","rb"), *f=fopen("AUDIOT.WL1","rb");
    byte offsets[8]; unsigned a,b;
    unsigned chunk=sound+(pc ? STARTPCSOUNDS : STARTADLIBSOUNDS);
    assert(h && f); assert(!fseek(h,chunk*4,SEEK_SET));
    assert(fread(offsets,1,8,h)==8); a=WL_ReadLE32(offsets); b=WL_ReadLE32(offsets+4);
    byte *raw=malloc(b-a); assert(raw); assert(!fseek(f,a,SEEK_SET));
    assert(fread(raw,1,b-a,f)==b-a); fclose(h); fclose(f);
    sd_audio *expected=sd_audio_create(48000);
    short golden[9600*2];
    SD_SetSoundMode(pc ? sdm_PC : sdm_AdLib); CA_LoadAllSounds();
    SD_PlaySound(sound);
    assert(pc ? sd_audio_pc(expected,raw,b-a) : sd_audio_adlib(expected,raw,b-a));
    mixer(output,9600); sd_audio_mix(expected,golden,9600);
    assert(!memcmp(output,golden,sizeof(output)));
    sd_audio_destroy(expected); free(raw); SD_StopSound();
    /* Reset engine OPL state so the next comparison begins at reset. */
    SD_Shutdown(); SD_Startup();
}
#endif
int main(int argc,char **argv) {
    char *gameargv[]={"sd-integration",NULL};
    if (argc<2 || chdir(argv[1]) || access("MAPHEAD.WL1",R_OK)) return 77;
    _argc=1; _argv=gameargv;
    strcpy(extension,"WL1"); MM_Startup(); PM_Startup(); CA_Startup(); SD_Startup();
#ifdef SD_DEVICE
    SD_Default(false,sdm_AdLib,smm_AdLib); CA_LoadAllSounds();
    CA_CacheAudioChunk(STARTMUSIC+NAZI_NOR_MUS);
    SD_StartMusic((MusicGroup *)audiosegs[STARTMUSIC+NAZI_NOR_MUS]);
    puts("0..12 seconds: title music (NAZI_NOR_MUS)"); fflush(stdout); pump(12);
    SD_MusicOff();
    puts("12..14: AdLib SHOOTSND"); fflush(stdout); SD_PlaySound(SHOOTSND); pump(2);
    SD_SetSoundMode(sdm_PC); CA_LoadAllSounds();
    puts("14..16: PC speaker SHOOTSND"); fflush(stdout); SD_PlaySound(SHOOTSND); pump(2);
    SD_SetDigiDevice(sds_SoundBlaster);
    puts("16..20: VSWAP digitized HALT (entry 0), left channel"); fflush(stdout);
    SD_PlayDigitized(0,0,15); pump(2);
    puts("18..20: VSWAP digitized entry 1, right channel"); fflush(stdout);
    SD_PlayDigitized(1,15,0); pump(2);
#else
    compare_effect(24,0); compare_effect(24,1);
    SD_SetSoundMode(sdm_AdLib); CA_LoadAllSounds();
    SD_PlaySound(HALTSND); assert(SD_SoundPlaying()==HALTSND);
    SD_PlaySound(SHOOTSND); assert(SD_SoundPlaying()==HALTSND); /* lower priority */
    SD_StopSound();
    SD_PlaySound(SHOOTSND); assert(SD_SoundPlaying()==SHOOTSND);
    pump(2); assert(!SD_SoundPlaying());
    SD_Shutdown(); SD_Startup();
    SD_SetDigiDevice(sds_SoundBlaster); SD_PlayDigitized(0,0,15);
    mixer(output,9600);
    int nonzero=0; for (unsigned i=0;i<9600;++i) { assert(output[i*2+1]==0); nonzero |= output[i*2]!=0; }
    assert(nonzero); pump(3); assert(!DigiPlaying);
    SD_SetSoundMode(sdm_AdLib); CA_LoadAllSounds();
    DigiMap[HALTSND]=0; DigiMap[SHOOTSND]=1;
    assert(SD_PlaySound(HALTSND));
    assert(!SD_PlaySound(SHOOTSND)); /* independent digi priority */
    SD_StopDigitized(); DigiMap[HALTSND]=-1;
    SD_SetSoundMode(sdm_Off); DigiMap[SHOOTSND]=0;
    assert(SD_PlaySound(SHOOTSND)); assert(DigiPlaying); pump(3); assert(!DigiPlaying);
    SD_SetDigiDevice(sds_Off); assert(!SD_PlaySound(SHOOTSND));
    SD_SetSoundMode(sdm_PC); CA_LoadAllSounds(); SD_SetDigiDevice(sds_PC);
    DigiMap[HALTSND]=0;
    assert(SD_PlaySound(HALTSND)); mixer(output,100);
    assert(SD_SoundPlaying()==HALTSND);
    assert(!SD_PlaySound(SHOOTSND)); assert(SD_SoundPlaying()==HALTSND);
    pump(3); assert(!SD_SoundPlaying());
    DigiMap[HALTSND]=DigiMap[SHOOTSND]=-1;
    SD_PlaySound(SHOOTSND); mixer(output,100); assert(SD_SoundPlaying()==SHOOTSND);
    SD_StopSound(); SD_SetDigiDevice(sds_Off);
    SD_SetMusicMode(smm_AdLib); CA_CacheAudioChunk(STARTMUSIC+NAZI_NOR_MUS);
    SD_StartMusic((MusicGroup *)audiosegs[STARTMUSIC+NAZI_NOR_MUS]);
    assert(SD_MusicPlaying()); pump(.2); SD_MusicOff(); assert(!SD_MusicPlaying());
    SD_MusicOn(); assert(SD_MusicPlaying()); pump(.2);
    SD_SetMusicMode(smm_Off); assert(!SD_MusicPlaying());
    SD_Shutdown(); setenv("WOLF3D_NOMUSIC","1",1); SD_Startup();
    SD_Default(false,sdm_AdLib,smm_AdLib);
    assert(SoundMode==sdm_AdLib && MusicMode==smm_Off);
    SD_Shutdown(); unsetenv("WOLF3D_NOMUSIC");
    setenv("WOLF3D_NOSOUND","1",1); SD_Startup();
    SD_Default(false,sdm_AdLib,smm_AdLib); SD_SetDigiDevice(sds_SoundBlaster);
    assert(SoundMode==sdm_Off && MusicMode==smm_Off && DigiMode==sds_Off);
    assert(!SD_PlaySound(SHOOTSND)); unsetenv("WOLF3D_NOSOUND");
    puts("PASS: cache layout, completion, stereo, digi with effects off, music modes, silent startup");
#endif
    SD_Shutdown(); CA_Shutdown(); PM_Shutdown(); MM_Shutdown();
    return 0;
}
