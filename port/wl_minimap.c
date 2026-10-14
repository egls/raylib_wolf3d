/*
 * wl_minimap.c - an overhead minimap in the top right corner of the 3D view.
 *
 * Drawn into the 8-bit view buffer with VL_Plot after the walls, sprites
 * and weapon, so it looks the same on every backend (desktop and web) and
 * uses the game palette.  North is up; the map scrolls with the player.
 *
 * Fog of war: the raycaster marks every floor tile a ray crosses in
 * spotvis[] each frame.  Those tiles are remembered in seen[] for the rest
 * of the level, and a wall or door is drawn when it touches a seen tile.
 * A secret pushwall is a wall in tilemap[] until it moves, so the map does
 * not reveal secrets.
 *
 * Drawing never changes game state, so demos stay in sync with it on.
 */

#include "WL_DEF.H"
#include "wl_minimap.h"

boolean		minimapon;

static byte	seen[MAPSIZE][MAPSIZE];

// palette indices (GAMEPAL)
#define C_UNSEEN	0		// black
#define C_FLOOR		27		// dark grey
#define C_WALL		18		// light grey
#define C_DOOR		9		// light blue
#define C_GOLDDOOR	14		// yellow
#define C_SILVERDOOR 11		// light cyan
#define C_EXIT		10		// light green: elevator door and switch
#define C_BORDER	24
#define C_PLAYER	15		// white
#define C_HEADING	12		// light red


void Minimap_ResetSeen (void)
{
	memset (seen,0,sizeof(seen));
}


static void MarkSeen (void)
{
	int	x,y;

	for (x=0;x<MAPSIZE;x++)
		for (y=0;y<MAPSIZE;y++)
			if (spotvis[x][y])
				seen[x][y] = 1;
}


static boolean NearSeen (int x, int y)
{
	int	dx,dy,nx,ny;

	for (dx=-1;dx<=1;dx++)
		for (dy=-1;dy<=1;dy++)
		{
			nx = x+dx;
			ny = y+dy;
			if (nx >= 0 && nx < MAPSIZE && ny >= 0 && ny < MAPSIZE && seen[nx][ny])
				return true;
		}
	return false;
}


static int TileColor (int x, int y)
{
	unsigned	tile;
	doorobj_t	*door;

	if (x < 0 || x >= MAPSIZE || y < 0 || y >= MAPSIZE)
		return C_UNSEEN;

	tile = tilemap[x][y];
	if (!tile)							// floor
		return seen[x][y] ? C_FLOOR : C_UNSEEN;

	if (!NearSeen (x,y))
		return C_UNSEEN;

	if ((tile & 0xc0) == 0x80)			// door: 0x80 | door number
	{
		door = &doorobjlist[tile & 0x3f];
		switch (door->lock)
		{
		case dr_lock1:		return C_GOLDDOOR;
		case dr_lock2:		return C_SILVERDOOR;
		case dr_elevator:	return C_EXIT;
		default:			return C_DOOR;
		}
	}

	// a wall, a door frame (0x40) or a moving pushwall (0xc0)
	if ((tile & 0x3f) == ELEVATORTILE)
		return C_EXIT;
	return C_WALL;
}


static void Cell (int x, int y, int size, int color)
{
	int	i,j;

	for (j=0;j<size;j++)
		for (i=0;i<size;i++)
			VL_Plot (x+i,y+j,color);
}


// sintable holds sign-magnitude values: bit 31 is the sign (see BuildTables)
static int32_t Trig (fixed v)
{
	int32_t	mag = (int32_t)(v & 0x7fffffffl);

	return (v & 0x80000000l) ? -mag : mag;
}


static void Draw (void)
{
	int		cell,radius,size,ox,oy,cx,cy,dx,dy,i,x,y;
	int32_t	cosv,sinv;

	if (viewheight < 48 || !player)
		return;

	cell = viewheight >= 140 ? 3 : 2;
	radius = viewheight*2/5 / (2*cell);
	if (radius > 12)
		radius = 12;
	size = (2*radius+1)*cell;

	ox = viewwidth - size - 4;			// top left of the map, inside the border
	oy = 4;

	for (i=-1;i<=size;i++)
	{
		VL_Plot (ox+i,oy-1,C_BORDER);
		VL_Plot (ox+i,oy+size,C_BORDER);
		VL_Plot (ox-1,oy+i,C_BORDER);
		VL_Plot (ox+size,oy+i,C_BORDER);
	}

	for (dy=-radius;dy<=radius;dy++)
		for (dx=-radius;dx<=radius;dx++)
			Cell (ox+(dx+radius)*cell,oy+(dy+radius)*cell,cell,
				TileColor (player->tilex+dx,player->tiley+dy));

	// the player: where it is inside its tile, and which way it faces
	cx = ox + radius*cell + (int)(((player->x & 0xffff)*cell) >> 16);
	cy = oy + radius*cell + (int)(((player->y & 0xffff)*cell) >> 16);

	cosv = Trig (costable[player->angle]);
	sinv = Trig (sintable[player->angle]);
	for (i=1;i<=cell+3;i++)
	{
		x = cx + (int)((cosv*i + 0x8000) >> 16);
		y = cy - (int)((sinv*i + 0x8000) >> 16);	// angles grow towards north (-y)
		if (x >= ox && x < ox+size && y >= oy && y < oy+size)
			VL_Plot (x,y,C_HEADING);
	}
	VL_Plot (cx,cy,C_PLAYER);
	if (cell > 2)
	{
		VL_Plot (cx-1,cy,C_PLAYER);
		VL_Plot (cx+1,cy,C_PLAYER);
		VL_Plot (cx,cy-1,C_PLAYER);
		VL_Plot (cx,cy+1,C_PLAYER);
	}
}


void Minimap_ViewOverlay (void)
{
	MarkSeen ();
	if (minimapon)
		Draw ();
}


void Minimap_Save (wl_stream *s)
{
	byte	bits[MINIMAP_SAVEBYTES];
	int		i;

	memset (bits,0,sizeof(bits));
	for (i=0;i<MAPSIZE*MAPSIZE;i++)
		if ((&seen[0][0])[i])
			bits[i>>3] |= 1 << (i&7);
	WS_WBytes (s,bits,sizeof(bits));
}


void Minimap_Load (wl_stream *s)
{
	byte	bits[MINIMAP_SAVEBYTES];
	int		i;

	WS_RBytes (s,bits,sizeof(bits));
	for (i=0;i<MAPSIZE*MAPSIZE;i++)
		(&seen[0][0])[i] = (bits[i>>3] >> (i&7)) & 1;
}
