/*
 * id_raylib.c - the raylib backend.  The ONLY translation unit that includes
 * raylib.h (see id_raylib.h).
 */
#include <math.h>

#include "raylib.h"

#include "id_raylib.h"

#define MAX_SCALE		8
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
static int			swallow_enter;

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

/* Compares raylib's key state with what was last reported and posts events. */
static void SyncKeys (void)
{
	int i, key, down;

	/* raylib's pressed-key queue is the only record of taps shorter than a poll */
	while ((key = GetKeyPressed()) != 0)
		for (i = 0; i < NUMKEYS; i++)
			if (keymap[i].key == key)
				keymap[i].tapped = 1;

	for (i = 0; i < NUMKEYS; i++)
	{
		keymap_t *k = &keymap[i];

		down = IsKeyDown(k->key);
		if (k->tapped && !k->down && !down)
		{
			/* pressed and released between two polls: report both */
			if (!(k->key == KEY_ENTER && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))))
			{
				PushKey(k->scan, k->ext, 1);
				PushKey(k->scan, k->ext, 0);
			}
		}
		else if (down != k->down)
		{
			if (k->key == KEY_ENTER && down && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))
			{
				RL_ToggleFullscreen();		/* Alt+Enter never reaches the game */
				swallow_enter = 1;
			}
			else if (k->key == KEY_ENTER && !down && swallow_enter)
				swallow_enter = 0;
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
	Vector2 d = GetMouseDelta();

	mouse_dx += (int)lroundf(d.x);
	mouse_dy += (int)lroundf(d.y);
}

void RL_SetMouseGrab (int on)
{
	if (!window_open || on == mouse_grab)
		return;
	mouse_grab = on;
	if (on)
		DisableCursor();
	else
		EnableCursor();
	mouse_dx = mouse_dy = 0;
	(void)GetMouseDelta();
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
	if (!window_open)
		return;
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
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(width, height, title);
	if (!IsWindowReady())
		return 0;
	SetWindowMinSize(RL_FRAME_W, RL_FRAME_H);
	SetExitKey(KEY_NULL);		/* Esc belongs to the game; closing the window quits */
	window_open = 1;

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
	int		w, h;
	float	s, dw, dh;

	if (!window_open)
		return;

	SyncKeys();					/* before EndDrawing() resets the key queue */

	UpdateTexture(frame_tex, rgba);
	w = GetScreenWidth();
	h = GetScreenHeight();
	EnsureBigTarget(w, h);

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

	SyncKeys();
	PollMouse();
	if (WindowShouldClose())
		quit_requested = 1;
}

void RL_ToggleFullscreen (void)
{
	if (window_open)
		ToggleBorderlessWindowed();
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
