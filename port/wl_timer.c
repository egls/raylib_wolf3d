/*
 * wl_timer.c - 70 Hz TimeCount from a monotonic clock (see wl_timer.h).
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>

#include "ID_HEADS.H"
#include "wl_timer.h"

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
			__atomic_add_fetch(&TimeCount, 1, __ATOMIC_RELAXED);
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
