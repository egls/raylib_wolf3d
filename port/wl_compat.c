/*
 * wl_compat.c - Borland C runtime / DOS API replacements (see wl_compat.h).
 */
#define _GNU_SOURCE		// FNM_CASEFOLD
#include <dirent.h>
#include <fnmatch.h>

#include "wl_compat.h"

uint16_t	_AX, _BX, _CX, _DX, _SI, _DI, _BP, _SP, _CS, _DS, _ES, _SS;
uint8_t		_AL, _AH, _BL, _BH, _CL, _CH, _DL, _DH;
uint16_t	_FLAGS;

/* ------------------------------------------------------------------------
 * Portable implementations
 * ------------------------------------------------------------------------ */

long filelength (int handle)
{
	struct stat st;

	if (fstat(handle, &st))
		return -1;
	return (long)st.st_size;
}

static char *WL_ToRadix (unsigned long value, int negative, char *string, int radix)
{
	char	tmp[sizeof(unsigned long) * CHAR_BIT + 1];
	char	*out = string;
	int		n = 0;

	if (radix < 2 || radix > 36)
	{
		*string = 0;
		return string;
	}
	do
	{
		int d = (int)(value % (unsigned)radix);
		tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
		value /= (unsigned)radix;
	} while (value);
	if (negative)
		*out++ = '-';
	while (n)
		*out++ = tmp[--n];
	*out = 0;
	return string;
}

char *itoa (int value, char *string, int radix)
{
	// Borland: only radix 10 is signed
	if (radix == 10 && value < 0)
		return WL_ToRadix(-(unsigned long)(long)value, 1, string, radix);
	return WL_ToRadix((unsigned)value, 0, string, radix);
}

char *ltoa (long value, char *string, int radix)
{
	if (radix == 10 && value < 0)
		return WL_ToRadix(-(unsigned long)value, 1, string, radix);
	return WL_ToRadix((unsigned long)value, 0, string, radix);
}

char *ultoa (unsigned long value, char *string, int radix)
{
	return WL_ToRadix(value, 0, string, radix);
}

/*
 * findfirst/findnext: DOS wildcard search in the current directory (no path
 * components in the patterns the game uses).  Matching is case-insensitive
 * like DOS.  Only regular files are reported.
 *
 * WOLF-7: no directory handle is kept between calls (callers such as
 * CheckForEpisodes only test for existence and never finish the search,
 * which leaked the DIR).  findnext rescans and returns the match after the
 * ones already reported.
 */
int findnext (struct ffblk *ffblk)
{
	struct dirent	*de;
	DIR				*dir;
	long			seen = 0;

	dir = opendir(".");
	if (!dir)
		return -1;
	while ((de = readdir(dir)) != NULL)
	{
		struct stat st;

		if (fnmatch(ffblk->ff_pattern, de->d_name, FNM_CASEFOLD))
			continue;
		if (stat(de->d_name, &st) || !S_ISREG(st.st_mode))
			continue;
		if (seen++ < ffblk->ff_found)
			continue;
		snprintf(ffblk->ff_name, sizeof(ffblk->ff_name), "%s", de->d_name);
		ffblk->ff_attrib = FA_ARCH;
		ffblk->ff_fsize = (long)st.st_size;
		ffblk->ff_ftime = ffblk->ff_fdate = 0;
		ffblk->ff_found++;
		closedir(dir);
		return 0;
	}
	closedir(dir);
	errno = ENOENT;
	return -1;
}

int findfirst (const char *pathname, struct ffblk *ffblk, int attrib)
{
	(void)attrib;
	memset(ffblk, 0, sizeof(*ffblk));
	snprintf(ffblk->ff_pattern, sizeof(ffblk->ff_pattern), "%s", pathname);
	return findnext(ffblk);
}

/* ------------------------------------------------------------------------
 * Real-mode PC only: abort until the owning stage replaces the caller
 * ------------------------------------------------------------------------ */

unsigned long farcoreleft (void)	{ NOTPORTED("farcoreleft"); }
unsigned coreleft (void)			{ NOTPORTED("coreleft"); }

void *WL_MK_FP (unsigned seg, unsigned ofs)
{
	(void)seg; (void)ofs;
	NOTPORTED("MK_FP");
}

unsigned WL_FP_SEG (const void *p)	{ (void)p; NOTPORTED("FP_SEG"); }
unsigned WL_FP_OFF (const void *p)	{ (void)p; NOTPORTED("FP_OFF"); }

unsigned char peekb (unsigned seg, unsigned ofs)
{
	(void)seg; (void)ofs;
	NOTPORTED("peekb");
}

unsigned peek (unsigned seg, unsigned ofs)
{
	(void)seg; (void)ofs;
	NOTPORTED("peek");
}

void pokeb (unsigned seg, unsigned ofs, unsigned char value)
{
	(void)seg; (void)ofs; (void)value;
	NOTPORTED("pokeb");
}

void poke (unsigned seg, unsigned ofs, unsigned value)
{
	(void)seg; (void)ofs; (void)value;
	NOTPORTED("poke");
}

unsigned char inportb (unsigned port)	{ (void)port; NOTPORTED("inportb"); }
unsigned inport (unsigned port)			{ (void)port; NOTPORTED("inport"); }

void outportb (unsigned port, unsigned char value)
{
	(void)port; (void)value;
	NOTPORTED("outportb");
}

void outport (unsigned port, unsigned value)
{
	(void)port; (void)value;
	NOTPORTED("outport");
}

int int86 (int intr_num, union REGS *inregs, union REGS *outregs)
{
	(void)intr_num; (void)inregs; (void)outregs;
	NOTPORTED("int86");
}

void gotoxy (int x, int y)	{ (void)x; (void)y; NOTPORTED("gotoxy"); }
void clrscr (void)			{ NOTPORTED("clrscr"); }

void disable (void)					{ NOTPORTED("disable"); }
void enable (void)					{ NOTPORTED("enable"); }
void geninterrupt (int intr_num)
{
	// WOLF-7: int 10h with AX=3 (back to text mode before printing a quit
	// message) has nothing to do here; the window simply closes later.
	if (intr_num == 0x10 && _AX == 3)
		return;
	NOTPORTED("geninterrupt");
}

WL_IsrPtr getvect (int intr_num)
{
	(void)intr_num;
	NOTPORTED("getvect");
}

void setvect (int intr_num, WL_IsrPtr isr)
{
	(void)intr_num; (void)isr;
	NOTPORTED("setvect");
}

void movedata (unsigned srcseg, unsigned srcoff,
	unsigned dstseg, unsigned dstoff, size_t n)
{
	(void)srcseg; (void)srcoff; (void)dstseg; (void)dstoff; (void)n;
	NOTPORTED("movedata");
}

void harderr (int (*handler)())	{ (void)handler; NOTPORTED("harderr"); }
int bioskey (int cmd)			{ (void)cmd; NOTPORTED("bioskey"); }

void _dos_getdate (struct dosdate_t *d)	{ (void)d; NOTPORTED("_dos_getdate"); }
void _dos_gettime (struct dostime_t *t)	{ (void)t; NOTPORTED("_dos_gettime"); }

unsigned _dos_getdiskfree (unsigned drive, struct diskfree_t *df)
{
	(void)drive; (void)df;
	NOTPORTED("_dos_getdiskfree");
}

unsigned _dos_write (int handle, const void *buf, unsigned len, unsigned *nwritten)
{
	(void)handle; (void)buf; (void)len; (void)nwritten;
	NOTPORTED("_dos_write");
}
