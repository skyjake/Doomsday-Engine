
//**************************************************************************
//**
//** AM_MAP.C
//**
//** DOOM Automap. Modified GL version of the original.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <math.h>

#include "doomdef.h"
#include "d_config.h"
#include "st_stuff.h"
#include "p_local.h"

#include "m_cheat.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"

#include "am_map.h"
#include "Mn_def.h"
#include "m_menu.h"
#include "hu_stuff.h"
#include "wi_stuff.h"

// TYPES -------------------------------------------------------------------

typedef struct
{
    int x, y;
} fpoint_t;

typedef struct
{
    fpoint_t a, b;
} fline_t;

typedef struct
{
    fixed_t		x,y;
} mpoint_t;

typedef struct
{
    mpoint_t a, b;
} mline_t;

typedef struct
{
    fixed_t slp, islp;
} islope_t;

// MACROS ------------------------------------------------------------------

// Counter Cheat flags.
#define CCH_KILLS			0x1
#define CCH_ITEMS			0x2
#define CCH_SECRET			0x4
#define CCH_KILLS_PRCNT		0x8
#define CCH_ITEMS_PRCNT		0x10
#define CCH_SECRET_PRCNT	0x20

// For use if I do walls with outsides/insides
#define REDS		(256-5*16)
#define REDRANGE	16
#define BLUES		(256-4*16+8)
#define BLUERANGE	8
#define GREENS		(7*16)
#define GREENRANGE	16
#define GRAYS		(6*16)
#define GRAYSRANGE	16
#define BROWNS		(4*16)
#define BROWNRANGE	16
#define YELLOWS		(256-32+7)
#define YELLOWRANGE	1
#define BLACK		0
#define WHITE		(256-47)

// Automap colors
#define BACKGROUND		BLACK
#define YOURCOLORS		WHITE
#define YOURRANGE		0
#define WALLCOLORS		REDS
#define WALLRANGE		REDRANGE
#define TSWALLCOLORS	GRAYS
#define TSWALLRANGE		GRAYSRANGE
#define FDWALLCOLORS	BROWNS
#define FDWALLRANGE		BROWNRANGE
#define CDWALLCOLORS	YELLOWS
#define CDWALLRANGE		YELLOWRANGE
#define THINGCOLORS		GREENS
#define THINGRANGE		GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS		(GRAYS + GRAYSRANGE/2)
#define GRIDRANGE		0
#define XHAIRCOLORS		GRAYS

// drawing stuff
#define	FB		0

#define AM_PANDOWNKEY	DDKEY_DOWNARROW
#define AM_PANUPKEY		DDKEY_UPARROW
#define AM_PANRIGHTKEY	DDKEY_RIGHTARROW
#define AM_PANLEFTKEY	DDKEY_LEFTARROW
#define AM_ZOOMINKEY	'='
#define AM_ZOOMOUTKEY	'-'
#define AM_STARTKEY		DDKEY_TAB
#define AM_ENDKEY		DDKEY_TAB
#define AM_GOBIGKEY		'0'
#define AM_FOLLOWKEY	'f'
#define AM_GRIDKEY		'g'
#define AM_MARKKEY		'm'
#define AM_CLEARMARKKEY	'c'

#define AM_NUMMARKPOINTS 10

// scale on entry
#define INITSCALEMTOF (.2*FRACUNIT)
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC	4
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN        ((int) (1.02*FRACUNIT))
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT       ((int) (FRACUNIT/1.02))

// translates between frame-buffer and map distances
#define FTOM(x) FixedMul(((x)<<16),scale_ftom)
#define MTOF(x) (FixedMul((x),scale_mtof)>>16)
#define MTOFX(x) FixedMul((x),scale_mtof)
// translates between frame-buffer and map coordinates
#define CXMTOF(x)  (f_x + MTOF((x)-m_x))
#define CYMTOF(y)  (f_y + (f_h - MTOF((y)-m_y)))
#define CXMTOFX(x)	((f_x<<16) + MTOFX((x)-m_x))
#define CYMTOFX(y)  ((f_y<<16) + ((f_h<<16) - MTOFX((y)-m_y)))

// the following is crap
#define LINE_NEVERSEE ML_DONTDRAW

//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//
#define R ((8*PLAYERRADIUS)/7)
mline_t player_arrow[] = {
    { { -R+R/8, 0 }, { R, 0 } }, // -----
    { { R, 0 }, { R-R/2, R/4 } },  // ----->
    { { R, 0 }, { R-R/2, -R/4 } },
    { { -R+R/8, 0 }, { -R-R/8, R/4 } }, // >---->
    { { -R+R/8, 0 }, { -R-R/8, -R/4 } },
    { { -R+3*R/8, 0 }, { -R+R/8, R/4 } }, // >>--->
    { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
};
#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#define R ((8*PLAYERRADIUS)/7)
mline_t cheat_player_arrow[] = {
    { { -R+R/8, 0 }, { R, 0 } }, // -----
    { { R, 0 }, { R-R/2, R/6 } },  // ----->
    { { R, 0 }, { R-R/2, -R/6 } },
    { { -R+R/8, 0 }, { -R-R/8, R/6 } }, // >----->
    { { -R+R/8, 0 }, { -R-R/8, -R/6 } },
    { { -R+3*R/8, 0 }, { -R+R/8, R/6 } }, // >>----->
    { { -R+3*R/8, 0 }, { -R+R/8, -R/6 } },
    { { -R/2, 0 }, { -R/2, -R/6 } }, // >>-d--->
    { { -R/2, -R/6 }, { -R/2+R/6, -R/6 } },
    { { -R/2+R/6, -R/6 }, { -R/2+R/6, R/4 } },
    { { -R/6, 0 }, { -R/6, -R/6 } }, // >>-dd-->
    { { -R/6, -R/6 }, { 0, -R/6 } },
    { { 0, -R/6 }, { 0, R/4 } },
    { { R/6, R/4 }, { R/6, -R/7 } }, // >>-ddt->
    { { R/6, -R/7 }, { R/6+R/32, -R/7-R/32 } },
    { { R/6+R/32, -R/7-R/32 }, { R/6+R/10, -R/7 } }
};
#undef R
#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))

#define R (FRACUNIT)
mline_t triangle_guy[] = {
    { { -.867*R, -.5*R }, { .867*R, -.5*R } },
    { { .867*R, -.5*R } , { 0, R } },
    { { 0, R }, { -.867*R, -.5*R } }
};
#undef R
#define NUMTRIANGLEGUYLINES (sizeof(triangle_guy)/sizeof(mline_t))

#define R (FRACUNIT)
mline_t thintriangle_guy[] = {
    { { -.5*R, -.7*R }, { R, 0 } },
    { { R, 0 }, { -.5*R, .7*R } },
    { { -.5*R, .7*R }, { -.5*R, -.7*R } }
};
#undef R
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int 		cheating = 0;
boolean    	automapactive = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int	bigstate=0;
static int 	grid = 0;

static int 	leveljuststarted = 1; 	// kluge until AM_LevelInit() is called

static int 	finit_width = SCREENWIDTH;
static int 	finit_height = SCREENHEIGHT - 32;

// location of window on screen
static int 	f_x;
static int	f_y;

// size of window on screen
static int 	f_w;
static int	f_h;

static int 	lightlev; 		// used for funky strobing effect
//static byte*	fb; 			// pseudo-frame buffer
static int 	amclock;

static mpoint_t m_paninc; // how far the window pans each tic (map coords)
static fixed_t 	mtof_zoommul; // how far the window zooms in each tic (map coords)
static fixed_t 	ftom_zoommul; // how far the window zooms in each tic (fb coords)

static fixed_t 	m_x, m_y;   // LL x,y where the window is on the map (map coords)
static fixed_t 	m_x2, m_y2; // UR x,y where the window is on the map (map coords)

//
// width/height of window on map (map coords)
//
static fixed_t 	m_w;
static fixed_t	m_h;

// based on level size
static fixed_t 	min_x;
static fixed_t	min_y; 
static fixed_t 	max_x;
static fixed_t  max_y;

static fixed_t 	max_w; // max_x-min_x,
static fixed_t  max_h; // max_y-min_y

// based on player size
static fixed_t 	min_w;
static fixed_t  min_h;


static fixed_t 	min_scale_mtof; // used to tell when to stop zooming out
static fixed_t 	max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static fixed_t old_m_w, old_m_h;
static fixed_t old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr; // the player represented by an arrow

//static patch_t *marknums[10]; // numbers used for marking by the automap
static int markpnums[10];		// numbers used for marking by the automap (lump indices)
static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
static int markpointnum = 0; // next point to be assigned

static int followplayer = 1; // specifies whether to follow the player around

static unsigned char cheat_amap_seq[] = { 0xb2, 0x26, 0x26, 0x2e, 0xff };
static cheatseq_t cheat_amap = { cheat_amap_seq, 0 };

static boolean stopped = true;

extern boolean viewactive;

// CODE --------------------------------------------------------------------

// Calculates the slope and slope according to the x-axis of a line
// segment in map coordinates (with the upright y-axis n' all) so
// that it can be used with the brain-dead drawing stuff.

void
AM_getIslope
( mline_t*	ml,
  islope_t*	is )
{
    int dx, dy;

    dy = ml->a.y - ml->b.y;
    dx = ml->b.x - ml->a.x;
    if (!dy) is->islp = (dx<0?-MAXINT:MAXINT);
    else is->islp = FixedDiv(dx, dy);
    if (!dx) is->slp = (dy<0?-MAXINT:MAXINT);
    else is->slp = FixedDiv(dy, dx);

}

//
//
//
void AM_activateNewScale(void)
{
    m_x += m_w/2;
    m_y += m_h/2;
    m_w = FTOM(f_w);
    m_h = FTOM(f_h);
    m_x -= m_w/2;
    m_y -= m_h/2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
//
//
void AM_saveScaleAndLoc(void)
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

//
//
//
void AM_restoreScaleAndLoc(void)
{
    m_w = old_m_w;
    m_h = old_m_h;
    if (!followplayer)
    {
		m_x = old_m_x;
		m_y = old_m_y;
    } else {
		m_x = plr->plr->mo->x - m_w/2;
		m_y = plr->plr->mo->y - m_h/2;
    }
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w<<FRACBITS, m_w);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
// adds a marker at the current location
//
void AM_addMark(void)
{
    markpoints[markpointnum].x = m_x + m_w/2;
    markpoints[markpointnum].y = m_y + m_h/2;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;

}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
void AM_findMinMaxBoundaries(void)
{
    int i;
    fixed_t a;
    fixed_t b;

    min_x = min_y =  MAXINT;
    max_x = max_y = -MAXINT;
  
    for (i=0;i<numvertexes;i++)
    {
	if (vertexes[i].x < min_x)
	    min_x = vertexes[i].x;
	else if (vertexes[i].x > max_x)
	    max_x = vertexes[i].x;
    
	if (vertexes[i].y < min_y)
	    min_y = vertexes[i].y;
	else if (vertexes[i].y > max_y)
	    max_y = vertexes[i].y;
    }
  
    max_w = max_x - min_x;
    max_h = max_y - min_y;

    min_w = 2*PLAYERRADIUS; // const? never changed?
    min_h = 2*PLAYERRADIUS;

    a = FixedDiv(f_w<<FRACBITS, max_w);
    b = FixedDiv(f_h<<FRACBITS, max_h);
  
    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = FixedDiv(f_h<<FRACBITS, 2*PLAYERRADIUS);

}


//
//
//
void AM_changeWindowLoc(void)
{
    if (m_paninc.x || m_paninc.y)
    {
		followplayer = 0;
		f_oldloc.x = MAXINT;
    }
	
    m_x += m_paninc.x;
    m_y += m_paninc.y;
	
    if (m_x + m_w/2 > max_x)
		m_x = max_x - m_w/2;
    else if (m_x + m_w/2 < min_x)
		m_x = min_x - m_w/2;
	
    if (m_y + m_h/2 > max_y)
		m_y = max_y - m_h/2;
    else if (m_y + m_h/2 < min_y)
		m_y = min_y - m_h/2;
	
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}


//
//
//
void AM_initVariables(void)
{
    int pnum;
    static event_t st_notify = { ev_keyup, AM_MSGENTERED };

    automapactive = true;
//    fb = screens[0];

    f_oldloc.x = MAXINT;
    amclock = 0;
    lightlev = 0;

    m_paninc.x = m_paninc.y = 0;
    ftom_zoommul = FRACUNIT;
    mtof_zoommul = FRACUNIT;

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    // find player to center on initially
    if (!players[pnum = consoleplayer].plr->ingame)
		for (pnum=0;pnum<MAXPLAYERS;pnum++)
			if (players[pnum].plr->ingame)
				break;
  
    plr = &players[pnum];
    m_x = plr->plr->mo->x - m_w/2;
    m_y = plr->plr->mo->y - m_h/2;
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    // inform the status bar of the change
    ST_Responder(&st_notify);

}

//
// 
//
void AM_loadPics(void)
{
    int i;
    char namebuf[9];
  
    for (i=0;i<10;i++)
    {
		sprintf(namebuf, "AMMNUM%d", i);
		//marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
		markpnums[i] = W_GetNumForName(namebuf);
    }

}

void AM_unloadPics(void)
{
/*    int i;
  
    for (i=0;i<10;i++)
		Z_ChangeTag(marknums[i], PU_CACHE);
*/
}

void AM_clearMarks(void)
{
    int i;

    for (i=0;i<AM_NUMMARKPOINTS;i++)
	markpoints[i].x = -1; // means empty
    markpointnum = 0;
}

//
// should be called at the start of every level
// right now, i figure it out myself
//
void AM_LevelInit(void)
{
    leveljuststarted = 0;

    f_x = f_y = 0;
    f_w = finit_width;
    f_h = finit_height;

    AM_clearMarks();

    AM_findMinMaxBoundaries();
    scale_mtof = FixedDiv(min_scale_mtof, (int) (0.7*FRACUNIT));
    if (scale_mtof > max_scale_mtof)
	scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}




//
//
//
void AM_Stop (void)
{
    static event_t st_notify = { 0, ev_keyup, AM_MSGEXITED };

    AM_unloadPics();
    automapactive = false;
    ST_Responder(&st_notify);
    stopped = true;
}

//
//
//
void AM_Start (void)
{
    static int lastlevel = -1, lastepisode = -1;

    if (!stopped) AM_Stop();
    stopped = false;
    if (lastlevel != gamemap || lastepisode != gameepisode)
    {
		AM_LevelInit();
		lastlevel = gamemap;
		lastepisode = gameepisode;
    }
    AM_initVariables();
    AM_loadPics();
}

//
// set the window scale to the maximum size
//
void AM_minOutWindowScale(void)
{
    scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

//
// set the window scale to the minimum size
//
void AM_maxOutWindowScale(void)
{
    scale_mtof = max_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

int CCmdAutoMap(int argc, char **argv)
{
	if(gamestate != GS_LEVEL) return true;
	// Activate or deactivate automap.
    if(!automapactive)
    {
		AM_Start ();
		viewactive = false;
    }
	else
	{
		bigstate = 0;
		viewactive = true;
		AM_Stop ();
	}
	return true;
}

//
// Handle events (user inputs) in automap mode
//
boolean
AM_Responder
( event_t*	ev )
{
	
    int rc;
    static int cheatstate=0;
    static char buffer[20];
	
    rc = false;
	
    if(!automapactive)
    {
	/*	if(ev->type == ev_keydown && ev->data1 == AM_STARTKEY)
		{
			AM_Start ();
			viewactive = false;
			rc = true;
		}*/
    }
	else if (ev->type == ev_keydown)
    {
		rc = true;
		switch(ev->data1)
		{
		case AM_PANRIGHTKEY: // pan right
			if (!followplayer) m_paninc.x = FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_PANLEFTKEY: // pan left
			if (!followplayer) m_paninc.x = -FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_PANUPKEY: // pan up
			if (!followplayer) m_paninc.y = FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_PANDOWNKEY: // pan down
			if (!followplayer) m_paninc.y = -FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_ZOOMOUTKEY: // zoom out
			mtof_zoommul = M_ZOOMOUT;
			ftom_zoommul = M_ZOOMIN;
			break;
		case AM_ZOOMINKEY: // zoom in
			mtof_zoommul = M_ZOOMIN;
			ftom_zoommul = M_ZOOMOUT;
			break;
/*		case AM_ENDKEY:
			bigstate = 0;
			viewactive = true;
			AM_Stop ();
			break;*/
		case AM_GOBIGKEY:
			bigstate = !bigstate;
			if (bigstate)
			{
				AM_saveScaleAndLoc();
				AM_minOutWindowScale();
			}
			else AM_restoreScaleAndLoc();
			break;
		case AM_FOLLOWKEY:
			followplayer = !followplayer;
			f_oldloc.x = MAXINT;
			//plr->message = followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF;
			P_SetMessage(plr, followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF);
			break;
		case AM_GRIDKEY:
			grid = !grid;
			//plr->message = grid ? AMSTR_GRIDON : AMSTR_GRIDOFF;
			P_SetMessage(plr, grid ? AMSTR_GRIDON : AMSTR_GRIDOFF);
			break;
		case AM_MARKKEY:
			sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
			//plr->message = buffer;
			P_SetMessage(plr, buffer);
			AM_addMark();
			break;
		case AM_CLEARMARKKEY:
			AM_clearMarks();
			//plr->message = AMSTR_MARKSCLEARED;
			P_SetMessage(plr, AMSTR_MARKSCLEARED);
			break;
		default:
			cheatstate=0;
			rc = false;
		}
		if (!deathmatch && cht_CheckCheat(&cheat_amap, (char) ev->data1))
		{
			rc = false;
			cheating = (cheating+1) % 3;
		}
    }
    else if (ev->type == ev_keyup)
    {
		rc = false;
		switch (ev->data1)
		{
		case AM_PANRIGHTKEY:
			if (!followplayer) m_paninc.x = 0;
			break;
		case AM_PANLEFTKEY:
			if (!followplayer) m_paninc.x = 0;
			break;
		case AM_PANUPKEY:
			if (!followplayer) m_paninc.y = 0;
			break;
		case AM_PANDOWNKEY:
			if (!followplayer) m_paninc.y = 0;
			break;
		case AM_ZOOMOUTKEY:
		case AM_ZOOMINKEY:
			mtof_zoommul = FRACUNIT;
			ftom_zoommul = FRACUNIT;
			break;
		}
    }
	else if(ev->type == ev_keyrepeat) return true;
	
    return rc;
	
}


//
// Zooming
//
void AM_changeWindowScale(void)
{

    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    if (scale_mtof < min_scale_mtof)
		AM_minOutWindowScale();
    else if (scale_mtof > max_scale_mtof)
		AM_maxOutWindowScale();
    else
		AM_activateNewScale();
}


//
//
//
void AM_doFollowPlayer(void)
{
	
    if (f_oldloc.x != plr->plr->mo->x || f_oldloc.y != plr->plr->mo->y)
    {
		// Now that a high resolution is used (compared to 320x200!)
		// there is no need to quantify map scrolling. -jk
		m_x = /*FTOM(MTOF(*/plr->plr->mo->x/*))*/ - m_w/2;
		m_y = /*FTOM(MTOF(*/plr->plr->mo->y/*))*/ - m_h/2;
		m_x2 = m_x + m_w;
		m_y2 = m_y + m_h;
		f_oldloc.x = plr->plr->mo->x;
		f_oldloc.y = plr->plr->mo->y;
    }
}

//
//
//
void AM_updateLightLev(void)
{
    static int nexttic = 0;
    //static int litelevels[] = { 0, 3, 5, 6, 6, 7, 7, 7 };
    static int litelevels[] = { 0, 4, 7, 10, 12, 14, 15, 15 };
    static int litelevelscnt = 0;
   
    // Change light level
    if(amclock > nexttic)
    {
		lightlev = litelevels[litelevelscnt++];
		if (litelevelscnt == sizeof(litelevels)/sizeof(int)) litelevelscnt = 0;
		nexttic = amclock + 6 - (amclock % 6);
    }

}


//
// Updates on Game Tick
//
void AM_Ticker (void)
{

    if (!automapactive)
	return;

    amclock++;

    if (followplayer) AM_doFollowPlayer();

    // Change the zoom if necessary
    if (ftom_zoommul != FRACUNIT) AM_changeWindowScale();

    // Change x,y location
    if (m_paninc.x || m_paninc.y) AM_changeWindowLoc();

    // Update light level
    // AM_updateLightLev();
}


//
// Clear automap frame buffer.
//
void AM_clearFB(int color)
{
	extern char *BorderLumps[];
	float scaler;

	// Black background.
	GL_SetNoTexture();
	GL_DrawRect(0, 0, finit_width, finit_height, 0, 0, 0, 
		cfg.automapAlpha);
	
	//gl.Clear(DGL_COLOR_BUFFER_BIT);

	GL_SetColorAndAlpha(1, 1, 1, 1);
	GL_SetFlat(R_FlatNumForName(BorderLumps[0]));
	scaler = cfg.sbarscale/20.0f;
	GL_DrawCutRectTiled(0, finit_height+3, 320, 200-finit_height-3, 64, 64, 
		160-160*scaler+1, finit_height, 320*scaler-2, 200-finit_height);

	GL_SetPatch(W_GetNumForName("brdr_b"));
	GL_DrawCutRectTiled(0, finit_height, 320, 3, 
		16, 3,
		160-160*scaler+1, finit_height, 320*scaler-2, 3);

}


//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle  the common cases.
//
/*boolean
AM_clipMline
( mline_t*	ml,
  fline_t*	fl )
{
    enum
    {
	LEFT	=1,
	RIGHT	=2,
	BOTTOM	=4,
	TOP	=8
    };
    
    register	outcode1 = 0;
    register	outcode2 = 0;
    register	outside;
    
    fpoint_t	tmp;
    int		dx;
    int		dy;

    
#define DOOUTCODE(oc, mx, my) \
    (oc) = 0; \
    if ((my) < 0) (oc) |= TOP; \
    else if ((my) >= f_h) (oc) |= BOTTOM; \
    if ((mx) < 0) (oc) |= LEFT; \
    else if ((mx) >= f_w) (oc) |= RIGHT;

    
    // do trivial rejects and outcodes
    if (ml->a.y > m_y2)
	outcode1 = TOP;
    else if (ml->a.y < m_y)
	outcode1 = BOTTOM;

    if (ml->b.y > m_y2)
	outcode2 = TOP;
    else if (ml->b.y < m_y)
	outcode2 = BOTTOM;
    
    if (outcode1 & outcode2)
	return false; // trivially outside

    if (ml->a.x < m_x)
	outcode1 |= LEFT;
    else if (ml->a.x > m_x2)
	outcode1 |= RIGHT;
    
    if (ml->b.x < m_x)
	outcode2 |= LEFT;
    else if (ml->b.x > m_x2)
	outcode2 |= RIGHT;
    
    if (outcode1 & outcode2)
	return false; // trivially outside

    // transform to frame-buffer coordinates.
    fl->a.x = CXMTOF(ml->a.x);
    fl->a.y = CYMTOF(ml->a.y);
    fl->b.x = CXMTOF(ml->b.x);
    fl->b.y = CYMTOF(ml->b.y);

    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    DOOUTCODE(outcode2, fl->b.x, fl->b.y);

    if (outcode1 & outcode2)
	return false;

    while (outcode1 | outcode2)
    {
	// may be partially inside box
	// find an outside point
	if (outcode1)
	    outside = outcode1;
	else
	    outside = outcode2;
	
	// clip to each side
	if (outside & TOP)
	{
	    dy = fl->a.y - fl->b.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.x = fl->a.x + (dx*(fl->a.y))/dy;
	    tmp.y = 0;
	}
	else if (outside & BOTTOM)
	{
	    dy = fl->a.y - fl->b.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.x = fl->a.x + (dx*(fl->a.y-f_h))/dy;
	    tmp.y = f_h-1;
	}
	else if (outside & RIGHT)
	{
	    dy = fl->b.y - fl->a.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.y = fl->a.y + (dy*(f_w-1 - fl->a.x))/dx;
	    tmp.x = f_w-1;
	}
	else if (outside & LEFT)
	{
	    dy = fl->b.y - fl->a.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.y = fl->a.y + (dy*(-fl->a.x))/dx;
	    tmp.x = 0;
	}

	if (outside == outcode1)
	{
	    fl->a = tmp;
	    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
	}
	else
	{
	    fl->b = tmp;
	    DOOUTCODE(outcode2, fl->b.x, fl->b.y);
	}
	
	if (outcode1 & outcode2)
	    return false; // trivially outside
    }

    return true;
}
#undef DOOUTCODE


//
// Classic Bresenham w/ whatever optimizations needed for speed
//
void
AM_drawFline
( fline_t*	fl,
  int		color )
{
    register int x;
    register int y;
    register int dx;
    register int dy;
    register int sx;
    register int sy;
    register int ax;
    register int ay;
    register int d;
    
    static fuck = 0;

    // For debugging only
    if (      fl->a.x < 0 || fl->a.x >= f_w
	   || fl->a.y < 0 || fl->a.y >= f_h
	   || fl->b.x < 0 || fl->b.x >= f_w
	   || fl->b.y < 0 || fl->b.y >= f_h)
    {
	fprintf(stderr, "fuck %d \r", fuck++);
	return;
    }

#define PUTDOT(xx,yy,cc) fb[(yy)*f_w+(xx)]=(cc)

    dx = fl->b.x - fl->a.x;
    ax = 2 * (dx<0 ? -dx : dx);
    sx = dx<0 ? -1 : 1;

    dy = fl->b.y - fl->a.y;
    ay = 2 * (dy<0 ? -dy : dy);
    sy = dy<0 ? -1 : 1;

    x = fl->a.x;
    y = fl->a.y;

    if (ax > ay)
    {
	d = ay - ax/2;
	while (1)
	{
	    PUTDOT(x,y,color);
	    if (x == fl->b.x) return;
	    if (d>=0)
	    {
		y += sy;
		d -= ax;
	    }
	    x += sx;
	    d += ay;
	}
    }
    else
    {
	d = ax - ay/2;
	while (1)
	{
	    PUTDOT(x, y, color);
	    if (y == fl->b.y) return;
	    if (d >= 0)
	    {
		x += sx;
		d -= ay;
	    }
	    y += sy;
	    d += ax;
	}
    }
}
*/

int AM_getDoorColor(int special, boolean glowColor)
{
	// The colors are from the Doom PLAYPAL.
	switch(special)
	{
	default:
		// If it's not a door, zero is returned.
		break;

	case 32:		// Blue locked door open
	case 26:		// Blue Door/Locked
	case 99: 
	case 133:
		return glowColor? 198 : 195;
	
	case 33:		// Red locked door open
	case 28:		// Red Door /Locked
	case 134: 
	case 135:
		return glowColor? 174 : 170;

	case 34:		// Yellow locked door open
	case 27:		// Yellow Door /Locked
	case 136: 
	case 137:
		return glowColor? 231 : 224;
	}
	return 0;
}

//
// Clip lines, draw visible part sof lines.
//
void
AM_drawMline
( mline_t*	ml,
  int		color )
{
	GL_SetColor2(color, cfg.automapLineAlpha);
	gl.Vertex2f(FIX2FLT(CXMTOFX(ml->a.x)), FIX2FLT(CYMTOFX(ml->a.y)));
	gl.Vertex2f(FIX2FLT(CXMTOFX(ml->b.x)), FIX2FLT(CYMTOFX(ml->b.y)));
}

void AM_drawMlineGlow(mline_t *ml, int color)
{
	float a[2], b[2], normal[2], unit[2], thickness;
	float length, dx, dy;

	// Scale thickness according to map zoom.
	thickness = cfg.automapDoorGlow * FIX2FLT(scale_mtof) * 2.5f + 3;
	
	GL_SetColor2(color, cfg.automapLineAlpha/3);
	a[VX] = FIX2FLT(CXMTOFX(ml->a.x));
	a[VY] = FIX2FLT(CYMTOFX(ml->a.y));
	b[VX] = FIX2FLT(CXMTOFX(ml->b.x));
	b[VY] = FIX2FLT(CYMTOFX(ml->b.y));

	dx = b[VX] - a[VX];
	dy = b[VY] - a[VY];
	length = sqrt(dx * dx + dy * dy);
	if(length <= 0) return;

	unit[VX] = dx / length;
	unit[VY] = dy / length;
	normal[VX] = unit[VY];
	normal[VY] = -unit[VX];

	// Start of the line.
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(
		a[VX] - unit[VX] * thickness + normal[VX] * thickness,
		a[VY] - unit[VY] * thickness + normal[VY] * thickness);

	gl.TexCoord2f(0.5f, 0);
	gl.Vertex2f(
		a[VX] + normal[VX] * thickness,
		a[VY] + normal[VY] * thickness);

	gl.TexCoord2f(0.5f, 1);
	gl.Vertex2f(
		a[VX] - normal[VX] * thickness,
		a[VY] - normal[VY] * thickness);

	gl.TexCoord2f(0, 1);
	gl.Vertex2f(
		a[VX] - unit[VX] * thickness - normal[VX] * thickness,
		a[VY] - unit[VY] * thickness - normal[VY] * thickness);

	// The middle part of the line.
	gl.TexCoord2f(0.5f, 0);
	gl.Vertex2f(
		a[VX] + normal[VX] * thickness,
		a[VY] + normal[VY] * thickness);
	gl.Vertex2f(
		b[VX] + normal[VX] * thickness,
		b[VY] + normal[VY] * thickness);

	gl.TexCoord2f(0.5f, 1);
	gl.Vertex2f(
		b[VX] - normal[VX] * thickness,
		b[VY] - normal[VY] * thickness);
	gl.Vertex2f(
		a[VX] - normal[VX] * thickness,
		a[VY] - normal[VY] * thickness);

	// End of the line.
	gl.TexCoord2f(0.5f, 0);
	gl.Vertex2f(
		b[VX] + normal[VX] * thickness,
		b[VY] + normal[VY] * thickness);

	gl.TexCoord2f(1, 0);
	gl.Vertex2f(
		b[VX] + unit[VX] * thickness + normal[VX] * thickness,
		b[VY] + unit[VY] * thickness + normal[VY] * thickness);

	gl.TexCoord2f(1, 1);
	gl.Vertex2f(
		b[VX] + unit[VX] * thickness - normal[VX] * thickness,
		b[VY] + unit[VY] * thickness - normal[VY] * thickness);

	gl.TexCoord2f(0.5f, 1);
	gl.Vertex2f(
		b[VX] - normal[VX] * thickness,
		b[VY] - normal[VY] * thickness);
}

//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
void AM_drawGrid(int color)
{
    fixed_t x, y;
    fixed_t start, end;
    mline_t ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS))
	start += (MAPBLOCKUNITS<<FRACBITS)
	    - ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS));
    end = m_x + m_w;

    // draw vertical gridlines
    ml.a.y = m_y;
    ml.b.y = m_y+m_h;
	
	gl.Begin(DGL_LINES);
    for (x=start; x<end; x+=(MAPBLOCKUNITS<<FRACBITS))
    {
		ml.a.x = x;
		ml.b.x = x;
		AM_drawMline(&ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS))
		start += (MAPBLOCKUNITS<<FRACBITS) - ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS));
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.x = m_x;
    ml.b.x = m_x + m_w;
    for (y=start; y<end; y+=(MAPBLOCKUNITS<<FRACBITS))
    {
		ml.a.y = y;
		ml.b.y = y;
		AM_drawMline(&ml, color);
    }
	gl.End();
}

//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//
void AM_drawWalls(void)
{
    int i;
    static mline_t l;

	gl.Begin(DGL_LINES);
    for(i = 0; i < numlines; i++)
    {
		l.a.x = lines[i].v1->x;
		l.a.y = lines[i].v1->y;
		l.b.x = lines[i].v2->x;
		l.b.y = lines[i].v2->y;

		if(cheating == 2) // Debug cheat.
		{
			// Show active XG lines.
			if(lines[i].xg && lines[i].xg->active && (leveltime & 4))
			{
				AM_drawMline(&l, 250);
				continue;
			}
		}

		if (cheating || (lines[i].flags & ML_MAPPED))
		{
			if ((lines[i].flags & LINE_NEVERSEE) && !cheating)
				continue;
			if (!lines[i].backsector)
			{
				AM_drawMline(&l, WALLCOLORS + lightlev);
			}
			else
			{
				if (lines[i].special == 39)
				{ // teleporters
					AM_drawMline(&l, WALLCOLORS+WALLRANGE/2);
				}
				else if (lines[i].flags & ML_SECRET) // secret door
				{
					if (cheating) AM_drawMline(&l, SECRETWALLCOLORS + lightlev);
					else AM_drawMline(&l, WALLCOLORS+lightlev);
				}
				else if (cfg.automapShowDoors
					&& AM_getDoorColor(lines[i].special, false))
				{
					AM_drawMline(&l, AM_getDoorColor(lines[i].special, false));
				}
				else if (lines[i].backsector->floorheight
					!= lines[i].frontsector->floorheight) 
				{
					AM_drawMline(&l, FDWALLCOLORS + lightlev); // floor level change
				}
				else if (lines[i].backsector->ceilingheight
					!= lines[i].frontsector->ceilingheight) 
				{
					AM_drawMline(&l, CDWALLCOLORS + lightlev); // ceiling level change
				}
				else if (cheating) 
				{
					AM_drawMline(&l, TSWALLCOLORS + lightlev);
				}
			}
		}
		else if (plr->powers[pw_allmap])
		{
			if (!(lines[i].flags & LINE_NEVERSEE)) AM_drawMline(&l, GRAYS+3);
		}
    }
	gl.End();

	// Any glows?
	if(cfg.automapDoorGlow > 0)
	{
		gl.Enable( DGL_TEXTURING );
		gl.Bind( Get(DD_DYNLIGHT_TEXTURE) );
		gl.Func( DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE );
		gl.Begin( DGL_QUADS );

		for(i = 0; i < numlines; i++)
		{
			l.a.x = lines[i].v1->x;
			l.a.y = lines[i].v1->y;
			l.b.x = lines[i].v2->x;
			l.b.y = lines[i].v2->y;
			if(cheating || (lines[i].flags & ML_MAPPED))
			{
				if((lines[i].flags & LINE_NEVERSEE) && !cheating)
					continue;

				if(lines[i].backsector
					&& cfg.automapShowDoors
					&& AM_getDoorColor(lines[i].special, true))
				{
					AM_drawMlineGlow(&l, 
						AM_getDoorColor(lines[i].special, true));
				}
			}
		}
		gl.End();
		gl.Func( DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA );
		gl.Disable( DGL_TEXTURING );
	}
}


//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
void
AM_rotate
( fixed_t*	x,
  fixed_t*	y,
  angle_t	a )
{
    fixed_t tmpx;

    tmpx =
	FixedMul(*x,finecosine[a>>ANGLETOFINESHIFT])
	- FixedMul(*y,finesine[a>>ANGLETOFINESHIFT]);
    
    *y   =
	FixedMul(*x,finesine[a>>ANGLETOFINESHIFT])
	+ FixedMul(*y,finecosine[a>>ANGLETOFINESHIFT]);

    *x = tmpx;
}

void
AM_drawLineCharacter
( mline_t*	lineguy,
  int		lineguylines,
  fixed_t	scale,
  angle_t	angle,
  int		color,
  fixed_t	x,
  fixed_t	y )
{
    int		i;
    mline_t	l;

	gl.Begin(DGL_LINES);
    for (i=0;i<lineguylines;i++)
    {
		l.a.x = lineguy[i].a.x;
		l.a.y = lineguy[i].a.y;

		if (scale)
		{
			l.a.x = FixedMul(scale, l.a.x);
			l.a.y = FixedMul(scale, l.a.y);
		}

		if (angle) AM_rotate(&l.a.x, &l.a.y, angle);

		l.a.x += x;
		l.a.y += y;

		l.b.x = lineguy[i].b.x;
		l.b.y = lineguy[i].b.y;

		if (scale)
		{
			l.b.x = FixedMul(scale, l.b.x);
			l.b.y = FixedMul(scale, l.b.y);
		}

		if (angle) AM_rotate(&l.b.x, &l.b.y, angle);
	
		l.b.x += x;
		l.b.y += y;

		AM_drawMline(&l, color);
    }
	gl.End();
}

void AM_drawPlayers(void)
{
    int		i;
    player_t*	p;
    static int 	their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
    int		their_color = -1;
    int		color;
	angle_t ang;

    if(!IS_NETGAME)
    {
		ang = plr->plr->clAngle;
		if (cheating)
		    AM_drawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, 0,
				ang, WHITE, plr->plr->mo->x, plr->plr->mo->y);
		else
			AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, ang,
				WHITE, plr->plr->mo->x, plr->plr->mo->y);
		return;
    }

    for(i = 0; i < MAXPLAYERS; i++)
    {
		their_color++;
		p = &players[i];

		if ( deathmatch/* && !singledemo)*/ && p != plr)
			continue;

		if (!players[i].plr->ingame)
			continue;

		if (p->powers[pw_invisibility])
			color = 246; // *close* to black
		else
			color = their_colors[cfg.PlayerColor[i]];
	
		AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, 
			consoleplayer==i? p->plr->clAngle : p->plr->mo->angle,
			color, p->plr->mo->x, p->plr->mo->y);
    }
}

void
AM_drawThings
( int	colors,
  int 	colorrange)
{
    int		i;
    mobj_t*	t;

    for (i=0;i<numsectors;i++)
    {
		t = sectors[i].thinglist;
		while (t)
		{
			AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,
				16<<FRACBITS, t->angle, colors+lightlev, t->x, t->y);
			t = t->snext;
		}
    }
}

void AM_drawMarks(void)
{
    int i, fx, fy, w, h;

    for (i = 0; i < AM_NUMMARKPOINTS; i++)
    {
		if (markpoints[i].x != -1)
		{
			//      w = SHORT(marknums[i]->width);
			//      h = SHORT(marknums[i]->height);
			w = 5; // because something's wrong with the wad, i guess
			h = 6; // because something's wrong with the wad, i guess
			fx = CXMTOF(markpoints[i].x);
			fy = CYMTOF(markpoints[i].y)/1.2f;
			if (fx >= f_x && fx <= f_w - w && fy >= f_y && fy <= f_h - h)
			//V_DrawPatch(fx, fy, FB, marknums[i]);
				WI_DrawPatch(fx, fy, markpnums[i]);
		}
    }
}

void AM_drawCrosshair(int color)
{
//    fb[(f_w*(f_h+1))/2] = color; // single point for now
}

int scissorState[5];

void AM_GL_SetupState()
{
	int scrwidth = Get(DD_SCREEN_WIDTH);
	int scrheight = Get(DD_SCREEN_HEIGHT);
	float /*xs = scrwidth/320.0f,*/ ys = scrheight/200.0f;

	// Let's set up a scissor box to clip the map lines and stuff.
	// Store the old scissor state.
	gl.GetIntegerv(DGL_SCISSOR_TEST, scissorState);
	gl.GetIntegerv(DGL_SCISSOR_BOX, scissorState + 1);
	gl.Scissor(0, 0, scrwidth, finit_height*ys);
	gl.Enable(DGL_SCISSOR_TEST);

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Translatef(160, 83.5f, 0);
	gl.Scalef(1, 1/1.2f, 1);
	if(cfg.automapRotate && followplayer) // Rotate map?
		gl.Rotatef(plr->plr->clAngle/(float)ANGLE_MAX * 360 - 90, 0, 0, 1);
	gl.Translatef(-160, -83.5f, 0);
}

void AM_GL_RestoreState()
{
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
	
	if(!scissorState[0]) gl.Disable(DGL_SCISSOR_TEST);
	gl.Scissor(scissorState[1], scissorState[2], scissorState[3], scissorState[4]);
}

void AM_drawCounters(void)
{
	int x = 0, y = LINEHEIGHT_A*3/2;
	char buf[40], tmp[20];
	
	// Kills.
	if(cfg.counterCheat & (CCH_KILLS | CCH_KILLS_PRCNT))
	{
		strcpy(buf, "Kills: ");
		if(cfg.counterCheat & CCH_KILLS)
		{
			sprintf(tmp, "%i/%i ", plr->killcount, totalkills);
			strcat(buf, tmp);
		}
		if(cfg.counterCheat & CCH_KILLS_PRCNT)
		{
			sprintf(tmp, "%s%i%%%s", 
				cfg.counterCheat & CCH_KILLS? "(" : "",
				totalkills? plr->killcount*100/totalkills
				: 100,
				cfg.counterCheat & CCH_KILLS? ")" : "");
			strcat(buf, tmp);
		}
		M_WriteText(x, y, buf);
		y += LINEHEIGHT_A;
	}
	// Items.
	if(cfg.counterCheat & (CCH_ITEMS | CCH_ITEMS_PRCNT))
	{
		strcpy(buf, "Items: ");
		if(cfg.counterCheat & CCH_ITEMS)
		{
			sprintf(tmp, "%i/%i ", plr->itemcount, totalitems);
			strcat(buf, tmp);
		}
		if(cfg.counterCheat & CCH_ITEMS_PRCNT)
		{
			sprintf(tmp, "%s%i%%%s", 
				cfg.counterCheat & CCH_ITEMS? "(" : "",
				totalitems? plr->itemcount*100/totalitems
				: 100,
				cfg.counterCheat & CCH_ITEMS? ")" : "");
			strcat(buf, tmp);
		}
		M_WriteText(x, y, buf);
		y += LINEHEIGHT_A;
	}
	// Secrets.
	if(cfg.counterCheat & (CCH_SECRET | CCH_SECRET_PRCNT))
	{
		strcpy(buf, "Secret: ");
		if(cfg.counterCheat & CCH_SECRET)
		{
			sprintf(tmp, "%i/%i ", plr->secretcount, totalsecret);
			strcat(buf, tmp);
		}
		if(cfg.counterCheat & CCH_SECRET_PRCNT)
		{
			sprintf(tmp, "%s%i%%%s", 
				cfg.counterCheat & CCH_SECRET? "(" : "",
				totalsecret? plr->secretcount*100/totalsecret
				: 100,
				cfg.counterCheat & CCH_SECRET? ")" : "");
			strcat(buf, tmp);
		}
		M_WriteText(x, y, buf);
		y += LINEHEIGHT_A;
	}
}

//===========================================================================
// AM_drawFragsTable
//	Draws a sorted frags list in the lower right corner of the automap.
//===========================================================================
void AM_drawFragsTable(void)
{
#define FRAGS_DRAWN	-99999
	int i, k, y, inCount = 0;	// How many players in the game?
	int totalFrags[MAXPLAYERS];
	int max, choose;
	int w = 30;
	char *name, tmp[40];

	memset(totalFrags, 0, sizeof(totalFrags));
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].plr->ingame) continue;
		inCount++;
		for(k = 0; k < MAXPLAYERS; k++)
			totalFrags[i] += players[i].frags[k] * (k!=i? 1 : -1);
	}
	
	// Start drawing from the top.
	for(y = HU_TITLEY + 32*(20-cfg.sbarscale)/20 - (inCount-1)*LINEHEIGHT_A, 
		i = 0; i < inCount; i++, y += LINEHEIGHT_A)
	{
		// Find the largest.
		for(max = FRAGS_DRAWN+1, k = 0; k < MAXPLAYERS; k++)
		{
			if(!players[k].plr->ingame || totalFrags[k] == FRAGS_DRAWN)
				continue;
			if(totalFrags[k] > max)
			{
				choose = k;
				max = totalFrags[k];
			}
		}
		// Draw the choice.
		name = Net_GetPlayerName(choose);
		switch(cfg.PlayerColor[choose])
		{
		case 0: // green 
			gl.Color3f(0, .8f, 0);
			break;

		case 1: // gray
			gl.Color3f(.45f, .45f, .45f);
			break;

		case 2: // brown
			gl.Color3f(.7f, .5f, .4f);
			break;

		case 3: // red
			gl.Color3f(1, 0, 0);
			break;
		}
		M_WriteText2(320 - w - M_StringWidth(name, hu_font_a) - 6, 
			y, name, hu_font_a, -1, -1, -1);
		// A colon.
		M_WriteText2(320 - w - 5, y, ":", hu_font_a, -1, -1, -1);
		// The frags count.
		sprintf(tmp, "%i", totalFrags[choose]);
		M_WriteText2(320 - w, y, tmp, hu_font_a, 1, 1, 1);
		// Mark to ignore in the future.
		totalFrags[choose] = FRAGS_DRAWN;
	}
}

void AM_Drawer (void)
{
    if (!automapactive) return;

	// Setup.
	finit_height = SCREENHEIGHT - 32*cfg.sbarscale/20;
	GL_Update(DDUF_FULLSCREEN);
	AM_clearFB(BACKGROUND);
	AM_GL_SetupState();
	gl.Disable(DGL_TEXTURING);
	
	// Draw.
    if (grid) AM_drawGrid(GRIDCOLORS);
    AM_drawWalls();
    AM_drawPlayers();
    if (cheating==2) AM_drawThings(THINGCOLORS, THINGRANGE);
    AM_drawCrosshair(XHAIRCOLORS);
	gl.Enable(DGL_TEXTURING);
    AM_drawMarks();
	
	// Restore.
	AM_GL_RestoreState();

	if(cfg.counterCheat) AM_drawCounters();
	if(deathmatch) AM_drawFragsTable();
}


