/*
 * sd_silent.c - TEMPORARY silent Sound Manager (WOLF-7).
 *
 * WOLFSRC/ID_SD.C still stops at its DOS timer/AdLib/Sound Blaster code,
 * which WOLF-6 is porting.  Until that lands this file stands in for the
 * whole of ID_SD.C (it is built instead of it, see WOLF3D_SILENT_SD in
 * CMakeLists.txt) so the game can reach the menus and play.
 *
 * It behaves like a DOS machine with only a PC speaker and the speaker
 * unplugged: no AdLib, no Sound Blaster, no Sound Source.  Sound modes are
 * accepted and remembered exactly like the original so CA_LoadAllSounds and
 * the sound menu run their normal paths; nothing is ever played and every
 * "is it still playing" query answers no.  Sound never feeds back into game
 * state, so demos play back identically with or without it.
 *
 * Drop this file (and the CMake option) when WOLF-6 is merged.
 */

#include "ID_HEADS.H"

//	Global variables (same set and types as ID_SD.C)
	boolean		SoundSourcePresent,
				AdLibPresent,
				SoundBlasterPresent,SBProPresent,
				NeedsDigitized,NeedsMusic,
				SoundPositioned;
	SDMode		SoundMode;
	SMMode		MusicMode;
	SDSMode		DigiMode;
	_Atomic longword	TimeCount;
	word		HackCount;
	word		*SoundTable;
	boolean		ssIsTandy;
	word		ssPort = 2;
	int			DigiMap[LASTSOUND];
	boolean		DigiPlaying;

static	boolean	SD_Started;

void SD_Startup (void)
{
	if (SD_Started)
		return;
	SoundSourcePresent = AdLibPresent = SoundBlasterPresent = SBProPresent = false;
	SoundMode = sdm_Off;
	MusicMode = smm_Off;
	DigiMode = sds_Off;
	NeedsDigitized = NeedsMusic = false;
	SD_Started = true;
}

void SD_Shutdown (void)
{
	SD_Started = false;
}

boolean SD_SetSoundMode (SDMode mode)
{
	word	tableoffset = 0;

	if (mode == sdm_AdLib)		// no AdLib: same fallback as ID_SD.C
		mode = sdm_PC;
	if (mode == sdm_PC)
		tableoffset = STARTPCSOUNDS;

	if (mode != SoundMode)
	{
		SoundMode = mode;
		SoundTable = (word *)(&audiosegs[tableoffset]);
	}
	NeedsDigitized = false;
	return true;
}

boolean SD_SetMusicMode (SMMode mode)
{
	if (mode != smm_Off)		// music needs an AdLib
		return false;
	MusicMode = mode;
	NeedsMusic = false;
	return true;
}

void SD_Default (boolean gotit,SDMode sd,SMMode sm)
{
	(void)gotit;
	(void)sm;
	if (sd == sdm_AdLib || !gotit)
		sd = sdm_PC;
	if (sd != SoundMode)
		SD_SetSoundMode(sd);
	if (MusicMode != smm_Off)
		SD_SetMusicMode(smm_Off);
}

void SD_SetDigiDevice (SDSMode mode)
{
	(void)mode;
	DigiMode = sds_Off;			// no digitized device
}

void SD_PositionSound (int leftvol,int rightvol)	{ (void)leftvol; (void)rightvol; }
void SD_SetPosition (int leftvol,int rightvol)		{ (void)leftvol; (void)rightvol; }
boolean SD_PlaySound (soundnames sound)				{ (void)sound; return false; }
word SD_SoundPlaying (void)							{ return 0; }
void SD_StopSound (void)							{ DigiPlaying = false; }
void SD_WaitSoundDone (void)						{ }
void SD_StartMusic (MusicGroup far *music)			{ (void)music; }
void SD_MusicOn (void)								{ }
void SD_MusicOff (void)								{ }
void SD_FadeOutMusic (void)							{ }
boolean SD_MusicPlaying (void)						{ return false; }
void SD_SetUserHook (void (*hook)(void))			{ (void)hook; }
void SD_PlayDigitized (word which,int leftpos,int rightpos)	{ (void)which; (void)leftpos; (void)rightpos; }
void SD_StopDigitized (void)						{ DigiPlaying = false; }
void SD_Poll (void)									{ }

//	Used by the debug sound viewer (WL_DEBUG.C) and the menu (WL_MENU.C)
	word		NumDigi;
	word		*DigiList;

void alOut (byte n,byte b)							{ (void)n; (void)b; }
