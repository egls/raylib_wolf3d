#!/usr/bin/env python3
"""xdrive.py - drive raylib_wolf3d under an X server (normally Xvfb) for tests.

Sends real key events through XTest (so they go through raylib/GLFW like a
keyboard would) and takes screenshots of the game window only.  Needs
libX11, libXtst and Pillow; no xdotool.

    xdrive.py [-w TITLE] COMMAND...

Commands (run in order):
    wait:SECONDS          wait until the game window exists (timeout)
    sleep:MS              pause
    key:NAME[:MS]         press and hold a key (default 150 ms), then release
    down:NAME / up:NAME   press / release without releasing / pressing
    type:TEXT             tap each character of TEXT
    shot:FILE.png         save the game window as PNG (window contents only)

NAME is an X keysym name: Return, Escape, space, Up, Down, Left, Right,
Control_L, Alt_L, Shift_L, y, n, F2, ...
"""
import ctypes
import ctypes.util
import sys
import time

xlib = ctypes.CDLL(ctypes.util.find_library("X11"))
xtst = ctypes.CDLL(ctypes.util.find_library("Xtst"))

xlib.XOpenDisplay.restype = ctypes.c_void_p
xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
xlib.XDefaultRootWindow.restype = ctypes.c_ulong
xlib.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
xlib.XStringToKeysym.restype = ctypes.c_ulong
xlib.XStringToKeysym.argtypes = [ctypes.c_char_p]
xlib.XKeysymToKeycode.restype = ctypes.c_ubyte
xlib.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
xlib.XFlush.argtypes = [ctypes.c_void_p]
xlib.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
xlib.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
                            ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
                            ctypes.POINTER(ctypes.c_uint)]
xlib.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_char_p)]
xlib.XFree.argtypes = [ctypes.c_void_p]


class XWindowAttributes(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int),
                ("height", ctypes.c_int), ("border_width", ctypes.c_int), ("depth", ctypes.c_int),
                ("visual", ctypes.c_void_p), ("root", ctypes.c_ulong), ("c_class", ctypes.c_int),
                ("bit_gravity", ctypes.c_int), ("win_gravity", ctypes.c_int),
                ("backing_store", ctypes.c_int), ("backing_planes", ctypes.c_ulong),
                ("backing_pixel", ctypes.c_ulong), ("save_under", ctypes.c_int),
                ("colormap", ctypes.c_ulong), ("map_installed", ctypes.c_int),
                ("map_state", ctypes.c_int), ("all_event_masks", ctypes.c_long),
                ("your_event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long),
                ("override_redirect", ctypes.c_int), ("screen", ctypes.c_void_p)]


class XImage(ctypes.Structure):
    _fields_ = [("width", ctypes.c_int), ("height", ctypes.c_int), ("xoffset", ctypes.c_int),
                ("format", ctypes.c_int), ("data", ctypes.POINTER(ctypes.c_ubyte)),
                ("byte_order", ctypes.c_int), ("bitmap_unit", ctypes.c_int),
                ("bitmap_bit_order", ctypes.c_int), ("bitmap_pad", ctypes.c_int),
                ("depth", ctypes.c_int), ("bytes_per_line", ctypes.c_int),
                ("bits_per_pixel", ctypes.c_int)]


xlib.XGetWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XWindowAttributes)]
xlib.XGetImage.restype = ctypes.POINTER(XImage)
xlib.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                           ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]

dpy = xlib.XOpenDisplay(None)
if not dpy:
    sys.exit("xdrive: cannot open display")
root = xlib.XDefaultRootWindow(dpy)
title = b"Wolfenstein 3-D"


def find_window(w):
    name = ctypes.c_char_p()
    if xlib.XFetchName(dpy, w, ctypes.byref(name)) and name.value:
        found = title in name.value
        xlib.XFree(name)
        if found:
            return w
    r, p, n = ctypes.c_ulong(), ctypes.c_ulong(), ctypes.c_uint()
    kids = ctypes.POINTER(ctypes.c_ulong)()
    if not xlib.XQueryTree(dpy, w, ctypes.byref(r), ctypes.byref(p), ctypes.byref(kids), ctypes.byref(n)):
        return 0
    try:
        for i in range(n.value):
            hit = find_window(kids[i])
            if hit:
                return hit
    finally:
        if kids:
            xlib.XFree(kids)
    return 0


def keycode(name):
    sym = xlib.XStringToKeysym(name.encode())
    if not sym:
        sys.exit("xdrive: unknown key %r" % name)
    return xlib.XKeysymToKeycode(dpy, sym)


def press(name, down):
    xtst.XTestFakeKeyEvent(dpy, keycode(name), 1 if down else 0, 0)
    xlib.XFlush(dpy)


def tap(name, ms=150):
    press(name, True)
    time.sleep(ms / 1000.0)
    press(name, False)
    time.sleep(0.05)


def shot(path):
    from PIL import Image
    w = find_window(root)
    if not w:
        sys.exit("xdrive: no game window for screenshot")
    a = XWindowAttributes()
    xlib.XGetWindowAttributes(dpy, w, ctypes.byref(a))
    img = xlib.XGetImage(dpy, w, 0, 0, a.width, a.height, 0xFFFFFFFF, 2)  # ZPixmap
    if not img:
        sys.exit("xdrive: XGetImage failed")
    im = img.contents
    raw = ctypes.string_at(im.data, im.bytes_per_line * im.height)
    Image.frombuffer("RGBX", (im.width, im.height), raw, "raw", "BGRX", im.bytes_per_line, 1) \
        .convert("RGB").save(path)
    print("xdrive: saved %s (%dx%d)" % (path, im.width, im.height))


def main(argv):
    global title
    if len(argv) > 2 and argv[1] == "-w":
        title = argv[2].encode()
        argv = argv[2:]
    for cmd in argv[1:]:
        op, _, arg = cmd.partition(":")
        if op == "wait":
            end = time.time() + float(arg or 10)
            while not find_window(root):
                if time.time() > end:
                    sys.exit("xdrive: game window did not appear")
                time.sleep(0.1)
        elif op == "sleep":
            time.sleep(int(arg) / 1000.0)
        elif op == "key":
            name, _, ms = arg.partition(":")
            tap(name, int(ms) if ms else 150)
        elif op == "down":
            press(arg, True)
        elif op == "up":
            press(arg, False)
        elif op == "type":
            for ch in arg:
                tap({" ": "space", "-": "minus", ".": "period"}.get(ch, ch))
        elif op == "shot":
            shot(arg)
        else:
            sys.exit("xdrive: unknown command %r" % cmd)


if __name__ == "__main__":
    main(sys.argv)
