/*
 * rl_keys_test.c - tests the polling-to-events keyboard translation of
 * port/id_raylib.c against a scripted fake raylib (no window, no display).
 *
 * The fake lives in this file: id_raylib.c is compiled against raylib's real
 * header but linked against these stand-ins.
 */
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "id_raylib.h"

/* ---- scripted raylib state ------------------------------------------------ */
static int	down[512];			/* IsKeyDown() */
static int	queue[16], queued;	/* GetKeyPressed() queue */
static int	toggles;

bool IsKeyDown (int key)				{ return down[key]; }
int  GetKeyPressed (void)				{ return queued ? queue[--queued] : 0; }
void PollInputEvents (void)				{ queued = 0; }	/* like EndDrawing: loses the queue */
Vector2 GetMouseDelta (void)			{ return (Vector2){0, 0}; }
bool IsMouseButtonDown (int b)			{ (void)b; return 0; }
bool WindowShouldClose (void)			{ return 0; }
bool IsGamepadAvailable (int g)			{ (void)g; return 0; }
bool IsGamepadButtonDown (int g, int b)	{ (void)g; (void)b; return 0; }
float GetGamepadAxisMovement (int g, int a) { (void)g; (void)a; return 0; }
void ToggleBorderlessWindowed (void)	{ toggles++; }
int  WL_GLAvailable (void)				{ return 1; }	/* fake raylib: no real GL needed */
void SetTraceLogLevel (int l)			{ (void)l; }
void SetConfigFlags (unsigned f)		{ (void)f; }
void InitWindow (int w, int h, const char *t) { (void)w; (void)h; (void)t; }
bool IsWindowReady (void)				{ return 1; }
void SetWindowMinSize (int w, int h)	{ (void)w; (void)h; }
void SetExitKey (int k)					{ (void)k; }
Image GenImageColor (int w, int h, Color c) { (void)w; (void)h; (void)c; return (Image){0}; }
void ImageFormat (Image *i, int f)		{ (void)i; (void)f; }
Texture2D LoadTextureFromImage (Image i) { (void)i; return (Texture2D){0}; }
void UnloadImage (Image i)				{ (void)i; }
void SetTextureFilter (Texture2D t, int f) { (void)t; (void)f; }
void BeginDrawing (void)				{}
void EndDrawing (void)					{ queued = 0; }
void ClearBackground (Color c)			{ (void)c; }
void DisableCursor (void)				{}
void EnableCursor (void)				{}
void CloseWindow (void)					{}
void UnloadTexture (Texture2D t)		{ (void)t; }
void UnloadRenderTexture (RenderTexture2D t) { (void)t; }
double GetTime (void)					{ return 0; }
void UpdateTexture (Texture2D t, const void *p) { (void)t; (void)p; }
int GetScreenWidth (void)				{ return 640; }
int GetScreenHeight (void)				{ return 480; }
int GetRenderWidth (void)				{ return 1280; }
int GetRenderHeight (void)				{ return 960; }
void *GetWindowHandle (void)			{ return NULL; }	/* RL_ToggleFullscreen falls back */
RenderTexture2D LoadRenderTexture (int w, int h) { (void)w; (void)h; return (RenderTexture2D){0}; }
void BeginTextureMode (RenderTexture2D t) { (void)t; }
void EndTextureMode (void)				{}
void DrawTexturePro (Texture2D t, Rectangle s, Rectangle d, Vector2 o, float r, Color c)
{ (void)t; (void)s; (void)d; (void)o; (void)r; (void)c; }

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void Expect (int scan, int ext, int dn)
{
	int s, e, d;

	if (!RL_GetKeyEvent(&s, &e, &d))
	{
		printf("FAIL: expected event %02x ext=%d down=%d, queue empty\n", scan, ext, dn);
		failures++;
		return;
	}
	if (s != scan || e != ext || d != dn)
	{
		printf("FAIL: got %02x ext=%d down=%d, expected %02x ext=%d down=%d\n", s, e, d, scan, ext, dn);
		failures++;
	}
}

static void Empty (void)
{
	int s, e, d;

	CHECK(!RL_GetKeyEvent(&s, &e, &d));
}

int main (void)
{
	CHECK(RL_OpenWindow(640, 480, "test"));

	/* held key: down on the poll that sees it, up when it is released */
	down[KEY_UP] = 1;
	RL_PumpEvents();
	Expect(0x48, 1, 1);
	Empty();
	RL_PumpEvents();
	Empty();							/* no repeat events */
	down[KEY_UP] = 0;
	RL_PumpEvents();
	Expect(0x48, 1, 0);
	Empty();

	/* tap shorter than a poll: only the pressed-key queue knows */
	queue[queued++] = KEY_A;
	RL_PumpEvents();
	Expect(0x1e, 0, 1);
	Expect(0x1e, 0, 0);
	Empty();

	/* the queue is drained before the poll that would reset it */
	queue[queued++] = KEY_SPACE;
	RL_Present((const unsigned char *)"");
	Expect(0x39, 0, 1);
	Expect(0x39, 0, 0);
	Empty();

	/* modifiers: left/right ctrl share a scan code, right one is extended */
	down[KEY_LEFT_CONTROL] = 1;
	down[KEY_RIGHT_CONTROL] = 1;
	RL_PumpEvents();
	Expect(0x1d, 0, 1);
	Expect(0x1d, 1, 1);
	down[KEY_LEFT_CONTROL] = down[KEY_RIGHT_CONTROL] = 0;
	RL_PumpEvents();
	Expect(0x1d, 0, 0);
	Expect(0x1d, 1, 0);
	Empty();

	/* Enter alone reaches the game */
	down[KEY_ENTER] = 1;
	RL_PumpEvents();
	Expect(0x1c, 0, 1);
	down[KEY_ENTER] = 0;
	RL_PumpEvents();
	Expect(0x1c, 0, 0);
	Empty();

	/* Alt+Enter toggles fullscreen and never reaches the game */
	down[KEY_LEFT_ALT] = 1;
	RL_PumpEvents();
	Expect(0x38, 0, 1);
	down[KEY_ENTER] = 1;
	queue[queued++] = KEY_ENTER;
	RL_PumpEvents();
	CHECK(toggles == 1);
	Empty();
	down[KEY_ENTER] = 0;
	down[KEY_LEFT_ALT] = 0;
	RL_PumpEvents();
	Expect(0x38, 0, 0);
	Empty();					/* Enter's release was swallowed too */

	/* Alt+Enter tapped faster than a poll: still a toggle... and no Enter event */
	down[KEY_LEFT_ALT] = 1;
	RL_PumpEvents();
	Expect(0x38, 0, 1);
	queue[queued++] = KEY_ENTER;
	RL_PumpEvents();
	Empty();
	CHECK(toggles == 2);
	down[KEY_LEFT_ALT] = 0;
	RL_PumpEvents();
	Expect(0x38, 0, 0);
	Empty();

#ifdef __APPLE__
	/* Command never reaches the game; Cmd+F and Cmd+Return toggle fullscreen */
	down[KEY_LEFT_SUPER] = 1;
	RL_PumpEvents();
	Empty();
	down[KEY_F] = 1;
	RL_PumpEvents();
	CHECK(toggles == 3);
	Empty();
	down[KEY_F] = 0;
	RL_PumpEvents();
	Empty();					/* F's release was swallowed too */
	queue[queued++] = KEY_ENTER;	/* Cmd+Return tapped faster than a poll */
	RL_PumpEvents();
	CHECK(toggles == 4);
	Empty();

	/* other Command shortcuts (Cmd+Q, Cmd+H...) belong to the system */
	down[KEY_Q] = 1;
	RL_PumpEvents();
	Empty();
	down[KEY_LEFT_SUPER] = 0;	/* Command released first: Q's release stays swallowed */
	RL_PumpEvents();
	Empty();
	down[KEY_Q] = 0;
	RL_PumpEvents();
	Empty();
	CHECK(toggles == 4);

	/* a key held before Command keeps working */
	down[KEY_UP] = 1;
	RL_PumpEvents();
	Expect(0x48, 1, 1);
	down[KEY_RIGHT_SUPER] = 1;
	RL_PumpEvents();
	Empty();
	down[KEY_UP] = 0;
	RL_PumpEvents();
	Expect(0x48, 1, 0);
	down[KEY_RIGHT_SUPER] = 0;
	RL_PumpEvents();
	Empty();

	/* Cmd+F pressed and released within one poll: only the queue saw them */
	queue[queued++] = KEY_LEFT_SUPER;
	queue[queued++] = KEY_F;
	RL_PumpEvents();
	CHECK(toggles == 5);
	Empty();

	/* Command released in the same poll as F is pressed; F stays held */
	queue[queued++] = KEY_LEFT_SUPER;
	queue[queued++] = KEY_F;
	down[KEY_F] = 1;
	RL_PumpEvents();
	CHECK(toggles == 6);
	Empty();
	down[KEY_F] = 0;
	RL_PumpEvents();
	Empty();					/* F's release stays swallowed */

	/* Command held at the last poll, both released before this one */
	down[KEY_LEFT_SUPER] = 1;
	RL_PumpEvents();
	Empty();
	down[KEY_LEFT_SUPER] = 0;
	queue[queued++] = KEY_ENTER;
	RL_PumpEvents();
	CHECK(toggles == 7);
	Empty();

	/* after that, F is the game's again */
	queue[queued++] = KEY_F;
	RL_PumpEvents();
	Expect(0x21, 0, 1);
	Expect(0x21, 0, 0);
	Empty();
	CHECK(toggles == 7);
#endif

	if (!failures)
		printf("rl_keys_test: all checks passed\n");
	return failures != 0;
}
