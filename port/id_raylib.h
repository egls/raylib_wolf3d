/*
 * id_raylib.h - plain-C interface to the raylib backend.
 *
 * port/id_raylib.c is the only file that includes raylib.h: raylib's KEY_*
 * enums and its bool usage clash with the id headers (boolean {false,true}).
 * Keep this header free of raylib types so any game file can include it.
 *
 * Video:    RL_Present() takes an expanded 320x200 RGBA frame.
 * Keyboard: raylib is poll based, the game is event based (it was a keyboard
 *           ISR).  RL_PumpEvents() polls and queues PC set-1 scan code events;
 *           RL_GetKeyEvent() drains them.
 * Mouse and gamepad: relative mouse motion while captured, gamepad 0 as the
 *           DOS "joystick".
 */
#ifndef ID_RAYLIB_H
#define ID_RAYLIB_H

#define RL_FRAME_W	320
#define RL_FRAME_H	200

/* Opens the game window.  Returns 1 on success, 0 on failure. */
int		RL_OpenWindow (int width, int height, const char *title);

/* Presents frames for `ms` milliseconds (window stays responsive). */
void	RL_Hold (int ms);

/* Closes the window if it is open.  Safe to call more than once. */
void	RL_CloseWindow (void);

/* 1 while the window is open. */
int		RL_WindowIsOpen (void);

/*
 * Draws one RL_FRAME_W x RL_FRAME_H frame (4 bytes per pixel, R,G,B,A in
 * memory order) and swaps buffers.  Scaling is two-step: whole-number nearest
 * enlargement into a render texture, then a smooth scale to the window,
 * letterboxed to 4:3.
 */
void	RL_Present (const unsigned char *rgba);

/* Polls the OS for input without drawing. */
void	RL_PumpEvents (void);

/*
 * Pops one keyboard event.  `scancode` is the PC set-1 make code (0x01-0x7f),
 * `extended` is 1 for keys the PC keyboard sends behind an 0xE0 prefix (arrow
 * keys, right ctrl/alt, ...) and 2 for the Pause key's 0xE1 prefix (only the
 * web touch controls send it), `down` is 1 for press, 0 for release.
 * Returns 0 when the queue is empty.
 */
int		RL_GetKeyEvent (int *scancode, int *extended, int *down);

/* 1 once the user asked to close the window. */
int		RL_QuitRequested (void);

/* Mouse capture (hidden, locked cursor); relative motion accumulates either way. */
void	RL_SetMouseGrab (int on);
void	RL_MouseDelta (int *dx, int *dy);	/* motion since the last call */
int		RL_MouseButtons (void);				/* bit0 left, bit1 right, bit2 middle */

/* Gamepad 0 as a joystick. */
int		RL_JoyPresent (void);
void	RL_JoyAxes (int *x, int *y);		/* -1000..1000 */
int		RL_JoyButtons (void);				/* bit0 = A, bit1 = B/X */

void	RL_ToggleFullscreen (void);

#ifdef __EMSCRIPTEN__
/*
 * Touch controls (WOLF-47).  The page (web/shell.html) draws them and
 * reports them through RL_WebTouch and RL_WebTouchKey in id_raylib.c, the
 * one touch entry point.  Keys the menus read (Esc, Enter, Y, N, the
 * arrows, Pause, typed letters) go into the keyboard event queue as scan
 * codes; play controls are read once per frame by PollControls
 * (WL_PLAY.C), so they work whatever the key bindings and with the mouse
 * and joystick turned off.
 *
 * RL_TouchControls returns the play buttons held now or pressed since the
 * last call, and the move stick in -1000..1000 (x turns, or strafes with
 * RL_TOUCH_STRAFE; y is negative forward) and the turn drag in mouse-like
 * counts since the last call.  Calling it marks the game as being played
 * (the page then shows the play layout rather than the menu one).
 */
#define RL_TOUCH_FIRE		0x01
#define RL_TOUCH_STRAFE		0x02
#define RL_TOUCH_RUN		0x04
#define RL_TOUCH_USE		0x08
#define RL_TOUCH_NEXTWEAPON	0x10
#define RL_TOUCH_PREVWEAPON	0x20

int		RL_TouchControls (int *movex, int *movey, int *turn);

/* 1 when the page shows its touch controls (a phone or ?touch=1). */
int		RL_TouchActive (void);
#endif

/* Mixer runs only on the game thread. Callback consumes copied PCM only. */
int RL_AudioOpen(void (*mixer)(short *samples, unsigned frames));
void RL_AudioPump(void);
void RL_AudioClose(void);

#endif
