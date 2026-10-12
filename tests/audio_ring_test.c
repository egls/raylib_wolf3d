/* Real backend callback with a scripted device; no OS audio required. */
#define main keyboard_test_main
#include "rl_keys_test.c"
#undef main
#include <assert.h>
#include <time.h>
#include <stdint.h>
static AudioCallback callback;
static unsigned mixed;
static int64_t clock_ns;
/* The backend's clock (built with RL_EXTERNAL_CLOCK): scripted time. */
void RL_MonotonicNow(struct timespec *out) {
    out->tv_sec=(time_t)(clock_ns/1000000000L); out->tv_nsec=(long)(clock_ns%1000000000L);
}
void InitAudioDevice(void) { }
void CloseAudioDevice(void) { }
bool IsAudioDeviceReady(void) { return true; }
void SetAudioStreamBufferSizeDefault(int n) { assert(n==512); }
AudioStream LoadAudioStream(unsigned r,unsigned bits,unsigned channels) {
    assert(r==48000 && bits==16 && channels==2); return (AudioStream){0};
}
bool IsAudioStreamValid(AudioStream s) { (void)s; return true; }
void SetAudioStreamCallback(AudioStream s,AudioCallback cb) { (void)s; callback=cb; }
void PlayAudioStream(AudioStream s) { (void)s; }
void StopAudioStream(AudioStream s) { (void)s; }
void UnloadAudioStream(AudioStream s) { (void)s; callback=NULL; }
static void generate(short *out,unsigned n) {
    for (unsigned i=0;i<n;++i) { out[i*2]=(mixed%30000)+1; out[i*2+1]=-out[i*2]; mixed++; }
}
#ifndef RL_POLICY_ONLY
static void test_drift(int ppm) {
    short out[512*2];
    uint64_t consumed=0;
    unsigned expected=0;
    clock_ns=0; mixed=0;
    assert(RL_AudioOpen(generate));
    /* Independent device clock: 512-frame callbacks with 1024 frames of
     * pull-ahead, before each 70 Hz game pump, for one simulated hour.
     * Integer scheduling avoids accumulated floating-point clock error. */
    for (unsigned tick=0;tick<=3600u*70u;++tick) {
        uint64_t target=(uint64_t)tick*48000u*(1000000+ppm)/(70u*1000000u)+1024u;
        clock_ns=(int64_t)((uint64_t)tick*1000000000u/70u);
        while (consumed+512<=target) {
            callback(out,512);
            for (unsigned i=0;i<512;++i) {
                int sample=(expected++%30000)+1;
                assert(out[i*2]==sample && out[i*2+1]==-sample);
            }
            consumed+=512;
        }
        RL_AudioPump();
    }
    RL_AudioClose();
    printf("PASS: one-hour DAC drift %+d ppm, no silent or reordered frames\n",ppm);
}
int main(void) {
    short out[8192*2];
    assert(RL_AudioOpen(generate)); assert(callback); assert(mixed==3840);
    callback(out,8192);
    for (unsigned i=0;i<3840;++i) assert(out[i*2]==i+1 && out[i*2+1]==-(int)(i+1));
    for (unsigned i=3840;i<8192;++i) assert(out[i*2]==0 && out[i*2+1]==0);
    unsigned previous=mixed;
    RL_AudioPump();
    /* Null sink cannot make the producer synthesize another 80 ms instantly. */
    assert(mixed==previous);
    callback(out,8192);
    clock_ns+=90000000;
    previous=mixed; RL_AudioPump(); assert(mixed-previous==3840);
    callback(out,3840);
    for (unsigned i=0;i<3840;++i) assert(out[i*2]==(int)((previous+i)%30000)+1);
    /* Exercise wraparound across many callback/producer chunks. */
    for (unsigned pass=0;pass<4;++pass) {
        clock_ns+=90000000; previous=mixed; RL_AudioPump(); callback(out,3840);
        for (unsigned i=0;i<3840;++i) assert(out[i*2]==(int)((previous+i)%30000)+1);
    }
    RL_AudioClose(); RL_AudioClose(); assert(!callback);
    test_drift(-50);
    test_drift(100);
    test_drift(200);
    puts("PASS: ring ordering/wrap, silence on underrun, wall-clock throttle, teardown");
    return 0;
}

#endif /* RL_POLICY_ONLY */
