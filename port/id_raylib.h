/*
 * id_raylib.h - plain-C interface to the raylib backend.
 *
 * port/id_raylib.c is the only file that includes raylib.h: raylib's KEY_*
 * enums and its bool usage clash with the id headers (boolean {false,true}).
 * Keep this header free of raylib types so any game file can include it.
 *
 * WOLF-2 scope: window open/close only.  The framebuffer, palette, input and
 * timer entry points arrive with the Stage 2 backend work.
 */
#ifndef ID_RAYLIB_H
#define ID_RAYLIB_H

/* Opens the game window.  Returns 1 on success, 0 on failure. */
int		RL_OpenWindow (int width, int height, const char *title);

/* Presents frames for `ms` milliseconds (window stays responsive). */
void	RL_Hold (int ms);

/* Closes the window if it is open.  Safe to call more than once. */
void	RL_CloseWindow (void);

/* 1 while the window is open. */
int		RL_WindowIsOpen (void);

#endif
