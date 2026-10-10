/* SPDX-License-Identifier: GPL-2.0-or-later
 * Portable sequencing follows Jason Blochowiak / id Software's ID_SD.C and
 * ID_SD_A.ASM. PC synthesis and PCM interpolation follow Wolf4SDL contributors'
 * id_sd.c (PC mixer adapted there from Chocolate Doom). See README.md.
 */
#include "sd_audio.h"
#include "vendor/opl3.h"
#include <stdlib.h>
#ifdef SD_AUDIO_SPEAR
#define MUSIC_HZ 560u
#else
#define MUSIC_HZ 700u
#endif
struct sd_audio {
    opl3_chip opl;
    unsigned rate, fx_count, priority, pcm_priority, left, right;
    uint64_t frame, music_clock, pc_clock, music_time, next_event, pcm_pos, pc_phase;
    const uint8_t *music, *adlib, *pc, *pcm;
    size_t music_len, music_pos, adlib_len, pc_len, pcm_len, pcm_frames;
    unsigned block, pc_value, pc_frequency;
    int loop;
    sd_register_hook hook;
    void *user;
};
static unsigned le16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t)le16(p+2) << 16; }
static void reg(sd_audio *s, uint8_t r, uint8_t v) {
    if (s->hook) s->hook(s->user, s->frame, r, v);
    /* Unbuffered writes preserve the same event/sample boundary as Wolf4SDL's
     * YM3812Write; buffered Nuked writes add an unrelated write queue delay. */
    OPL3_WriteReg(&s->opl, r, v);
}
unsigned sd_audio_music_hz(void) { return MUSIC_HZ; }
sd_audio *sd_audio_create(unsigned rate) {
    sd_audio *s;
    if (rate < MUSIC_HZ || rate > 384000) return NULL;
    s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->rate = rate; s->fx_count = MUSIC_HZ/140;
    s->music_clock = rate; s->pc_clock = rate;
    OPL3_Reset(&s->opl, rate);
    reg(s, 1, 0x20); /* waveform select enabled; OPL3 mode remains disabled */
    return s;
}
void sd_audio_destroy(sd_audio *s) { free(s); }
void sd_audio_set_register_hook(sd_audio *s, sd_register_hook hook, void *user) {
    s->hook = hook; s->user = user;
}
void sd_audio_stop_music(sd_audio *s) {
    unsigned i;
    s->music = NULL;
    reg(s, 0xbd, 0);
    for (i=1; i<9; ++i) reg(s, (uint8_t)(0xb0+i), 0);
}
int sd_audio_music(sd_audio *s, const uint8_t *p, size_t n, int loop) {
    if (!p || n < 4) return 0;
    if (le16(p)) {
        size_t len = le16(p);
        if (len > n-2 || len % 4) return 0;
        p += 2; n = len;
    }
    if (!n || n % 4) return 0;
    sd_audio_stop_music(s);
    s->music=p; s->music_len=n; s->music_pos=0;
    s->music_time=0; s->next_event=0; s->loop=loop;
    return 1;
}
void sd_audio_stop_effect(sd_audio *s) {
    s->adlib=NULL; s->pc=NULL; s->pc_frequency=0; s->priority=0;
    reg(s, 0xb0, 0);
}
static int effect_valid(sd_audio *s, const uint8_t *p, size_t n, size_t header) {
    return p && n >= header && le32(p) && le32(p) <= n-header && le16(p+4) >= s->priority;
}
int sd_audio_adlib(sd_audio *s, const uint8_t *p, size_t n) {
    static const uint8_t regs[10] = {0x20,0x40,0x60,0x80,0xe0,0x23,0x43,0x63,0x83,0xe3};
    static const uint8_t offsets[10] = {0,2,4,6,8,1,3,5,7,9};
    unsigned i;
    if (!effect_valid(s,p,n,23) || !(p[12] | p[13])) return 0;
    sd_audio_stop_effect(s);
    for (i=0;i<10;++i) reg(s,regs[i],p[6+offsets[i]]);
    reg(s,0xc0,0); /* nConn intentionally ignored, as in the original */
    s->priority=le16(p+4); s->block=((p[22]&7)<<2)|0x20;
    s->adlib=p+23; s->adlib_len=le32(p);
    return 1;
}
int sd_audio_pc(sd_audio *s, const uint8_t *p, size_t n) {
    if (!effect_valid(s,p,n,6)) return 0;
    sd_audio_stop_effect(s);
    s->priority=le16(p+4); s->pc=p+6; s->pc_len=le32(p);
    s->pc_clock=s->rate; s->pc_value=256;
    return 1;
}
void sd_audio_stop_pcm(sd_audio *s) { s->pcm=NULL; s->pcm_priority=0; }
int sd_audio_pcm(sd_audio *s, const uint8_t *p, size_t n, unsigned priority, unsigned l, unsigned r) {
    if (!p || !n || l>15 || r>15 || (l==15 && r==15) || priority<s->pcm_priority) return 0;
    if (n > 64000000) return 0;
    size_t dest=(size_t)((float)n*(float)s->rate/7042.0f);
    if (!dest) return 0;
    s->pcm=p; s->pcm_len=n; s->pcm_frames=dest; s->pcm_pos=0; s->pcm_priority=priority; s->left=l; s->right=r;
    return 1;
}
static void music_tick(sd_audio *s) {
    if (!--s->fx_count) {
        s->fx_count=MUSIC_HZ/140;
        if (s->adlib) {
            uint8_t v=*s->adlib++;
            if (v) { reg(s,0xa0,v); reg(s,0xb0,(uint8_t)s->block); }
            else reg(s,0xb0,0);
            if (!--s->adlib_len) { s->adlib=NULL; s->priority=0; reg(s,0xb0,0); }
        }
    }
    if (!s->music) return;
    while (s->music_pos<s->music_len && s->next_event<=s->music_time) {
        const uint8_t *p=s->music+s->music_pos;
        s->next_event=s->music_time+le16(p+2);
        reg(s,p[0],p[1]); s->music_pos+=4;
    }
    ++s->music_time;
    /* Original ISR rewinds immediately after the last write, ignoring its
     * trailing delay. At most one pass per tick, even for all-zero delays. */
    if (s->music_pos==s->music_len) {
        if (s->loop) { s->music_pos=0; s->music_time=0; s->next_event=0; }
        else s->music=NULL; /* allow the final notes to ring */
    }
}
static int16_t clip(int v) { return (int16_t)(v>32767 ? 32767 : v< -32768 ? -32768 : v); }
void sd_audio_mix(sd_audio *s, int16_t *out, size_t frames) {
    size_t i;
    for (i=0;i<frames;++i) {
        int16_t fm[2]; int pc=0, pcm=0;
        if (s->music_clock>=s->rate) { s->music_clock-=s->rate; music_tick(s); }
        if (s->pc_clock>=s->rate) {
            s->pc_clock-=s->rate; s->pc_phase=0;
            if (s->pc) {
                unsigned v=*s->pc++;
                if (v!=s->pc_value) { s->pc_value=v; s->pc_frequency=v ? 1193180/(v*60) : 0; }
                if (!--s->pc_len) { s->pc=NULL; s->priority=0; }
            } else s->pc_frequency=0;
        }
        OPL3_GenerateResampled(&s->opl,fm);
        if (s->pc_frequency) pc=((s->pc_phase*s->pc_frequency*2/s->rate)&1) ? -8192 : 8192;
        if (s->pcm) {
            if (s->pcm_pos>=s->pcm_frames) sd_audio_stop_pcm(s);
            else {
                /* Wolf4SDL GetSample / SD_PrepareSound: three-point quadratic
                 * interpolation, zero outside the source, clipped to int16. */
                float at=(float)s->pcm_len*(float)s->pcm_pos/(float)s->pcm_frames;
                size_t pos=(size_t)at;
                if (pos>=s->pcm_len) { pos=s->pcm_len-1; at=(float)pos; }
                float f=at-(float)pos;
                float a=pos ? (float)((int)s->pcm[pos-1]-128) : 0;
                float b=(float)((int)s->pcm[pos]-128);
                float c=pos+1<s->pcm_len ? (float)((int)s->pcm[pos+1]-128) : 0;
                float val=a*f*(f-1)/2-b*(f*f-1)+c*(f+1)*f/2;
                pcm=clip((int)(val*256));
                ++s->pcm_pos;
                if (s->pcm_pos==s->pcm_frames) s->pcm_priority=0;
            }
        }
        out[i*2]=clip(fm[0]+pc+pcm*(int)(15-s->left)/15);
        out[i*2+1]=clip(fm[1]+pc+pcm*(int)(15-s->right)/15);
        ++s->frame; ++s->pc_phase;
        s->music_clock+=MUSIC_HZ; s->pc_clock+=140;
    }
}
