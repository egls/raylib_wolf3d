/* Exercise the browser producer policy against a scripted clock/device. */
#define RL_POLICY_ONLY
#include "audio_ring_test.c"

static unsigned generation, policy_queued, resets;
void WebAudioInit(void) { }
unsigned WebAudioGeneration(void) { return generation; }
void WebAudioStats(unsigned q, unsigned u, unsigned r, double g) {
    (void)u; (void)g; policy_queued=q; resets=r;
}
void WebAudioClose(void) { }
int main(void) {
    short out[4096*2];
    generation=0; mixed=0; clock_ns=0;
    assert(RL_AudioOpen(generate));
    assert(mixed==0 && policy_queued==0);
    clock_ns=10000000000LL; RL_AudioPump();
    assert(mixed==0 && policy_queued==0); /* autoplay never advances synthesis */
    generation=1; RL_AudioPump(); assert(policy_queued==1536 && mixed==1536);
    callback(out,512); assert(out[0]==1);
    unsigned previous=mixed;
    RL_AudioPump(); callback(out,4096); RL_AudioPump();
    assert(mixed<=2048); /* fixed prefetch allowance, racing sink bounded */
    generation=0;
    callback(out,512);
    for (unsigned i=0;i<1024;++i) assert(out[i]==0); /* silence before next pump */
    RL_AudioPump(); previous=mixed;
    clock_ns+=60000000000LL; RL_AudioPump(); assert(mixed==previous);
    generation=2; RL_AudioPump(); callback(out,512);
    assert(out[0]==(int)(previous%30000)+1); /* no stale pre-hide ring */
    unsigned oldresets=resets;
    previous=mixed; clock_ns+=1000000000LL; RL_AudioPump();
    assert(resets==oldresets+1 && mixed-previous==1536 && policy_queued==1536);
    /* Irregular short frames may underrun, but cannot burst policy_queued catch-up. */
    for (unsigned i=0;i<100;++i) {
        callback(out,512); clock_ns+=(i%3+1)*7000000LL; RL_AudioPump();
        assert(policy_queued<=1536);
    }
    RL_AudioClose();
    puts("PASS: autoplay freeze, callback silence, resume discard, long-gap resync, bounded jitter and clock cap");
    return 0;
}
