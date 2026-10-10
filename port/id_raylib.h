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
 * keys, right ctrl/alt, ...), `down` is 1 for press, 0 for release.
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

#endif
