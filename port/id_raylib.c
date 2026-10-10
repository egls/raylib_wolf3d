/*
 * id_raylib.c - the raylib backend.  The ONLY translation unit that includes
 * raylib.h (see id_raylib.h).
 */
#include "raylib.h"

#include "id_raylib.h"

static int window_open;

int RL_OpenWindow (int width, int height, const char *title)
{
	if (window_open)
		return 1;

	SetTraceLogLevel(LOG_WARNING);
	SetTargetFPS(70);			// VGA mode 13h refresh rate
	InitWindow(width, height, title);
	if (!IsWindowReady())
		return 0;
	window_open = 1;

	BeginDrawing();
	ClearBackground(BLACK);
	EndDrawing();
	return 1;
}

void RL_Hold (int ms)
{
	double end;

	if (!window_open || ms <= 0)
		return;
	end = GetTime() + ms / 1000.0;
	while (GetTime() < end && !WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(BLACK);
		EndDrawing();
	}
}

void RL_CloseWindow (void)
{
	if (!window_open)
		return;
	window_open = 0;
	CloseWindow();
}

int RL_WindowIsOpen (void)
{
	return window_open;
}
