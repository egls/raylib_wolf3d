// ID_VL.C
//
// WOLF-5: the VGA Mode X (planar) video layer, rewritten for the raylib port.
//
// The game keeps its VGA memory model: 64K "bytes" per plane, screen pages and
// latch pictures at plane offsets, bufferofs/displayofs/linewidth in plane
// bytes.  Video memory is now one linear array of 4 * 64K palette indices in
// which plane byte `ofs` of plane `p` lives at vl_vram[4*ofs + p].  A pixel
// (x,y) of the page starting at plane offset `ofs` is therefore
//
//		vl_vram[4*(ofs + ylookup[y]) + x]
//
// and a "latch" (4 pixels, one byte per plane) is 4 consecutive bytes, which
// turns write mode 1 (latch copy) and the map mask into plain memcpy/memset.
// VGAMAPMASK / VGAWRITEMODE / VGAREADMAP are therefore no-ops.
//
// Showing the frame: VL_Refresh() expands the page at the CRTC start address
// through the 256-entry palette and hands it to port/id_raylib.c.

#include <math.h>
#include <string.h>
#include "ID_HEAD.H"
#include "ID_VL.H"
#pragma hdrstop
#include "id_raylib.h"
#include "wl_timer.h"

void IN_PumpEvents (void);		// ID_IN.C

unsigned	bufferofs;
unsigned	displayofs,pelpan;

unsigned	screenseg=SCREENSEG;		// kept for the asm-era code; no segment any more

unsigned	linewidth;
unsigned	ylookup[MAXSCANLINES];

boolean		screenfaded;
unsigned	bordercolor;

boolean		fastpalette;				// unused: kept for the palette test

byte		far	palette1[256][3],far palette2[256][3];

byte		vl_vram[VL_VRAMSIZE];		// the whole VGA memory, 4 planes interleaved

static byte		vl_palette[256][3];		// 6-bit DAC values, like the VGA
static byte		vl_rgba[256][4];		// the same, expanded for the backend
static unsigned	vl_crtcstart;			// CRTC start address: the page on screen
static double	vl_lastpresent;

//===========================================================================

// pointer to `count` pixels of the page `ofs`, row y, column x (NULL if outside video memory)
byte *VL_Span (unsigned ofs, int y, int x, long count)
{
	long	index;

	if (y < 0 || y >= MAXSCANLINES)
		return NULL;
	index = 4L*(ofs + ylookup[y]) + x;
	if (index < 0 || count < 0 || index + count > VL_VRAMSIZE)
		return NULL;
	return vl_vram + index;
}

/*
=======================
=
= VL_Startup
=
=======================
*/

void	VL_Startup (void)
{
	// no VGA detection: raylib always gives us a 320x200 8-bit display
	VL_SetLineWidth (40);
}

/*
=======================
=
= VL_Shutdown
=
=======================
*/

void	VL_Shutdown (void)
{
	VL_SetTextMode ();
}

/*
=======================
=
= VL_SetVGAPlaneMode
=
=======================
*/

void	VL_SetVGAPlaneMode (void)
{
	VL_DePlaneVGA ();
	VL_SetLineWidth (40);
}

void	VL_SetTextMode (void)
{
}

//===========================================================================

/*
=================
=
= VL_ClearVideo
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearVideo (byte color)
{
	memset (vl_vram,color,sizeof(vl_vram));
}

/*
=================
=
= VL_DePlaneVGA
=
= Nothing to switch: video memory is already addressable per page and pixel.
=
=================
*/

void VL_DePlaneVGA (void)
{
	VL_ClearVideo (0);
}

/*
====================
=
= VL_SetLineWidth
=
= Line witdh is in WORDS, 40 words is normal width for vgaplanegr
=
====================
*/

void VL_SetLineWidth (unsigned width)
{
	int i,offset;

	linewidth = width*2;

	offset = 0;

	for (i=0;i<MAXSCANLINES;i++)
	{
		ylookup[i]=offset;
		offset += linewidth;
	}
}

/*
====================
=
= VL_SetSplitScreen
=
= Wolf3D never splits the screen; the status bar is part of the page.
=
====================
*/

void VL_SetSplitScreen (int linenum)
{
	(void)linenum;
}

/*
=============================================================================

						TIMING AND PAGE FLIPPING

=============================================================================
*/

/*
==============
=
= VL_Refresh
=
= Expands the page on screen to RGBA and presents it.  Cheap enough to call
= from every wait loop; draws at most once per 70 Hz tick unless forced.
=
==============
*/

static void VL_Present (void)
{
	static unsigned char	frame[VL_SCREENW*VL_SCREENH*4];
	unsigned char			*dst = frame;
	const byte				*src;
	int						x,y;
	long					stride = 4L*linewidth;

	for (y=0;y<VL_SCREENH;y++)
	{
		src = vl_vram + 4L*vl_crtcstart + y*stride + (pelpan&3);
		if (src + VL_SCREENW > vl_vram + sizeof(vl_vram))
		{
			memset (dst,0,VL_SCREENW*4);		// page runs off the end of video memory
			dst += VL_SCREENW*4;
			continue;
		}
		for (x=0;x<VL_SCREENW;x++,dst+=4)
			memcpy (dst,vl_rgba[src[x]],4);
	}
	RL_Present (frame);
	vl_lastpresent = WL_TickClock();
}

void VL_Refresh (boolean force)
{
	if (force || WL_TickClock() - vl_lastpresent >= 1.0)
		VL_Present ();
}

/*
==============
=
= VL_WaitVBL
=
= Waits for `vbls` 70 Hz retraces and shows the frame.
=
==============
*/

void VL_WaitVBL (int vbls)
{
	double	target;

	if (vbls < 1)
		vbls = 1;
	target = floor(WL_TickClock()) + vbls;
	WL_SleepUntilTick (target);
	VL_Present ();
	IN_PumpEvents ();
}

/*
==============
=
= VL_SetCRTC / VL_SetScreen
=
= Select the page that is shown (plane offset) and the pel panning.
=
==============
*/

void VL_SetCRTC (int crtc)
{
	vl_crtcstart = (unsigned)crtc & 0xffff;
}

void VL_SetScreen (int crtc, int pel)
{
	vl_crtcstart = (unsigned)crtc & 0xffff;
	pelpan = pel;
	VL_WaitVBL (1);				// the page flip happens at the retrace
}

int VL_VideoID (void)
{
	return 5;					// VGA
}

/*
=============================================================================

						PALETTE OPS

=============================================================================
*/

static void VL_UpdateRGBA (int color)
{
	int i;

	for (i=0;i<3;i++)
		vl_rgba[color][i] = (vl_palette[color][i]<<2) | (vl_palette[color][i]>>4);
	vl_rgba[color][3] = 255;
}

/*
=================
=
= VL_FillPalette
=
=================
*/

void VL_FillPalette (int red, int green, int blue)
{
	int	i;

	for (i=0;i<256;i++)
		VL_SetColor (i,red,green,blue);
}

/*
=================
=
= VL_SetColor
=
=================
*/

void VL_SetColor	(int color, int red, int green, int blue)
{
	vl_palette[color][0] = red & 63;
	vl_palette[color][1] = green & 63;
	vl_palette[color][2] = blue & 63;
	VL_UpdateRGBA (color);
}

/*
=================
=
= VL_GetColor
=
=================
*/

void VL_GetColor	(int color, int *red, int *green, int *blue)
{
	*red = vl_palette[color][0];
	*green = vl_palette[color][1];
	*blue = vl_palette[color][2];
}

/*
=================
=
= VL_SetPalette
=
= 768 bytes of 6-bit DAC values
=
=================
*/

void VL_SetPalette (byte far *palette)
{
	int	i;

	for (i=0;i<256;i++,palette+=3)
		VL_SetColor (i,palette[0],palette[1],palette[2]);
}

/*
=================
=
= VL_GetPalette
=
=================
*/

void VL_GetPalette (byte far *palette)
{
	memcpy (palette,vl_palette,768);
}

/*
=================
=
= VL_FadeOut
=
= Fades the current palette to the given color in the given number of steps
=
=================
*/

void VL_FadeOut (int start, int end, int red, int green, int blue, int steps)
{
	int		i,j,orig,delta;
	byte	far *origptr, far *newptr;

	VL_WaitVBL(1);
	VL_GetPalette (&palette1[0][0]);
	_fmemcpy (palette2,palette1,768);

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		origptr = &palette1[start][0];
		newptr = &palette2[start][0];
		for (j=start;j<=end;j++)
		{
			orig = *origptr++;
			delta = red-orig;
			*newptr++ = orig + delta * i / steps;
			orig = *origptr++;
			delta = green-orig;
			*newptr++ = orig + delta * i / steps;
			orig = *origptr++;
			delta = blue-orig;
			*newptr++ = orig + delta * i / steps;
		}

		VL_WaitVBL(1);
		VL_SetPalette (&palette2[0][0]);
	}

//
// final color
//
	VL_FillPalette (red,green,blue);
	VL_Refresh (true);

	screenfaded = true;
}


/*
=================
=
= VL_FadeIn
=
=================
*/

void VL_FadeIn (int start, int end, byte far *palette, int steps)
{
	int		i,j,delta;

	VL_WaitVBL(1);
	VL_GetPalette (&palette1[0][0]);
	_fmemcpy (&palette2[0][0],&palette1[0][0],sizeof(palette1));

	start *= 3;
	end = end*3+2;

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		for (j=start;j<=end;j++)
		{
			// WOLF-7: index the flat 768 bytes (palette1[0][j] runs past row 0)
			delta = palette[j]-(&palette1[0][0])[j];
			(&palette2[0][0])[j] = (&palette1[0][0])[j] + delta * i / steps;
		}

		VL_WaitVBL(1);
		VL_SetPalette (&palette2[0][0]);
	}

//
// final color
//
	VL_SetPalette (palette);
	VL_Refresh (true);
	screenfaded = false;
}



/*
=================
=
= VL_TestPaletteSet
=
= The DAC is memory now, so fast palette setting always works.
=
=================
*/

void VL_TestPaletteSet (void)
{
	fastpalette = true;
}


/*
==================
=
= VL_ColorBorder
=
= The overscan colour is not drawn; the window is letterboxed in black.
=
==================
*/

void VL_ColorBorder (int color)
{
	bordercolor = color;
}



/*
=============================================================================

							PIXEL OPS

=============================================================================
*/

/*
=================
=
= VL_Plot
=
=================
*/

void VL_Plot (int x, int y, int color)
{
	byte *dest = VL_Span (bufferofs,y,x,1);

	if (dest)
		*dest = color;
}


/*
=================
=
= VL_Hlin
=
=================
*/

void VL_Hlin (unsigned x, unsigned y, unsigned width, unsigned color)
{
	byte *dest = VL_Span (bufferofs,y,x,width);

	if (dest)
		memset (dest,color,width);
}


/*
=================
=
= VL_Vlin
=
=================
*/

void VL_Vlin (int x, int y, int height, int color)
{
	byte	*dest = VL_Span (bufferofs,y,x,1);

	if (!dest || !VL_Span (bufferofs,y+height-1,x,1))
		return;

	while (height--)
	{
		*dest = color;
		dest += 4*linewidth;
	}
}


/*
=================
=
= VL_Bar
=
=================
*/

void VL_Bar (int x, int y, int width, int height, int color)
{
	byte	*dest;

	if (width <= 0 || height <= 0)
		return;
	if (!VL_Span (bufferofs,y,x,width) || !VL_Span (bufferofs,y+height-1,x,width))
		return;

	dest = VL_Span (bufferofs,y,x,width);
	while (height--)
	{
		memset (dest,color,width);
		dest += 4*linewidth;
	}
}

/*
============================================================================

							MEMORY OPS

============================================================================
*/

/*
=================
=
= VL_MemToLatch
=
= Source is planar: all of plane 0, then plane 1, ...  Each plane holds
= ((width+3)/4)*height bytes, stored to successive latch bytes at `dest`.
=
=================
*/

void VL_MemToLatch (byte far *source, int width, int height, unsigned dest)
{
	unsigned	count,i;
	int			plane;

	count = ((width+3)/4)*height;
	if (4L*(dest+count) > VL_VRAMSIZE)
		return;
	for (plane = 0; plane<4 ; plane++)
	{
		for (i=0;i<count;i++)
			vl_vram[4L*(dest+i)+plane] = source[i];
		source+= count;
	}
}


//===========================================================================


/*
=================
=
= VL_MemToScreen
=
= Draws a block of planar data (see VL_MungePic) to the screen.
=
=================
*/

void VL_MemToScreen (byte far *source, int width, int height, int x, int y)
{
	byte	*dest;
	int		plane,row,col,p;

	width>>=2;
	for (plane = 0; plane<4; plane++)
	{
		p = ((x&3)+plane)&3;			// the plane the mask selects
		for (row=0;row<height;row++,source+=width)
		{
			dest = VL_Span (bufferofs,y+row,x&~3,width*4);
			if (!dest)
				continue;
			for (col=0;col<width;col++)
				dest[col*4+p] = source[col];
		}
	}
}

//==========================================================================


/*
=================
=
= VL_MaskedToScreen
=
= Unused by Wolf3D; it had no mask handling in the original either.
=
=================
*/

void VL_MaskedToScreen (byte far *source, int width, int height, int x, int y)
{
	VL_MemToScreen (source,width,height,x,y);
}

//==========================================================================

/*
=================
=
= VL_LatchToScreen
=
= Copies width*4 x height pixels from latch memory (rows of `width` latch
= bytes, stored back to back) to the page at bufferofs.
=
=================
*/

void VL_LatchToScreen (unsigned source, int width, int height, int x, int y)
{
	byte		*dest;
	const byte	*src;
	int			row;

	src = vl_vram + 4L*source;
	for (row=0;row<height;row++,src+=width*4)
	{
		dest = VL_Span (bufferofs,y+row,x&~3,width*4);
		if (!dest || src + width*4 > vl_vram + sizeof(vl_vram))
			continue;
		memmove (dest,src,width*4);
	}
}


//===========================================================================

/*
=================
=
= VL_ScreenToScreen
=
= Block copy inside video memory.  bufferofs is NOT accounted for.  width is
= in latch bytes (4 pixels).
=
=================
*/

void VL_ScreenToScreen (unsigned source, unsigned dest,int width, int height)
{
	long	srcpos = 4L*source, destpos = 4L*dest;
	int		row;

	for (row=0;row<height;row++,srcpos+=4L*linewidth,destpos+=4L*linewidth)
	{
		if (srcpos + width*4 > VL_VRAMSIZE || destpos + width*4 > VL_VRAMSIZE)
			continue;
		memmove (vl_vram+destpos,vl_vram+srcpos,width*4);
	}
}

/*
=============================================================================

						STRING OUTPUT ROUTINES

=============================================================================
*/




/*
===================
=
= VL_DrawTile8String
=
= tile8ptr: 64 bytes per character, planar (8 rows of 2 bytes per plane)
=
===================
*/

void VL_DrawTile8String (char *str, char far *tile8ptr, int printx, int printy)
{
	int		plane,row,col;
	byte	*dest;
	const byte *src;

	while (*str)
	{
		src = (const byte *)tile8ptr + (*str<<6);
		for (plane=0;plane<4;plane++)
			for (row=0;row<8;row++)
			{
				dest = VL_Span (bufferofs,printy+row,printx&~3,8);
				for (col=0;col<2;col++,src++)
					if (dest)
						dest[col*4+plane] = *src;
			}
		str++;
		printx += 8;
	}
}



/*
===================
=
= VL_DrawLatch8String
=
= tile8ptr: latch address of the characters, 16 latch bytes (8x8 pixels) each
=
===================
*/

void VL_DrawLatch8String (char *str, unsigned tile8ptr, int printx, int printy)
{
	int		row;
	byte	*dest;

	while (*str)
	{
		unsigned src = tile8ptr + (*str<<4);

		for (row=0;row<8;row++)
		{
			dest = VL_Span (bufferofs,printy+row,printx&~3,8);
			if (dest && 4L*(src+row*2+2) <= VL_VRAMSIZE)
				memmove (dest,vl_vram+4L*(src+row*2),8);
		}
		str++;
		printx += 8;
	}
}


/*
===================
=
= VL_SizeTile8String
=
===================
*/

void VL_SizeTile8String (char *str, int *width, int *height)
{
	*height = 8;
	*width = 8*strlen(str);
}
