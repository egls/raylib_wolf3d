/* Portable Sound Manager, based on Jason Blochowiak / id Software ID_SD.C.
 * Public policy follows the original Sound Manager;
 * synthesis/timing lives in audio/sd_audio.c (Wolf4SDL reference). */
#include "ID_HEADS.H"
#include "id_raylib.h"
#include "sd_audio.h"
#include "wl_data.h"
#include "wl_timer.h"

boolean SoundSourcePresent, AdLibPresent, SoundBlasterPresent, SBProPresent;
boolean NeedsDigitized, NeedsMusic, SoundPositioned, DigiPlaying;
SDMode SoundMode;
SMMode MusicMode;
SDSMode DigiMode;
_Atomic longword TimeCount;
int DigiMap[LASTSOUND];
static sd_audio *engine;
static boolean started, silent, nomusic, nextpos, musicactive;
static int leftpos, rightpos;
static soundnames soundnumber;
static word soundpriority, digipriority;
static byte *effectbuf, *pcmbuf, *musicbuf;
static size_t musicbytes;
word *DigiList;
word NumDigi;
static void (*userhook)(void);
static longword hooktick;

static void put16(byte *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void put32(byte *p, uint32_t v) { put16(p,v); put16(p+2,v>>16); }
static int option(const char *name) {
    int i;
    for (i=1;i<_argc;++i) {
        const char *p=_argv[i];
        while (*p=='-') ++p;
        if (!strcmp(p,name)) return 1;
    }
    return 0;
}
static void finish(void) {
    if (!engine) return;
    if (!sd_audio_effect_playing(engine) &&
        !(DigiMode==sds_PC && SoundMode==sdm_PC && sd_audio_pcm_playing(engine)))
        soundnumber = soundpriority = 0;
    if (!sd_audio_pcm_playing(engine)) {
        DigiPlaying = false;
        digipriority = 0;
        SoundPositioned = false;
    }
}
static void mix(short *out, unsigned frames) {
    sd_audio_mix(engine, out, frames);
    finish();
}
void SD_Poll(void) {
    RL_AudioPump();
    finish();
    /* Hooks run on the game thread; TimeCount has its own monotonic timer. */
    if (userhook && hooktick != TimeCount) { hooktick=TimeCount; userhook(); }
}
void SD_Startup(void) {
    unsigned i, pg;
    const byte *p;
    if (started) return;
    silent = option("nosound") || getenv("WOLF3D_NOSOUND") != NULL;
    nomusic = option("nomusic") || getenv("WOLF3D_NOMUSIC") != NULL;
    for (i=0;i<LASTSOUND;++i) DigiMap[i]=-1;
    SoundMode=sdm_Off; MusicMode=smm_Off; DigiMode=sds_Off;
    NeedsDigitized=NeedsMusic=SoundPositioned=nextpos=false;
    leftpos=rightpos=0;
    if (!silent) {
        engine=sd_audio_create(48000);
        if (!engine) Quit("Could not create audio engine");
        if (!RL_AudioOpen(mix)) {
            sd_audio_destroy(engine); engine=NULL; silent=true;
            fprintf(stderr,"Audio device unavailable; continuing silently\n");
        }
    }
    AdLibPresent=SoundBlasterPresent=SBProPresent=!silent;
    SoundSourcePresent=false;
    if (!silent) {
        /* Original SDL_SetupDigi: final VSWAP page is (relative page,length).
         * Copy it before fetching any other page: PM can evict cached pages. */
        p=PM_GetPage(ChunksInFile-1);
        pg=PMSoundStart;
        for (NumDigi=0;NumDigi<PMPageSize/4 && pg<ChunksInFile-1;++NumDigi)
            pg+=(WL_ReadLE16(p+NumDigi*4+2)+PMPageSize-1)/PMPageSize;
        DigiList=malloc(NumDigi*2*sizeof(*DigiList));
        if (!DigiList) Quit("Out of memory for digitized sound list");
        for (i=0;i<NumDigi*2;++i) DigiList[i]=WL_ReadLE16(p+i*2);
    }
    started=true;
}
void SD_StopDigitized(void) {
    if (engine) sd_audio_stop_pcm(engine);
    if (DigiMode==sds_PC && SoundMode==sdm_PC) soundnumber=soundpriority=0;
    DigiPlaying=false; digipriority=0; SoundPositioned=false;
    free(pcmbuf); pcmbuf=NULL;
}
void SD_StopSound(void) {
    SD_StopDigitized();
    if (engine) sd_audio_stop_effect(engine);
    free(effectbuf); effectbuf=NULL;
    soundnumber=soundpriority=0;
}
void SD_MusicOff(void) {
    if (engine) sd_audio_music_pause(engine,1);
    musicactive=false;
}
void SD_Shutdown(void) {
    if (!started) return;
    RL_AudioClose(); /* stop callback before freeing engine/buffers */
    SD_StopSound(); SD_MusicOff();
    if (engine) sd_audio_destroy(engine);
    engine=NULL;
    free(musicbuf); musicbuf=NULL; musicbytes=0;
    free(DigiList); DigiList=NULL; NumDigi=0;
    started=false;
}
boolean SD_SetSoundMode(SDMode mode) {
    SD_StopSound();
    if (silent) mode=sdm_Off;
    if (mode<sdm_Off || mode>sdm_AdLib) return false;
    SoundMode=mode; NeedsDigitized=false;
    return true;
}
boolean SD_SetMusicMode(SMMode mode) {
    SD_MusicOff();
    if (silent || nomusic) mode=smm_Off;
    if (mode!=smm_Off && mode!=smm_AdLib) return false;
    MusicMode=mode; NeedsMusic=mode==smm_AdLib;
    return true;
}
void SD_SetDigiDevice(SDSMode mode) {
    if (silent) mode=sds_Off;
    if (mode==sds_SoundSource || mode<sds_Off || mode>sds_SoundBlaster) return;
    if (mode!=DigiMode) { SD_StopDigitized(); DigiMode=mode; }
}
void SD_Default(boolean gotit,SDMode sd,SMMode sm) {
    SD_SetSoundMode(gotit ? sd : sdm_AdLib);
    SD_SetMusicMode(gotit ? sm : smm_AdLib);
}
static void validpos(int l,int r) {
    if (l<0 || l>15 || r<0 || r>15 || (l==15 && r==15))
        Quit("SD_SetPosition: Illegal position");
}
void SD_SetPosition(int l,int r) {
    validpos(l,r);
    if (engine && DigiMode==sds_SoundBlaster) sd_audio_pcm_position(engine,l,r);
}
void SD_PositionSound(int l,int r) { leftpos=l; rightpos=r; nextpos=true; }
void SD_PlayDigitized(word which,int l,int r) {
    unsigned page, length, offset=0;
    if (DigiMode==sds_Off || !engine) return;
    validpos(l,r);
    SD_StopDigitized();
    if (which>=NumDigi) Quit("SD_PlayDigitized: bad sound number");
    page=DigiList[which*2]; length=DigiList[which*2+1];
    if (!length || PMSoundStart+page+(length+PMPageSize-1)/PMPageSize>ChunksInFile-1)
        Quit("SD_PlayDigitized: invalid sound pages");
    pcmbuf=malloc(length);
    if (!pcmbuf) Quit("Out of memory for digitized sound");
    while (offset<length) {
        unsigned n=length-offset;
        if (n>PMPageSize) n=PMPageSize;
        memcpy(pcmbuf+offset,PM_GetSoundPage(page++),n);
        offset+=n;
    }
    /* As in the reference, SoundSource/PC digi have no stereo attenuation. */
    DigiPlaying=DigiMode==sds_PC ? sd_audio_pc_pcm(engine,pcmbuf,length) :
        sd_audio_pcm(engine,pcmbuf,length,0,
                             DigiMode==sds_SoundBlaster ? l : 0,
                             DigiMode==sds_SoundBlaster ? r : 0);
}
boolean SD_PlaySound(soundnames sound) {
    SoundCommon *s;
    unsigned header, length;
    int l=leftpos,r=rightpos;
    boolean positioned=nextpos;
    leftpos=rightpos=0; nextpos=false;
    SD_Poll();
    if ((int)sound==-1 || silent) return false;
    if ((unsigned)sound>=LASTSOUND) Quit("SD_PlaySound: bad sound number");
    if (SoundMode==sdm_Off && (DigiMode==sds_Off || DigiMap[sound]==-1)) return false;
    /* Off+digi still needs AdLib priorities (Wolf4SDL uses this table too).
     * Cache it on demand because CA_LoadAllSounds skips the off mode. */
    unsigned chunk=(SoundMode==sdm_PC ? STARTPCSOUNDS : STARTADLIBSOUNDS)+sound;
    if (SoundMode==sdm_Off) CA_CacheAudioChunk(chunk);
    s=(SoundCommon *)audiosegs[chunk];
    if (!s) Quit("SD_PlaySound() - Uncached sound");
    if (DigiMode!=sds_Off && DigiMap[sound]!=-1) {
        if (DigiMode==sds_PC && SoundMode==sdm_PC) {
            if (s->priority<soundpriority) return false;
            sd_audio_stop_effect(engine);
            SD_PlayDigitized(DigiMap[sound],l,r);
            soundnumber=sound; soundpriority=s->priority;
        } else {
            if (s->priority<digipriority) return false;
            SD_PlayDigitized(DigiMap[sound],l,r);
            digipriority=s->priority;
        }
        SoundPositioned=positioned;
        return true;
    }
    if (!s->length) Quit("SD_PlaySound() - Zero length sound");
    if (s->priority<soundpriority) return false;
    if (DigiMode==sds_PC && SoundMode==sdm_PC) SD_StopDigitized();
    header=SoundMode==sdm_PC ? 6 : 23; length=s->length;
    /* Cache manager expands DOS structs to host layout; serialize explicitly
     * for the engine, and own the copy so cache purging cannot invalidate it. */
    sd_audio_stop_effect(engine);
    free(effectbuf); effectbuf=malloc(header+length);
    if (!effectbuf) Quit("Out of memory for sound effect");
    put32(effectbuf,length); put16(effectbuf+4,s->priority);
    if (SoundMode==sdm_PC) {
        memcpy(effectbuf+6,((PCSound *)s)->data,length);
        sd_audio_pc(engine,effectbuf,header+length);
    } else {
        AdLibSound *a=(AdLibSound *)s;
        memcpy(effectbuf+6,&a->inst,16); effectbuf[22]=a->block;
        memcpy(effectbuf+23,a->data,length);
        if (!sd_audio_adlib(engine,effectbuf,header+length)) Quit("SD_PlaySound: Bad instrument");
    }
    soundnumber=sound; soundpriority=s->priority;
    return false; /* Original return value means digitized, not accepted. */
}
word SD_SoundPlaying(void) {
    SD_Poll();
    return engine && (sd_audio_effect_playing(engine) ||
        (DigiMode==sds_PC && SoundMode==sdm_PC && DigiPlaying)) ? soundnumber : 0;
}
void SD_WaitSoundDone(void) { while (SD_SoundPlaying()) { RL_PumpEvents(); WL_Idle(); } }
void SD_StartMusic(MusicGroup *music) {
    unsigned i;
    SD_MusicOff();
    if (engine) sd_audio_stop_music(engine);
    free(musicbuf); musicbuf=NULL; musicbytes=0;
    if (MusicMode!=smm_AdLib || !engine) return;
    musicbytes=music->length+2;
    musicbuf=malloc(musicbytes);
    if (!musicbuf) Quit("Out of memory for music");
    put16(musicbuf,music->length);
    for (i=0;i<music->length/2;++i) put16(musicbuf+2+i*2,music->values[i]);
    musicactive=sd_audio_music(engine,musicbuf,musicbytes,1);
}
void SD_MusicOn(void) {
    if (engine && MusicMode==smm_AdLib && musicbuf)
        { sd_audio_music_pause(engine,0); musicactive=musicbytes>2; }
}
void SD_FadeOutMusic(void) { SD_MusicOff(); } /* original immediate stop */
boolean SD_MusicPlaying(void) { return musicactive; }
void SD_SetUserHook(void (*hook)(void)) { userhook=hook; hooktick=TimeCount; }

void alOut(byte reg,byte value) { if (engine) sd_audio_register(engine,reg,value); }
