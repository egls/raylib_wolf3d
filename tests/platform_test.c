/* Headless startup/timer/path integration test; assets are empty placeholders. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ID_HEADS.H"
#include "wl_timer.h"
#include "wl_persist.h"

_Atomic uint32_t TimeCount;
struct statestruct *const wl_states[] = {NULL};
const int wl_numstates = 0;
void WL_PlatformStartup(int argc, char **argv);
void Quit(char *msg) { fprintf(stderr, "%s\n", msg); exit(2); }
int RL_OpenWindow(int w, int h, const char *title) {
    (void)w; (void)h; (void)title; return 1;
}
void RL_CloseWindow(void) {}
void RL_Hold(int ms) { (void)ms; }
void WL_VideoTest(const char *what) { (void)what; }

/* This target tests startup paths with empty placeholder assets. The actual
 * loader is exercised by core_data/synthetic_wl6; fail if it is entered here. */
maptype *mapheaderseg[NUMMAPS];
int32_t *grstarts;
void MM_Startup(void) { abort(); }
void MM_Shutdown(void) { abort(); }
void CA_Startup(void) { abort(); }
void CA_Shutdown(void) { abort(); }
void PM_Startup(void) { abort(); }
void PM_Shutdown(void) { abort(); }
void CA_CacheMap(int n) { (void)n; abort(); }
void CA_CacheGrChunk(int n) { (void)n; abort(); }
void CA_CacheAudioChunk(int n) { (void)n; abort(); }

int main(int argc, char **argv) {
    double target, before;
    FILE *file;
    WL_PlatformStartup(argc, argv);
    target = WL_TickClock() + 7;
    WL_SleepUntilTick(target);
    assert(WL_TickClock() >= target);
    /* Allow the timer thread to be scheduled after the deadline. */
    target = WL_TickClock() + 70;
    while (TimeCount < 7 && WL_TickClock() < target) WL_Idle();
    assert(TimeCount >= 7);
    TimeCount = 0;
    target = WL_TickClock() + 70;
    while (TimeCount < 2 && WL_TickClock() < target) WL_Idle();
    assert(TimeCount >= 2);
    before = WL_TickClock();
    WL_TimerStart();
    assert(WL_TickClock() >= before && TimeCount >= 2);
    before = WL_TickClock();
    WL_Idle();
    assert(WL_TickClock() > before);
    file = fopen(WL_UserFile("CONFIG.WL1"), "wb");
    assert(file);
    assert(fwrite("test", 1, 4, file) == 4);
    assert(fclose(file) == 0);
    puts("PASS: startup, deadline, ticking/reset, idempotence, writable config");
    return 0;
}
