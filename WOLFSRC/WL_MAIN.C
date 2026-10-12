// WL_MAIN.C

#include "WL_DEF.H"
#include "wl_persist.h"	// WOLF-7: config/save file location and format
#pragma hdrstop


/*
=============================================================================

						   WOLFENSTEIN 3-D

					  An Id Software production

						   by John Carmack

=============================================================================
*/

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/


#define FOCALLENGTH     (0x5700l)               // in global coordinates
#define VIEWGLOBAL      0x10000                 // globals visable flush to wall

#define VIEWWIDTH       256                     // size of view window
#define VIEWHEIGHT      144

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

char            str[80],str2[20];
int				tedlevelnum;
boolean         tedlevel;
boolean         nospr;
boolean         IsA386;
int                     dirangle[9] = {0,ANGLES/8,2*ANGLES/8,3*ANGLES/8,4*ANGLES/8,
	5*ANGLES/8,6*ANGLES/8,7*ANGLES/8,ANGLES};

//
// proejection variables
//
fixed           focallength;
unsigned        screenofs;
int             viewwidth;
int             viewheight;
int             centerx;
int             shootdelta;                     // pixels away from centerx a target can be
fixed           scale,maxslope;
int32_t         heightnumerator;
int                     minheightdiv;


void            Quit (char *error);

boolean         startgame,loadedgame,virtualreality;
int             mouseadjustment;

char	configname[13]="CONFIG.";


/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/


/*
====================
=
= ReadConfig
=
====================
*/

// WOLF-7: the config is written field by field, little-endian, with fixed
// sizes (see port/wl_persist.h), to $XDG_CONFIG_HOME/raylib_wolf3d/.

static void WriteInts (wl_stream *s, int *v, int n)
{
	while (n--)
		WS_W16 (s,(word)*v++);
}

static void ReadInts (wl_stream *s, int *v, int n)
{
	while (n--)
		*v++ = WS_RS16 (s);
}

static boolean ReadConfigFile (SDMode *sd, SMMode *sm, SDSMode *sds)
{
	wl_stream	s;
	HighScore	scores[MaxScores];
	char		magic[8];
	int			file,i,n,ok,v[4+4+NUMBUTTONS+4+4+2];	// 4 flags, then the ints below

	if ( (file = open(WL_UserFile(configname),O_BINARY | O_RDONLY)) == -1)
		return false;
	WS_Init (&s);
	ok = WS_ReadFile (file,&s);
	close(file);

	WS_RBytes (&s,magic,sizeof(magic));
	ok = ok && !memcmp(magic,WL_CONFIG_MAGIC,sizeof(magic))
		&& WS_R16 (&s) == WL_CONFIG_VERSION;

	for (i=0;i<MaxScores;i++)
	{
		WS_RBytes (&s,scores[i].name,MaxHighName+1);
		scores[i].name[MaxHighName] = 0;
		scores[i].score = WS_RS32 (&s);
		scores[i].completed = WS_R16 (&s);
		scores[i].episode = WS_R16 (&s);
	}
	*sd = WS_R16 (&s);
	*sm = WS_R16 (&s);
	*sds = WS_R16 (&s);
	v[0] = WS_R8 (&s);			// mouseenabled
	v[1] = WS_R8 (&s);			// joystickenabled
	v[2] = WS_R8 (&s);			// joypadenabled
	v[3] = WS_R8 (&s);			// joystickprogressive
	i = WS_RS16 (&s);			// joystickport
	ReadInts (&s,v+4,4+NUMBUTTONS+4+4+2);

	ok = ok && !s.bad
		&& *sd <= sdm_AdLib && *sm <= smm_AdLib && *sds <= sds_SoundBlaster
		&& i >= 0 && i <= 1;
	// scan codes index Keyboard[], button mappings index buttonstate[]
	for (n=4;n<8+NUMBUTTONS;n++)
		ok = ok && v[n] >= 0 && v[n] < NumCodes;
	for (n=8+NUMBUTTONS;n<16+NUMBUTTONS;n++)
		ok = ok && v[n] >= bt_nobutton && v[n] < NUMBUTTONS;
	WS_Free (&s);
	if (!ok)
	{
		fprintf (stderr,"raylib_wolf3d: ignoring invalid %s\n",WL_UserFile(configname));
		return false;
	}

	memcpy (Scores,scores,sizeof(scores));
	mouseenabled = v[0];
	joystickenabled = v[1];
	joypadenabled = v[2];
	joystickprogressive = v[3];
	joystickport = i;
	memcpy (dirscan,v+4,sizeof(dirscan));
	memcpy (buttonscan,v+8,sizeof(buttonscan));
	memcpy (buttonmouse,v+8+NUMBUTTONS,sizeof(buttonmouse));
	memcpy (buttonjoy,v+12+NUMBUTTONS,sizeof(buttonjoy));
	viewsize = v[16+NUMBUTTONS];
	mouseadjustment = v[17+NUMBUTTONS];
	if (viewsize < 4 || viewsize > 19)
		viewsize = 15;
	if (mouseadjustment < 0 || mouseadjustment > 9)
		mouseadjustment = 5;
	return true;
}

/* Silent sessions keep loaded preferences, while runtime modes remain Off. */
static SDMode configSoundMode = sdm_AdLib;
static SMMode configMusicMode = smm_AdLib;
static SDSMode configDigiMode = sds_SoundBlaster;

void ReadConfig(void)
{
	SDMode          sd;
	SMMode          sm;
	SDSMode         sds;


	if (ReadConfigFile (&sd,&sm,&sds))
	{
	//
	// valid config file
	//

		configSoundMode = sd;
		configMusicMode = sm;
		configDigiMode = sds;

		if (sd == sdm_AdLib && !AdLibPresent && !SoundBlasterPresent)
		{
			sd = sdm_PC;
			sd = smm_Off;
		}

		if ((sds == sds_SoundBlaster && !SoundBlasterPresent) ||
			(sds == sds_SoundSource && !SoundSourcePresent))
			sds = sds_Off;

		if (!MousePresent)
			mouseenabled = false;
		if (!JoysPresent[joystickport])
			joystickenabled = false;

		MainMenu[6].active=1;
		MainItems.curpos=0;
	}
	else
	{
	//
	// no config file, so select by hardware
	//
		configSoundMode = sdm_AdLib;
		configMusicMode = smm_AdLib;
		configDigiMode = sds_SoundBlaster;
		if (SoundBlasterPresent || AdLibPresent)
		{
			sd = sdm_AdLib;
			sm = smm_AdLib;
		}
		else
		{
			sd = sdm_PC;
			sm = smm_Off;
		}

		if (SoundBlasterPresent)
			sds = sds_SoundBlaster;
		else if (SoundSourcePresent)
			sds = sds_SoundSource;
		else
			sds = sds_Off;

		if (MousePresent)
			mouseenabled = true;

		joystickenabled = false;
		joypadenabled = false;
		joystickport = 0;
		joystickprogressive = false;

		viewsize = 15;
		mouseadjustment=5;
	}

	if (SD_IsSilent())
	{
		sd = configSoundMode;
		sm = configMusicMode;
		sds = configDigiMode;
	}

	SD_SetMusicMode (sm);
	SD_SetSoundMode (sd);
	SD_SetDigiDevice (sds);

}


/*
====================
=
= WriteConfig
=
====================
*/

void WriteConfig(void)
{
	int                     file,i;
	wl_stream		s;

	WS_Init (&s);
	WS_WBytes (&s,WL_CONFIG_MAGIC,8);
	WS_W16 (&s,WL_CONFIG_VERSION);

	for (i=0;i<MaxScores;i++)
	{
		WS_WBytes (&s,Scores[i].name,MaxHighName+1);
		WS_W32 (&s,(longword)Scores[i].score);
		WS_W16 (&s,Scores[i].completed);
		WS_W16 (&s,Scores[i].episode);
	}

	WS_W16 (&s,SD_IsSilent() ? configSoundMode : SoundMode);
	WS_W16 (&s,SD_IsSilent() ? configMusicMode : MusicMode);
	WS_W16 (&s,SD_IsSilent() ? configDigiMode : DigiMode);

	WS_W8 (&s,mouseenabled);
	WS_W8 (&s,joystickenabled);
	WS_W8 (&s,joypadenabled);
	WS_W8 (&s,joystickprogressive);
	WS_W16 (&s,joystickport);

	WriteInts (&s,dirscan,4);
	WriteInts (&s,buttonscan,NUMBUTTONS);
	WriteInts (&s,buttonmouse,4);
	WriteInts (&s,buttonjoy,4);

	WriteInts (&s,&viewsize,1);
	WriteInts (&s,&mouseadjustment,1);

	file = open(WL_UserFile(configname),O_CREAT | O_TRUNC | O_BINARY | O_WRONLY,0644);

	if (file != -1)
	{
		if (!WS_WriteFile (file,&s))
			fprintf (stderr,"raylib_wolf3d: could not write %s\n",WL_UserFile(configname));
		close(file);
		WL_UserFilesChanged ();	// WOLF-43
	}
	WS_Free (&s);
}


//===========================================================================


/*
========================
=
= Patch386
=
= Patch ldiv to use 32 bit instructions
=
========================
*/

char    *JHParmStrings[] = {"no386",nil};
void Patch386 (void)
{
extern void far jabhack2(void);
extern int far  CheckIs386(void);

	int     i;

	for (i = 1;i < _argc;i++)
		if (US_CheckParm(_argv[i],JHParmStrings) == 0)
		{
			IsA386 = false;
			return;
		}

	if (CheckIs386())
	{
		IsA386 = true;
		jabhack2();
	}
	else
		IsA386 = false;
}

//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame (int difficulty,int episode)
{
	memset (&gamestate,0,sizeof(gamestate));
	gamestate.difficulty = difficulty;
	gamestate.weapon = gamestate.bestweapon
		= gamestate.chosenweapon = wp_pistol;
	gamestate.health = 100;
	gamestate.ammo = STARTAMMO;
	gamestate.lives = 3;
	gamestate.nextextra = EXTRAPOINTS;
	gamestate.episode=episode;

	startgame = true;
}

//===========================================================================

void DiskFlopAnim(int x,int y)
{
 static char which=0;
 if (!x && !y)
   return;
 VWB_DrawPic(x,y,C_DISKLOADING1PIC+which);
 VW_UpdateScreen();
 which^=1;
}


long DoChecksum(byte far *source,unsigned size,long checksum)
{
 unsigned i;

 for (i=0;i<size-1;i++)
   checksum += source[i]^source[i+1];

 return checksum;
}


/*
==================
=
= SaveTheGame
=
= WOLF-7: written field by field, little-endian (see port/wl_persist.h).
= Pointers are stored as indices: actor states into the state table,
= actorat actors by their objlist slot, statics by their position.  The
= file is the 32-byte name written by the menu, then:
=
=   magic, version, gamestate, LevelRatios, tilemap, actorat, areaconnect,
=   areabyplayer, actors, laststatobj, statobjlist, doorposition,
=   doorobjlist, pushwall state, FNV-1a checksum of everything before it
=
==================
*/

#define SAVE_ACTOR		0x8000		// actorat: objlist slot follows in the low bits

static void SaveActor (wl_stream *s, objtype *ob)
{
	WS_W8 (s,ob->active);
	WS_W16 (s,ob->ticcount);
	WS_W16 (s,ob->obclass);
	WS_W16 (s,WL_StateIndex (ob->state));
	WS_W8 (s,ob->flags);
	WS_W32 (s,(longword)ob->distance);
	WS_W16 (s,ob->dir);
	WS_W32 (s,ob->x);
	WS_W32 (s,ob->y);
	WS_W16 (s,ob->tilex);
	WS_W16 (s,ob->tiley);
	WS_W8 (s,ob->areanumber);
	WS_W16 (s,ob->viewx);
	WS_W16 (s,ob->viewheight);
	WS_W32 (s,ob->transx);
	WS_W32 (s,ob->transy);
	WS_W16 (s,ob->angle);
	WS_W16 (s,ob->hitpoints);
	WS_W32 (s,(longword)ob->speed);
	WS_W16 (s,ob->temp1);
	WS_W16 (s,ob->temp2);
	WS_W16 (s,ob->temp3);
	WS_W16 (s,ob - objlist);	// slot, for actorat
}

static int LoadActor (wl_stream *s, objtype *ob)
{
	int	state;

	ob->active = (signed char)WS_R8 (s);
	ob->ticcount = WS_RS16 (s);
	ob->obclass = WS_R16 (s);
	state = WS_RS16 (s);
	ob->state = WL_StateFromIndex (state);
	ob->flags = WS_R8 (s);
	ob->distance = WS_RS32 (s);
	ob->dir = WS_R16 (s);
	ob->x = WS_RS32 (s);
	ob->y = WS_RS32 (s);
	ob->tilex = WS_R16 (s);
	ob->tiley = WS_R16 (s);
	ob->areanumber = WS_R8 (s);
	ob->viewx = WS_RS16 (s);
	ob->viewheight = WS_R16 (s);
	ob->transx = WS_RS32 (s);
	ob->transy = WS_RS32 (s);
	ob->angle = WS_RS16 (s);
	ob->hitpoints = WS_RS16 (s);
	ob->speed = WS_RS32 (s);
	ob->temp1 = WS_RS16 (s);
	ob->temp2 = WS_RS16 (s);
	ob->temp3 = WS_RS16 (s);
	if (ob->tilex >= MAPSIZE || ob->tiley >= MAPSIZE || (state >= 0 && !ob->state))
		s->bad = 1;
	return WS_R16 (s);			// the slot it had when saved
}

static void SaveGameState (wl_stream *s)
{
	int	i;

	WS_WBytes (s,WL_SAVE_MAGIC,8);
	WS_W16 (s,WL_SAVE_VERSION);

	WS_W32 (s,gamestate.difficulty);	WS_W32 (s,gamestate.mapon);
	WS_W32 (s,gamestate.oldscore);		WS_W32 (s,gamestate.score);
	WS_W32 (s,gamestate.nextextra);		WS_W32 (s,gamestate.lives);
	WS_W32 (s,gamestate.health);		WS_W32 (s,gamestate.ammo);
	WS_W32 (s,gamestate.keys);			WS_W32 (s,gamestate.bestweapon);
	WS_W32 (s,gamestate.weapon);		WS_W32 (s,gamestate.chosenweapon);
	WS_W32 (s,gamestate.faceframe);		WS_W32 (s,gamestate.attackframe);
	WS_W32 (s,gamestate.attackcount);	WS_W32 (s,gamestate.weaponframe);
	WS_W32 (s,gamestate.episode);		WS_W32 (s,gamestate.secretcount);
	WS_W32 (s,gamestate.treasurecount);	WS_W32 (s,gamestate.killcount);
	WS_W32 (s,gamestate.secrettotal);	WS_W32 (s,gamestate.treasuretotal);
	WS_W32 (s,gamestate.killtotal);		WS_W32 (s,gamestate.TimeCount);
	WS_W32 (s,gamestate.killx);			WS_W32 (s,gamestate.killy);
	WS_W32 (s,gamestate.victoryflag);

	for (i=0;i<8;i++)
	{
		WS_W32 (s,LevelRatios[i].kill);
		WS_W32 (s,LevelRatios[i].secret);
		WS_W32 (s,LevelRatios[i].treasure);
		WS_W32 (s,LevelRatios[i].time);
	}
}

static void LoadGameState (wl_stream *s)
{
	int	i;

	gamestate.difficulty = WS_RS32 (s);		gamestate.mapon = WS_RS32 (s);
	gamestate.oldscore = WS_RS32 (s);		gamestate.score = WS_RS32 (s);
	gamestate.nextextra = WS_RS32 (s);		gamestate.lives = WS_RS32 (s);
	gamestate.health = WS_RS32 (s);			gamestate.ammo = WS_RS32 (s);
	gamestate.keys = WS_RS32 (s);			gamestate.bestweapon = WS_RS32 (s);
	gamestate.weapon = WS_RS32 (s);			gamestate.chosenweapon = WS_RS32 (s);
	gamestate.faceframe = WS_RS32 (s);		gamestate.attackframe = WS_RS32 (s);
	gamestate.attackcount = WS_RS32 (s);	gamestate.weaponframe = WS_RS32 (s);
	gamestate.episode = WS_RS32 (s);		gamestate.secretcount = WS_RS32 (s);
	gamestate.treasurecount = WS_RS32 (s);	gamestate.killcount = WS_RS32 (s);
	gamestate.secrettotal = WS_RS32 (s);	gamestate.treasuretotal = WS_RS32 (s);
	gamestate.killtotal = WS_RS32 (s);		gamestate.TimeCount = WS_RS32 (s);
	gamestate.killx = WS_RS32 (s);			gamestate.killy = WS_RS32 (s);
	gamestate.victoryflag = WS_RS32 (s);

	for (i=0;i<8;i++)
	{
		LevelRatios[i].kill = WS_RS32 (s);
		LevelRatios[i].secret = WS_RS32 (s);
		LevelRatios[i].treasure = WS_RS32 (s);
		LevelRatios[i].time = WS_RS32 (s);
	}
}

boolean SaveTheGame(int file,int x,int y)
{
	wl_stream	s;
	objtype		*ob;
	statobj_t	*st;
	doorobj_t	*door;
	uintptr_t	v;
	int			i,n;
	boolean		ok;

	WS_Init (&s);

	DiskFlopAnim(x,y);
	SaveGameState (&s);

	DiskFlopAnim(x,y);
	WS_WBytes (&s,tilemap,sizeof(tilemap));
	for (i=0;i<MAPSIZE*MAPSIZE;i++)
	{
		v = (uintptr_t)(&actorat[0][0])[i];
		if (v < 256)
			WS_W16 (&s,v);				// empty, wall or door number
		else
			WS_W16 (&s,SAVE_ACTOR | ((objtype *)v - objlist));
	}

	WS_WBytes (&s,areaconnect,sizeof(areaconnect));
	for (i=0;i<NUMAREAS;i++)
		WS_W8 (&s,areabyplayer[i]);

	DiskFlopAnim(x,y);
	for (n=0,ob = player ; ob ; ob=ob->next)
		n++;
	WS_W16 (&s,n);
	for (ob = player ; ob ; ob=ob->next)
		SaveActor (&s,ob);

	DiskFlopAnim(x,y);
	WS_W16 (&s,laststatobj - statobjlist);
	for (st = statobjlist ; st < &statobjlist[MAXSTATS] ; st++)
	{
		WS_W8 (&s,st->tilex);
		WS_W8 (&s,st->tiley);
		WS_W16 (&s,st->shapenum);
		WS_W8 (&s,st->flags);
		WS_W8 (&s,st->itemnumber);
	}

	DiskFlopAnim(x,y);
	for (i=0;i<MAXDOORS;i++)
		WS_W16 (&s,doorposition[i]);
	for (door = doorobjlist ; door < &doorobjlist[MAXDOORS] ; door++)
	{
		WS_W8 (&s,door->tilex);
		WS_W8 (&s,door->tiley);
		WS_W8 (&s,door->vertical);
		WS_W8 (&s,door->lock);
		WS_W8 (&s,door->action);
		WS_W16 (&s,door->ticcount);
	}

	WS_W16 (&s,pwallstate);
	WS_W16 (&s,pwallx);
	WS_W16 (&s,pwally);
	WS_W16 (&s,pwalldir);
	WS_W16 (&s,pwallpos);

	//
	// WRITE OUT CHECKSUM
	//
	WS_W32 (&s,WS_Checksum (&s,0,s.len));

	ok = WS_WriteFile (file,&s);
	WS_Free (&s);
	if (!ok)
	{
	 Message(STR_NOSPACE1"\n"
			 STR_NOSPACE2);
	 return false;
	}

	return(true);
}

//===========================================================================

/*
==================
=
= SaveGameValid
=
= WOLF-7: true if file (positioned after the 32-byte name) holds a save
= game in this port's format with a matching checksum
=
==================
*/

#define RANGE(v,lo,hi)	((v) >= (lo) && (v) <= (hi))

//
// SaveLayoutValid: reads the whole body the way LoadTheGame does, into
// throwaway variables, and checks every value that later indexes an array
// or picks a map, state or sprite.  Changes no game state.
//
static boolean SaveLayoutValid (wl_stream *s)
{
	int		i,n,v,ok = 1;
	int32_t	g[27];

	s->pos = 8+2;

	for (i=0;i<27;i++)
		g[i] = WS_RS32 (s);
	// difficulty, mapon, ..., lives, health, ammo, keys, best/weapon/chosen,
	// faceframe, attackframe, attackcount, weaponframe, episode (see SaveGameState)
	ok &= RANGE(g[0],gd_baby,gd_hard);
	ok &= RANGE(g[16],0,5) && RANGE(g[1],0,9);
	ok &= RANGE(g[16]*10+g[1],0,NUMMAPS-1) && mapheaderseg[g[16]*10+g[1]] != NULL;
	ok &= RANGE(g[5],0,99) && RANGE(g[6],0,100) && RANGE(g[7],0,99);
	ok &= RANGE(g[9],wp_knife,wp_chaingun) && RANGE(g[10],-1,wp_chaingun)
		&& RANGE(g[11],wp_knife,wp_chaingun);
	ok &= RANGE(g[12],0,3) && RANGE(g[13],0,13) && RANGE(g[15],0,4);
	for (i=0;i<8*4;i++)
		WS_R32 (s);							// LevelRatios: any value

	for (i=0;i<MAPSIZE*MAPSIZE;i++)
	{
		v = WS_R8 (s);						// tilemap: doors index doorobjlist
		if (v & 0x80)
			ok &= (v & 0x3f) < MAXDOORS;
	}
	for (i=0;i<MAPSIZE*MAPSIZE;i++)
	{
		v = WS_R16 (s);						// actorat
		if (v & SAVE_ACTOR)
			ok &= (v & ~SAVE_ACTOR) < MAXACTORS;
		else
			ok &= v < 256;
	}
	for (i=0;i<NUMAREAS*NUMAREAS+NUMAREAS;i++)
		WS_R8 (s);							// areaconnect, areabyplayer

	n = WS_R16 (s);
	ok &= RANGE(n,1,MAXACTORS);
	for (i=0;i<n && ok && !s->bad;i++)
	{
		v = (signed char)WS_R8 (s);	ok &= RANGE(v,ac_badobject,ac_allways);
		WS_R16 (s);						// ticcount
		v = WS_R16 (s);				ok &= RANGE(v,nothing,sparkobj);
		v = WS_RS16 (s);			ok &= RANGE(v,0,wl_numstates-1);
		WS_R8 (s);						// flags
		WS_R32 (s);						// distance
		v = WS_R16 (s);				ok &= RANGE(v,east,nodir);
		WS_R32 (s); WS_R32 (s);			// x, y
		v = WS_R16 (s);				ok &= v < MAPSIZE;
		v = WS_R16 (s);				ok &= v < MAPSIZE;
		v = WS_R8 (s);				ok &= v < NUMAREAS;
		WS_R16 (s); WS_R16 (s);			// viewx, viewheight
		WS_R32 (s); WS_R32 (s);			// transx, transy
		v = WS_RS16 (s);			ok &= RANGE(v,0,ANGLES-1);
		WS_R16 (s);						// hitpoints
		WS_R32 (s);						// speed
		WS_R16 (s); WS_R16 (s); WS_R16 (s);	// temp1-3
		v = WS_R16 (s);				ok &= v < MAXACTORS;	// slot
	}

	v = WS_R16 (s);					ok &= v <= MAXSTATS;
	for (i=0;i<MAXSTATS;i++)
	{
		v = WS_R8 (s);				ok &= v < MAPSIZE;
		v = WS_R8 (s);				ok &= v < MAPSIZE;
		v = WS_RS16 (s);			ok &= RANGE(v,-1,PMSoundStart-PMSpriteStart-1);
		WS_R8 (s); WS_R8 (s);			// flags, itemnumber
	}

	for (i=0;i<MAXDOORS;i++)
		WS_R16 (s);							// doorposition
	for (i=0;i<MAXDOORS;i++)
	{
		v = WS_R8 (s);				ok &= v < MAPSIZE;
		v = WS_R8 (s);				ok &= v < MAPSIZE;
		WS_R8 (s); WS_R8 (s);			// vertical, lock
		v = WS_R8 (s);				ok &= v <= dr_closing;
		WS_R16 (s);						// ticcount
	}

	WS_R16 (s);								// pwallstate
	v = WS_R16 (s);					ok &= v < MAPSIZE;
	v = WS_R16 (s);					ok &= v < MAPSIZE;
	v = WS_RS16 (s);				ok &= RANGE(v,0,3);
	v = WS_R16 (s);					ok &= v < 64;

	ok &= !s->bad && s->pos == s->len - 4;	// exactly the body, then the checksum
	s->pos = 0;
	s->bad = 0;
	return ok;
}

static boolean CheckSaveStream (wl_stream *s)
{
	return s->len >= 8+2+4
		&& !memcmp(s->buf,WL_SAVE_MAGIC,8)
		&& (s->buf[8] | s->buf[9]<<8) == WL_SAVE_VERSION
		&& WS_Checksum (s,0,s->len-4) ==
			(s->buf[s->len-4] | s->buf[s->len-3]<<8 |
			(longword)s->buf[s->len-2]<<16 | (longword)s->buf[s->len-1]<<24)
		&& SaveLayoutValid (s);
}

boolean SaveGameValid (int file)
{
	wl_stream	s;
	boolean		ok;

	WS_Init (&s);
	ok = WS_ReadFile (file,&s) && CheckSaveStream (&s);
	WS_Free (&s);
	return ok;
}

/*
==================
=
= LoadTheGame
=
= WOLF-7: returns false, changing nothing, if the file isn't a valid save
= (the DOS version loaded a bad file anyway and docked the player instead)
=
==================
*/

boolean LoadTheGame(int file,int x,int y)
{
	wl_stream	s;
	objtype		*ob;
	statobj_t	*st;
	doorobj_t	*door;
	objtype		*byslot[MAXACTORS];
	word		at[MAPSIZE*MAPSIZE];
	int			i,n,slot;

	WS_Init (&s);
	if (!WS_ReadFile (file,&s) || !CheckSaveStream (&s))
	{
		WS_Free (&s);
		Message(STR_SAVECHT1"\n"
				STR_SAVECHT2"\n"
				STR_SAVECHT3"\n"
				STR_SAVECHT4);
		IN_ClearKeysDown();
		IN_Ack();
		return false;
	}
	s.pos = 8+2;
	s.len -= 4;

	DiskFlopAnim(x,y);
	LoadGameState (&s);

	DiskFlopAnim(x,y);
	SetupGameLevel ();

	DiskFlopAnim(x,y);
	WS_RBytes (&s,tilemap,sizeof(tilemap));
	for (i=0;i<MAPSIZE*MAPSIZE;i++)
		at[i] = WS_R16 (&s);

	WS_RBytes (&s,areaconnect,sizeof(areaconnect));
	for (i=0;i<NUMAREAS;i++)
		areabyplayer[i] = WS_R8 (&s);

	for (i=0;i<MAXACTORS;i++)
		byslot[i] = &objlist[i];		// stale actorat entries keep their slot

	InitActorList ();
	DiskFlopAnim(x,y);
	n = WS_R16 (&s);
	if (n < 1 || n > MAXACTORS)
		s.bad = 1;
	for (i=0;i<n && !s.bad;i++)
	{
		if (i)
			GetNewActor ();
		ob = i ? new : player;
		slot = LoadActor (&s,ob);
		if (slot < MAXACTORS)
			byslot[slot] = ob;
	}

	for (i=0;i<MAPSIZE*MAPSIZE;i++)
	{
		if (at[i] & SAVE_ACTOR)
			(&actorat[0][0])[i] = byslot[(at[i] & ~SAVE_ACTOR) % MAXACTORS];
		else
			(&actorat[0][0])[i] = (objtype *)(uintptr_t)at[i];
	}

	DiskFlopAnim(x,y);
	n = WS_R16 (&s);
	laststatobj = &statobjlist[n <= MAXSTATS ? n : 0];
	for (st = statobjlist ; st < &statobjlist[MAXSTATS] ; st++)
	{
		st->tilex = WS_R8 (&s) % MAPSIZE;
		st->tiley = WS_R8 (&s) % MAPSIZE;
		st->visspot = &spotvis[st->tilex][st->tiley];
		st->shapenum = WS_RS16 (&s);
		st->flags = WS_R8 (&s);
		st->itemnumber = WS_R8 (&s);
	}

	DiskFlopAnim(x,y);
	for (i=0;i<MAXDOORS;i++)
		doorposition[i] = WS_R16 (&s);
	for (door = doorobjlist ; door < &doorobjlist[MAXDOORS] ; door++)
	{
		door->tilex = WS_R8 (&s);
		door->tiley = WS_R8 (&s);
		door->vertical = WS_R8 (&s);
		door->lock = WS_R8 (&s);
		door->action = WS_R8 (&s);
		door->ticcount = WS_RS16 (&s);
	}

	pwallstate = WS_R16 (&s);
	pwallx = WS_R16 (&s);
	pwally = WS_R16 (&s);
	pwalldir = WS_RS16 (&s);
	pwallpos = WS_R16 (&s);

	if (s.bad || s.pos != s.len)		// can't happen: SaveLayoutValid walked the same layout
		Quit ("LoadTheGame: save file doesn't match its own layout");
	WS_Free (&s);

	return true;
}

//===========================================================================

/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId (void)
{
	US_Shutdown ();
	SD_Shutdown ();
	PM_Shutdown ();
	IN_Shutdown ();
	VW_Shutdown ();
	CA_Shutdown ();
	MM_Shutdown ();
}


//===========================================================================

/*
==================
=
= BuildTables
=
= Calculates:
=
= scale                 projection constant
= sintable/costable     overlapping fractional tables
=
==================
*/

const   float   radtoint = (float)FINEANGLES/2/PI;

void BuildTables (void)
{
  int           i;
  float         angle,anglestep;
  double        tang;
  fixed         value;


//
// calculate fine tangents
//

	for (i=0;i<FINEANGLES/8;i++)
	{
		tang = tan( (i+0.5)/radtoint);
		finetangent[i] = tang*TILEGLOBAL;
		finetangent[FINEANGLES/4-1-i] = 1/tang*TILEGLOBAL;
	}

//
// costable overlays sintable with a quarter phase shift
// ANGLES is assumed to be divisable by four
//
// The low word of the value is the fraction, the high bit is the sign bit,
// bits 16-30 should be 0
//

  angle = 0;
  anglestep = PI/2/ANGLEQUAD;
  for (i=0;i<=ANGLEQUAD;i++)
  {
	value=GLOBAL1*sin(angle);
	sintable[i]=
	  sintable[i+ANGLES]=
	  sintable[ANGLES/2-i] = value;
	sintable[ANGLES-i]=
	  sintable[ANGLES/2+i] = value | 0x80000000l;
	angle += anglestep;
  }

}

//===========================================================================


/*
====================
=
= CalcProjection
=
= Uses focallength
=
====================
*/

void CalcProjection (long focal)
{
	int             i;
	long            intang;
	float   angle;
	double  tang;
	double  planedist;
	double  globinhalf;
	int             halfview;
	double  halfangle,facedist;


	focallength = focal;
	facedist = focal+MINDIST;
	halfview = viewwidth/2;                                 // half view in pixels

//
// calculate scale value for vertical height calculations
// and sprite x calculations
//
	scale = halfview*facedist/(VIEWGLOBAL/2);

//
// divide heightnumerator by a posts distance to get the posts height for
// the heightbuffer.  The pixel height is height>>2
//
	heightnumerator = (TILEGLOBAL*scale)>>6;
	minheightdiv = heightnumerator/0x7fff +1;

//
// calculate the angle offset from view angle of each pixel's ray
//

	for (i=0;i<halfview;i++)
	{
	// start 1/2 pixel over, so viewangle bisects two middle pixels
		tang = (long)i*VIEWGLOBAL/viewwidth/facedist;
		angle = atan(tang);
		intang = angle*radtoint;
		pixelangle[halfview-1-i] = intang;
		pixelangle[halfview+i] = -intang;
	}

//
// if a point's abs(y/x) is greater than maxslope, the point is outside
// the view area
//
	maxslope = finetangent[pixelangle[0]];
	maxslope >>= 8;
}



//===========================================================================

/*
===================
=
= SetupWalls
=
= Map tile values to scaled pics
=
===================
*/

void SetupWalls (void)
{
	int     i;

	for (i=1;i<MAXWALLTILES;i++)
	{
		horizwall[i]=(i-1)*2;
		vertwall[i]=(i-1)*2+1;
	}
}

//===========================================================================

/*
==========================
=
= SignonScreen
=
==========================
*/

void SignonScreen (void)                        // VGA version
{
	VL_SetVGAPlaneMode ();
	VL_TestPaletteSet ();
	VL_SetPalette (&gamepal);

	if (!virtualreality)
	{
		VW_SetScreen(0x8000,0);
		VL_MungePic (&introscn,320,200);
		VL_MemToScreen (&introscn,320,200,0,0);
		VW_SetScreen(0,0);
	}

/* Linked-in data has static storage; it is not allocator-owned. */
}


/*
==========================
=
= FinishSignon
=
==========================
*/

void FinishSignon (void)
{

#ifndef SPEAR
	VW_Bar (0,189,300,11,vl_vram[0]);	// WOLF-7: was peekb(0xa000,0), pixel 0 of plane 0
	WindowX = 0;
	WindowW = 320;
	PrintY = 190;

	#ifndef JAPAN
	SETFONTCOLOR(14,4);

	#ifdef SPANISH
	US_CPrint ("Oprima una tecla");
	#else
	US_CPrint ("Press a key");
	#endif

	#endif

	if (!NoWait)
		IN_Ack ();

	#ifndef JAPAN
	VW_Bar (0,189,300,11,vl_vram[0]);	// WOLF-7: was peekb(0xa000,0), pixel 0 of plane 0

	PrintY = 190;
	SETFONTCOLOR(10,4);

	#ifdef SPANISH
	US_CPrint ("pensando...");
	#else
	US_CPrint ("Working...");
	#endif

	#endif

	SETFONTCOLOR(0,15);
#else
	if (!NoWait)
		VW_WaitVBL(3*70);
#endif
}

//===========================================================================

/*
=================
=
= MS_CheckParm
=
=================
*/

boolean MS_CheckParm (char far *check)
{
	int             i;
	char    *parm;

	for (i = 1;i<_argc;i++)
	{
		parm = _argv[i];

		while ( !isalpha(*parm) )       // skip - / \ etc.. in front of parm
			if (!*parm++)
				break;                          // hit end of string without an alphanum

		if ( !_fstricmp(check,parm) )
			return true;
	}

	return false;
}

//===========================================================================

/*
=====================
=
= InitDigiMap
=
=====================
*/

static  int     wolfdigimap[] =
		{
			// These first sounds are in the upload version
#ifndef SPEAR
			HALTSND,                0,
			DOGBARKSND,             1,
			CLOSEDOORSND,           2,
			OPENDOORSND,            3,
			ATKMACHINEGUNSND,       4,
			ATKPISTOLSND,           5,
			ATKGATLINGSND,          6,
			SCHUTZADSND,            7,
			GUTENTAGSND,            8,
			MUTTISND,               9,
			BOSSFIRESND,            10,
			SSFIRESND,              11,
			DEATHSCREAM1SND,        12,
			DEATHSCREAM2SND,        13,
			DEATHSCREAM3SND,        13,
			TAKEDAMAGESND,          14,
			PUSHWALLSND,            15,

			LEBENSND,               20,
			NAZIFIRESND,            21,
			SLURPIESND,             22,

			YEAHSND,				32,

#ifndef UPLOAD
			// These are in all other episodes
			DOGDEATHSND,            16,
			AHHHGSND,               17,
			DIESND,                 18,
			EVASND,                 19,

			TOT_HUNDSND,            23,
			MEINGOTTSND,            24,
			SCHABBSHASND,           25,
			HITLERHASND,            26,
			SPIONSND,               27,
			NEINSOVASSND,           28,
			DOGATTACKSND,           29,
			LEVELDONESND,           30,
			MECHSTEPSND,			31,

			SCHEISTSND,				33,
			DEATHSCREAM4SND,		34,		// AIIEEE
			DEATHSCREAM5SND,		35,		// DEE-DEE
			DONNERSND,				36,		// EPISODE 4 BOSS DIE
			EINESND,				37,		// EPISODE 4 BOSS SIGHTING
			ERLAUBENSND,			38,		// EPISODE 6 BOSS SIGHTING
			DEATHSCREAM6SND,		39,		// FART
			DEATHSCREAM7SND,		40,		// GASP
			DEATHSCREAM8SND,		41,		// GUH-BOY!
			DEATHSCREAM9SND,		42,		// AH GEEZ!
			KEINSND,				43,		// EPISODE 5 BOSS SIGHTING
			MEINSND,				44,		// EPISODE 6 BOSS DIE
			ROSESND,				45,		// EPISODE 5 BOSS DIE

#endif
#else
//
// SPEAR OF DESTINY DIGISOUNDS
//
			HALTSND,                0,
			CLOSEDOORSND,           2,
			OPENDOORSND,            3,
			ATKMACHINEGUNSND,       4,
			ATKPISTOLSND,           5,
			ATKGATLINGSND,          6,
			SCHUTZADSND,            7,
			BOSSFIRESND,            8,
			SSFIRESND,              9,
			DEATHSCREAM1SND,        10,
			DEATHSCREAM2SND,        11,
			TAKEDAMAGESND,          12,
			PUSHWALLSND,            13,
			AHHHGSND,               15,
			LEBENSND,               16,
			NAZIFIRESND,            17,
			SLURPIESND,             18,
			LEVELDONESND,           22,
			DEATHSCREAM4SND,		23,		// AIIEEE
			DEATHSCREAM3SND,        23,		// DOUBLY-MAPPED!!!
			DEATHSCREAM5SND,		24,		// DEE-DEE
			DEATHSCREAM6SND,		25,		// FART
			DEATHSCREAM7SND,		26,		// GASP
			DEATHSCREAM8SND,		27,		// GUH-BOY!
			DEATHSCREAM9SND,		28,		// AH GEEZ!
			GETGATLINGSND,			38,		// Got Gat replacement

#ifndef SPEARDEMO
			DOGBARKSND,             1,
			DOGDEATHSND,            14,
			SPIONSND,               19,
			NEINSOVASSND,           20,
			DOGATTACKSND,           21,
			TRANSSIGHTSND,			29,		// Trans Sight
			TRANSDEATHSND,			30,		// Trans Death
			WILHELMSIGHTSND,		31,		// Wilhelm Sight
			WILHELMDEATHSND,		32,		// Wilhelm Death
			UBERDEATHSND,			33,		// Uber Death
			KNIGHTSIGHTSND,			34,		// Death Knight Sight
			KNIGHTDEATHSND,			35,		// Death Knight Death
			ANGELSIGHTSND,			36,		// Angel Sight
			ANGELDEATHSND,			37,		// Angel Death
			GETSPEARSND,			39,		// Got Spear replacement
#endif
#endif
			LASTSOUND
		};


void InitDigiMap (void)
{
	int                     *map;

	for (map = wolfdigimap;*map != LASTSOUND;map += 2)
		DigiMap[map[0]] = map[1];


}


#ifndef SPEAR
CP_iteminfo	MusicItems={CTL_X,CTL_Y,6,0,32};
CP_itemtype far MusicMenu[]=
	{
		{1,"Get Them!",0},
		{1,"Searching",0},
		{1,"P.O.W.",0},
		{1,"Suspense",0},
		{1,"War March",0},
		{1,"Around The Corner!",0},

		{1,"Nazi Anthem",0},
		{1,"Lurking...",0},
		{1,"Going After Hitler",0},
		{1,"Pounding Headache",0},
		{1,"Into the Dungeons",0},
		{1,"Ultimate Conquest",0},

		{1,"Kill the S.O.B.",0},
		{1,"The Nazi Rap",0},
		{1,"Twelfth Hour",0},
		{1,"Zero Hour",0},
		{1,"Ultimate Conquest",0},
		{1,"Wolfpack",0}
	};
#else
CP_iteminfo MusicItems={CTL_X,CTL_Y-20,9,0,32};
CP_itemtype far MusicMenu[]=
   {
		{1,"Funky Colonel Bill",0},
		{1,"Death To The Nazis",0},
		{1,"Tiptoeing Around",0},
		{1,"Is This THE END?",0},
		{1,"Evil Incarnate",0},
		{1,"Jazzin' Them Nazis",0},
		{1,"Puttin' It To The Enemy",0},
		{1,"The SS Gonna Get You",0},
		{1,"Towering Above",0}
	};
#endif

#ifndef SPEARDEMO
void DoJukebox(void)
{
	int which,lastsong=-1;
	unsigned start,songs[]=
		{
#ifndef SPEAR
			GETTHEM_MUS,
			SEARCHN_MUS,
			POW_MUS,
			SUSPENSE_MUS,
			WARMARCH_MUS,
			CORNER_MUS,

			NAZI_OMI_MUS,
			PREGNANT_MUS,
			GOINGAFT_MUS,
			HEADACHE_MUS,
			DUNGEON_MUS,
			ULTIMATE_MUS,

			INTROCW3_MUS,
			NAZI_RAP_MUS,
			TWELFTH_MUS,
			ZEROHOUR_MUS,
			ULTIMATE_MUS,
			PACMAN_MUS
#else
			XFUNKIE_MUS,             // 0
			XDEATH_MUS,              // 2
			XTIPTOE_MUS,             // 4
			XTHEEND_MUS,             // 7
			XEVIL_MUS,               // 17
			XJAZNAZI_MUS,            // 18
			XPUTIT_MUS,              // 21
			XGETYOU_MUS,             // 22
			XTOWER2_MUS              // 23
#endif
		};



	IN_ClearKeysDown();
	if (!AdLibPresent && !SoundBlasterPresent)
		return;


	MenuFadeOut();

#ifndef SPEAR
#ifndef UPLOAD
	/* Select one of the three jukebox pages from the portable 70 Hz clock. */
	start = (TimeCount % 3) * 6;
#else
	start = 0;
#endif
#else
	start = 0;
#endif


	CA_CacheGrChunk (STARTFONT+1);
#ifdef SPEAR
	CacheLump (BACKDROP_LUMP_START,BACKDROP_LUMP_END);
#else
	CacheLump (CONTROLS_LUMP_START,CONTROLS_LUMP_END);
#endif
	CA_LoadAllSounds ();

	fontnumber=1;
	ClearMScreen ();
	VWB_DrawPic(112,184,C_MOUSELBACKPIC);
	DrawStripes (10);
	SETFONTCOLOR (TEXTCOLOR,BKGDCOLOR);

#ifndef SPEAR
	DrawWindow (CTL_X-2,CTL_Y-6,280,13*7,BKGDCOLOR);
#else
	DrawWindow (CTL_X-2,CTL_Y-26,280,13*10,BKGDCOLOR);
#endif

	DrawMenu (&MusicItems,&MusicMenu[start]);

	SETFONTCOLOR (READHCOLOR,BKGDCOLOR);
	PrintY=15;
	WindowX = 0;
	WindowY = 320;
	US_CPrint ("Robert's Jukebox");

	SETFONTCOLOR (TEXTCOLOR,BKGDCOLOR);
	VW_UpdateScreen();
	MenuFadeIn();

	do
	{
		which = HandleMenu(&MusicItems,&MusicMenu[start],NULL);
		if (which>=0)
		{
			if (lastsong >= 0)
				MusicMenu[start+lastsong].active = 1;

			StartCPMusic(songs[start + which]);
			MusicMenu[start+which].active = 2;
			DrawMenu (&MusicItems,&MusicMenu[start]);
			VW_UpdateScreen();
			lastsong = which;
		}
	} while(which>=0);

	MenuFadeOut();
	IN_ClearKeysDown();
#ifdef SPEAR
	UnCacheLump (BACKDROP_LUMP_START,BACKDROP_LUMP_END);
#else
	UnCacheLump (CONTROLS_LUMP_START,CONTROLS_LUMP_END);
#endif
}
#endif


/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

void InitGame (void)
{
	int                     i,x,y;
	unsigned        *blockstart;

	if (MS_CheckParm ("virtual"))
		virtualreality = true;
	else
		virtualreality = false;

	MM_Startup ();                  // so the signon screen can be freed

	SignonScreen ();

	VW_Startup ();
	IN_Startup ();
	PM_Startup ();
	PM_UnlockMainMem ();
	SD_Startup ();
	CA_Startup ();
	US_Startup ();


//
// build some tables
//
	InitDigiMap ();

	for (i=0;i<MAPSIZE;i++)
	{
		nearmapylookup[i] = &tilemap[0][0]+MAPSIZE*i;
		farmapylookup[i] = i*64;
	}

	for (i=0;i<PORTTILESHIGH;i++)
		uwidthtable[i] = UPDATEWIDE*i;

	blockstart = &blockstarts[0];
	for (y=0;y<UPDATEHIGH;y++)
		for (x=0;x<UPDATEWIDE;x++)
			*blockstart++ = SCREENWIDTH*16*y+x*TILEWIDTH;

	updateptr = &update[0];

	bufferofs = 0;
	displayofs = 0;
	ReadConfig ();


//
// HOLDING DOWN 'M' KEY?
//
#ifndef SPEARDEMO
	if (Keyboard[sc_M])
	  DoJukebox();
	else
#endif
//
// draw intro screen stuff
//
	if (!virtualreality)
		IntroScreen ();

//
// load in and lock down some basic chunks
//

	CA_CacheGrChunk(STARTFONT);
	MM_SetLock (&grsegs[STARTFONT],true);

	LoadLatchMem ();
	BuildTables ();          // trig tables
	SetupWalls ();

#if 0
{
int temp,i;
temp = viewsize;
	profilehandle = open("SCALERS.TXT", O_CREAT | O_WRONLY | O_TEXT, S_IREAD | S_IWRITE);
for (i=1;i<20;i++)
	NewViewSize(i);
viewsize = temp;
close(profilehandle);
}
#endif

	NewViewSize (viewsize);


//
// initialize variables
//
	InitRedShifts ();
	if (!virtualreality)
		FinishSignon();

	displayofs = PAGE1START;
	bufferofs = PAGE2START;

	if (virtualreality)
	{
		NoWait = true;
		geninterrupt(0x60);
	}
}

//===========================================================================

/*
==========================
=
= SetViewSize
=
==========================
*/

boolean SetViewSize (unsigned width, unsigned height)
{
	viewwidth = width&~15;                  // must be divisable by 16
	viewheight = height&~1;                 // must be even
	centerx = viewwidth/2-1;
	shootdelta = viewwidth/10;
	screenofs = ((200-STATUSLINES-viewheight)/2*SCREENWIDTH+(320-viewwidth)/8);

//
// calculate trace angles and projection constants
//
	CalcProjection (FOCALLENGTH);

//
// build all needed compiled scalers
//
//	MM_BombOnError (false);
	SetupScaling (viewwidth*1.5);
#if 0
	MM_BombOnError (true);
	if (mmerror)
	{
		Quit ("Can't build scalers!");
		mmerror = false;
		return false;
	}
#endif
	return true;
}


void ShowViewSize (int width)
{
	int     oldwidth,oldheight;

	oldwidth = viewwidth;
	oldheight = viewheight;

	viewwidth = width*16;
	viewheight = width*16*HEIGHTRATIO;
	DrawPlayBorder ();

	viewheight = oldheight;
	viewwidth = oldwidth;
}


void NewViewSize (int width)
{
	CA_UpLevel ();
	MM_SortMem ();
	viewsize = width;
	SetViewSize (width*16,width*16*HEIGHTRATIO);
	CA_DownLevel ();
}



//===========================================================================

/*
==========================
=
= Quit
=
==========================
*/

void Quit (char *error)
{
    /* Print before teardown: CA startup errors must never recurse into CA. */
    int status=error && *error ? 1 : 0;
    if(status) fprintf(stderr,WOLF3D_PROGRAM_NAME ": %s\n",error);
    if(!status) WriteConfig();
    ShutdownId();
    exit(status);
}

//===========================================================================



/*
=====================
=
= DemoLoop
=
=====================
*/

static  char *ParmStrings[] = {"baby","easy","normal","hard",""};

void    DemoLoop (void)
{
	static int LastDemo;
	int     i,level;
	long nsize;
	memptr	nullblock;

//
// check for launch from ted
//
	if (tedlevel)
	{
		NoWait = true;
		NewGame(1,0);

		for (i = 1;i < _argc;i++)
		{
			if ( (level = US_CheckParm(_argv[i],ParmStrings)) != -1)
			{
			 gamestate.difficulty=level;
			 break;
			}
		}

#ifndef SPEAR
		gamestate.episode = tedlevelnum/10;
		gamestate.mapon = tedlevelnum%10;
#else
		gamestate.episode = 0;
		gamestate.mapon = tedlevelnum;
#endif
		GameLoop();
		Quit (NULL);
	}


//
// main game cycle
//


//	nsize = (long)40*1024;
//	MM_GetPtr(&nullblock,nsize);

#ifndef DEMOTEST

	#ifndef UPLOAD

		#ifndef GOODTIMES
		#ifndef SPEAR
		#ifndef JAPAN
		if (!NoWait)
			NonShareware();
		#endif
		#else

			#ifndef GOODTIMES
			#ifndef SPEARDEMO
			CopyProtection();
			#endif
			#endif

		#endif
		#endif
	#endif

	StartCPMusic(INTROSONG);

#ifndef JAPAN
	if (!NoWait)
		PG13 ();
#endif

#endif

	while (1)
	{
		while (!NoWait)
		{
//
// title page
//
			MM_SortMem ();
#ifndef DEMOTEST

#ifdef SPEAR
			CA_CacheGrChunk (TITLEPALETTE);

			CA_CacheGrChunk (TITLE1PIC);
			VWB_DrawPic (0,0,TITLE1PIC);
			UNCACHEGRCHUNK (TITLE1PIC);

			CA_CacheGrChunk (TITLE2PIC);
			VWB_DrawPic (0,80,TITLE2PIC);
			UNCACHEGRCHUNK (TITLE2PIC);
			VW_UpdateScreen ();
			VL_FadeIn(0,255,grsegs[TITLEPALETTE],30);

			UNCACHEGRCHUNK (TITLEPALETTE);
#else
			CA_CacheScreen (TITLEPIC);
			VW_UpdateScreen ();
			VW_FadeIn();
#endif
			if (IN_UserInput(TickBase*15))
				break;
			VW_FadeOut();
//
// credits page
//
			CA_CacheScreen (CREDITSPIC);
			VW_UpdateScreen();
			VW_FadeIn ();
			if (IN_UserInput(TickBase*10))
				break;
			VW_FadeOut ();
//
// high scores
//
			DrawHighScores ();
			VW_UpdateScreen ();
			VW_FadeIn ();

			if (IN_UserInput(TickBase*10))
				break;
#endif
//
// demo
//

			#ifndef SPEARDEMO
			PlayDemo (LastDemo++%4);
			#else
			PlayDemo (0);
			#endif

			if (playstate == ex_abort)
				break;
			StartCPMusic(INTROSONG);
		}

		VW_FadeOut ();

#ifndef SPEAR
		if (Keyboard[sc_Tab] && MS_CheckParm("goobers"))
#else
		if (Keyboard[sc_Tab] && MS_CheckParm("debugmode"))
#endif
			RecordDemo ();
		else
			US_ControlPanel (0);

		if (startgame || loadedgame)
		{
			GameLoop ();
			VW_FadeOut();
			StartCPMusic(INTROSONG);
		}
	}
}


//===========================================================================


/*
==========================
=
= main
=
==========================
*/

char    *nosprtxt[] = {"nospr",nil};

int main (int argc, char *argv[])	// WOLF-2: was void main (void)
{
	int     i;

	WL_PlatformStartup (argc, argv);	// WOLF-2: _argv, data dir, window

#ifdef BETA
	//
	// THIS IS FOR BETA ONLY!
	//
	struct dosdate_t d;

	_dos_getdate(&d);
	if (d.year > YEAR ||
		(d.month >= MONTH && d.day >= DAY))
	{
	 printf("Sorry, BETA-TESTING is over. Thanks for you help.\n");
	 exit(1);
	}
#endif

	CheckForEpisodes();

	Patch386 ();

	if (MS_CheckParm ("demotest") || MS_CheckParm ("savetest"))
		NoWait = true;		// WOLF-7: no "Press a key" on the signon screen

	InitGame ();

	WL_DemoTest ();		// WOLF-7: returns unless -demotest was given

	DemoLoop();

	Quit("Demo loop exited???");
	return 0;
}

