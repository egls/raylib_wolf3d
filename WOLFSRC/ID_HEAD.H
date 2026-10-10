// ID_HEAD.H


#define	EXTENSION	"WLF"
#define WOLF

#define	TEXTGR	0
#define	CGAGR	1
#define	EGAGR	2
#define	VGAGR	3

#define GRMODE	VGAGR

#include "wl_compat.h"	// WOLF-2: portable compatibility layer
#include "WOLF1VER.H"	// WOLF-2: shareware v1.4 (WL1) build, was VERSION.H

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


void	Quit (char *error);		// defined in user program

