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

#include "ID_HEADS.H"
#include "id_raylib.h"
#include "wl_timer.h"
#include "wl_persist.h"
#include "wl_asset.h"
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#endif

#define WL_WINDOW_W		960		// 4:3, 3x the 320x200 frame; resizable, letterboxed
#define WL_WINDOW_H		720
#define WL_EXIT_NOTPORTED	3

int		_argc;
char	**_argv;

/* Each binary accepts only its compiled data layout. */
static const char *data_files[] =
{
	"AUDIOHED." WOLF3D_DATA_EXT, "AUDIOT." WOLF3D_DATA_EXT, "GAMEMAPS." WOLF3D_DATA_EXT, "MAPHEAD." WOLF3D_DATA_EXT,
	"VGADICT." WOLF3D_DATA_EXT, "VGAGRAPH." WOLF3D_DATA_EXT, "VGAHEAD." WOLF3D_DATA_EXT, "VSWAP." WOLF3D_DATA_EXT
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
	fprintf(stderr, WOLF3D_PROGRAM_NAME ": not ported yet: %s (%s:%d)\n",
		what, slash ? slash + 1 : file, line);
	// WOLF3D_NOTPORTED_HOLD_MS keeps the window up, e.g. for screenshots
	RL_Hold(env_int("WOLF3D_NOTPORTED_HOLD_MS"));
	exit(WL_EXIT_NOTPORTED);
}

/*
 * The data directory is, in order: "-data <dir>" on the command line, the
 * WOLF3D_DATA environment variable, or automatic platform discovery (the
 * current directory on other platforms). The DOS code
 * opens its files by bare name, so we chdir() there.
 */
#ifdef __APPLE__
static int WL_HasData(const char *dir)
{
	char previous[PATH_MAX];
	unsigned i;
	int ok = 1;
	if (!getcwd(previous, sizeof(previous)) || chdir(dir)) return 0;
	for (i = 0; i < sizeof(data_files) / sizeof(data_files[0]); i++)
	{
		struct stat st;
		const char *name = WL_AssetName(data_files[i]);
		if (access(name, R_OK) || stat(name, &st) || !S_ISREG(st.st_mode)) ok = 0;
	}
	if (chdir(previous)) { perror("restore working directory"); exit(2); }
	return ok;
}

static const char *WL_MacDataDir(void)
{
	static char candidate[PATH_MAX];
	char executable[PATH_MAX], resolved[PATH_MAX], *slash;
	uint32_t size = sizeof(executable);
	/* Preserve terminal launches; Finder's cwd is usually unrelated to assets. */
	if (WL_HasData(".")) return ".";
	if (!_NSGetExecutablePath(executable, &size) && realpath(executable, resolved))
	{
		slash = strrchr(resolved, '/');
		if (slash)
		{
			*slash = 0;
			if (WL_HasData(resolved))
			{
				snprintf(candidate, sizeof(candidate), "%s", resolved);
				return candidate;
			}
			/* Resources is a sibling of MacOS only inside an actual bundle. */
			slash = strrchr(resolved, '/');
			if (slash && !strcmp(slash, "/MacOS"))
			{
				*slash = 0;
				slash = strrchr(resolved, '/');
				if (slash && !strcmp(slash, "/Contents") &&
				    strlen(resolved) + sizeof("/Resources") <= sizeof(candidate))
				{
					strcpy(candidate, resolved);
					strcat(candidate, "/Resources");
					if (WL_HasData(candidate)) return candidate;
				}
			}
		}
	}
	/* Data can share the writable Application Support directory. */
	snprintf(candidate, sizeof(candidate), "%s", WL_UserFile(""));
	if (WL_HasData(candidate)) return candidate;
	return ".";
}
#endif

static const char *WL_FindDataDir (int argc, char **argv)
{
	const char	*dir = NULL;
	int			i;

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-data"))
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, WOLF3D_PROGRAM_NAME ": -data needs a directory\n");
				exit(2);
			}
			dir = argv[++i];
		}
	if (!dir)
		dir = getenv("WOLF3D_DATA");
	if (!dir || !*dir)
#ifdef __APPLE__
		dir = WL_MacDataDir();
#else
		dir = ".";
#endif
	return dir;
}

void WL_PlatformStartup (int argc, char **argv)
{
	const char	*dir = WL_FindDataDir(argc, argv);
	unsigned	i;

	_argc = argc;
	_argv = argv;

#if defined(_WIN32) || defined(__APPLE__)
	/* Cache an absolute writable path before switching to the asset directory. */
	(void)WL_UserFile("CONFIG.WL1");
#endif
	if (WL_CHDIR(dir))
	{
		fprintf(stderr, WOLF3D_PROGRAM_NAME ": can't use data directory '%s': %s\n",
			dir, strerror(errno));
		exit(2);
	}
	for (i = 0; i < sizeof(data_files) / sizeof(data_files[0]); i++)
		if (WL_ACCESS(WL_AssetName(data_files[i])))
		{
			fprintf(stderr, WOLF3D_PROGRAM_NAME ": %s not found in '%s'.\n"
				"Use the matching " WOLF3D_DATA_EXT " data set and pass "
				"-data <dir> or set WOLF3D_DATA.\n", data_files[i], dir);
			exit(2);
		}

    /* Headless structural check: no renderer, audio device or game/config writes. */
    for (i = 1; i < (unsigned)argc; i++) {
        if (!strcmp(argv[i], "-datacheck")) {
            unsigned chunk;
            MM_Startup();
            CA_Startup();
            PM_Startup();
            for (chunk = 0; chunk < NUMMAPS; chunk++)
                if (mapheaderseg[chunk]) CA_CacheMap(chunk);
            for (chunk = 0; chunk < NUMCHUNKS; chunk++)
                if (grstarts[chunk] >= 0) CA_CacheGrChunk(chunk);
            for (chunk = 0; chunk < NUMSNDCHUNKS; chunk++)
                CA_CacheAudioChunk(chunk);
            PM_Shutdown();
            CA_Shutdown();
            MM_Shutdown();
            printf("PASS: " WOLF3D_DATA_EXT " structural data check\n");
            exit(0);
        }
    }

	if (!RL_OpenWindow(WL_WINDOW_W, WL_WINDOW_H, "Wolfenstein 3-D (raylib)"))
	{
		fprintf(stderr, WOLF3D_PROGRAM_NAME ": could not open a window\n");
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
