/*
 * wl_compat.h - portability layer for the Wolfenstein 3D DOS sources.
 *
 * Included from WOLFSRC/ID_HEADS.H in place of the Borland C++ 3.x headers
 * (ALLOC.H, DOS.H, IO.H, MEM.H, DIR.H, VALUES.H, ...).
 *
 * Rules (WOLF-2):
 *  - Things that have a faithful flat-memory meaning (far string/memory
 *    helpers, itoa, filelength, findfirst) are implemented for real.
 *  - Things that only make sense on a real-mode PC (segment arithmetic, port
 *    I/O, interrupt vectors, BIOS/DOS calls, inline asm) are stubs that abort
 *    with "not ported yet: <name>" instead of silently returning.  Each stub
 *    is listed in PORTING.md with the stage that replaces it.
 *
 * No raylib types or headers here: raylib lives only in port/id_raylib.c.
 */
#ifndef WL_COMPAT_H
#define WL_COMPAT_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------------
 * Borland memory-model and calling-convention keywords
 * ------------------------------------------------------------------------ */
#define far
#define huge
#define near
#define _seg
#define interrupt
#define cdecl
#define _cdecl
#define pascal
#define _pascal

/* ------------------------------------------------------------------------
 * Not-ported-yet aborts
 * ------------------------------------------------------------------------ */
void WL_NotPorted (const char *what, const char *file, int line)
	__attribute__((noreturn));

#define NOTPORTED(what)	WL_NotPorted((what), __FILE__, __LINE__)
/* Replaces each run of Borland inline asm (original kept under #if 0). */
#define PORT_ASM()		WL_NotPorted(__func__, __FILE__, __LINE__)

/* ------------------------------------------------------------------------
 * VALUES.H
 * ------------------------------------------------------------------------ */
#define MAXINT		0x7fff		/* 16-bit int range the game was tuned for */
#define MAXLONG		0x7fffffffL

/* Borland errno value for "invalid format" */
#ifndef EINVFMT
#define EINVFMT		EINVAL
#endif

/* ------------------------------------------------------------------------
 * IO.H / FCNTL.H / SYS\STAT.H
 * ------------------------------------------------------------------------ */
#ifndef O_BINARY
#define O_BINARY	0
#endif
#ifndef O_TEXT
#define O_TEXT		0
#endif
#define S_IREAD		S_IRUSR
#define S_IWRITE	S_IWUSR

long	filelength (int handle);

/* ------------------------------------------------------------------------
 * MEM.H / STRING.H far variants: flat memory makes these the plain ones
 * ------------------------------------------------------------------------ */
#define _fmemcpy	memcpy
#define _fmemset	memset
#define _fmemcmp	memcmp
#define _fstrcpy	strcpy
#define _fstrlen	strlen
#define _fstricmp	strcasecmp
#define stricmp		strcasecmp
#define strnicmp	strncasecmp

char	*itoa (int value, char *string, int radix);
char	*ltoa (long value, char *string, int radix);
char	*ultoa (unsigned long value, char *string, int radix);

/* ------------------------------------------------------------------------
 * ALLOC.H
 * ------------------------------------------------------------------------ */
#define farmalloc	malloc
#define farfree		free
unsigned long	farcoreleft (void);
unsigned		coreleft (void);

/* ------------------------------------------------------------------------
 * DIR.H
 * ------------------------------------------------------------------------ */
#define FA_RDONLY	0x01
#define FA_HIDDEN	0x02
#define FA_SYSTEM	0x04
#define FA_LABEL	0x08
#define FA_DIREC	0x10
#define FA_ARCH		0x20

struct ffblk
{
	char		ff_reserved[21];
	char		ff_attrib;
	unsigned	ff_ftime;
	unsigned	ff_fdate;
	long		ff_fsize;
	char		ff_name[256];
	/* WOLF-2: portable search state */
	void		*ff_dir;
	char		ff_pattern[256];
};

/* DOS semantics: pattern match is case-insensitive, returns 0 on success */
int		findfirst (const char *pathname, struct ffblk *ffblk, int attrib);
int		findnext (struct ffblk *ffblk);

/* ------------------------------------------------------------------------
 * DOS.H: segment:offset pointers, port I/O, interrupts, BIOS/DOS services.
 * All of these abort at runtime.
 * ------------------------------------------------------------------------ */
void		*WL_MK_FP (unsigned seg, unsigned ofs);
unsigned	WL_FP_SEG (const void *p);
unsigned	WL_FP_OFF (const void *p);
#define MK_FP(seg,ofs)	WL_MK_FP((unsigned)(uintptr_t)(seg),(unsigned)(uintptr_t)(ofs))
#define FP_SEG(p)		WL_FP_SEG((const void *)(p))
#define FP_OFF(p)		WL_FP_OFF((const void *)(p))

/* Stores into just the segment or offset half of a far pointer (Borland
 * cast-as-lvalue idioms such as "(unsigned)ptr = ofs" in the renderer). */
#define SET_FP_OFF(lv,ofs)	((void)(lv), (void)(ofs), NOTPORTED("far pointer offset store"))
#define SET_FP_SEG(lv,seg)	((void)(lv), (void)(seg), NOTPORTED("far pointer segment store"))

unsigned char	peekb (unsigned seg, unsigned ofs);
unsigned		peek (unsigned seg, unsigned ofs);
void			pokeb (unsigned seg, unsigned ofs, unsigned char value);
void			poke (unsigned seg, unsigned ofs, unsigned value);

unsigned char	inportb (unsigned port);
unsigned		inport (unsigned port);
void			outportb (unsigned port, unsigned char value);
void			outport (unsigned port, unsigned value);

struct WORDREGS { unsigned short ax, bx, cx, dx, si, di, cflag, flags; };
struct BYTEREGS { unsigned char al, ah, bl, bh, cl, ch, dl, dh; };
union REGS { struct WORDREGS x; struct BYTEREGS h; };
struct SREGS { unsigned short es, cs, ss, ds; };
int		int86 (int intr_num, union REGS *inregs, union REGS *outregs);

void	disable (void);
void	enable (void);
void	geninterrupt (int intr_num);

typedef void (*WL_IsrPtr) (void);
WL_IsrPtr	getvect (int intr_num);
void		setvect (int intr_num, WL_IsrPtr isr);

void	movedata (unsigned srcseg, unsigned srcoff,
			unsigned dstseg, unsigned dstoff, size_t n);
void	harderr (int (*handler)());
int		bioskey (int cmd);

struct date { int da_year; char da_day; char da_mon; };
struct time { unsigned char ti_min, ti_hour, ti_hund, ti_sec; };
struct dosdate_t { unsigned char day, month; unsigned year; unsigned char dayofweek; };
struct dostime_t { unsigned char hour, minute, second, hsecond; };
struct diskfree_t { unsigned total_clusters, avail_clusters,
	sectors_per_cluster, bytes_per_sector; };

void		_dos_getdate (struct dosdate_t *d);
void		_dos_gettime (struct dostime_t *t);
unsigned	_dos_getdiskfree (unsigned drive, struct diskfree_t *df);
unsigned	_dos_write (int handle, const void far *buf, unsigned len,
				unsigned *nwritten);

/*
 * Borland pseudo-registers.  Code that loads them is always followed by a
 * geninterrupt() or inline asm, both of which abort, so plain variables are
 * enough to compile.
 */
extern uint16_t	_AX, _BX, _CX, _DX, _SI, _DI, _BP, _SP, _CS, _DS, _ES, _SS;
extern uint8_t	_AL, _AH, _BL, _BH, _CL, _CH, _DL, _DH;
extern uint16_t	_FLAGS;

/* CONIO.H text-mode screen (error/quit screens) */
void	gotoxy (int x, int y);
void	clrscr (void);

/* PROCESS.H / startup: Borland's argv globals, filled by main() */
extern int		_argc;
extern char		**_argv;

/* Platform startup (port/wl_platform.c): argv, data directory, window. */
void	WL_PlatformStartup (int argc, char **argv);

#endif
