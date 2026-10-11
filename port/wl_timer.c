/*
 * wl_timer.c - 70 Hz TimeCount from a monotonic clock (see wl_timer.h).
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Same fixed-width atomic type as ID_SD.H, without DOS header macros. */
extern _Atomic uint32_t TimeCount;
#include "wl_timer.h"

#ifdef _WIN32
static LARGE_INTEGER t0, frequency;
#else
static struct timespec	t0;
static pthread_t thread;
#endif
static int				started;


#ifdef _WIN32
double WL_TickClock (void)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)(now.QuadPart - t0.QuadPart) * WL_TICKRATE / frequency.QuadPart;
}

void WL_SleepUntilTick (double tick)
{
	HANDLE timer;
	LARGE_INTEGER due;
	double remaining;

	/* Request a high-resolution timer (Windows 10 1803+), with fallback. */
	timer = CreateWaitableTimerExA(NULL, NULL, 0x00000002, TIMER_ALL_ACCESS);
	if (!timer)
		timer = CreateWaitableTimerA(NULL, TRUE, NULL);
	while ((remaining = (tick - WL_TickClock()) / WL_TICKRATE) > 0)
	{
		if (timer)
		{
			due.QuadPart = -(LONGLONG)(remaining * 10000000.0) - 1;
			if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
			{
				WaitForSingleObject(timer, INFINITE);
				continue;
			}
		}
		Sleep((DWORD)(remaining * 1000.0) + 1);
	}
	if (timer)
		CloseHandle(timer);
}

void WL_Idle (void)
{
	WL_SleepUntilTick(WL_TickClock() + 0.0002 * WL_TICKRATE);
}
#else
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

#endif

#ifdef _WIN32
static unsigned __stdcall TimerThread (void *arg)
#else
static void *TimerThread (void *arg)
#endif
{
	uint32_t done = 0, due;

	(void)arg;
	for (;;)
	{
		due = (uint32_t)WL_TickClock();
		while (done < due)
		{
			TimeCount++;			// _Atomic: the game thread reads and resets it concurrently
			done++;
		}
		WL_SleepUntilTick(done + 1);
	}
	return 0;
}

void WL_TimerStart (void)
{
	if (started)
		return;
#ifdef _WIN32
	if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&t0))
	{
		fprintf(stderr, "raylib_wolf3d: performance counter unavailable\n");
		exit(2);
	}
#else
	clock_gettime(CLOCK_MONOTONIC, &t0);
#endif
	TimeCount = 0;
	started = 1;
#ifdef _WIN32
	uintptr_t thread = _beginthreadex(NULL, 0, TimerThread, NULL, 0, NULL);
	if (!thread)
#else
	if (pthread_create(&thread, NULL, TimerThread, NULL))
#endif
	{
		fprintf(stderr, "raylib_wolf3d: could not start the timer thread\n");
		exit(2);
	}
#ifdef _WIN32
	CloseHandle((HANDLE)thread);
#else
	pthread_detach(thread);
#endif
}
