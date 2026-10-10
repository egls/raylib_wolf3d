/*
 * wl_videotest.c - developer harness for the WOLF-5 video/input/timer layer.
 *
 *   raylib_wolf3d -data <dir> -videotest signon|title|fade|keys|timing
 *
 * It exercises ID_VL/ID_VH/ID_IN without the engine core (memory manager and
 * cache manager are not ported yet), so it reads the shareware VGAGRAPH itself.
 * Not part of the game: the game never calls WL_VideoTest unless asked.
 */
#include "ID_HEADS.H"
#include "id_raylib.h"
#include "wl_timer.h"

#define GAMEPAL	((byte *)&gamepal)		/* declared as scalars in the headers, arrays in the data */
#define SIGNON	((byte *)&signon)

/* chunky 320x200 -> the planar order VL_MungePic produces */
static void Munge (byte *dest, const byte *src, int width, int height)
{
	int plane, x, y;

	for (plane = 0; plane < 4; plane++)
		for (y = 0; y < height; y++)
			for (x = 0; x < width / 4; x++)
				*dest++ = src[y * width + x * 4 + plane];
}

static void ShowPlanar (const byte *chunky, int page)
{
	static byte planar[64000];

	Munge(planar, chunky, 320, 200);
	bufferofs = page;
	VL_MemToScreen(planar, 320, 200, 0, 0);
}

/* ---- VGAGRAPH.WL1: Huffman chunks -------------------------------------- */

static byte *ReadFile (const char *name, long *size)
{
	FILE *f = fopen(name, "rb");
	byte *buf;

	if (!f)
	{
		fprintf(stderr, "videotest: can't open %s\n", name);
		exit(2);
	}
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc(*size + 1);
	if (fread(buf, 1, *size, f) != (size_t)*size)
		exit(2);
	fclose(f);
	return buf;
}

typedef struct { uint16_t bit0, bit1; } node_t;

static node_t	dict[256];
static byte		*grdata, *grhead;
static long		grsize, headsize;

static long ChunkOffset (int c)
{
	return grhead[c*3] | (grhead[c*3+1] << 8) | (grhead[c*3+2] << 16);
}

static long Expand (const byte *in, long inlen, byte *out, long outlen)
{
	long	o = 0, i;
	int		node = 254, bit;
	uint16_t next;

	for (i = 0; i < inlen && o < outlen; i++)
		for (bit = 0; bit < 8 && o < outlen; bit++)
		{
			next = (in[i] >> bit) & 1 ? dict[node].bit1 : dict[node].bit0;
			if (next < 256)
			{
				out[o++] = (byte)next;
				node = 254;
			}
			else
				node = next - 256;
		}
	return o;
}

static byte *GetChunk (int c, long *len)
{
	long	off = ChunkOffset(c), end = ChunkOffset(c + 1), n;
	byte	*out;

	if (off < 0 || off == 0xffffff || end < off)
		return NULL;
	n = *(uint32_t *)(grdata + off);		/* expanded length (little endian) */
	if (n <= 0 || n > 70000)
		return NULL;
	out = malloc(n);
	*len = Expand(grdata + off + 4, end - off - 4, out, n);
	return out;
}

static void OpenGraphics (void)
{
	long	n;
	byte	*d = ReadFile("VGADICT.WL1", &n);
	int		i;

	for (i = 0; i < 255; i++)
	{
		dict[i].bit0 = d[i*4] | (d[i*4+1] << 8);
		dict[i].bit1 = d[i*4+2] | (d[i*4+3] << 8);
	}
	free(d);
	grhead = ReadFile("VGAHEAD.WL1", &headsize);
	grdata = ReadFile("VGAGRAPH.WL1", &grsize);
}

/* first 320x200 picture in the file, by index among the pictures */
static byte *FindFullscreenPic (int nth)
{
	long	len, plen;
	byte	*pt, *pic, *chunky;
	int		c, found = 0;
	int		numchunks = (int)(headsize / 3);

	/* chunk 0 is the pic table: width,height pairs (16 bit) for chunks 3..; like
	 * every chunk it starts with its expanded length */
	{
		long off = ChunkOffset(0), end = ChunkOffset(1);
		long n = *(uint32_t *)(grdata + off);

		pt = malloc(n);
		plen = Expand(grdata + off + 4, end - off - 4, pt, n);
	}
	for (c = 3; c < numchunks - 1 && (c - 3) * 4 + 3 < plen; c++)
	{
		int w = pt[(c-3)*4] | (pt[(c-3)*4+1] << 8);
		int h = pt[(c-3)*4+2] | (pt[(c-3)*4+3] << 8);

		if (w != 320 || h != 200)
			continue;
		pic = GetChunk(c, &len);
		if (!pic || len != 64000)
			continue;
		if (found++ == nth)
		{
			/* chunk data is planar already; turn it chunky for ShowPlanar */
			int plane, x, y;

			chunky = malloc(64000);
			for (plane = 0; plane < 4; plane++)
				for (y = 0; y < 200; y++)
					for (x = 0; x < 80; x++)
						chunky[y*320 + x*4 + plane] = pic[plane*16000 + y*80 + x];
			free(pic);
			fprintf(stderr, "videotest: using chunk %d as 320x200 picture #%d\n", c, nth);
			return chunky;
		}
		free(pic);
	}
	return NULL;
}

static void Wait (int ms)
{
	double end = WL_TickClock() + ms * WL_TICKRATE / 1000.0;

	while (WL_TickClock() < end)
		IN_SpinWait();
}

void WL_VideoTest (const char *what)
{
	VL_Startup();
	VL_SetVGAPlaneMode();
	VL_TestPaletteSet();
	VL_SetPalette(GAMEPAL);

	if (!strcmp(what, "signon"))
	{
		/* the same sequence SignonScreen runs: draw off screen, then flip */
		VL_SetScreen(0x8000, 0);
		ShowPlanar(SIGNON, 0);
		VL_SetScreen(0, 0);
		Wait(4000);
	}
	else if (!strcmp(what, "title"))
	{
		byte *pic;

		OpenGraphics();
		pic = FindFullscreenPic(0);
		if (!pic)
		{
			fprintf(stderr, "videotest: no 320x200 picture found\n");
			exit(1);
		}
		VL_SetScreen(0x8000, 0);
		ShowPlanar(pic, 0);
		VL_SetScreen(0, 0);
		Wait(4000);
	}
	else if (!strcmp(what, "fade"))
	{
		VL_SetScreen(0x8000, 0);
		ShowPlanar(SIGNON, 0);
		VL_SetScreen(0, 0);
		Wait(500);
		VL_FadeOut(0, 255, 63, 0, 0, 30);		/* red flash, like a damage shift */
		Wait(300);
		VL_FadeIn(0, 255, GAMEPAL, 30);
		Wait(1000);
	}
	else if (!strcmp(what, "latch"))
	{
		/* picture -> latch memory -> screen (LoadLatchMem / LatchDrawPic), then a bar and text-sized blit */
		static byte planar[64000];

		Munge(planar, SIGNON, 320, 200);
		VL_MemToLatch(planar, 320, 200, 0x4000);
		VL_SetScreen(0x8000, 0);
		bufferofs = 0;
		VL_LatchToScreen(0x4000, 80, 200, 0, 0);
		VL_Bar(100, 20, 120, 30, 4);
		VL_Hlin(100, 60, 120, 14);
		VL_Vlin(90, 20, 40, 14);
		VL_Plot(95, 25, 15);
		VL_SetScreen(0, 0);
		Wait(3000);
	}
	else if (!strcmp(what, "fizzle"))
	{
		/* the screen-to-screen fizzle used when the level starts */
		static byte planar[64000];

		Munge(planar, SIGNON, 320, 200);
		bufferofs = 0x8000;
		VL_MemToScreen(planar, 320, 200, 0, 0);
		bufferofs = 0;
		VL_Bar(0, 0, 320, 200, 0);
		VL_SetScreen(0, 0);
		FizzleFade(0x8000, 0, 320, 200, 70, false);
		Wait(2000);
	}
	else if (!strcmp(what, "keys"))
	{
		int	i;
		byte seen[NumCodes];

		VL_SetScreen(0x8000, 0);
		ShowPlanar(SIGNON, 0);
		VL_SetScreen(0, 0);
		IN_Startup();
		memset(seen, 0, sizeof(seen));
		fprintf(stderr, "videotest: press keys (Esc quits)\n");
		for (;;)
		{
			IN_SpinWait();
			for (i = 0; i < NumCodes; i++)
				if (Keyboard[i] != seen[i])
				{
					seen[i] = Keyboard[i];
					printf("scancode 0x%02x %s\n", i, seen[i] ? "down" : "up");
					fflush(stdout);
				}
			if (LastScan)		/* also catches taps shorter than one poll */
			{
				printf("LastScan=0x%02x LastASCII=%d ('%c')\n", LastScan,
					LastASCII, LastASCII >= 32 ? LastASCII : ' ');
				fflush(stdout);
				if (LastScan == sc_Escape)
					break;
				LastScan = 0;
				LastASCII = 0;
			}
		}
	}
	else if (!strcmp(what, "timing"))
	{
		double t0 = WL_TickClock();
		longword c0 = TimeCount;
		int i;

		for (i = 0; i < 70; i++)
			VL_WaitVBL(1);
		printf("70 x VL_WaitVBL(1): %.1f ms, TimeCount advanced %u (expect ~1000 ms, 70)\n",
			(WL_TickClock() - t0) * 1000.0 / WL_TICKRATE, (unsigned)(TimeCount - c0));
		t0 = WL_TickClock();
		c0 = TimeCount;
		TimeCount = 0;
		while (TimeCount < 140)
			;
		printf("busy wait TimeCount 0->140: %.1f ms (expect ~2000)\n",
			(WL_TickClock() - t0) * 1000.0 / WL_TICKRATE);
	}
	else
	{
		fprintf(stderr, "videotest: unknown test '%s'\n", what);
		exit(2);
	}
}
