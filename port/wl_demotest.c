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
 */

#include "WL_DEF.H"

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

/* Called from main() after InitGame(); returns only if -demotest is absent. */
void WL_DemoTest (void)
{
	int		i, n, first, last, ok = 1;

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
