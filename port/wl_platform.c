/*
 * wl_platform.c - process startup for the port: argv globals, the game data
 * directory, the window, and the "not ported yet" exit path.
 */
#ifdef _WIN32
#include <direct.h>
/* MinGW aliases this DOS name to its CRT struct; keep the game type. */
#undef diskfree_t
#include <io.h>
#define WL_CHDIR _chdir
#define WL_ACCESS(path) _access((path), 4)
#else
#define WL_CHDIR chdir
#define WL_ACCESS(path) access((path), R_OK)
#endif

#include "wl_compat.h"
#include "id_raylib.h"
#include "wl_timer.h"
#include "wl_persist.h"

#define WL_WINDOW_W		960		// 4:3, 3x the 320x200 frame; resizable, letterboxed
#define WL_WINDOW_H		720
#define WL_EXIT_NOTPORTED	3

int		_argc;
char	**_argv;

/* Files the shareware v1.4 build opens from the data directory. */
static const char *wl1_files[] =
{
	"AUDIOHED.WL1", "AUDIOT.WL1", "GAMEMAPS.WL1", "MAPHEAD.WL1",
	"VGADICT.WL1", "VGAGRAPH.WL1", "VGAHEAD.WL1", "VSWAP.WL1"
};

static int env_int (const char *name)
{
	const char *v = getenv(name);
	return v ? atoi(v) : 0;
}

static void WL_Shutdown (void)
{
	RL_CloseWindow();
}

void WL_NotPorted (const char *what, const char *file, int line)
{
	const char *slash = strrchr(file, '/');
	const char *backslash = strrchr(file, '\\');
	if (backslash && (!slash || backslash > slash))
		slash = backslash;

	fflush(stdout);
	fprintf(stderr, "raylib_wolf3d: not ported yet: %s (%s:%d)\n",
		what, slash ? slash + 1 : file, line);
	// WOLF3D_NOTPORTED_HOLD_MS keeps the window up, e.g. for screenshots
	RL_Hold(env_int("WOLF3D_NOTPORTED_HOLD_MS"));
	exit(WL_EXIT_NOTPORTED);
}

/*
 * The data directory is, in order: "-data <dir>" on the command line, the
 * WOLF3D_DATA environment variable, or the current directory.  The DOS code
 * opens its files by bare name, so we chdir() there.
 */
static const char *WL_FindDataDir (int argc, char **argv)
{
	const char	*dir = NULL;
	int			i;

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-data"))
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "raylib_wolf3d: -data needs a directory\n");
				exit(2);
			}
			dir = argv[++i];
		}
	if (!dir)
		dir = getenv("WOLF3D_DATA");
	if (!dir || !*dir)
		dir = ".";
	return dir;
}

void WL_PlatformStartup (int argc, char **argv)
{
	const char	*dir = WL_FindDataDir(argc, argv);
	unsigned	i;

	_argc = argc;
	_argv = argv;

#ifdef _WIN32
	/* Cache an absolute writable path before switching to the asset directory. */
	(void)WL_UserFile("CONFIG.WL1");
#endif
	if (WL_CHDIR(dir))
	{
		fprintf(stderr, "raylib_wolf3d: can't use data directory '%s': %s\n",
			dir, strerror(errno));
		exit(2);
	}
	for (i = 0; i < sizeof(wl1_files) / sizeof(wl1_files[0]); i++)
		if (WL_ACCESS(wl1_files[i]))
		{
			fprintf(stderr, "raylib_wolf3d: %s not found in '%s'.\n"
				"Get the shareware data with scripts/fetch_shareware.sh and pass "
				"-data <dir> or set WOLF3D_DATA.\n", wl1_files[i], dir);
			exit(2);
		}

	if (!RL_OpenWindow(WL_WINDOW_W, WL_WINDOW_H, "Wolfenstein 3-D (raylib)"))
	{
		fprintf(stderr, "raylib_wolf3d: could not open a window\n");
		exit(2);
	}
	WL_TimerStart();		// 70 Hz TimeCount, replaces the PIT interrupt
	atexit(WL_Shutdown);

	for (i = 1; i + 1 < (unsigned)argc; i++)
		if (!strcmp(argv[i], "-videotest"))
		{
			WL_VideoTest(argv[i + 1]);		// developer harness, see wl_videotest.c
			exit(0);
		}
}
