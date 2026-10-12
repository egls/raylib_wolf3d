/*
 * id_raylib.c - the raylib backend.  The ONLY translation unit that includes
 * raylib.h (see id_raylib.h).
 */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "raylib.h"

#include "id_raylib.h"

#ifdef __APPLE__
/* The Objective-C runtime that GLFW's Cocoa backend already links. */
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/*
 * The browser build (WOLF-41) runs the game's blocking loops under ASYNCIFY:
 * the page only shows a frame, delivers input and runs timers while the game
 * is suspended in emscripten_sleep().  RL_Present and RL_PumpEvents yield
 * when the game has not yielded for WEB_YIELD_MS, so polling loops that
 * never present still let the page run, and bursts of presents (the
 * "Get Psyched" bar draws once per VSWAP page) don't wait for the browser
 * every time.  A yield costs a setTimeout, about 4 ms.  The game's own waits
 * (CalcTics, VL_WaitVBL, WL_Idle) sleep in port/wl_timer.c.
 */
#define WEB_YIELD_MS	8.0

static double web_lastyield;

static void WebYield (void)
{
	if (emscripten_get_now() - web_lastyield < WEB_YIELD_MS)
		return;
	emscripten_sleep(0);
	web_lastyield = emscripten_get_now();
}

/* raylib's WindowShouldClose() sleeps on the web; a page never closes. */
#define WindowShouldClose()	0

/*
 * Mouse: raylib's web backend keeps only the last mousemove of a frame, so
 * the page's own listener adds up every movement while the pointer is
 * locked.  The browser locks it only from a click, so the game says when it
 * wants the mouse (Module.wolfWantsPointer) and the page asks on the next
 * click on the canvas (web/shell.html).
 */
EM_JS(void, WebMouseInit, (void), {
	Module.wolfMouseDX = Module.wolfMouseDY = 0;
	Module.canvas.addEventListener('mousemove', function (e) {
		if (document.pointerLockElement === Module.canvas) {
			Module.wolfMouseDX += e.movementX;
			Module.wolfMouseDY += e.movementY;
		}
	});
});
EM_JS(int, WebMouseLocked, (void), {
	return document.pointerLockElement === Module.canvas ? 1 : 0;
});
EM_JS(void, WebMouseTake, (int *dx, int *dy), {
	HEAP32[dx >> 2] = Module.wolfMouseDX | 0;
	HEAP32[dy >> 2] = Module.wolfMouseDY | 0;
	Module.wolfMouseDX = Module.wolfMouseDY = 0;
});
EM_JS(void, WebMouseWant, (int on), {
	Module.wolfWantsPointer = on;
	if (!on && document.pointerLockElement === Module.canvas)
		document.exitPointerLock();
});
#endif

#define MAX_SCALE		12		/* a 2400-pixel-high picture: 4K, and Retina fullscreen */
#define KEYQ_SIZE		256

static int window_open;
static int quit_requested;

static Texture2D		frame_tex;
static RenderTexture2D	big_rt;
static int				big_scale;

/* ------------------------------------------------------------------------
 * Keyboard: raylib key -> PC set-1 scan code
 * ------------------------------------------------------------------------ */

typedef struct
{
	int		key;
	int		scan;
	int		ext;
	int		down;		/* last reported state */
	int		tapped;		/* seen in the pressed-key queue since the last sync */
	int		swallowed;	/* the press was a shortcut: its release is not reported */
} keymap_t;

static keymap_t keymap[] =
{
	{KEY_ESCAPE,0x01},
	{KEY_ONE,0x02},{KEY_TWO,0x03},{KEY_THREE,0x04},{KEY_FOUR,0x05},{KEY_FIVE,0x06},
	{KEY_SIX,0x07},{KEY_SEVEN,0x08},{KEY_EIGHT,0x09},{KEY_NINE,0x0a},{KEY_ZERO,0x0b},
	{KEY_MINUS,0x0c},{KEY_EQUAL,0x0d},{KEY_BACKSPACE,0x0e},{KEY_TAB,0x0f},
	{KEY_Q,0x10},{KEY_W,0x11},{KEY_E,0x12},{KEY_R,0x13},{KEY_T,0x14},{KEY_Y,0x15},
	{KEY_U,0x16},{KEY_I,0x17},{KEY_O,0x18},{KEY_P,0x19},
	{KEY_LEFT_BRACKET,0x1a},{KEY_RIGHT_BRACKET,0x1b},{KEY_ENTER,0x1c},
	{KEY_LEFT_CONTROL,0x1d},
	{KEY_A,0x1e},{KEY_S,0x1f},{KEY_D,0x20},{KEY_F,0x21},{KEY_G,0x22},{KEY_H,0x23},
	{KEY_J,0x24},{KEY_K,0x25},{KEY_L,0x26},
	{KEY_SEMICOLON,0x27},{KEY_APOSTROPHE,0x28},{KEY_GRAVE,0x29},
	{KEY_LEFT_SHIFT,0x2a},{KEY_BACKSLASH,0x2b},
	{KEY_Z,0x2c},{KEY_X,0x2d},{KEY_C,0x2e},{KEY_V,0x2f},{KEY_B,0x30},{KEY_N,0x31},
	{KEY_M,0x32},
	{KEY_COMMA,0x33},{KEY_PERIOD,0x34},{KEY_SLASH,0x35},{KEY_RIGHT_SHIFT,0x36},
	{KEY_KP_MULTIPLY,0x37},{KEY_LEFT_ALT,0x38},{KEY_SPACE,0x39},{KEY_CAPS_LOCK,0x3a},
	{KEY_F1,0x3b},{KEY_F2,0x3c},{KEY_F3,0x3d},{KEY_F4,0x3e},{KEY_F5,0x3f},
	{KEY_F6,0x40},{KEY_F7,0x41},{KEY_F8,0x42},{KEY_F9,0x43},{KEY_F10,0x44},
	{KEY_NUM_LOCK,0x45},{KEY_SCROLL_LOCK,0x46},
	{KEY_KP_7,0x47},{KEY_KP_8,0x48},{KEY_KP_9,0x49},{KEY_KP_SUBTRACT,0x4a},
	{KEY_KP_4,0x4b},{KEY_KP_5,0x4c},{KEY_KP_6,0x4d},{KEY_KP_ADD,0x4e},
	{KEY_KP_1,0x4f},{KEY_KP_2,0x50},{KEY_KP_3,0x51},{KEY_KP_0,0x52},{KEY_KP_DECIMAL,0x53},
	{KEY_F11,0x57},{KEY_F12,0x58},
	/* 0xE0-prefixed keys */
	{KEY_KP_ENTER,0x1c,1},{KEY_RIGHT_CONTROL,0x1d,1},{KEY_KP_DIVIDE,0x35,1},
	{KEY_RIGHT_ALT,0x38,1},
	{KEY_HOME,0x47,1},{KEY_UP,0x48,1},{KEY_PAGE_UP,0x49,1},{KEY_LEFT,0x4b,1},
	{KEY_RIGHT,0x4d,1},{KEY_END,0x4f,1},{KEY_DOWN,0x50,1},{KEY_PAGE_DOWN,0x51,1},
	{KEY_INSERT,0x52,1},{KEY_DELETE,0x53,1},
};
#define NUMKEYS ((int)(sizeof(keymap) / sizeof(keymap[0])))

typedef struct { short scan; unsigned char ext, down; } keyevent_t;
static keyevent_t	keyq[KEYQ_SIZE];
static int			keyq_head, keyq_tail;

static void PushKey (int scan, int ext, int down)
{
	int next = (keyq_head + 1) % KEYQ_SIZE;

	if (next == keyq_tail)
		return;					/* full: drop, like a BIOS keyboard buffer */
	keyq[keyq_head].scan = (short)scan;
	keyq[keyq_head].ext = (unsigned char)ext;
	keyq[keyq_head].down = (unsigned char)down;
	keyq_head = next;
}

/*
 * Shortcuts that never reach the game.  Alt+Enter (Option+Return on a Mac)
 * toggles fullscreen everywhere.  On macOS, Command is the shortcut key: the
 * game never sees Command, Cmd+F and Cmd+Return toggle fullscreen, and other
 * keys pressed with Command are left to the system (Cmd+Q, Cmd+H and Cmd+M
 * are menu shortcuts that GLFW's menu bar handles).  Mac keyboards play like
 * PC ones: Control fires and Option is Alt (strafe).
 * Returns 1 if the press of `key` is a shortcut.
 */
#ifdef __APPLE__
/*
 * Command counts for a whole poll: held now, held at the last sync, or
 * pressed in between.  Cmd+F pressed and released within one poll leaves
 * only F in raylib's pressed-key queue, with Command already up.
 */
static int command, command_was_down, command_tapped;
#endif

static int Shortcut (int key)
{
#ifdef __APPLE__
	if (command)
	{
		if (key == KEY_F || key == KEY_ENTER)
			RL_ToggleFullscreen();
		return 1;
	}
#endif
	if (key == KEY_ENTER && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))
	{
		RL_ToggleFullscreen();
		return 1;
	}
	return 0;
}

/* Compares raylib's key state with what was last reported and posts events. */
static void SyncKeys (void)
{
	int i, key, down;

	/* raylib's pressed-key queue is the only record of taps shorter than a poll */
	while ((key = GetKeyPressed()) != 0)
	{
#ifdef __APPLE__
		if (key == KEY_LEFT_SUPER || key == KEY_RIGHT_SUPER)
			command_tapped = 1;
#endif
		for (i = 0; i < NUMKEYS; i++)
			if (keymap[i].key == key)
				keymap[i].tapped = 1;
	}
#ifdef __APPLE__
	down = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
	command = down || command_was_down || command_tapped;
	command_was_down = down;
	command_tapped = 0;
#endif

	for (i = 0; i < NUMKEYS; i++)
	{
		keymap_t *k = &keymap[i];

		down = IsKeyDown(k->key);
		if (k->tapped && !k->down && !down)
		{
			/* pressed and released between two polls: report both */
			if (!Shortcut(k->key))		/* a quick shortcut tap still counts */
			{
				PushKey(k->scan, k->ext, 1);
				PushKey(k->scan, k->ext, 0);
			}
		}
		else if (down != k->down)
		{
			if (down && Shortcut(k->key))
				k->swallowed = 1;		/* neither the press nor the release */
			else if (!down && k->swallowed)
				k->swallowed = 0;
			else
				PushKey(k->scan, k->ext, down);
		}
		k->down = down;
		k->tapped = 0;
	}
}

/*
 * EndDrawing() polls the OS and resets raylib's pressed-key queue, so the
 * queue is drained before and after every poll.
 */
static void PollKeyboard (void)
{
	SyncKeys();
	PollInputEvents();
	SyncKeys();
}

/* ------------------------------------------------------------------------
 * Mouse and gamepad
 * ------------------------------------------------------------------------ */

static int mouse_grab;
static int mouse_dx, mouse_dy;

static void PollMouse (void)
{
#ifdef __EMSCRIPTEN__
	int dx, dy;

	WebMouseTake(&dx, &dy);
	mouse_dx += dx;
	mouse_dy += dy;
#else
	Vector2 d = GetMouseDelta();

	mouse_dx += (int)lroundf(d.x);
	mouse_dy += (int)lroundf(d.y);
#endif
}

void RL_SetMouseGrab (int on)
{
	if (!window_open || on == mouse_grab)
		return;
	mouse_grab = on;
#ifdef __EMSCRIPTEN__
	WebMouseWant(on);
	mouse_dx = mouse_dy = 0;
	{
		int dx, dy;

		WebMouseTake(&dx, &dy);		/* drop motion from before the grab */
	}
#else
	if (on)
		DisableCursor();
	else
		EnableCursor();
	mouse_dx = mouse_dy = 0;
	(void)GetMouseDelta();
#endif
}

void RL_MouseDelta (int *dx, int *dy)
{
	*dx = mouse_dx;
	*dy = mouse_dy;
	mouse_dx = mouse_dy = 0;
}

int RL_MouseButtons (void)
{
	int b = 0;

	if (!window_open)
		return 0;
#ifdef __EMSCRIPTEN__
	if (mouse_grab && !WebMouseLocked())
		return 0;		/* the click that locks the pointer is not a shot */
#endif
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		b |= 1;
	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
		b |= 2;
	if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
		b |= 4;
	return b;
}

int RL_JoyPresent (void)
{
	return window_open && IsGamepadAvailable(0);
}

void RL_JoyAxes (int *x, int *y)
{
	*x = *y = 0;
	if (!RL_JoyPresent())
		return;
	*x = (int)(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) * 1000);
	*y = (int)(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) * 1000);
	if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
		*x = -1000;
	else if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
		*x = 1000;
	if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
		*y = -1000;
	else if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
		*y = 1000;
}

int RL_JoyButtons (void)
{
	int b = 0;

	if (!RL_JoyPresent())
		return 0;
	if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
		IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2))
		b |= 1;
	if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
		IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||
		IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2))
		b |= 2;
	return b;
}

void RL_PumpEvents (void)
{
	RL_AudioPump();
	if (!window_open)
		return;
#ifdef __EMSCRIPTEN__
	WebYield();
#endif
	PollKeyboard();
	PollMouse();
	if (WindowShouldClose())
		quit_requested = 1;
}

int RL_GetKeyEvent (int *scancode, int *extended, int *down)
{
	if (keyq_tail == keyq_head)
		return 0;
	*scancode = keyq[keyq_tail].scan;
	*extended = keyq[keyq_tail].ext;
	*down = keyq[keyq_tail].down;
	keyq_tail = (keyq_tail + 1) % KEYQ_SIZE;
	return 1;
}

int RL_QuitRequested (void)
{
	return quit_requested;
}

/* ------------------------------------------------------------------------
 * Window and presentation
 * ------------------------------------------------------------------------ */

/*
 * The size of the window's framebuffer in pixels.  On a Mac (Retina) display
 * raylib's screen size is in points and the framebuffer has two (or more)
 * pixels per point; drawing still uses points.
 */
static void FramebufferSize (int *w, int *h)
{
#ifdef __APPLE__
	*w = GetRenderWidth();
	*h = GetRenderHeight();
#else
	*w = GetScreenWidth();
	*h = GetScreenHeight();
#endif
}

static void EnsureBigTarget (int win_w, int win_h)
{
	int s = (win_h + RL_FRAME_H - 1) / RL_FRAME_H;
	int sx = (win_w + RL_FRAME_W - 1) / RL_FRAME_W;

	if (sx > s)
		s = sx;
	if (s < 1)
		s = 1;
	if (s > MAX_SCALE)
		s = MAX_SCALE;
	if (s == big_scale)
		return;
	if (big_scale)
		UnloadRenderTexture(big_rt);
	big_rt = LoadRenderTexture(RL_FRAME_W * s, RL_FRAME_H * s);
	SetTextureFilter(big_rt.texture, TEXTURE_FILTER_BILINEAR);
	big_scale = s;
}

int RL_OpenWindow (int width, int height, const char *title)
{
	Image img;

	if (window_open)
		return 1;

	SetTraceLogLevel(LOG_WARNING);
	/*
	 * Not FLAG_WINDOW_HIGHDPI: on macOS GLFW already gives the window a
	 * Retina framebuffer (GLFW_SCALE_FRAMEBUFFER defaults to true) and raylib
	 * 5.5 draws in points scaled to it.  The flag only adds a content-scale
	 * callback that scales the drawing a second time once the window moves
	 * from a 1x display back to a Retina one.
	 */
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(width, height, title);
	if (!IsWindowReady())
		return 0;
	SetWindowMinSize(RL_FRAME_W, RL_FRAME_H);
	SetExitKey(KEY_NULL);		/* Esc belongs to the game; closing the window quits */
	window_open = 1;
#ifdef __EMSCRIPTEN__
	WebMouseInit();
#endif

	img = GenImageColor(RL_FRAME_W, RL_FRAME_H, BLACK);
	ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
	frame_tex = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(frame_tex, TEXTURE_FILTER_POINT);

	BeginDrawing();
	ClearBackground(BLACK);
	EndDrawing();
	return 1;
}

void RL_Present (const unsigned char *rgba)
{
	RL_AudioPump();
	int		w, h, pw, ph;
	float	s, dw, dh;

	if (!window_open)
		return;

	SyncKeys();					/* before EndDrawing() resets the key queue */

	UpdateTexture(frame_tex, rgba);
	w = GetScreenWidth();
	h = GetScreenHeight();
	FramebufferSize(&pw, &ph);
	EnsureBigTarget(pw, ph);

	BeginTextureMode(big_rt);
	DrawTexturePro(frame_tex,
		(Rectangle){0, 0, RL_FRAME_W, RL_FRAME_H},
		(Rectangle){0, 0, (float)(RL_FRAME_W * big_scale), (float)(RL_FRAME_H * big_scale)},
		(Vector2){0, 0}, 0, WHITE);
	EndTextureMode();

	s = fminf(w / 4.0f, h / 3.0f);
	dw = 4 * s;
	dh = 3 * s;
	BeginDrawing();
	ClearBackground(BLACK);
	DrawTexturePro(big_rt.texture,
		(Rectangle){0, 0, (float)(RL_FRAME_W * big_scale), -(float)(RL_FRAME_H * big_scale)},
		(Rectangle){(w - dw) / 2, (h - dh) / 2, dw, dh},
		(Vector2){0, 0}, 0, WHITE);
	EndDrawing();				/* swaps and polls */
#ifdef __EMSCRIPTEN__
	WebYield();					/* the page shows the frame and runs its events */
#endif

	SyncKeys();
	PollMouse();
	if (WindowShouldClose())
		quit_requested = 1;
}

#ifdef __APPLE__
/*
 * macOS fullscreen is the window's own Space, the same as the green window
 * button and View > Enter Full Screen (Ctrl+Cmd+F): -[NSWindow
 * toggleFullScreen:] keeps the menu bar, the notch and Mission Control
 * right, where a borderless window would sit under the menu bar.
 */
static int MacToggleFullscreen (void)
{
	void *window = GetWindowHandle();	/* the NSWindow */

	if (!window)
		return 0;
	((void (*)(void *, SEL, void *))objc_msgSend)(window,
		sel_registerName("toggleFullScreen:"), NULL);
	return 1;
}
#endif

void RL_ToggleFullscreen (void)
{
#ifndef __EMSCRIPTEN__		/* the browser wants a user gesture; the page does Alt+Enter */
	if (!window_open)
		return;
#ifdef __APPLE__
	if (MacToggleFullscreen())
		return;
#endif
	ToggleBorderlessWindowed();
#endif
}

void RL_Hold (int ms)
{
	double end;

	if (!window_open || ms <= 0)
		return;
	end = GetTime() + ms / 1000.0;
	while (GetTime() < end && !WindowShouldClose())
	{
		RL_AudioPump();
		BeginDrawing();
		ClearBackground(BLACK);
		EndDrawing();
#ifdef __EMSCRIPTEN__
		WebYield();
#endif
	}
}

void RL_CloseWindow (void)
{
	if (!window_open)
		return;
	window_open = 0;
	if (big_scale)
		UnloadRenderTexture(big_rt);
	UnloadTexture(frame_tex);
	big_scale = 0;
	CloseWindow();
}

int RL_WindowIsOpen (void)
{
	return window_open;
}

/* SPSC: game thread owns write/mixing; raylib audio thread owns read.
 * C11 atomics (GCC, Clang and MSVC with /experimental:c11atomics) work with
 * the legacy C99 build, including on ARM. Never take a game/cache lock in the
 * callback. The ring contains 16-bit stereo frames. */
#ifdef WOLF3D_AUDIO_BACKEND
#include <stdatomic.h>
#include <stdio.h>
/* Monotonic clock for pacing.  A test defines RL_EXTERNAL_CLOCK and supplies
 * RL_MonotonicNow() itself to script time (MinGW's clock_gettime is an inline
 * function, so unlike on Linux it cannot be replaced at link time).  The MSVC
 * CRT has no clock_gettime, so there raylib's GetTime() (the same
 * QueryPerformanceCounter clock) stands in; only differences are used. */
#ifdef RL_EXTERNAL_CLOCK
void RL_MonotonicNow(struct timespec *ts);
#elif defined(_MSC_VER)
static void RL_MonotonicNow(struct timespec *ts) {
    double t=GetTime();
    ts->tv_sec=(time_t)t;
    ts->tv_nsec=(long)((t-(double)ts->tv_sec)*1e9);
}
#else
static void RL_MonotonicNow(struct timespec *ts) { clock_gettime(CLOCK_MONOTONIC,ts); }
#endif
#define AUDIO_RING 8192u
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
#define AUDIO_AHEAD 1536u /* 32 ms, six 256-frame WebAudio blocks */
/* ScriptProcessor can pull a block before its presentation time. This fixed
 * allowance absorbs that prefetch without relaxing the 1000 ppm rate cap. */
#define AUDIO_CLOCK_AHEAD (AUDIO_AHEAD+512u)
#else
#define AUDIO_AHEAD 3840u /* 80 ms at 48 kHz */
#define AUDIO_CLOCK_AHEAD AUDIO_AHEAD
#endif
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
/* raylib 5.5/miniaudio uses a main-thread ScriptProcessor, not a worker.
 * Keep policy here with the device, rather than coupling it to the shell. */
#ifndef RL_WEB_AUDIO_TEST
EM_JS(void, WebAudioInit, (void), {
    var device = window.miniaudio.devices.find(function(d) { return d && d.webaudio; });
    var ctx = device.webaudio;
    var state = Module.wolfAudio = { context: ctx, generation: 1 };
    state.unlock = function() {
        if (!document.hidden && ctx.state !== 'closed') ctx.resume().catch(function() {});
    };
    state.visibility = function() {
        state.generation++;
        if (document.hidden) ctx.suspend().catch(function() {});
        else state.unlock();
    };
    state.changed = function() { state.generation++; };
    document.addEventListener('keydown', state.unlock, true);
    document.addEventListener('click', state.unlock, true);
    document.addEventListener('touchend', state.unlock, true);
    document.addEventListener('visibilitychange', state.visibility);
    ctx.addEventListener('statechange', state.changed);
    if (document.hidden) state.visibility();
});
EM_JS(unsigned, WebAudioGeneration, (void), {
    var s = Module.wolfAudio;
    return s && !document.hidden && s.context.state === 'running' ? s.generation : 0;
});
EM_JS(void, WebAudioStats, (unsigned queued, unsigned underruns, unsigned resets, double generated), {
    Module.wolfAudioStats = { queuedFrames: queued, underruns: underruns,
        resyncs: resets, generatedFrames: generated,
        running: !document.hidden && Module.wolfAudio.context.state === 'running' };
});
EM_JS(void, WebAudioClose, (void), {
    var s = Module.wolfAudio;
    document.removeEventListener('keydown', s.unlock, true);
    document.removeEventListener('click', s.unlock, true);
    document.removeEventListener('touchend', s.unlock, true);
    document.removeEventListener('visibilitychange', s.visibility);
    s.context.removeEventListener('statechange', s.changed);
    delete Module.wolfAudio;
});
#else
void WebAudioInit(void);
unsigned WebAudioGeneration(void);
void WebAudioStats(unsigned queued, unsigned underruns, unsigned resets, double generated);
void WebAudioClose(void);
#endif
static unsigned audio_web_generation, audio_web_resets;
static struct timespec audio_web_lastpump;
#endif
static short audio_ring[AUDIO_RING*2];
static _Atomic unsigned audio_read, audio_write;
static _Atomic unsigned audio_underruns;
static AudioStream audio_stream;
static int audio_open;
static void (*audio_mixer)(short *, unsigned);
static struct timespec audio_epoch;
static uint64_t audio_generated;
static void WolfAudioCallback(void *buffer, unsigned frames) {
    short *out=buffer;
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
    /* Visibility/state events can run before the next game pump. Silence
     * immediately, and leave the producer to reset the ring on its next turn. */
    if (!audio_web_generation || WebAudioGeneration()!=audio_web_generation) {
        memset(out,0,frames*2*sizeof(*out));
        return;
    }
#endif
    unsigned r=atomic_load_explicit(&audio_read,memory_order_relaxed);
    unsigned w=atomic_load_explicit(&audio_write,memory_order_acquire);
    unsigned n=w-r, i;
    if (n>frames) n=frames;
    if (n<frames) atomic_fetch_add_explicit(&audio_underruns,1,memory_order_relaxed);
    for (i=0;i<n;++i) {
        out[i*2]=audio_ring[((r+i)%AUDIO_RING)*2];
        out[i*2+1]=audio_ring[((r+i)%AUDIO_RING)*2+1];
    }
    memset(out+n*2,0,(frames-n)*2*sizeof(*out));
    atomic_store_explicit(&audio_read,r+n,memory_order_release);
}
void RL_AudioPump(void) {
    struct timespec now;
    short samples[512*2];
    unsigned r,w,queued,n;
    uint64_t due;
    if (!audio_open) return;
    RL_MonotonicNow(&now);
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
    unsigned generation=WebAudioGeneration();
    double gap=(now.tv_sec-audio_web_lastpump.tv_sec)+
               (now.tv_nsec-audio_web_lastpump.tv_nsec)*1e-9;
    if (generation!=audio_web_generation || gap>0.25) {
        /* Both callbacks and pumps run on this browser thread. Discard the
         * pre-suspension ring and rebase the cap: never catch up hidden time.
         * Stop/play also resets raylib's stream cursor. Sequencer stays paused. */
        StopAudioStream(audio_stream);
        atomic_store_explicit(&audio_read,0,memory_order_relaxed);
        atomic_store_explicit(&audio_write,0,memory_order_relaxed);
        audio_epoch=now; audio_generated=0;
        audio_web_generation=generation;
        ++audio_web_resets;
        PlayAudioStream(audio_stream);
    }
    audio_web_lastpump=now;
    if (!generation) {
        WebAudioStats(0,atomic_load(&audio_underruns),audio_web_resets,0);
        return;
    }
#endif
    /* Allow 1000 ppm of DAC clock drift relative to CLOCK_MONOTONIC.
     * The queue bounds synthesis lead to 80 ms native / 32 ms web; this cap prevents
     * a null/racing sink from advancing synthesis without wall-clock time. */
    due=(uint64_t)(((now.tv_sec-audio_epoch.tv_sec)+
                   (now.tv_nsec-audio_epoch.tv_nsec)*1e-9)*48000*1.001)+AUDIO_CLOCK_AHEAD;
    r=atomic_load_explicit(&audio_read,memory_order_acquire);
    w=atomic_load_explicit(&audio_write,memory_order_relaxed);
    queued=w-r;
    while (queued<AUDIO_AHEAD && audio_generated<due) {
        n=AUDIO_AHEAD-queued;
        if (n>512) n=512;
        if (n>due-audio_generated) n=(unsigned)(due-audio_generated);
        audio_mixer(samples,n);
        for (unsigned i=0;i<n;++i) {
            audio_ring[((w+i)%AUDIO_RING)*2]=samples[i*2];
            audio_ring[((w+i)%AUDIO_RING)*2+1]=samples[i*2+1];
        }
        w+=n; queued+=n; audio_generated+=n;
        atomic_store_explicit(&audio_write,w,memory_order_release);
    }
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
    WebAudioStats(queued,atomic_load(&audio_underruns),audio_web_resets,(double)audio_generated);
#endif
}
int RL_AudioOpen(void (*mixer)(short *, unsigned)) {
    if (audio_open) return 1;
    /* Require actual lock-free 32-bit atomics on the target. */
    if (ATOMIC_INT_LOCK_FREE!=2) return 0;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) { CloseAudioDevice(); return 0; }
    SetAudioStreamBufferSizeDefault(512);
    audio_stream=LoadAudioStream(48000,16,2);
    if (!IsAudioStreamValid(audio_stream)) { CloseAudioDevice(); return 0; }
    atomic_store_explicit(&audio_read,0,memory_order_relaxed);
    atomic_store_explicit(&audio_write,0,memory_order_relaxed);
    atomic_store_explicit(&audio_underruns,0,memory_order_relaxed);
    audio_generated=0; audio_mixer=mixer;
    RL_MonotonicNow(&audio_epoch);
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
    WebAudioInit();
    audio_web_generation=audio_web_resets=0;
    audio_web_lastpump=audio_epoch;
#endif
    audio_open=1;
    RL_AudioPump();
    SetAudioStreamCallback(audio_stream,WolfAudioCallback);
    PlayAudioStream(audio_stream);
    return 1;
}
void RL_AudioClose(void) {
    if (!audio_open) return;
#if defined(__EMSCRIPTEN__) || defined(RL_WEB_AUDIO_TEST)
    WebAudioClose();
#endif
    StopAudioStream(audio_stream);
    UnloadAudioStream(audio_stream);
    CloseAudioDevice();
    fprintf(stderr,"Wolf audio: 48000 Hz, signed 16-bit stereo, underrun callbacks: %u\n",
            atomic_load_explicit(&audio_underruns,memory_order_relaxed));
    audio_open=0; audio_mixer=NULL;
}
#else
/* Keyboard tests link a fake video/input raylib with no audio device. */
int RL_AudioOpen(void (*mixer)(short *, unsigned)) { (void)mixer; return 0; }
void RL_AudioPump(void) { }
void RL_AudioClose(void) { }
#endif
