#ifndef __JHEXEN_DEMO_H__
#define __JHEXEN_DEMO_H__

// Demo playing modes.
typedef struct
{
	int		mode;			// Demo mode.
	int		targetplr;		// Target player (if needed).
	float	x, y, z;		// The position.
	float	viewangle;		// 0..360
	float	lookdir;		// -110..110
	float	dist;
	mobj_t	*mo;			// The camera mobj.
} democamdata_t;

// G_GAME.C
extern boolean			timingdemo;             // if true, exit with report on completion
extern democamdata_t	democam; 

#endif //__JHEXEN_DEMO_H__