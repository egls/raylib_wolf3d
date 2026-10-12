/*
 * wl_glprobe.c - Windows: is there an OpenGL 3.3 driver to open a window on?
 *
 * raylib 5.5 ignores a failed InitPlatform() and keeps initialising, so on a
 * machine with no usable OpenGL driver (headless CI runners, bare VMs, remote
 * desktop sessions) InitWindow() ends in an access violation instead of an
 * error.  Probing with a throwaway context first lets the caller fail cleanly.
 * Kept out of id_raylib.c: <windows.h> and raylib.h do not mix.
 */
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

#include "wl_glprobe.h"

int WL_GLAvailable (void)
{
	static const char cls[] = "WolfGLProbe";
	WNDCLASSA wc;
	PIXELFORMATDESCRIPTOR pfd;
	HINSTANCE inst = GetModuleHandleA(NULL);
	HWND wnd;
	HDC dc;
	HGLRC rc;
	const char *version;
	int fmt, major = 0, minor = 0, ok = 0;

	ZeroMemory(&wc, sizeof wc);
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = inst;
	wc.lpszClassName = cls;
	if (!RegisterClassA(&wc))
		return 1;			/* can't probe: let raylib try */
	wnd = CreateWindowA(cls, "", WS_OVERLAPPED, 0, 0, 1, 1, NULL, NULL, inst, NULL);
	if (!wnd)
	{
		UnregisterClassA(cls, inst);
		return 1;
	}
	dc = GetDC(wnd);
	ZeroMemory(&pfd, sizeof pfd);
	pfd.nSize = sizeof pfd;
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	fmt = ChoosePixelFormat(dc, &pfd);
	if (fmt && SetPixelFormat(dc, fmt, &pfd) && (rc = wglCreateContext(dc)) != NULL)
	{
		if (wglMakeCurrent(dc, rc))
		{
			version = (const char *)glGetString(GL_VERSION);
			/* The Microsoft GDI fallback renderer is 1.1; raylib needs 3.3. */
			if (version && sscanf(version, "%d.%d", &major, &minor) == 2)
				ok = major > 3 || (major == 3 && minor >= 3);
			wglMakeCurrent(NULL, NULL);
		}
		wglDeleteContext(rc);
	}
	ReleaseDC(wnd, dc);
	DestroyWindow(wnd);
	UnregisterClassA(cls, inst);
	return ok;
}
#endif
