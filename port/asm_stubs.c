/*
 * asm_stubs.c - C stand-ins for the TASM modules of WOLFSRC/WOLF3D.PRJ.
 *
 * WOLF-2 only needs the program to link.  Every routine keeps the signature
 * its C callers use and aborts with "not ported yet"; the real translations
 * land in Stage 2 (see PORTING.md).  Data tables defined in the .ASM files
 * are carried over verbatim because they are data, not code.
 *
 *   ID_VL_A.ASM  VL_WaitVBL VL_SetCRTC VL_SetScreen VL_ScreenToScreen VL_VideoID
 *   ID_VH_A.ASM  VH_UpdateScreen
 *   ID_US_A.ASM  US_InitRndT US_RndT rndtable
 *   ID_SD_A.ASM  SDL_SetDS SDL_IndicatePC SDL_t0{Extreme,Fast,Slow}AsmService
 *   WL_DR_A.ASM  AsmRefresh (with near helpers xpartialbyystep/ypartialbyxstep)
 *   WHACK_A.ASM  MapRow planepics
 *   WL_ASM.ASM   CheckIs386 jabhack2 (Patch386 in WL_MAIN.C)
 *   H_LDIV.ASM   Borland 32-bit long divide helpers: native on 64-bit, dropped
 *   C0.ASM       Borland startup code: replaced by the C runtime + main()
 */

#include "ID_HEADS.H"

/* ---- ID_VL_A.ASM ------------------------------------------------------- */

void VL_WaitVBL (int vbls)
{
	(void)vbls;
	NOTPORTED("VL_WaitVBL");
}

void VL_SetCRTC (int crtc)
{
	(void)crtc;
	NOTPORTED("VL_SetCRTC");
}

void VL_SetScreen (int crtc, int pelpan)
{
	(void)crtc; (void)pelpan;
	NOTPORTED("VL_SetScreen");
}

void VL_ScreenToScreen (unsigned source, unsigned dest, int width, int height)
{
	(void)source; (void)dest; (void)width; (void)height;
	NOTPORTED("VL_ScreenToScreen");
}

int VL_VideoID (void)
{
	NOTPORTED("VL_VideoID");
}

/* ---- ID_VH_A.ASM ------------------------------------------------------- */

void VH_UpdateScreen (void)
{
	NOTPORTED("VH_UpdateScreen");
}

/* ---- ID_US_A.ASM ------------------------------------------------------- */

byte rndtable[256] =
{
	  0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,  74,  21,
	211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,  95, 110,  85,  48,
	212, 140, 211, 249,  22,  79, 200,  50,  28, 188,  52, 140, 202, 120,  68, 145,
	 62,  70, 184, 190,  91, 197, 152, 224, 149, 104,  25, 178, 252, 182, 202, 182,
	141, 197,   4,  81, 181, 242, 145,  42,  39, 227, 156, 198, 225, 193, 219,  93,
	122, 175, 249,   0, 175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,
	  2, 235,  25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
	 94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75, 136, 156,
	 11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196, 135, 106,  63, 197,
	195,  86,  96, 203, 113, 101, 170, 247, 181, 113,  80, 250, 108,   7, 255, 237,
	129, 226,  79, 107, 112, 166, 103, 241,  24, 223, 239, 120, 198,  58,  60,  82,
	128,   3, 184,  66, 143, 224, 145, 224,  81, 206, 163,  45,  63,  90, 168, 114,
	 59,  33, 159,  95,  28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14,
	109, 226,  71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
	 17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106, 197, 242,
	 98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136, 120, 163, 236, 249,
};

void US_InitRndT (boolean randomize)
{
	(void)randomize;
	NOTPORTED("US_InitRndT");
}

int US_RndT (void)
{
	NOTPORTED("US_RndT");
}

/* ---- ID_SD_A.ASM ------------------------------------------------------- */

void SDL_SetDS (void)
{
	NOTPORTED("SDL_SetDS");
}

void SDL_IndicatePC (boolean on)
{
	(void)on;
	NOTPORTED("SDL_IndicatePC");
}

void SDL_t0ExtremeAsmService (void)
{
	NOTPORTED("SDL_t0ExtremeAsmService");
}

void SDL_t0FastAsmService (void)
{
	NOTPORTED("SDL_t0FastAsmService");
}

void SDL_t0SlowAsmService (void)
{
	NOTPORTED("SDL_t0SlowAsmService");
}

/* ---- WL_DR_A.ASM ------------------------------------------------------- */

void AsmRefresh (void)
{
	NOTPORTED("AsmRefresh");
}

/* ---- WL_ASM.ASM -------------------------------------------------------- */

int CheckIs386 (void)
{
	NOTPORTED("CheckIs386");
}

void jabhack2 (void)
{
	NOTPORTED("jabhack2");
}

/* ---- WHACK_A.ASM ------------------------------------------------------- */

char planepics[8192];		// 4k of ceiling, 4k of floor

void MapRow (void)
{
	NOTPORTED("MapRow");
}
