/*
 * wl_timer.h - the 70 Hz tick that the PIT interrupt used to deliver.
 *
 * TimeCount (defined in ID_SD.C) is advanced by a small thread from a
 * monotonic clock, so busy loops such as "while (TimeCount < n);" and the
 * "TimeCount = 0" resets in the game keep working unchanged.  Ticks are never
 * lost: the thread adds however many ticks elapsed, so a stalled process
 * catches up, like a DOS machine that re-enables interrupts.  The browser
 * build has no thread; there every clock read catches TimeCount up.
 */
#ifndef WL_TIMER_H
#define WL_TIMER_H

#define WL_TICKRATE		70		/* VGA vertical refresh, the id timebase */

void	WL_TimerStart (void);			/* resets TimeCount to 0 and starts ticking */
double	WL_TickClock (void);			/* monotonic time in (fractional) ticks since start */
void	WL_SleepUntilTick (double tick);	/* returns at or after that clock value */
void	WL_Idle (void);					/* short nap for spin loops */

#ifdef __EMSCRIPTEN__
/*
 * In the browser TimeCount only moves when the clock is read, and the page
 * only runs while the game yields.  Loops that spin on TimeCount alone call
 * this: it sleeps (yields) until the next tick.
 */
void	WL_WebWait (void);
#endif

#endif
