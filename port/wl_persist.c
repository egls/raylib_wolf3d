/*
 * wl_persist.c - config/save file location and little-endian streams
 * (see wl_persist.h).
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "WL_DEF.H"
#include "wl_persist.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/em_asm.h>
#endif

#define WL_APPDIR	"raylib_wolf3d"

static int MakeDir (const char *path)
{
	return mkdir(path, 0755) == 0 || errno == EEXIST;
}

const char *WL_UserFile (const char *name)
{
	static char	dir[1024], path[1100];
	const char	*base;

	if (!dir[0])
	{
		base = getenv("XDG_CONFIG_HOME");
		if (base && base[0] == '/')
			snprintf(dir, sizeof(dir), "%s", base);
		else
		{
			base = getenv("HOME");
			snprintf(dir, sizeof(dir), "%s/.config", base && *base ? base : ".");
		}
		MakeDir(dir);
		strncat(dir, "/" WL_APPDIR, sizeof(dir) - strlen(dir) - 1);
		if (!MakeDir(dir))
			fprintf(stderr, "raylib_wolf3d: can't create %s: %s\n", dir, strerror(errno));
	}
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	return path;
}

void WL_UserFilesChanged (void)
{
#ifdef __EMSCRIPTEN__
	// web/shell.html: the directory is on IDBFS, FS.syncfs() writes it out
	EM_ASM({ if (Module.wolfSyncFS) Module.wolfSyncFS(); });
#endif
}

/* ---- streams ------------------------------------------------------------ */

void WS_Init (wl_stream *s)
{
	memset(s, 0, sizeof(*s));
}

void WS_Free (wl_stream *s)
{
	free(s->buf);
	WS_Init(s);
}

void WS_WBytes (wl_stream *s, const void *p, size_t n)
{
	if (s->len + n > s->cap)
	{
		size_t	cap = s->cap ? s->cap : 4096;
		uint8_t	*nb;

		while (cap < s->len + n)
			cap *= 2;
		nb = realloc(s->buf, cap);
		if (!nb)
			Quit("Out of memory writing a file");
		s->buf = nb;
		s->cap = cap;
	}
	memcpy(s->buf + s->len, p, n);
	s->len += n;
}

void WS_W8 (wl_stream *s, uint32_t v)
{
	uint8_t	b = (uint8_t)v;

	WS_WBytes(s, &b, 1);
}

void WS_W16 (wl_stream *s, uint32_t v)
{
	uint8_t	b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };

	WS_WBytes(s, b, 2);
}

void WS_W32 (wl_stream *s, uint32_t v)
{
	uint8_t	b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };

	WS_WBytes(s, b, 4);
}

void WS_RBytes (wl_stream *s, void *p, size_t n)
{
	if (s->bad || n > s->len - s->pos)
	{
		s->bad = 1;
		memset(p, 0, n);
		return;
	}
	memcpy(p, s->buf + s->pos, n);
	s->pos += n;
}

uint32_t WS_R8 (wl_stream *s)
{
	uint8_t	b[1];

	WS_RBytes(s, b, 1);
	return b[0];
}

uint32_t WS_R16 (wl_stream *s)
{
	uint8_t	b[2];

	WS_RBytes(s, b, 2);
	return b[0] | (uint32_t)b[1] << 8;
}

uint32_t WS_R32 (wl_stream *s)
{
	uint8_t	b[4];

	WS_RBytes(s, b, 4);
	return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

int WS_WriteFile (int fd, const wl_stream *s)
{
	size_t	done = 0;
	ssize_t	n;

	while (done < s->len)
	{
		n = write(fd, s->buf + done, s->len - done);
		if (n <= 0)
			return 0;
		done += (size_t)n;
	}
	return 1;
}

int WS_ReadFile (int fd, wl_stream *s)
{
	uint8_t	chunk[8192];
	ssize_t	n;

	WS_Free(s);
	while ((n = read(fd, chunk, sizeof(chunk))) > 0)
		WS_WBytes(s, chunk, (size_t)n);
	return n == 0;
}

uint32_t WS_Checksum (const wl_stream *s, size_t from, size_t to)
{
	uint32_t	h = 2166136261u;		// FNV-1a

	for (; from < to && from < s->len; from++)
	{
		h ^= s->buf[from];
		h *= 16777619u;
	}
	return h;
}

/* ---- states -------------------------------------------------------------- */

int WL_StateIndex (const struct statestruct *state)
{
	int	i;

	if (!state)
		return -1;
	for (i = 0; i < wl_numstates; i++)
		if (wl_states[i] == state)
			return i;
	Quit("Save game: actor state missing from the state table (rerun scripts/gen_states.py)");
	return -1;
}

struct statestruct *WL_StateFromIndex (int index)
{
	if (index < 0 || index >= wl_numstates)
		return NULL;
	return wl_states[index];
}
