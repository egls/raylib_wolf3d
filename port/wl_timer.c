/*
 * wl_timer.c - 70 Hz TimeCount from a monotonic clock (see wl_timer.h).
 */
#define _GNU_SOURCE
#include <time.h>

#include "ID_HEADS.H"
#include "wl_timer.h"

#ifdef __EMSCRIPTEN__

/*
 * The browser has no timer thread: TimeCount catches up with the clock
 * whenever the game reads the clock, idles or waits (WL_TickClock,
 * WL_Idle, WL_SleepUntilTick, WL_WebWait), and the waits give the
 * browser its turn.  Ticks are never lost, as with the thread.
 */
#include <emscripten.h>

static double		t0;			// ms, emscripten_get_now()
static int			started;
static longword		done;		// ticks added to TimeCount so far

static void Catchup (double now)
{
	longword due = (longword)now;

	if (!started)
		return;
	while (done < due)
	{
		TimeCount++;
		done++;
	}
}

double WL_TickClock (void)
{
	double now = (emscripten_get_now() - t0) * WL_TICKRATE / 1000.0;

	Catchup(now);
	return now;
}

void WL_SleepUntilTick (double tick)
{
	double ms = tick * 1000.0 / WL_TICKRATE - (emscripten_get_now() - t0);

	// emscripten_sleep is a setTimeout, so it can wake early or late
	while (ms > 0)
	{
		emscripten_sleep((unsigned)ms ? (unsigned)ms : 1);
		ms = tick * 1000.0 / WL_TICKRATE - (emscripten_get_now() - t0);
	}
	WL_TickClock();
}

void WL_Idle (void)
{
	emscripten_sleep(0);
	WL_TickClock();
}

void WL_WebWait (void)
{
	WL_TickClock();
	WL_SleepUntilTick(done + 1);		// the next tick
}

void WL_TimerStart (void)
{
	if (started)
		return;
	t0 = emscripten_get_now();
	TimeCount = 0;
	done = 0;
	started = 1;
}

#else

#include <pthread.h>

static struct timespec	t0;
static int				started;
static pthread_t		thread;

static double ts_to_ticks (const struct timespec *ts)
{
	return ((ts->tv_sec - t0.tv_sec) + (ts->tv_nsec - t0.tv_nsec) * 1e-9) * WL_TICKRATE;
}

double WL_TickClock (void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ts_to_ticks(&now);
}

void WL_SleepUntilTick (double tick)
{
	struct timespec ts = t0;
	double secs = tick / WL_TICKRATE;

	ts.tv_sec += (time_t)secs;
	ts.tv_nsec += (long)((secs - (time_t)secs) * 1e9);
	if (ts.tv_nsec >= 1000000000L)
	{
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000L;
	}
	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR)
		;
}

void WL_Idle (void)
{
	struct timespec ts = {0, 200000};		/* 0.2 ms */

	nanosleep(&ts, NULL);
}

static void *TimerThread (void *arg)
{
	longword done = 0, due;

	(void)arg;
	for (;;)
	{
		due = (longword)WL_TickClock();
		while (done < due)
		{
			TimeCount++;			// _Atomic: the game thread reads and resets it concurrently
			done++;
		}
		WL_SleepUntilTick(done + 1);
	}
	return NULL;
}

void WL_TimerStart (void)
{
	if (started)
		return;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	TimeCount = 0;
	started = 1;
	if (pthread_create(&thread, NULL, TimerThread, NULL))
	{
		fprintf(stderr, "raylib_wolf3d: could not start the timer thread\n");
		exit(2);
	}
	pthread_detach(thread);
}

#endif
