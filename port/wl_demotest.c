/*
 * wl_demotest.c - "-demotest N|all": play the built-in demos at full speed
 * and print the final game state, for desync regression checks (WOLF-7).
 *
 * Demo playback is deterministic: every frame takes exactly DEMOTICS tics
 * and the random table restarts at index 0.  So the state at the end of a
 * demo only depends on the game logic, and any change in it (a 64-bit
 * overflow difference, a bad port of a think routine...) shows up as a
 * different hash.  The expected values live in tests/demo_sync.cmake.
 *
 * The demo still renders every frame (the renderer is part of the logic:
 * it sets which actors are visible), but PollControls doesn't wait for the
 * 70 Hz clock (see demotest in WL_PLAY.C).
 *
 * Output, one line per demo:
 *   demotest 0: map=0 frames=... left=0 tics=... end=died hash=xxxxxxxx ...
 * Exit status 0 if every demo used up all of its recorded input, 1 if one
 * ended early (the player died or left the level before the recording did,
 * which is what a desync looks like).
 *
 * "-savetest" (also here) checks the save game format: a save/load round
 * trip of E1M1 after a demo must restore the same state hash, and damaged
 * files (with valid checksums) must be rejected by SaveGameValid without
 * touching the game.
 */

#include <fcntl.h>
#include <unistd.h>
#include "WL_DEF.H"
#include "wl_persist.h"

boolean	demotest;

static uint32_t	hash;

static void H (int32_t v)
{
	int	i;

	for (i = 0; i < 4; i++)				// FNV-1a over the little-endian bytes
	{
		hash ^= (uint32_t)v >> (8*i) & 0xff;
		hash *= 16777619u;
	}
}

static uint32_t StateHash (void)
{
	objtype		*ob;
	doorobj_t	*door;
	int			x, y;

	hash = 2166136261u;

	H(gamestate.mapon);			H(gamestate.difficulty);
	H(gamestate.score);			H(gamestate.lives);
	H(gamestate.health);		H(gamestate.ammo);
	H(gamestate.keys);			H(gamestate.weapon);
	H(gamestate.bestweapon);	H(gamestate.killcount);
	H(gamestate.secretcount);	H(gamestate.treasurecount);
	H(gamestate.TimeCount);

	// every actor, in list order (player first); pointers are not hashed
	for (ob = player; ob; ob = ob->next)
	{
		H(ob->active);	H(ob->obclass);	H(ob->flags);
		H(ob->x);		H(ob->y);		H(ob->tilex);	H(ob->tiley);
		H(ob->angle);	H(ob->dir);		H(ob->hitpoints);
		H(ob->ticcount);	H((int32_t)ob->distance);	H((int32_t)ob->speed);
		H(ob->temp1);	H(ob->temp2);	H(ob->temp3);
		H(ob->state ? ob->state->shapenum : -2);
		H(ob->state ? ob->state->tictime : -2);
	}

	for (door = doorobjlist; door < lastdoorobj; door++)
	{
		H(door->action);	H(door->ticcount);
		H(doorposition[door - doorobjlist]);
	}

	for (y = 0; y < MAPSIZE; y++)		// walls, opened secret doors
		for (x = 0; x < MAPSIZE; x++)
			H(tilemap[x][y]);

	return hash;
}

static const char *EndName (exit_t state)
{
	switch (state)
	{
	case ex_stillplaying:	return "stillplaying";
	case ex_completed:		return "completed";
	case ex_died:			return "died";
	case ex_warped:			return "warped";
	case ex_resetgame:		return "resetgame";
	case ex_loadedgame:		return "loadedgame";
	case ex_victorious:		return "victorious";
	case ex_abort:			return "abort";
	case ex_demodone:		return "demodone";
	case ex_secretlevel:	return "secretlevel";
	}
	return "?";
}

static int RunOne (int n)
{
	exit_t	end;
	long	left;

	PlayDemo(n);
	end = playstate;
	left = (long)(lastdemoptr - demoptr) / 3;	// frames of input not played
	printf("demotest %d: map=%d frames=%ld left=%ld tics=%ld end=%s hash=%08x"
		" x=%d y=%d angle=%d health=%d ammo=%d score=%ld kills=%d/%d\n",
		n, gamestate.mapon, (long)gamestate.TimeCount / DEMOTICS, left, (long)gamestate.TimeCount,
		EndName(end), StateHash(),
		player->x, player->y, player->angle, gamestate.health, gamestate.ammo,
		(long)gamestate.score, gamestate.killcount, gamestate.killtotal);
	fflush(stdout);
	// id recorded each demo until the level ended or the player died, so a
	// demo in sync uses up all of its input; a desync ends it early.
	return left == 0;
}

/* ---- -savetest ----------------------------------------------------------- */

#define SAVETEST_FILE	"SAVETEST.TMP"

static int ReadAll (const char *path, wl_stream *s)
{
	int	fd = open(path, O_RDONLY), ok;

	if (fd == -1)
		return 0;
	ok = WS_ReadFile(fd, s);
	close(fd);
	return ok;
}

/* writes the save body s (with its checksum recomputed) after a 32-byte
 * name and reports whether SaveGameValid accepts it */
static int Accepted (wl_stream *s)
{
	char		name[32] = "savetest";
	uint32_t	sum;
	int			fd, ok;

	if (s->len >= 4)
	{
		sum = WS_Checksum(s, 0, s->len - 4);
		s->buf[s->len-4] = sum;			s->buf[s->len-3] = sum >> 8;
		s->buf[s->len-2] = sum >> 16;	s->buf[s->len-1] = sum >> 24;
	}
	fd = open(WL_UserFile(SAVETEST_FILE), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
		Quit("-savetest: can't write the test file");
	write(fd, name, 32);
	WS_WriteFile(fd, s);
	close(fd);
	fd = open(WL_UserFile(SAVETEST_FILE), O_RDONLY);
	lseek(fd, 32, SEEK_SET);
	ok = SaveGameValid(fd);
	close(fd);
	return ok;
}

static void SaveTest (void)
{
	wl_stream	good, bad;
	uint32_t	before, after;
	size_t		actors;
	int			fd, failed = 0, i;
	struct { const char *what; int expect; } *c, cases[] =
	{
		{"unchanged file", 1},
		{"magic + version + checksum only (14 bytes)", 0},
		{"truncated by 100 bytes", 0},
		{"one trailing byte", 0},
		{"actor count 0", 0},
		{"player state index 9999", 0},
		{"actorat entry 0x7000", 0},
		{"map number 42", 0},
		{"health 1000", 0},
	};

	// a level in progress: the end of demo 0
	PlayDemo(0);
	before = StateHash();

	fd = open(WL_UserFile(SAVETEST_FILE), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	write(fd, "savetest\0", 9);
	lseek(fd, 32, SEEK_SET);
	if (fd == -1 || !SaveTheGame(fd, 0, 0))
		Quit("-savetest: SaveTheGame failed");
	close(fd);

	WS_Init(&good);
	if (!ReadAll(WL_UserFile(SAVETEST_FILE), &good) || good.len < 32 + 14)
		Quit("-savetest: can't read the save back");
	memmove(good.buf, good.buf + 32, good.len - 32);
	good.len -= 32;

	// offset of the actor count: header, gamestate, LevelRatios, tilemap,
	// actorat, areaconnect, areabyplayer
	actors = 8+2 + 27*4 + 8*4*4 + MAPSIZE*MAPSIZE + MAPSIZE*MAPSIZE*2
		+ NUMAREAS*NUMAREAS + NUMAREAS;

	for (i = 0, c = cases; i < (int)(sizeof(cases)/sizeof(cases[0])); i++, c++)
	{
		WS_Init(&bad);
		WS_WBytes(&bad, good.buf, good.len);
		switch (i)
		{
		case 1: memmove(bad.buf + 10, bad.buf + bad.len - 4, 4); bad.len = 14; break;
		case 2: bad.len -= 100; break;
		case 3: WS_W8(&bad, 0); memmove(bad.buf + bad.len - 4, bad.buf + bad.len - 5, 4); break;
		case 4: bad.buf[actors] = bad.buf[actors+1] = 0; break;
		case 5: bad.buf[actors+2+5] = 9999 & 0xff; bad.buf[actors+2+6] = 9999 >> 8; break;
		case 6: bad.buf[10+27*4+8*4*4+MAPSIZE*MAPSIZE] = 0x00;
				bad.buf[10+27*4+8*4*4+MAPSIZE*MAPSIZE+1] = 0x70; break;
		case 7: bad.buf[10+4] = 42; break;
		case 8: bad.buf[10+6*4] = 1000 & 0xff; bad.buf[10+6*4+1] = 1000 >> 8; break;
		}
		if (Accepted(&bad) != c->expect)
		{
			printf("savetest: FAIL %s: %s\n", c->what, c->expect ? "rejected" : "accepted");
			failed = 1;
		}
		else
			printf("savetest: ok   %s: %s\n", c->what, c->expect ? "accepted" : "rejected");
		WS_Free(&bad);
		if (StateHash() != before)
		{
			printf("savetest: FAIL checking \"%s\" changed the game\n", c->what);
			failed = 1;
		}
	}

	// round trip: change the game, load the save, compare
	gamestate.health = 1;
	player->x += 0x10000;
	WS_Init(&bad);
	WS_WBytes(&bad, good.buf, good.len);
	Accepted(&bad);				// rewrites SAVETEST.TMP with the good save
	WS_Free(&bad);
	fd = open(WL_UserFile(SAVETEST_FILE), O_RDONLY);
	lseek(fd, 32, SEEK_SET);
	loadedgame = true;
	if (!LoadTheGame(fd, 0, 0))
		Quit("-savetest: LoadTheGame rejected a good save");
	loadedgame = false;
	close(fd);
	after = StateHash();
	printf("savetest: round trip hash %08x -> %08x %s\n", before, after, before == after ? "ok" : "FAIL");
	failed |= before != after;

	WS_Free(&good);
	unlink(WL_UserFile(SAVETEST_FILE));
	ShutdownId();
	exit(failed);
}

/* Called from main() after InitGame(); returns only if -demotest/-savetest are absent. */
void WL_DemoTest (void)
{
	int		i, n, first, last, ok = 1;

	for (i = 1; i < _argc; i++)
		if (!strcmp(_argv[i], "-savetest"))
		{
			demotest = true;
			SaveTest();
		}

	for (i = 1; i < _argc - 1; i++)
		if (!strcmp(_argv[i], "-demotest"))
			break;
	if (i >= _argc - 1)
		return;

	if (!strcmp(_argv[i+1], "all"))
	{
		first = 0;
		last = 3;
	}
	else
	{
		n = atoi(_argv[i+1]);
		if (n < 0 || n > 3)
			Quit("-demotest: demo number must be 0-3 or all");
		first = last = n;
	}

	// The demos depend on the view size: GunAttack only considers actors
	// within shootdelta (viewwidth/10) pixels of centerx.  -viewsize N
	// overrides the configured size for the test.
	for (i = 1; i < _argc - 1; i++)
		if (!strcmp(_argv[i], "-viewsize"))
		{
			n = atoi(_argv[i+1]);
			if (n < 4 || n > 19)
				Quit("-viewsize: must be 4-19");
			NewViewSize(n);
		}
	printf("demotest: viewsize %d\n", viewsize);

	demotest = true;
	for (n = first; n <= last; n++)
		ok &= RunOne(n);

	ShutdownId();
	exit(ok ? 0 : 1);
}
