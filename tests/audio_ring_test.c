/* Real backend callback with a scripted device; no OS audio required. */
#define main keyboard_test_main
#include "rl_keys_test.c"
#undef main
#include <assert.h>
#include <time.h>
static AudioCallback callback;
static unsigned mixed;
static long clock_ns;
int clock_gettime(clockid_t id,struct timespec *out) {
    assert(id==CLOCK_MONOTONIC);
    out->tv_sec=clock_ns/1000000000L; out->tv_nsec=clock_ns%1000000000L;
    return 0;
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
    puts("PASS: ring ordering/wrap, silence on underrun, wall-clock throttle, teardown");
    return 0;
}
