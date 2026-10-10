// ID_GLOB.H


// WOLF-2: Borland/DOS headers replaced by the portable compatibility layer
#include "wl_compat.h"
#define __ID_GLOB__
#include "WOLF1VER.H"	// WOLF-2: shareware v1.4 (WL1) build, was VERSION.H

//--------------------------------------------------------------------------

extern	char		far signon;

#define	introscn	signon

#ifdef JAPAN
#ifdef JAPDEMO
#include "FOREIGN\JAPAN\GFXV_WJ1.H"
#else
#include "FOREIGN\JAPAN\GFXV_WJ6.H"
#endif
#include "AUDIOWL6.H"
#include "MAPSWL6.H"
#else

#ifndef SPEAR

// WOLF-2: the *WL1.H headers in this release predate shareware v1.4 and do
// not match its data.  v1.4 AUDIOHED.WL1 has 288 chunks = AUDIOWL6.H.
// v1.4 VGAHEAD.WL1 has 156 chunks; GFXV_WL6.H (149) is a compile-time
// placeholder until Stage 2 adds a matching header (see PORTING.md).
#include "GFXV_WL6.H"
#include "AUDIOWL6.H"
#include "MAPSWL6.H"

#else

#ifndef SPEARDEMO
#include "GFXV_SOD.H"
#include "AUDIOSOD.H"
#include "MAPSSOD.H"
#else
#include "GFXV_SDM.H"
#include "AUDIOSDM.H"
#include "MAPSSDM.H"
#endif

#endif
#endif
//-----------------


#define GREXT	"VGA"

//
//	ID Engine
//	Types.h - Generic types, #defines, etc.
//	v1.0d1
//

// WOLF-2: fixed-width so 16-bit word / 32-bit longword survive on LP64
typedef	enum	{false,true}	boolean;
typedef	uint8_t				byte;
typedef	uint16_t			word;
typedef	uint32_t			longword;
typedef	byte *					Ptr;

typedef	struct
		{
			int	x,y;
		} Point;
typedef	struct
		{
			Point	ul,lr;
		} Rect;

#define	nil	((void *)0)


#include "ID_MM.H"
#include "ID_PM.H"
#include "ID_CA.H"
#include "ID_VL.H"
#include "ID_VH.H"
#include "ID_IN.H"
#include "ID_SD.H"
#include "ID_US.H"


void	Quit (char *error);		// defined in user program

//
// replacing refresh manager with custom routines
//

#define	PORTTILESWIDE		20      // all drawing takes place inside a
#define	PORTTILESHIGH		13		// non displayed port of this size

#define UPDATEWIDE			PORTTILESWIDE
#define UPDATEHIGH			PORTTILESHIGH

#define	MAXTICS				10
#define DEMOTICS			4

#define	UPDATETERMINATE	0x0301

extern	unsigned	mapwidth,mapheight,tics;
extern	boolean		compatability;

extern	byte		*updateptr;
extern	unsigned	uwidthtable[UPDATEHIGH];
extern	unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];

extern	byte		fontcolor,backcolor;

#define SETFONTCOLOR(f,b) fontcolor=f;backcolor=b;

