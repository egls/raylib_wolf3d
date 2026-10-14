/*
 * wl_minimap.h - an overhead minimap drawn in the corner of the 3D view.
 *
 * Tab toggles it during play (WL_PLAY.C CheckKeys).  Only tiles the player
 * has seen are shown, so it does not give away secret pushwalls.  The on/off
 * setting is kept in CONFIG.WL1 and the explored tiles in save games.
 */
#ifndef WL_MINIMAP_H
#define WL_MINIMAP_H

#include "wl_persist.h"

#define MINIMAP_SAVEBYTES	(MAPSIZE*MAPSIZE/8)	// explored tiles, one bit each

extern boolean	minimapon;

void	Minimap_ResetSeen (void);		// a new level: nothing explored yet
void	Minimap_ViewOverlay (void);		// per frame, from ThreeDRefresh (viewoverlay)
void	Minimap_Save (wl_stream *s);		// MINIMAP_SAVEBYTES bytes
void	Minimap_Load (wl_stream *s);

#endif
