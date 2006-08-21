/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Compiles for jDoom/jHeretic/jHexen
 *
 *  TODO: Clean up!
 *    Baby mode Keys in jHexen!
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <math.h>
#include <ctype.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "g_common.h"
#include "p_player.h"

// TYPES -------------------------------------------------------------------

typedef struct mpoint_s {
    float pos[3];
} mpoint_t;

typedef struct mapline_s {
    float r, g, b, a, a2, w;    // a2 is used in glow mode in case the glow
                                // needs a different alpha
    boolean glow;
    boolean scale;
} mapline_t;

typedef struct mline_s {
    mpoint_t a, b;
} mline_t;

typedef struct islope_s {
    fixed_t slp, islp;
} islope_t;

typedef enum glowtype_e {
    NO_GLOW = -1,
    TWOSIDED_GLOW,
    BACK_GLOW,
    FRONT_GLOW
} glowtype_t;

// MACROS ------------------------------------------------------------------

#define thinkercap    (*gi.thinkercap)

//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//

#define R (1.0f)
mline_t         keysquare[] = {
    {{0, 0}, {R / 4, -R / 2}},
    {{R / 4, -R / 2}, {R / 2, -R / 2}},
    {{R / 2, -R / 2}, {R / 2, R / 2}},
    {{R / 2, R / 2}, {R / 4, R / 2}},
    {{R / 4, R / 2}, {0, 0}},       // handle part type thing
    {{0, 0}, {-R, 0}},               // stem
    {{-R, 0}, {-R, -R / 2}},       // end lockpick part
    {{-3 * R / 4, 0}, {-3 * R / 4, -R / 4}}
};
#undef R
#define NUMKEYSQUARELINES (sizeof(keysquare) / sizeof(mline_t))

#define R (1.0f)
mline_t         thintriangle_guy[] = {
    {{-R / 2, R - R / 2}, {R, 0}}, // >
    {{R, 0}, {-R / 2, -R + R / 2}},
    {{-R / 2, -R + R / 2}, {-R / 2, R - R / 2}} // |>
};
#undef R

#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy) / sizeof(mline_t))


// JDOOM Stuff
#ifdef __JDOOM__

#define R (1.0f)
mline_t player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},    // -----
    {{R, 0}, {R - R / 2, R / 4}},    // ----->
    {{R, 0}, {R - R / 2, -R / 4}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 4}},    // >---->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 4}},    // >>--->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4}}
};
#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#define R (1.0f)
mline_t cheat_player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},    // -----
    {{R, 0}, {R - R / 2, R / 6}},    // ----->
    {{R, 0}, {R - R / 2, -R / 6}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 6}},    // >----->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 6}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 6}},    // >>----->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6}},
    {{-R / 2, 0}, {-R / 2, -R / 6}},    // >>-d--->
    {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
    {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
    {{-R / 6, 0}, {-R / 6, -R / 6}},    // >>-dd-->
    {{-R / 6, -R / 6}, {0, -R / 6}},
    {{0, -R / 6}, {0, R / 4}},
    {{R / 6, R / 4}, {R / 6, -R / 7}},    // >>-ddt->
    {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
    {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}
};

#undef R
#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))
#endif


//------------------------------------------------------------------------
// JHERETIC Stuff
#ifdef __JHERETIC__

#define R (1.0f)
mline_t         player_arrow[] = {
    {{-R + R / 4, 0}, {0, 0}},       // center line.
    {{-R + R / 4, R / 8}, {R, 0}}, // blade
    {{-R + R / 4, -R / 8}, {R, 0}},
    {{-R + R / 4, -R / 4}, {-R + R / 4, R / 4}},    // crosspiece
    {{-R + R / 8, -R / 4}, {-R + R / 8, R / 4}},
    {{-R + R / 8, -R / 4}, {-R + R / 4, -R / 4}},    //crosspiece connectors
    {{-R + R / 8, R / 4}, {-R + R / 4, R / 4}},
    {{-R - R / 4, R / 8}, {-R - R / 4, -R / 8}},    //pommel
    {{-R - R / 4, R / 8}, {-R + R / 8, R / 8}},
    {{-R - R / 4, -R / 8}, {-R + R / 8, -R / 8}}
};
#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#define R (1.0f)
mline_t         cheat_player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},       // -----
    {{R, 0}, {R - R / 2, R / 6}},  // ----->
    {{R, 0}, {R - R / 2, -R / 6}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 6}},    // >----->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 6}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 6}},    // >>----->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6}},
    {{-R / 2, 0}, {-R / 2, -R / 6}},    // >>-d--->
    {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
    {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
    {{-R / 6, 0}, {-R / 6, -R / 6}},    // >>-dd-->
    {{-R / 6, -R / 6}, {0, -R / 6}},
    {{0, -R / 6}, {0, R / 4}},
    {{R / 6, R / 4}, {R / 6, -R / 7}},    // >>-ddt->
    {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
    {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}
};

#undef R
#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))
#endif


// JHEXEN Stuff ------------------------------------------------------------
#ifdef __JHEXEN__

#define R (1.0f)
mline_t         player_arrow[] = {
    {{-R + R / 4, 0}, {0, 0}},       // center line.
    {{-R + R / 4, R / 8}, {R, 0}}, // blade
    {{-R + R / 4, -R / 8}, {R, 0}},
    {{-R + R / 4, -R / 4}, {-R + R / 4, R / 4}},    // crosspiece
    {{-R + R / 8, -R / 4}, {-R + R / 8, R / 4}},
    {{-R + R / 8, -R / 4}, {-R + R / 4, -R / 4}},    //crosspiece connectors
    {{-R + R / 8, R / 4}, {-R + R / 4, R / 4}},
    {{-R - R / 4, R / 8}, {-R - R / 4, -R / 8}},    //pommel
    {{-R - R / 4, R / 8}, {-R + R / 8, R / 8}},
    {{-R - R / 4, -R / 8}, {-R + R / 8, -R / 8}}
};

#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#endif

// JSTRIFE Stuff ------------------------------------------------------------
#if __JSTRIFE__

#define R (1.0f)
mline_t         player_arrow[] = {
    {{-R + R / 4, 0}, {0, 0}},       // center line.
    {{-R + R / 4, R / 8}, {R, 0}}, // blade
    {{-R + R / 4, -R / 8}, {R, 0}},
    {{-R + R / 4, -R / 4}, {-R + R / 4, R / 4}},    // crosspiece
    {{-R + R / 8, -R / 4}, {-R + R / 8, R / 4}},
    {{-R + R / 8, -R / 4}, {-R + R / 4, -R / 4}},    //crosspiece connectors
    {{-R + R / 8, R / 4}, {-R + R / 4, R / 4}},
    {{-R - R / 4, R / 8}, {-R - R / 4, -R / 8}},    //pommel
    {{-R - R / 4, R / 8}, {-R + R / 8, R / 8}},
    {{-R - R / 4, -R / 8}, {-R + R / 8, -R / 8}}
};

#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#endif

// Used for Baby mode
ddvertex_t KeyPoints[NUMBEROFKEYS];

// scale on entry
#define INITSCALEMTOF (.2*FRACUNIT)

// how much the automap moves window per tic in frame-buffer coordinates
#define F_PANINC    4                   // moves 140 pixels in 1 second

// how much zoom-in per tic
#define M_ZOOMIN        ((int) (1.02*FRACUNIT))        // goes to 2x in 1 second

// how much zoom-out per tic
#define M_ZOOMOUT       ((int) (FRACUNIT/1.02))        // pulls out to 0.5x in 1 second

// translates between frame-buffer and map distances
#define FTOM(x) FixedMul(((x)<<16),scale_ftom)
#define MTOF(x) (FixedMul((x),scale_mtof)>>16)
#define MTOFX(x) FixedMul((x),scale_mtof)

// translates between frame-buffer and map coordinates
#define CXMTOF(x)  (f_x + MTOF((x)-m_x))
#define CYMTOF(y)  (f_y + (f_h - MTOF((y)-m_y)))
#define CXMTOFX(x)  ((f_x<<16) + MTOFX((x)-m_x))
#define CYMTOFX(y)  ((f_y<<16) + ((f_h<<16) - MTOFX((y)-m_y)))

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdMapAction);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#ifdef __JDOOM__
void AM_drawFragsTable(void);
#elif __JHEXEN__
void AM_drawDeathmatchStats(void);
#elif __JSTRIFE__
void AM_drawDeathmatchStats(void);
#endif

static void AM_changeWindowScale(void);
static void AM_drawLevelName(void);
static void AM_drawWorldTimer(void);

void    AM_drawMline2(mline_t *ml, mapline_t *c, boolean caps, \
                      boolean glowmode, boolean blend);

void    AM_Start(void);
void    AM_addMark(void);
void    AM_clearMarks(void);
void    AM_saveScaleAndLoc(void);
void    AM_minOutWindowScale(void);
void    AM_saveScaleAndLoc(void);
void    AM_restoreScaleAndLoc(void);
void    AM_setWinPos(void);

void    M_DrawMapMenu(void);

//automap menu items
void    M_MapPosition(int option, void *data);
void    M_MapWidth(int option, void *data);
void    M_MapHeight(int option, void *data);
void    M_MapLineAlpha(int option, void *data);
void    M_MapDoorColors(int option, void *data);
void    M_MapDoorGlow(int option, void *data);
void    M_MapRotate(int option, void *data);
void    M_MapStatusbar(int option, void *data);
void    M_MapKills(int option, void *data);
void    M_MapItems(int option, void *data);
void    M_MapSecrets(int option, void *data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float menu_alpha;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     cheating = 0;
boolean automapactive = false;
boolean amap_fullyopen = false;

cvar_t  mapCVars[] = {
/*    {"map-position", 0, CVT_INT, &cfg.automapPos, 0, 8},
    {"map-width", 0, CVT_FLOAT, &cfg.automapWidth, 0, 1},
    {"map-height", 0, CVT_FLOAT, &cfg.automapHeight, 0, 1},*/
    {"map-color-unseen-r", 0, CVT_FLOAT, &cfg.automapL0[0], 0, 1},
    {"map-color-unseen-g", 0, CVT_FLOAT, &cfg.automapL0[1], 0, 1},
    {"map-color-unseen-b", 0, CVT_FLOAT, &cfg.automapL0[2], 0, 1},
    {"map-color-wall-r", 0, CVT_FLOAT, &cfg.automapL1[0], 0, 1},
    {"map-color-wall-g", 0, CVT_FLOAT, &cfg.automapL1[1], 0, 1},
    {"map-color-wall-b", 0, CVT_FLOAT, &cfg.automapL1[2], 0, 1},
    {"map-color-floor-r", 0, CVT_FLOAT, &cfg.automapL2[0], 0, 1},
    {"map-color-floor-g", 0, CVT_FLOAT, &cfg.automapL2[1], 0, 1},
    {"map-color-floor-b", 0, CVT_FLOAT, &cfg.automapL2[2], 0, 1},
    {"map-color-ceiling-r", 0, CVT_FLOAT, &cfg.automapL3[0], 0, 1},
    {"map-color-ceiling-g", 0, CVT_FLOAT, &cfg.automapL3[1], 0, 1},
    {"map-color-ceiling-b", 0, CVT_FLOAT, &cfg.automapL3[2], 0, 1},
    {"map-background-r", 0, CVT_FLOAT, &cfg.automapBack[0], 0, 1},
    {"map-background-g", 0, CVT_FLOAT, &cfg.automapBack[1], 0, 1},
    {"map-background-b", 0, CVT_FLOAT, &cfg.automapBack[2], 0, 1},
    {"map-background-a", 0, CVT_FLOAT, &cfg.automapBack[3], 0, 1},
    {"map-alpha-lines", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1},
    {"map-rotate", 0, CVT_BYTE, &cfg.automapRotate, 0, 1},
    {"map-huddisplay", 0, CVT_INT, &cfg.automapHudDisplay, 0, 2},
    {"map-door-colors", 0, CVT_BYTE, &cfg.automapShowDoors, 0, 1},
    {"map-door-glow", 0, CVT_FLOAT, &cfg.automapDoorGlow, 0, 200},
#ifndef __JHEXEN__
#ifndef __JSTRIFE__
    {"map-cheat-counter", 0, CVT_BYTE, &cfg.counterCheat, 0, 63},
    {"map-cheat-counter-scale", 0, CVT_FLOAT, &cfg.counterCheatScale, .1f, 1},
    {"map-babykeys", 0, CVT_BYTE, &cfg.automapBabyKeys, 0, 1},
#endif
#endif
    {NULL}
};

ccmd_t  mapCCmds[] = {
    {"automap",     CCmdMapAction},
    {"follow",      CCmdMapAction},
    {"rotate",      CCmdMapAction},
    {"addmark",     CCmdMapAction},
    {"clearmarks",  CCmdMapAction},
    {"grid",        CCmdMapAction},
    {"zoommax",     CCmdMapAction},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifdef __JDOOM__

static int keycolors[] = { KEY1, KEY2, KEY3, KEY4, KEY5, KEY6 };

static int maplumpnum = 0;    // pointer to the raw data for the automap background.
                // if 0 no background image will be drawn

#elif __JHERETIC__

static int keycolors[] = { KEY1, KEY2, KEY3 };

static int maplumpnum = 1;    // pointer to the raw data for the automap background.
                // if 0 no background image will be drawn
#else

static int keycolors[] = { KEY1, KEY2, KEY3 };

boolean ShowKills = 0;
unsigned ShowKillsCount = 0;

static int their_colors[] = {
    AM_PLR1_COLOR,
    AM_PLR2_COLOR,
    AM_PLR3_COLOR,
    AM_PLR4_COLOR,
    AM_PLR5_COLOR,
    AM_PLR6_COLOR,
    AM_PLR7_COLOR,
    AM_PLR8_COLOR
};

static int maplumpnum = 1;    // pointer to the raw data for the automap background.
                // if 0 no background image will be drawn

#endif

static int scrwidth = 0;    // real screen dimensions
static int scrheight = 0;

static int finit_height = SCREENHEIGHT;
//static int finit_width = SCREENWIDTH;

static float am_alpha = 0;

static int bigstate = 0;
static int grid = 0;
static int followplayer = 1;        // specifies whether to follow the player around

static int leveljuststarted = 1;    // kluge until AM_LevelInit() is called

// location of window on screen
static int f_x;
static int f_y;

// size of window on screen
static int f_w;
static int f_h;

static int lightlev;            // used for funky strobing effect

//static byte*  fb;                 // pseudo-frame buffer
static int amclock;

static mpoint_t m_paninc;        // how far the window pans each tic (map coords)
static fixed_t mtof_zoommul;        // how far the window zooms in each tic (map coords)
static fixed_t ftom_zoommul;        // how far the window zooms in each tic (fb coords)

static fixed_t m_x, m_y;        // LL x,y where the window is on the map (map coords)
static fixed_t m_x2, m_y2;        // UR x,y where the window is on the map (map coords)

// width/height of window on map (map coords)
static fixed_t m_w;
static fixed_t m_h;

// based on level size
static fixed_t min_x;
static fixed_t min_y;
static fixed_t max_x;
static fixed_t max_y;

static fixed_t max_w;            // max_x-min_x,
static fixed_t max_h;            // max_y-min_y

// based on player size
static fixed_t min_w;
static fixed_t min_h;

static fixed_t min_scale_mtof;        // used to tell when to stop zooming out
static fixed_t max_scale_mtof;        // used to tell when to stop zooming in

// old stuff for recovery later
static fixed_t old_m_w, old_m_h;
static fixed_t old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = INITSCALEMTOF;

// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr;                // the player represented by an arrow

static int markpnums[10];            // numbers used for marking by the automap (lump indices)
static mpoint_t markpoints[AM_NUMMARKPOINTS];    // where the points are
static int markpointnum = 0;            // next point to be assigned

static boolean stopped = true;

extern boolean viewactive;

// Where the window should be on screen, and the dimensions
static int sx0, sy0, sx1, sy1;

// where the window currently is on screen, and the dimensions
static int winx, winy, winw, winh;

static int winw = 0;
static int winh = 0;

// Used to track if a re-scale is needed (automap cfg is changed)
static int oldwin_w = 0;
static int oldwin_h = 0;

// Used in subsector debug display.
static mapline_t subColors[10];

// CODE --------------------------------------------------------------------

/*
 * Register cvars and ccmds for the automap
 * Called during the PreInit of each game
 */
void AM_Register(void)
{
    int     i;

    for(i = 0; mapCVars[i].name; i++)
        Con_AddVariable(mapCVars + i);
    for(i = 0; mapCCmds[i].name; i++)
        Con_AddCommand(mapCCmds + i);
}

/*
 * Handle the console commands for the automap
 */
DEFCC(CCmdMapAction)
{
    static char buffer[20];

    if(gamestate != GS_LEVEL)
    {
        Con_Printf("The automap is only available in-game.\n");
        return false;
    }

    // Active commands while automap is active
    if(automapactive)
    {
        if(!stricmp(argv[0], "automap"))  // close automap
        {
            bigstate = 0;
            viewactive = true;

            // Disable the automap binding classes
            DD_SetBindClass(GBC_CLASS1, false);

            if(!followplayer)
                DD_SetBindClass(GBC_CLASS2, false);

            AM_Stop();
            return true;
        }

        if(!stricmp(argv[0], "follow"))  // follow mode toggle
        {
            followplayer = !followplayer;
            f_oldloc.pos[VX] = (float) DDMAXINT;

            // Enable/disable the follow mode binding class
            if(!followplayer)
                DD_SetBindClass(GBC_CLASS2, true);
            else
                DD_SetBindClass(GBC_CLASS2, false);

            P_SetMessage(plr, (followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), false);
            Con_Printf("Follow mode toggle.\n");
            return true;
        }

        if(!stricmp(argv[0], "rotate"))  // rotate mode toggle
        {
            cfg.automapRotate = !cfg.automapRotate;
            P_SetMessage(plr, (cfg.automapRotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF), false);
            Con_Printf("Rotate mode toggle.\n");
            return true;
        }

        if(!stricmp(argv[0], "addmark"))  // add a mark
        {
            sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
            P_SetMessage(plr, buffer, false);
            AM_addMark();
            Con_Printf("Marker added at current location.\n");
            return true;
        }

        if(!stricmp(argv[0], "clearmarks"))  // clear all marked points
        {
            AM_clearMarks();
            P_SetMessage(plr, AMSTR_MARKSCLEARED, false);
            Con_Printf("All markers cleared on automap.\n");
            return true;
        }

        if(!stricmp(argv[0], "grid")) // toggle drawing of grid
        {
            grid = !grid;
            P_SetMessage(plr, (grid ? AMSTR_GRIDON : AMSTR_GRIDOFF), false);
            Con_Printf("Grid toggled in automap.\n");
            return true;
        }

        if(!stricmp(argv[0], "zoommax"))  // max zoom
        {
            bigstate = !bigstate;
            if(bigstate)
            {
                AM_saveScaleAndLoc();
                AM_minOutWindowScale();
            }
            else
                AM_restoreScaleAndLoc();

            Con_Printf("Maximum zoom toggle in automap.\n");
            return true;
        }
    }
    else  // Automap is closed
    {
        if(!stricmp(argv[0], "automap"))  // open automap
        {
            AM_Start();
            // Enable/disable the automap binding classes
            DD_SetBindClass(GBC_CLASS1, true);
            if(!followplayer)
                DD_SetBindClass(GBC_CLASS2, true);

            viewactive = false;
            return true;
        }
    }

    return false;
}

/*
 * Calculates the slope and slope according to the x-axis of a line
 * segment in map coordinates (with the upright y-axis n' all) so
 * that it can be used with the brain-dead drawing stuff.
 *
 * DJS - not used currently...
 */
void AM_getIslope(mline_t * ml, islope_t * is)
{
    int     dx, dy;

    dy = ml->a.pos[VY] - ml->b.pos[VY];
    dx = ml->b.pos[VX] - ml->a.pos[VX];

    if(!dy)
        is->islp = (dx < 0 ? -DDMAXINT : DDMAXINT);
    else
        is->islp = FixedDiv(dx, dy);
    if(!dx)
        is->slp = (dy < 0 ? -DDMAXINT : DDMAXINT);
    else
        is->slp = FixedDiv(dy, dx);

}

/*
 * Activate the new scale
 */
void AM_activateNewScale(void)
{
    m_x += m_w / 2;
    m_y += m_h / 2;
    m_w = FTOM(f_w);
    m_h = FTOM(f_h);
    m_x -= m_w / 2;
    m_y -= m_h / 2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

/*
 * Save the current scale/location
 */
void AM_saveScaleAndLoc(void)
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

/*
 * Restore the scale/location from last automap viewing
 */
void AM_restoreScaleAndLoc(void)
{
    m_w = old_m_w;
    m_h = old_m_h;
    if(!followplayer)
    {
        m_x = old_m_x;
        m_y = old_m_y;
    }
    else
    {
        m_x = plr->plr->mo->pos[VX] - m_w / 2;
        m_y = plr->plr->mo->pos[VY] - m_h / 2;
    }
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w << FRACBITS, m_w);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

/*
 * adds a marker at the current location
 */
void AM_addMark(void)
{
    /*
    Con_Message("Adding mark point %d at X=%d Y=%d\n", (markpointnum + 1) % AM_NUMMARKPOINTS,
                            m_x + m_w / 2,
                            m_y + m_h / 2);
    */

    markpoints[markpointnum].pos[VX] = m_x + m_w / 2;
    markpoints[markpointnum].pos[VY] = m_y + m_h / 2;
    markpoints[markpointnum].pos[VZ] = 0;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;

}

/*
 * Determines bounding box of all vertices,
 * sets global variables controlling zoom range.
 */
void AM_findMinMaxBoundaries(void)
{
    int     i;
    fixed_t x, y;
    fixed_t a;
    fixed_t b;

    min_x = min_y = DDMAXINT;
    max_x = max_y = -DDMAXINT;

    for(i = 0; i < numvertexes; i++)
    {
        x = P_GetFixed(DMU_VERTEX, i, DMU_X);
        y = P_GetFixed(DMU_VERTEX, i, DMU_Y);

        if(x < min_x)
            min_x = x;
        else if(x > max_x)
            max_x = x;

        if(y < min_y)
            min_y = y;
        else if(y > max_y)
            max_y = y;
    }

    max_w = max_x - min_x;
    max_h = max_y - min_y;

    min_w = 2 * PLAYERRADIUS;    // const? never changed?
    min_h = 2 * PLAYERRADIUS;

    a = FixedDiv(f_w << FRACBITS, max_w);
    b = FixedDiv(f_h << FRACBITS, max_h);

    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = FixedDiv(f_h << FRACBITS, 2 * PLAYERRADIUS);

}

/*
 * Change the window scale
 */
void AM_changeWindowLoc(void)
{
    if(m_paninc.pos[VX] || m_paninc.pos[VY])
    {
        followplayer = 0;

        f_oldloc.pos[VX] = (float) DDMAXINT;
    }

    m_x += m_paninc.pos[VX];
    m_y += m_paninc.pos[VY];

    if(m_x + m_w / 2 > max_x)
        m_x = max_x - m_w / 2;
    else if(m_x + m_w / 2 < min_x)
        m_x = min_x - m_w / 2;

    if(m_y + m_h / 2 > max_y)
        m_y = max_y - m_h / 2;
    else if(m_y + m_h / 2 < min_y)
        m_y = min_y - m_h / 2;

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

/*
 * Init variables used in automap
 */
void AM_initVariables(void)
{
    int     i, pnum;

    thinker_t *think;
    mobj_t *mo;

    automapactive = true;

    f_oldloc.pos[VX] = (float) DDMAXINT;

    amclock = 0;
    lightlev = 0;

    m_paninc.pos[VX] = m_paninc.pos[VY] = 0;

    // Set the colors for the subsector debug display
    for(i = 0; i < 10; ++i)
    {
        subColors[i].r = ((float) M_Random()) / 255.f;
        subColors[i].g = ((float) M_Random()) / 255.f;
        subColors[i].b = ((float) M_Random()) / 255.f;
        subColors[i].a = 1.0f;
        subColors[i].a2 =  1.0f;
        subColors[i].glow = FRONT_GLOW;
        subColors[i].w = 7.5f;
        subColors[i].scale = false;
    }

//    if(cfg.automapWidth == 1 && cfg.automapHeight == 1)
    {
        winx = 0;
        winy = 0;
        winw = scrwidth;
        winh = scrheight;
    }
/*    else
    {
        // smooth scale/move from center
        winx = 160;
        winy = 100;
        winw = 0;
        winh = 0;
    }*/

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    // find player to center on initially
    if(!players[pnum = consoleplayer].plr->ingame)
    {
        for(pnum = 0; pnum < MAXPLAYERS; pnum++)
            if(players[pnum].plr->ingame)
                break;
    }

    plr = &players[pnum];
    m_x = plr->plr->mo->pos[VX] - m_w / 2;
    m_y = plr->plr->mo->pos[VY] - m_h / 2;
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    AM_setWinPos();

    memset(KeyPoints, 0, sizeof(ddvertex_t) * NUMBEROFKEYS);

    // load in the location of keys, if in baby mode
    if(gameskill == sk_baby)
    {
        for(think = thinkercap.next; think != &thinkercap; think = think->next)
        {
            if(think->function != P_MobjThinker)
            {                    //not a mobj
                continue;
            }
            mo = (mobj_t *) think;
#ifdef __JDOOM__
            if(mo->type == MT_MISC4)
            {
                KeyPoints[0].x = mo->pos[VX];
                KeyPoints[0].y = mo->pos[VY];
            }
            if(mo->type == MT_MISC5)
            {
                KeyPoints[1].x = mo->pos[VX];
                KeyPoints[1].y = mo->pos[VY];
            }
            if(mo->type == MT_MISC6)
            {
                KeyPoints[2].x = mo->pos[VX];
                KeyPoints[2].y = mo->pos[VY];
            }
            if(mo->type == MT_MISC7)
            {
                KeyPoints[3].x = mo->pos[VX];
                KeyPoints[3].y = mo->pos[VY];
            }
            if(mo->type == MT_MISC8)
            {
                KeyPoints[4].x = mo->pos[VX];
                KeyPoints[4].y = mo->pos[VY];
            }
            if(mo->type == MT_MISC9)
            {
                KeyPoints[5].x = mo->pos[VX];
                KeyPoints[5].y = mo->pos[VY];
            }
#elif __JHERETIC__              // NB - Should really put the keys into a struct for neatness.
                                // name of the vector object, object keyname, colour etc.
                                // could easily display other objects on the map then...
            if(mo->type == MT_CKEY)
            {
                KeyPoints[0].x = mo->pos[VX];
                KeyPoints[0].y = mo->pos[VY];
            }
            else if(mo->type == MT_BKYY)
            {
                KeyPoints[1].x = mo->pos[VX];
                KeyPoints[1].y = mo->pos[VY];
            }
            else if(mo->type == MT_AKYY)
            {
                KeyPoints[2].x = mo->pos[VX];
                KeyPoints[2].y = mo->pos[VY];
            }
#endif                                // FIXME: Keys in jHexen!
        }
    }
}

/*
 * Load any graphics needed for the automap
 */
void AM_loadPics(void)
{
    int     i;
    char    namebuf[9];

    for(i = 0; i < 10; i++)
    {
        MARKERPATCHES;        // Check the macros eg: "sprintf(namebuf, "AMMNUM%d", i)" for jDoom

        markpnums[i] = W_GetNumForName(namebuf);
    }

    if (maplumpnum != 0){
        maplumpnum = W_GetNumForName("AUTOPAGE");
    }
}

/*
 * Is this stub still needed?
 */
void AM_unloadPics(void)
{
    // nothing to unload
}

/*
 * Clears markpoint array
 * fixme THIS IS BOLLOCKS!
 */
void AM_clearMarks(void)
{
    int     i;

    for(i = 0; i < AM_NUMMARKPOINTS; i++)
        markpoints[i].pos[VX] = -1;    // means empty
    markpointnum = 0;
}

/*
 * should be called at the start of every level
 * right now, i figure it out myself
 */
void AM_LevelInit(void)
{
    leveljuststarted = 0;

    f_x = f_y = 0;

    f_w = Get(DD_SCREEN_WIDTH);
    f_h = Get(DD_SCREEN_HEIGHT);

    AM_clearMarks();

    AM_findMinMaxBoundaries();
    scale_mtof = FixedDiv(min_scale_mtof, (int) (0.7 * FRACUNIT));
    if(scale_mtof > max_scale_mtof)
        scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

/*
 * Stop the automap
 */
void AM_Stop(void)
{
    AM_unloadPics();
    automapactive = false;
    amap_fullyopen = false;
    am_alpha = 0;

    stopped = true;

    GL_Update(DDUF_BORDER);
}

/*
 * Start the automap
 */
void AM_Start(void)
{
    //static int lastlevel = -1, lastepisode = -1;

    if(!stopped)
        AM_Stop();

    stopped = false;

    if(gamestate != GS_LEVEL)
        return;                    // don't show automap if we aren't in a game!

    AM_initVariables();
    AM_loadPics();
}

/*
 *  set the window scale to the minimum size.
 */
void AM_minOutWindowScale(void)
{
    scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

/*
 *  set the window scale to the maximum size.
 */
void AM_maxOutWindowScale(void)
{
    scale_mtof = max_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

/*
 * Changes the map scale values
 */
void AM_changeWindowScale(void)
{

    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    if(scale_mtof < min_scale_mtof)
        AM_minOutWindowScale();
    else if(scale_mtof > max_scale_mtof)
        AM_maxOutWindowScale();
    else
        AM_activateNewScale();
}

/*
 * Align the map camera to the players position
 */
void AM_doFollowPlayer(void)
{
    if(f_oldloc.pos[VX] != plr->plr->mo->pos[VX] || f_oldloc.pos[VY] != plr->plr->mo->pos[VY])
    {
        // Now that a high resolution is used (compared to 320x200!)
        // there is no need to quantify map scrolling. -jk
        m_x =  plr->plr->mo->pos[VX] - m_w / 2;
        m_y =  plr->plr->mo->pos[VY]  - m_h / 2;
        m_x2 = m_x + m_w;
        m_y2 = m_y + m_h;
        f_oldloc.pos[VX] = plr->plr->mo->pos[VX];
        f_oldloc.pos[VY] = plr->plr->mo->pos[VY];
    }
}

/*
 * unused now?
 */
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
        if(litelevelscnt == sizeof(litelevels) / sizeof(int))
            litelevelscnt = 0;
        nexttic = amclock + 6 - (amclock % 6);
    }
}

/*
 *  Updates on Game Tick.
 */
void AM_Ticker(void)
{
    // If the automap is not active do nothing
    if(!automapactive)
        return;

    amclock++;

    // Increase alpha level of the map
    if(am_alpha < 1)
        am_alpha+= (1 - am_alpha)/3;

    // smooth scale the position/dimensions
    if(winx < sx0) winx+= (sx0 - winx)/2;
    if(winx > sx0) winx-= (winx - sx0)/2;
    if(winy < sy0) winy+= (sy0 - winy)/2;
    if(winy > sy0) winy-= (winy - sy0)/2;
    if(winw < sx1) winw+= (sx1 - winw)/2;
    if(winw > sx1) winw-= (winw - sx1)/2;
    if(winh < sy1) winh+= (sy1 - winh)/2;
    if(winh > sy1) winh-= (winh - sy1)/2;

    if( winx == sx0 && winy == sy0 && winw == sx1 && winh == sy1)
        amap_fullyopen = true;
    else
        amap_fullyopen = false;

    // Zooming
    if(actions[A_MAPZOOMOUT].on)  // zoom out
    {
        mtof_zoommul = M_ZOOMOUT;
        ftom_zoommul = M_ZOOMIN;
    }
    else if(actions[A_MAPZOOMIN].on) // zoom in
    {
        mtof_zoommul = M_ZOOMIN;
        ftom_zoommul = M_ZOOMOUT;
    }
    else
    {
        mtof_zoommul = FRACUNIT;
        ftom_zoommul = FRACUNIT;
    }

    // Camera location paning
    if(!followplayer)
    {
        // X axis pan
        if(actions[A_MAPPANRIGHT].on) // pan right?
            m_paninc.pos[VX] = FTOM(F_PANINC);
        else if(actions[A_MAPPANLEFT].on) // pan left?
            m_paninc.pos[VX] = -FTOM(F_PANINC);
        else
            m_paninc.pos[VX] = 0;

        // Y axis pan
        if(actions[A_MAPPANUP].on)  // pan up?
            m_paninc.pos[VY] = FTOM(F_PANINC);
        else if(actions[A_MAPPANDOWN].on)  // pan down?
            m_paninc.pos[VY] = -FTOM(F_PANINC);
        else
            m_paninc.pos[VY] = 0;
    }
    else  // Camera follows the player
        AM_doFollowPlayer();

    // Change the zoom
    AM_changeWindowScale();

    // Change window location
    if(m_paninc.pos[VX] || m_paninc.pos[VY] /*|| oldwin_w != cfg.automapWidth || oldwin_h != cfg.automapHeight*/)
        AM_changeWindowLoc();
}

/*
 *  Draw a border if needed.
 */
void AM_clearFB(int color)
{
    float   scaler;

    scaler = cfg.sbarscale / 20.0f;

    finit_height = SCREENHEIGHT;

    GL_Update(DDUF_FULLSCREEN);

    if (cfg.automapHudDisplay != 1) {

        GL_SetPatch(W_GetNumForName( BORDERGRAPHIC ));

        GL_DrawCutRectTiled(0, finit_height, 320, BORDEROFFSET, 16, BORDEROFFSET, 0, 0, 160 - 160 * scaler + 1,
                        finit_height, 320 * scaler - 2, BORDEROFFSET);
    }

}

/*
 * Returns the settings for the given type of line.
 * Not a very pretty routine. Will do this with an array when I rewrite the map...
 */
mapline_t AM_getLine(int type, int special)
{
    mapline_t l;

    switch (type)
    {
    default:
        // An unseen line (got the computer map)
        l.r = cfg.automapL0[0];
        l.g = cfg.automapL0[1];
        l.b = cfg.automapL0[2];
        l.a = cfg.automapLineAlpha;
        l.a2 =  cfg.automapLineAlpha;
        l.glow = NO_GLOW;
        l.w = 0;
        l.scale = false;
        break;
    case 1:
        // onesided linedef (regular wall)
        l.r = cfg.automapL1[0];
        l.g = cfg.automapL1[1];
        l.b = cfg.automapL1[2];
        l.a = cfg.automapLineAlpha;
        l.a2 = cfg.automapLineAlpha/3;
        l.glow = NO_GLOW;
        l.w = 0;
        l.scale = false;
        break;
    case 2:
        // regular twosided linedef no sector height changes
        // could be a door or teleport or something...

        switch (special)
        {
        default:
            // nope nothing special about it
            l.r = cfg.automapL0[0];
            l.g = cfg.automapL0[1];
            l.b = cfg.automapL0[2];
            l.a = 1;
            l.a2 = 1;
            l.glow = NO_GLOW;
            l.w = 0;
            l.scale = false;
            break;
#ifdef __JDOOM__
            case 32:                    // Blue locked door open
            case 26:                    // Blue Door/Locked
            case 99:
            case 133:
                l.r = 0;
                l.g = 0;
                l.b = 0.776f;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;

                l.w = 5.0f;
                l.scale = true;
                break;
            case 33:                    // Red locked door open
            case 28:                    // Red Door /Locked
            case 134:
            case 135:
                l.r = 0.682f;
                l.g = 0;
                l.b = 0;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
            case 34:                    // Yellow locked door open
            case 27:                    // Yellow Door /Locked
            case 136:
            case 137:

                l.r = 0.905f;
                l.g = 0.9f;
                l.b = 0;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
#elif __JHERETIC__
            case 26:                    // Blue
            case 32:
                l.r = 0;
                l.g = 0;
                l.b = 0.776f;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
            case 27:                    // Yellow
            case 34:
                l.r = 0.905f;
                l.g = 0.9f;
                l.b = 0;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
            case 28:                    // Green
            case 33:
                l.r = 0;
                l.g = 0.9f;
                l.b = 0;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
#else
            case 13:                    // Locked door line -- all locked doors are green
            case 83:
                l.r = 0;
                l.g = 0.9f;
                l.b = 0;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
            case 70:                    // intra-level teleports are blue
            case 71:
                l.r = 0;
                l.g = 0;
                l.b = 0.776f;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
            case 74:                    // inter-level teleport/game-winning exit -- both are red
            case 75:
                l.r = 0.682f;
                l.g = 0;
                l.b = 0;
                l.a = cfg.automapLineAlpha/2;
                l.a2 = cfg.automapLineAlpha/1.5;
                if ( cfg.automapDoorGlow > 0)
                    l.glow = TWOSIDED_GLOW;
                else
                    l.glow = NO_GLOW;
                l.w = 5.0f;
                l.scale = true;
                break;
#endif
        }

        break;
    case 3:
        // twosided linedef, floor change
        l.r = cfg.automapL2[0];
        l.g = cfg.automapL2[1];
        l.b = cfg.automapL2[2];
        l.a = cfg.automapLineAlpha;
        l.a2 = cfg.automapLineAlpha/2;
        l.glow = NO_GLOW;
        l.w = 0;
        l.scale = false;
        break;
    case 4:
        // twosided linedef, ceiling change
        l.r = cfg.automapL3[0];
        l.g = cfg.automapL3[1];
        l.b = cfg.automapL3[2];
        l.a = cfg.automapLineAlpha;
        l.a2 = cfg.automapLineAlpha/2;
        l.glow = NO_GLOW;
        l.w = 0;
        l.scale = false;
        break;
    }

    return l;
}

/*
 * Returns a value > 0 if the line is some sort of special that we
 * might want to add a glow to, or change the colour of etc.
 */
int AM_checkSpecial(int special)
{
    switch (special)
    {
    default:
        // If it's not a special, zero is returned.
        break;
#ifdef __JDOOM__
    case 32:                    // Blue locked door open
    case 26:                    // Blue Door/Locked
    case 99:
    case 133:
    case 33:                    // Red locked door open
    case 28:                    // Red Door /Locked
    case 134:
    case 135:
    case 34:                    // Yellow locked door open
    case 27:                    // Yellow Door /Locked
    case 136:
    case 137:
        // its a door
        return 1;
        break;
#elif __JHERETIC__
    case 26:
    case 32:
    case 27:
    case 34:
    case 28:
    case 33:
        // its a door
        return 1;
        break;
#else
    case 13:
    case 83:
        // its a door
        return 1;
        break;
    case 70:
    case 71:
        // intra-level teleports
        return 2;
        break;
    case 74:
    case 75:
        // inter-level teleport/game-winning exit
        return 3;
        break;
#endif
    }
    return 0;
}

/*
 * Draws the given line. No cliping is done!
 */
void AM_drawMline(mline_t * ml, int color)
{
    GL_SetColor2(color, (am_alpha - (1-cfg.automapLineAlpha)));

    gl.Vertex2f(FIX2FLT(CXMTOFX(FLT2FIX(ml->a.pos[VX]))),
                FIX2FLT(CYMTOFX(FLT2FIX(ml->a.pos[VY]))));
    gl.Vertex2f(FIX2FLT(CXMTOFX(FLT2FIX(ml->b.pos[VX]))),
                FIX2FLT(CYMTOFX(FLT2FIX(ml->b.pos[VY]))));
}

/*
 * Draws the given line including any optional extras
 */
void AM_drawMline2(mline_t *ml, mapline_t *c, boolean caps, boolean glowmode,
                   boolean blend)
{

    float   a[2], b[2], normal[2], unit[2], thickness;
    float   length, dx, dy;

    // scale line thickness relative to zoom level?
    if(c->scale)
        thickness = cfg.automapDoorGlow * FIX2FLT(scale_mtof) * 2.5f + 3;
    else
        thickness = c->w;

    // get colour from the passed line.
    // if the line glows and this is glow mode - use alpha 2 else alpha 1
    if(glowmode && c->glow)
        GL_SetColorAndAlpha(c->r, c->g, c->b, am_alpha - (1 - c->a2));
    else
        GL_SetColorAndAlpha(c->r, c->g, c->b, am_alpha - (1 - c->a));

    // If we are in glow mode and the line has glow - draw it
    if(glowmode && c->glow > NO_GLOW)
    {
        gl.Enable(DGL_TEXTURING);

        if(blend)
            gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);

        gl.Bind(Get(DD_DYNLIGHT_TEXTURE));

        gl.Begin(DGL_QUADS);

        a[VX] = FIX2FLT(CXMTOFX(FLT2FIX(ml->a.pos[VX])));
        a[VY] = FIX2FLT(CYMTOFX(FLT2FIX(ml->a.pos[VY])));
        b[VX] = FIX2FLT(CXMTOFX(FLT2FIX(ml->b.pos[VX])));
        b[VY] = FIX2FLT(CYMTOFX(FLT2FIX(ml->b.pos[VY])));

        dx = b[VX] - a[VX];
        dy = b[VY] - a[VY];
        length = sqrt(dx * dx + dy * dy);
        if(length <= 0)
            return;

        unit[VX] = dx / length;
        unit[VY] = dy / length;
        normal[VX] = unit[VY];
        normal[VY] = -unit[VX];

        if(caps){
            // Draws a "cap" at the start of the line
            gl.TexCoord2f(0, 0);
            gl.Vertex2f(a[VX] - unit[VX] * thickness + normal[VX] * thickness,
                        a[VY] - unit[VY] * thickness + normal[VY] * thickness);

            gl.TexCoord2f(0.5f, 0);
            gl.Vertex2f(a[VX] + normal[VX] * thickness,
                        a[VY] + normal[VY] * thickness);

            gl.TexCoord2f(0.5f, 1);
            gl.Vertex2f(a[VX] - normal[VX] * thickness,
                        a[VY] - normal[VY] * thickness);

            gl.TexCoord2f(0, 1);
            gl.Vertex2f(a[VX] - unit[VX] * thickness - normal[VX] * thickness,
                        a[VY] - unit[VY] * thickness - normal[VY] * thickness);
        }

        // The middle part of the line.
        if(c->glow == TWOSIDED_GLOW )
        {
            // two sided
            gl.TexCoord2f(0.5f, 0);
            gl.Vertex2f(a[VX] + normal[VX] * thickness,
                        a[VY] + normal[VY] * thickness);
            gl.Vertex2f(b[VX] + normal[VX] * thickness,
                        b[VY] + normal[VY] * thickness);

            gl.TexCoord2f(0.5f, 1);
            gl.Vertex2f(b[VX] - normal[VX] * thickness,
                        b[VY] - normal[VY] * thickness);
            gl.Vertex2f(a[VX] - normal[VX] * thickness,
                        a[VY] - normal[VY] * thickness);

        }
        else if (c->glow == BACK_GLOW)
        {
            // back side only
            gl.TexCoord2f(0, 0.25f);
            gl.Vertex2f(a[VX] + normal[VX] * thickness,
                        a[VY] + normal[VY] * thickness);
            gl.Vertex2f(b[VX] + normal[VX] * thickness,
                        b[VY] + normal[VY] * thickness);

            gl.TexCoord2f(0.5f, 0.25f);
            gl.Vertex2f(b[VX], b[VY]);
            gl.Vertex2f(a[VX], a[VY]);

        }
        else
        {
            // front side only
            gl.TexCoord2f(0.75f, 0.5f);
            gl.Vertex2f(a[VX], a[VY]);
            gl.Vertex2f(b[VX], b[VY]);

            gl.TexCoord2f(0.75f, 1);
            gl.Vertex2f(b[VX] - normal[VX] * thickness,
                        b[VY] - normal[VY] * thickness);
            gl.Vertex2f(a[VX] - normal[VX] * thickness,
                        a[VY] - normal[VY] * thickness);
        }

        if(caps)
        {
            // Draws a "cap" at the end of the line
            gl.TexCoord2f(0.5f, 0);
            gl.Vertex2f(b[VX] + normal[VX] * thickness,
                        b[VY] + normal[VY] * thickness);

            gl.TexCoord2f(1, 0);
            gl.Vertex2f(b[VX] + unit[VX] * thickness + normal[VX] * thickness,
                        b[VY] + unit[VY] * thickness + normal[VY] * thickness);

            gl.TexCoord2f(1, 1);
            gl.Vertex2f(b[VX] + unit[VX] * thickness - normal[VX] * thickness,
                        b[VY] + unit[VY] * thickness - normal[VY] * thickness);

            gl.TexCoord2f(0.5f, 1);
            gl.Vertex2f(b[VX] - normal[VX] * thickness,
                        b[VY] - normal[VY] * thickness);

        }

        gl.End();

        if(blend)
            gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

        gl.Disable(DGL_TEXTURING);

    }

    if(!glowmode)
    {
        // No glow? then draw a regular line
        gl.Begin(DGL_LINES);

        gl.Vertex2f(FIX2FLT(CXMTOFX(FLT2FIX(ml->a.pos[VX]))),
                    FIX2FLT(CYMTOFX(FLT2FIX(ml->a.pos[VY]))));
        gl.Vertex2f(FIX2FLT(CXMTOFX(FLT2FIX(ml->b.pos[VX]))),
                    FIX2FLT(CYMTOFX(FLT2FIX(ml->b.pos[VY]))));
        gl.End();
    }
}

/*
 *  Draws blockmap aligned grid lines.
 */
void AM_drawGrid(int color)
{
    fixed_t x, y;
    fixed_t start, end;

    fixed_t originx = *((fixed_t*) DD_GetVariable(DD_BLOCKMAP_ORIGIN_X));
    fixed_t originy = *((fixed_t*) DD_GetVariable(DD_BLOCKMAP_ORIGIN_Y));

    mline_t ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if((start - originx) % (MAPBLOCKUNITS << FRACBITS))
        start +=
            (MAPBLOCKUNITS << FRACBITS) -
            ((start - originx) % (MAPBLOCKUNITS << FRACBITS));
    end = m_x + m_w;

    // draw vertical gridlines
    ml.a.pos[VY] = FIX2FLT(m_y);
    ml.b.pos[VY] = FIX2FLT(m_y + m_h);

    gl.Begin(DGL_LINES);
    for(x = start; x < end; x += (MAPBLOCKUNITS << FRACBITS))
    {
        ml.a.pos[VX] = FIX2FLT(x);
        ml.b.pos[VX] = FIX2FLT(x);
        AM_drawMline(&ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if((start - originy) % (MAPBLOCKUNITS << FRACBITS))
        start +=
            (MAPBLOCKUNITS << FRACBITS) -
            ((start - originy) % (MAPBLOCKUNITS << FRACBITS));
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.pos[VX] = FIX2FLT(m_x);
    ml.b.pos[VX] = FIX2FLT(m_x + m_w);
    for(y = start; y < end; y += (MAPBLOCKUNITS << FRACBITS))
    {
        ml.a.pos[VY] = FIX2FLT(y);
        ml.b.pos[VY] = FIX2FLT(y);
        AM_drawMline(&ml, color);
    }
    gl.End();
}

/*
 * Determines visible lines, draws them. This is LineDef based, not LineSeg based.
 */
void AM_drawWalls(boolean glowmode)
{
    int     i, s, subColor = 0;
    line_t  *line;
    xline_t *xline;
    mline_t l;
    subsector_t *ssec;
    sector_t *sec, *frontsector, *backsector;
    seg_t *seg;
    mapline_t templine;
    boolean withglow = false;

    for(s = 0; s < numsubsectors; ++s)
    {
        ssec = P_ToPtr(DMU_SUBSECTOR, s);
        sec = P_GetPtrp(ssec, DMU_SECTOR);

        if(cheating == 3)        // Debug cheat, show subsectors
        {
            subColor++;
            if(subColor == 10)
                subColor = 0;
        }

        for(i = 0; i < P_GetIntp(ssec, DMU_LINE_COUNT); ++i)
        {
            seg = P_GetPtrp(ssec, DMU_SEG_OF_SUBSECTOR | i);

            frontsector = P_GetPtrp(seg, DMU_FRONT_SECTOR);
            backsector = P_GetPtrp(seg, DMU_BACK_SECTOR);

            line = P_GetPtrp(seg, DMU_LINE);
            if(!line)
                continue;

            l.a.pos[VX] = P_GetFloatp(seg, DMU_VERTEX1_X);
            l.a.pos[VY] = P_GetFloatp(seg, DMU_VERTEX1_Y);
            l.b.pos[VX] = P_GetFloatp(seg, DMU_VERTEX2_X);
            l.b.pos[VY] = P_GetFloatp(seg, DMU_VERTEX2_Y);

            if(cheating == 3)        // Debug cheat, show subsectors
            {
                AM_drawMline2(&l, &subColors[subColor], false, glowmode, true);
                continue;
            }

            if(frontsector != P_GetPtrp(line, DMU_SIDE0_OF_LINE | DMU_SECTOR))
                continue; // we only want to draw twosided lines once.

            xline = P_XLine(line);

#if !__JHEXEN__
#if !__JSTRIFE__
            if(cheating == 2)        // Debug cheat.
            {
                // Show active XG lines.
                if(xline->xg && xline->xg->active && (leveltime & 4))
                {
                    templine = AM_getLine( 1, 0);
                    AM_drawMline2(&l, &templine, false, glowmode, true);
                }
            }
#endif
#endif
            if(cheating || (P_GetIntp(line, DMU_FLAGS) & ML_MAPPED))
            {
                if((P_GetIntp(line, DMU_FLAGS) & LINE_NEVERSEE) && !cheating)
                    continue;

                if(!backsector)
                {
                    // solid wall (well probably anyway...)
                    templine = AM_getLine( 1, 0);

                    AM_drawMline2(&l, &templine, false, glowmode, false);
                }
                else
                {
                    if(P_GetIntp(line, DMU_FLAGS) & ML_SECRET)
                    {
                        // secret door
                        templine = AM_getLine( 1, 0);

                        AM_drawMline2(&l, &templine, false, glowmode, false);
                    }
                    else if(cfg.automapShowDoors && AM_checkSpecial(xline->special) > 0 )
                    {
                        // some sort of special

                        if(cfg.automapDoorGlow > 0 && glowmode)
                            withglow = true;

                        templine = AM_getLine( 2, xline->special);

                        AM_drawMline2(&l, &templine, withglow, glowmode, withglow);
                    }
                    else if(P_GetFixedp(backsector, DMU_FLOOR_HEIGHT) !=
                            P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT))
                    {
                        // floor level change
                        templine = AM_getLine( 3, 0);

                        AM_drawMline2(&l, &templine, false, glowmode, false);
                    }
                    else if(P_GetFixedp(backsector, DMU_CEILING_HEIGHT) !=
                            P_GetFixedp(frontsector, DMU_CEILING_HEIGHT))
                    {
                        // ceiling level change
                        templine = AM_getLine( 4, 0);

                        AM_drawMline2(&l, &templine, false, glowmode, false);
                    }
                    else if(cheating)
                    {
                        templine = AM_getLine( 0, 0);

                        AM_drawMline2(&l, &templine, false, glowmode, false);
                    }
                }
            }
            else if(plr->powers[pw_allmap])
            {
                if(!(P_GetIntp(line, DMU_FLAGS) & LINE_NEVERSEE))
                {
                    // as yet unseen line
                    templine = AM_getLine( 0, 0);
                    AM_drawMline2(&l, &templine, false, glowmode, false);
                }
            }
        }
    }
}

/*
 * Rotation in 2D. Used to rotate player arrow line character.
 */
void AM_rotate(float *x, float *y, angle_t a)
{
    float tmpx;

    tmpx = (*x * FIX2FLT(finecosine[a >> ANGLETOFINESHIFT])) -
           (*y * FIX2FLT(finesine[a >> ANGLETOFINESHIFT]));

    *y = (*x * FIX2FLT(finesine[a >> ANGLETOFINESHIFT])) +
         (*y * FIX2FLT(finecosine[a >> ANGLETOFINESHIFT]));

    *x = tmpx;
}

/*
 * Draws a line character (eg the player arrow)
 */
void AM_drawLineCharacter(mline_t * lineguy, int lineguylines, float scale,
                          angle_t angle, int color, float x, float y)
{
    int     i;
    mline_t l;

    gl.Begin(DGL_LINES);
    for(i = 0; i < lineguylines; i++)
    {
        l.a.pos[VX] = lineguy[i].a.pos[VX];
        l.a.pos[VY] = lineguy[i].a.pos[VY];

        l.a.pos[VX] = scale * l.a.pos[VX];
        l.a.pos[VY] = scale * l.a.pos[VY];

        AM_rotate(&l.a.pos[VX], &l.a.pos[VY], angle);

        l.a.pos[VX] += x;
        l.a.pos[VY] += y;

        l.b.pos[VX] = lineguy[i].b.pos[VX];
        l.b.pos[VY] = lineguy[i].b.pos[VY];

        l.b.pos[VX] = scale * l.b.pos[VX];
        l.b.pos[VY] = scale * l.b.pos[VY];

        AM_rotate(&l.b.pos[VX], &l.b.pos[VY], angle);

        l.b.pos[VX] += x;
        l.b.pos[VY] += y;

        AM_drawMline(&l, color);
    }
    gl.End();
}

/*
 * Draws all players on the map using a line character
 *  Could definetly use a clean up!
 */
void AM_drawPlayers(void)
{
    int     i;
    player_t *p;
#if __JDOOM__
    static int their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
    int     their_color = -1;
#elif __JHERETIC__
    static int their_colors[] = { KEY3, KEY2, BLOODRED, KEY1 };
#endif
    int     color;
    float   size = FIX2FLT(PLAYERRADIUS);
    angle_t ang;

    if(!IS_NETGAME)
    {
        ang = plr->plr->clAngle;
#if __JDOOM__
        if(cheating)
            AM_drawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, size, ang,
                                 WHITE, FIX2FLT(plr->plr->mo->pos[VX]),
                                 FIX2FLT(plr->plr->mo->pos[VY]));
        else
            AM_drawLineCharacter(player_arrow, NUMPLYRLINES, size, ang, WHITE,
                                 FIX2FLT(plr->plr->mo->pos[VX]),
                                 FIX2FLT(plr->plr->mo->pos[VY]));
#else
        AM_drawLineCharacter(player_arrow, NUMPLYRLINES, size, plr->plr->clAngle,    //mo->angle,
                             WHITE, FIX2FLT(plr->plr->mo->pos[VX]),
                             FIX2FLT(plr->plr->mo->pos[VY]));
#endif
        return;
    }

    for(i = 0; i < MAXPLAYERS; i++)
    {
        p = &players[i];

#if __JDOOM__
        their_color++;

        if(deathmatch && p != plr)
            continue;
#endif
#if __JHERETIC__
        if(deathmatch && !singledemo && p != plr)
            continue;
#endif

        if(!players[i].plr->ingame)
            continue;
#if !__JHEXEN__
#if !__JSTRIFE__
        if(p->powers[pw_invisibility])
#if __JDOOM__
            color = 246;        // *close* to black
#elif __JHERETIC__
            color = 102;        // *close* to the automap color
#endif
        else
#endif
#endif
            color = their_colors[cfg.PlayerColor[i]];

        AM_drawLineCharacter(player_arrow, NUMPLYRLINES, size,
                             consoleplayer ==
                             i ? p->plr->clAngle : p->plr->mo->angle, color,
                             FIX2FLT(p->plr->mo->pos[VX]),
                             FIX2FLT(p->plr->mo->pos[VY]));
    }
}

/*
 * Draws all things on the map
 */
void AM_drawThings(int colors, int colorrange)
{
    int     i;
    mobj_t *iter;
    float size = FIX2FLT(PLAYERRADIUS);

    for(i = 0; i < numsectors; i++)
    {
        for(iter = P_GetPtr(DMU_SECTOR,i,DMU_THINGS); iter; iter = iter->snext)
        {
            AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,
                                 size, iter->angle, colors + lightlev,
                                 FIX2FLT(iter->pos[VX]), FIX2FLT(iter->pos[VY]));
        }
    }
}

/*
 * Draws all the points marked by the player
 *  FIXME: marks could be drawn without rotation
 */
void AM_drawMarks(void)
{
    int     i, fx, fy, w, h;

    for(i = 0; i < AM_NUMMARKPOINTS; i++)
    {
        if(markpoints[i].pos[VX] != -1)
        {
            w = 5;                // because something's wrong with the wad, i guess
            h = 6;                // because something's wrong with the wad, i guess

            fx = FIX2FLT( CXMTOF(markpoints[i].pos[VX]) << FRACBITS );
            fy = FIX2FLT( CYMTOF(markpoints[i].pos[VY]) << FRACBITS );

            GL_DrawPatch_CS(fx, fy, markpnums[i]);
        }
    }
}

/*
 * Draws all the keys on the map using the keysquare line character
 */
void AM_drawKeys(void)
{
    int i;
    float size = FIX2FLT(PLAYERRADIUS);

    gl.Begin(DGL_LINES);

    for (i=0; i< NUMBEROFKEYS; i++){
        if(KeyPoints[i].x != 0 || KeyPoints[i].y != 0)
        {
            AM_drawLineCharacter(keysquare, NUMKEYSQUARELINES, size, 0, keycolors[i],
                                 FIX2FLT(KeyPoints[i].x), FIX2FLT(KeyPoints[i].y));
        }
    }
    gl.End();
}

/*
 *  Changes the position of the automap window
 */
void AM_setWinPos(void)
{
    scrwidth = Get(DD_SCREEN_WIDTH);
    scrheight = Get(DD_SCREEN_HEIGHT);

    winw = f_w = (scrwidth/ 1.0f) * 1; //(cfg.automapWidth);
    winh = f_h = (scrheight/ 1.0f) * 1; //(cfg.automapHeight);

    // work out the position & dimensions of the automap when fully open
    /*
     * 0 = TopLeft
     * 1 = TopCenter
     * 2 = TopRight
      * 3 = CenterLeft
      * 4 = Center
      * 5 = CenterRight
      * 6 = BottomLeft
      * 7 = BottomCenter
      * 8 = BottomRight
     */

    // X Position
    switch (4)//cfg.automapPos)
    {
       case 1:
       case 4:
       case 7:
        sx0 = (scrwidth/2)-(winw/2);
        break;
       case 0:
       case 3:
       case 6:
        sx0 = 0;
        break;
       case 2:
       case 5:
       case 8:
        sx0 = (scrwidth - winw);
        break;
    }

    // Y Position
    switch (4)//cfg.automapPos)
    {
       case 0:
       case 1:
       case 2:
        sy0 = 0;
        break;
      case 3:
       case 4:
       case 5:
        sy0 = (scrheight/2)-(winh/2);
        break;
       case 6:
       case 7:
       case 8:
        sy0 = (scrheight - winh);
        break;
    }

    f_x = sx0;
    f_y = sy0;
    sx1 = winw;
    sy1 = winh;

    oldwin_w = 1; //cfg.automapWidth;
    oldwin_h = 1; //cfg.automapHeight;
}

/*
 * Sets up the state for automap drawing
 */
int     scissorState[5];

void AM_GL_SetupState()
{
    //float extrascale = 0;

    // Update the window scale vars
    if(oldwin_w != 1 /*cfg.automapWidth*/ || oldwin_h != 1 /*cfg.automapHeight*/)
        AM_setWinPos();

    // check for scissor box (to clip the map lines and stuff).
    // Store the old scissor state.
    gl.GetIntegerv(DGL_SCISSOR_TEST, scissorState);
    gl.GetIntegerv(DGL_SCISSOR_BOX, scissorState + 1);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, scrwidth, scrheight, -1, 1);

    // Do we want a background texture?
    if (maplumpnum)
    {
        gl.Enable(DGL_TEXTURING);

        GL_SetColorAndAlpha(cfg.automapBack[0],
                            cfg.automapBack[1],
                            cfg.automapBack[2],
                            (am_alpha - (1- cfg.automapBack[3])));
        GL_SetRawImage(maplumpnum, 0);        // We only want the left portion.
        GL_DrawRectTiled(winx, winy, winw, winh, 128, 100);
    }
    else
    {
        // nope just a solid color
        GL_SetNoTexture();
        GL_DrawRect(winx, winy, winw, winh,
                    cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2],
                    (am_alpha - (1- cfg.automapBack[3])));
    }

    // How about an outside border?
/*    if (cfg.automapHeight != 1 || cfg.automapWidth != 1 )
    {
        gl.Begin(DGL_LINES);
        gl.Color4f(0.5f, 1, 0.5f, am_alpha - (1 - (cfg.automapLineAlpha /2)) );

        if (cfg.automapHeight != 1)
        {
            gl.Vertex2f(winx-1, winy-1);
            gl.Vertex2f(winx+winw+1, winy-1);

            gl.Vertex2f(winx+winw+1, winy+winh+1);
            gl.Vertex2f(winx-1, winy+winh+1);
        }
        if (cfg.automapWidth != 1)
        {
            gl.Vertex2f(winx+winw+1, winy-1);
            gl.Vertex2f(winx+winw+1, winy+winh+1);

            gl.Vertex2f(winx-1, winy+winh+1);
            gl.Vertex2f(winx-1, winy-1);
        }
        gl.End();
    }*/

    // setup the scissor clipper
    gl.Scissor( winx, winy, winw, winh);
    //gl.Enable(DGL_SCISSOR_TEST);

    gl.Translatef(winx+(winw / 2.0f), winy+(winh /2.0f), 0);

    // apply some extra zoom to counteract the size of the window
    //extrascale = ((cfg.automapWidth + cfg.automapHeight)/2);
    //gl.Scalef( extrascale, extrascale , 0);

    if(cfg.automapRotate && followplayer)    // Rotate map?
        gl.Rotatef(plr->plr->clAngle / (float) ANGLE_MAX * 360 - 90, 0, 0, 1);

    gl.Translatef(-(winx+(winw / 2.0f)), -(winy+(winh /2.0f)), 0);
}

/*
 * Restores the previous gl draw state
 */
void AM_GL_RestoreState()
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    if(!scissorState[0])
        gl.Disable(DGL_SCISSOR_TEST);
    gl.Scissor(scissorState[1], scissorState[2], scissorState[3],
               scissorState[4]);
}

/*
 * Handles what counters to draw eg title, timer, dm stats etc
 */
void AM_drawCounters(void)
{

#if __JDOOM__ || __JHERETIC__
    char    buf[40], tmp[20];

    int     x = 5, y = LINEHEIGHT_A * 3;

    gl.Color3f(1, 1, 1);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
#endif

    gl.Enable(DGL_TEXTURING);

    AM_drawWorldTimer();

#if __JDOOM__ || __JHERETIC__
    Draw_BeginZoom(cfg.counterCheatScale, x, y);

    if(cfg.counterCheat){
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
            sprintf(tmp, "%s%i%%%s", cfg.counterCheat & CCH_KILLS ? "(" : "",
                    totalkills ? plr->killcount * 100 / totalkills : 100,
                    cfg.counterCheat & CCH_KILLS ? ")" : "");
            strcat(buf, tmp);
        }

        M_WriteText2(x, y, buf, hu_font_a, 1, 1, 1, 1);

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
            sprintf(tmp, "%s%i%%%s", cfg.counterCheat & CCH_ITEMS ? "(" : "",
                    totalitems ? plr->itemcount * 100 / totalitems : 100,
                    cfg.counterCheat & CCH_ITEMS ? ")" : "");
            strcat(buf, tmp);
        }

        M_WriteText2(x, y, buf, hu_font_a, 1, 1, 1, 1);

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
            sprintf(tmp, "%s%i%%%s", cfg.counterCheat & CCH_SECRET ? "(" : "",
                    totalsecret ? plr->secretcount * 100 / totalsecret : 100,
                    cfg.counterCheat & CCH_SECRET ? ")" : "");
            strcat(buf, tmp);
        }

        M_WriteText2(x, y, buf, hu_font_a, 1, 1, 1, 1);

        y += LINEHEIGHT_A;
    }
    }

    Draw_EndZoom();

#if __JDOOM__
    if(deathmatch)
        AM_drawFragsTable();
#endif

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

#endif

#if __JHEXEN__ || __JSTRIFE__
    if(IS_NETGAME)                    //  if(ShowKills && netgame && deathmatch)
    {
        // Always draw deathmatch stats in a netgame, even in coop
        AM_drawDeathmatchStats();
    }
#endif
}

/*
 * Draws the level name into the automap window
 */
void AM_drawLevelName(void)
{
#define FIXXTOSCREENX(x) (scrwidth * (x / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * (y / (float) SCREENHEIGHT))
    int    x, y, otherY;
    const char *lname = "";

#if __JHERETIC__
    if((gameepisode < (gamemode == extended? 6 : 4)) && gamemap < 10)
    {
        lname = DD_GetVariable(DD_MAP_NAME);
    }
#else
    lname = DD_GetVariable(DD_MAP_NAME);
#endif

    if(lname)
    {
        gl.MatrixMode(DGL_PROJECTION);
        gl.PushMatrix();
        gl.LoadIdentity();
        gl.Ortho(0, 0, scrwidth, scrheight, -1, 1);

        while(*lname && isspace(*lname))
                lname++;

        x = (sx0+(sx1 * .5f) - (M_StringWidth((char*)lname, hu_font_a) * .5f));
        y = (sy0+sy1);

        if(cfg.setblocks <= 11 || cfg.automapHudDisplay == 2)
        {   // We may need to adjust for the height of the statusbar
            otherY = FIXYTOSCREENY(ST_Y);
            otherY += FIXYTOSCREENY(ST_HEIGHT) * (1 - (cfg.sbarscale / 20.0f));

            if(y > otherY)
                y = otherY;
        }
        else if(cfg.setblocks == 12)
        {   // We may need to adjust for the height of the HUD icons.
            otherY = y;
            otherY += -(y * (cfg.hudScale / 10.0f));

            if(y > otherY)
                y = otherY;
        }

        y -= 24; // border

        M_WriteText2(x, y, (char*)lname, hu_font_a, 1, 1, 1, am_alpha);

        gl.MatrixMode(DGL_PROJECTION);
        gl.PopMatrix();
    }
#undef FIXXTOSCREENX
#undef FIXYTOSCREENY
}

/*
 * The automap drawing loop.
 */
void AM_Drawer(void)
{
    // If the automap isn't active, draw nothing
    if(!automapactive)
        return;

    // Setup
    AM_clearFB(BACKGROUND);
    AM_GL_SetupState();

    gl.Disable(DGL_TEXTURING);

    // Draw.
    if(grid)
        AM_drawGrid(GRIDCOLORS);

    AM_drawWalls(true);    // draw the glowing lines first
    AM_drawWalls(false);    // then the regular lines

    AM_drawPlayers();

    if(cheating == 2) AM_drawThings(THINGCOLORS, THINGRANGE);

#if !__JHEXEN__
    if(gameskill == sk_baby && cfg.automapBabyKeys)
    {
        AM_drawKeys();
    }
#endif

    gl.Enable(DGL_TEXTURING);
    gl.Color4f(1, 1, 1, 1);

    // Draw any marked points
    AM_drawMarks();

    gl.PopMatrix();

    // Draw the level names
    AM_drawLevelName();

    // Restore the state
    AM_GL_RestoreState();

    // Draw the counters
    AM_drawCounters();
}

#if __JDOOM__
/*
 * Draws a sorted frags list in the lower right corner of the screen.
 */
void AM_drawFragsTable(void)
{
#define FRAGS_DRAWN    -99999
    int     i, k, y, inCount = 0;    // How many players in the game?
    int     totalFrags[MAXPLAYERS];
    int     max, choose = 0;
    int     w = 30;
    char   *name, tmp[40];

    memset(totalFrags, 0, sizeof(totalFrags));
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;
        inCount++;
        for(k = 0; k < MAXPLAYERS; k++)
            totalFrags[i] += players[i].frags[k] * (k != i ? 1 : -1);
    }

    // Start drawing from the top.
    for(y =
        HU_TITLEY + 32 * (20 - cfg.sbarscale) / 20 - (inCount - 1) * LINEHEIGHT_A, i = 0;
        i < inCount; i++, y += LINEHEIGHT_A)
    {
        // Find the largest.
        for(max = FRAGS_DRAWN + 1, k = 0; k < MAXPLAYERS; k++)
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
        switch (cfg.PlayerColor[choose])
        {
        case 0:                // green
            gl.Color3f(0, .8f, 0);
            break;

        case 1:                // gray
            gl.Color3f(.45f, .45f, .45f);
            break;

        case 2:                // brown
            gl.Color3f(.7f, .5f, .4f);
            break;

        case 3:                // red
            gl.Color3f(1, 0, 0);
            break;
        }
        M_WriteText2(320 - w - M_StringWidth(name, hu_font_a) - 6, y, name,
                     hu_font_a, -1, -1, -1, -1);
        // A colon.
        M_WriteText2(320 - w - 5, y, ":", hu_font_a, -1, -1, -1, -1);
        // The frags count.
        sprintf(tmp, "%i", totalFrags[choose]);
        M_WriteText2(320 - w, y, tmp, hu_font_a, -1, -1, -1, -1);
        // Mark to ignore in the future.
        totalFrags[choose] = FRAGS_DRAWN;
    }
}
#endif


#ifndef __JHERETIC__
#ifndef __JDOOM__
/*
 * Draws the deathmatch stats
 * Should be combined with AM_drawFragsTable()
 */

// 8-player note:  Proper player color names here, too

/*char *PlayerColorText[MAXPLAYERS] =
   {
   "BLUE:",
   "RED:",
   "YELLOW:",
   "GREEN:",
   "JADE:",
   "WHITE:",
   "HAZEL:",
   "PURPLE:"
   }; */

void AM_drawDeathmatchStats(void)
{
    int     i, j, k, m;
    int     fragCount[MAXPLAYERS];
    int     order[MAXPLAYERS];
    char    textBuffer[80];
    int     yPosition;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        fragCount[i] = 0;
        order[i] = -1;
    }
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
        {
            continue;
        }
        else
        {
            for(j = 0; j < MAXPLAYERS; j++)
            {
                if(players[i].plr->ingame)
                {
                    fragCount[i] += players[i].frags[j];
                }
            }
            for(k = 0; k < MAXPLAYERS; k++)
            {
                if(order[k] == -1)
                {
                    order[k] = i;
                    break;
                }
                else if(fragCount[i] > fragCount[order[k]])
                {
                    for(m = MAXPLAYERS - 1; m > k; m--)
                    {
                        order[m] = order[m - 1];
                    }
                    order[k] = i;
                    break;
                }
            }
        }
    }
    yPosition = 15;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(order[i] < 0 || !players[order[i]].plr ||
           !players[order[i]].plr->ingame)
        {
            continue;
        }
        else
        {
            GL_SetColor(their_colors[cfg.PlayerColor[order[i]]]);
            memset(textBuffer, 0, 80);
            strncpy(textBuffer, Net_GetPlayerName(order[i]), 78);
            strcat(textBuffer, ":");
            MN_TextFilter(textBuffer);

            M_WriteText2(4, yPosition, textBuffer, hu_font_a, -1, -1, -1, -1);
            j = M_StringWidth(textBuffer, hu_font_a);

            sprintf(textBuffer, "%d", fragCount[order[i]]);
            M_WriteText2(j + 8, yPosition, textBuffer, hu_font_a, -1, -1, -1, -1);
            yPosition += 10;
        }
    }
}
#endif
#endif

/*
 * Draws the world time in the top right corner of the screen
 */
static void AM_drawWorldTimer(void)
{
#ifndef __JDOOM__
#ifndef __JHERETIC__
    int     days;
    int     hours;
    int     minutes;
    int     seconds;
    int     worldTimer;
    char    timeBuffer[15];
    char    dayBuffer[20];

    worldTimer = players[consoleplayer].worldTimer;

    worldTimer /= 35;
    days = worldTimer / 86400;
    worldTimer -= days * 86400;
    hours = worldTimer / 3600;
    worldTimer -= hours * 3600;
    minutes = worldTimer / 60;
    worldTimer -= minutes * 60;
    seconds = worldTimer;

    sprintf(timeBuffer, "%.2d : %.2d : %.2d", hours, minutes, seconds);
    M_WriteText2(240, 8, timeBuffer, hu_font_a, 1, 1, 1, 1);

    if(days)
    {
        if(days == 1)
        {
            sprintf(dayBuffer, "%.2d DAY", days);
        }
        else
        {
            sprintf(dayBuffer, "%.2d DAYS", days);
        }

        M_WriteText2(240, 20, dayBuffer, hu_font_a, 1, 1, 1, 1);
        if(days >= 5)
        {
            M_WriteText2(230, 35, "YOU FREAK!!!", hu_font_a, 1, 1, 1, 1);
        }
    }
#endif
#endif
}

// ------------------------------------------------------------------------------
// Automap Menu
//-------------------------------------------------------------------------------

#ifdef __JDOOM__

MenuItem_t MAPItems[] = {
/*    {ITT_LRFUNC, 0, "window position : ",    M_MapPosition, 0 },
    {ITT_LRFUNC, 0, "window width :       ",    M_MapWidth, 0 },
    {ITT_LRFUNC, 0, "window height :     ",    M_MapHeight, 0 },*/
    {ITT_LRFUNC, 0, "hud display :        ",    M_MapStatusbar, 0 },
    {ITT_LRFUNC, 0, "kills count :         ", M_MapKills, 0 },
    {ITT_LRFUNC, 0, "items count :         ", M_MapItems, 0 },
    {ITT_LRFUNC, 0, "secrets count :    ",    M_MapSecrets, 0 },
    {ITT_EMPTY, 0, "automap colours",        NULL, 0 },
    {ITT_EFUNC, 0, "   walls",        SCColorWidget, 1 },
    {ITT_EFUNC, 0, "   floor height changes", SCColorWidget, 2 },
    {ITT_EFUNC, 0, "   ceiling height changes",SCColorWidget, 3 },
    {ITT_EFUNC, 0, "   unseen areas",        SCColorWidget, 0 },
    {ITT_EFUNC, 0, "   background",        SCColorWidget, 4 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EFUNC, 0, "door colors :        ",    M_MapDoorColors, 0 },
    {ITT_LRFUNC, 0, "door glow : ",        M_MapDoorGlow, 0 },
    {ITT_LRFUNC, 0, "line alpha :          ",    M_MapLineAlpha, 0 },
};

Menu_t MapDef = {
    70, 40,
    M_DrawMapMenu,
    14, MAPItems,
    0, MENU_OPTIONS, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 14
};

#else

MenuItem_t MAPItems[] = {
/*    {ITT_LRFUNC, 0, "window position : ", M_MapPosition, 0 },
    {ITT_LRFUNC, 0, "window width :       ", M_MapWidth, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_LRFUNC, 0, "window height :     ", M_MapHeight, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },*/
    {ITT_LRFUNC, 0, "hud display :      ", M_MapStatusbar, 0 },
#ifdef __JHERETIC__
    {ITT_LRFUNC, 0, "kills count :           ", M_MapKills, 0 },
    {ITT_LRFUNC, 0, "items count :          ", M_MapItems, 0 },
    {ITT_LRFUNC, 0, "secrets count :     ", M_MapSecrets, 0 },
#endif
    {ITT_NAVLEFT, 0, "automap colours", NULL, 0 },
    {ITT_EFUNC, 0, "   walls", SCColorWidget, 1 },
    {ITT_EFUNC, 0, "   floor height changes", SCColorWidget, 2 },
    {ITT_EFUNC, 0, "   ceiling height changes", SCColorWidget, 3 },
    {ITT_EFUNC, 0, "   unseen areas", SCColorWidget, 0 },
    {ITT_EFUNC, 0, "   background", SCColorWidget, 4 },
    {ITT_EFUNC, 0, "door colors :        ", M_MapDoorColors, 0 },
    {ITT_LRFUNC, 0, "door glow :", M_MapDoorGlow, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_LRFUNC, 0, "line alpha :         ", M_MapLineAlpha, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 }
};

Menu_t MapDef = {
    64, 28,
    M_DrawMapMenu,
#if __JHERETIC__
    17, MAPItems,
#else
    14, MAPItems,
#endif
    0, MENU_OPTIONS, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
#if __JHERETIC__
    0, 17
#else
    0, 14
#endif
};
#endif

/*
 * Draws the automap options menu
 */
void M_DrawMapMenu(void)
{
    const Menu_t *menu = &MapDef;
    char   *hudviewnames[3] = { "NONE", "CURRENT", "STATUSBAR" };
    char *yesno[2] = { "NO", "YES" };
#if !defined(__JHEXEN__) && !defined(__JSTRIFE__)
    char   *countnames[4] = { "NO", "YES", "PERCENT", "COUNT+PCNT" };
#endif

    M_DrawTitle("Automap OPTIONS", menu->y - 26);

#ifdef __JDOOM__
    M_WriteMenuText(menu, 0, hudviewnames[cfg.automapHudDisplay]);
    M_WriteMenuText(menu, 1, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 2, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 3, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    M_DrawColorBox(menu, 5, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    M_DrawColorBox(menu, 6, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    M_DrawColorBox(menu, 7, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    M_DrawColorBox(menu, 8, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    M_DrawColorBox(menu, 9, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 11, yesno[cfg.automapShowDoors]);
    M_DrawSlider(menu, 12, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    M_DrawSlider(menu, 13, 11, cfg.automapLineAlpha * 10 + .5f);

#else

    M_WriteMenuText(menu, 0, hudviewnames[cfg.automapHudDisplay]);
#ifdef __JHERETIC__
    M_WriteMenuText(menu, 1, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 2, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 3, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    M_DrawColorBox(menu, 5, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    M_DrawColorBox(menu, 6, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    M_DrawColorBox(menu, 7, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    M_DrawColorBox(menu, 8, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    M_DrawColorBox(menu, 9, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 10, yesno[cfg.automapShowDoors]);
    M_DrawSlider(menu, 12, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    M_DrawSlider(menu, 15, 11, cfg.automapLineAlpha * 10 + .5f);
#else
    M_DrawColorBox(menu, 2, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    M_DrawColorBox(menu, 3, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    M_DrawColorBox(menu, 4, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    M_DrawColorBox(menu, 5, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    M_DrawColorBox(menu, 6, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 7, yesno[cfg.automapShowDoors]);
    M_DrawSlider(menu, 9, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    M_DrawSlider(menu, 12, 11, cfg.automapLineAlpha * 10 + .5f);
#endif

#endif

}

#if 0
/*
 * Set automap window width
 */
void M_MapWidth(int option, void *data)
{
    M_FloatMod10(&cfg.automapWidth, option);
}

/*
 * Set automap window height
 */
void M_MapHeight(int option, void *data)
{
    M_FloatMod10(&cfg.automapHeight, option);
}
#endif

/*
 * Set automap line alpha
 */
void M_MapLineAlpha(int option, void *data)
{
    M_FloatMod10(&cfg.automapLineAlpha, option);
}

/*
 * Set show line/teleport lines in different color
 */
void M_MapDoorColors(int option, void *data)
{
    cfg.automapShowDoors = !cfg.automapShowDoors;
}

/*
 * Set glow line amount
 */
void M_MapDoorGlow(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.automapDoorGlow < 200)
            cfg.automapDoorGlow++;
    }
    else if(cfg.automapDoorGlow > 0)
        cfg.automapDoorGlow--;
}

/*
 * Set rotate mode
 */
void M_MapRotate(int option, void *data)
{
    cfg.automapRotate = !cfg.automapRotate;
}

/*
 * Set which HUD to draw when in automap
 */
void M_MapStatusbar(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.automapHudDisplay < 2)
            cfg.automapHudDisplay++;
    }
    else if(cfg.automapHudDisplay > 0)
        cfg.automapHudDisplay--;
}

#if 0
/*
 * Set map window position
 */
void M_MapPosition(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.automapPos < 8)
            cfg.automapPos++;
    }
    else if(cfg.automapPos > 0)
        cfg.automapPos--;
}
#endif

/*
 * Set the show kills counter
 */
void M_MapKills(int option, void *data)
{
    int     op = (cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x9;
    cfg.counterCheat |= (op & 0x1) | ((op & 0x2) << 2);
}

/*
 * Set the show items counter
 */
void M_MapItems(int option, void *data)
{
    int     op =
        ((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x12;
    cfg.counterCheat |= ((op & 0x1) << 1) | ((op & 0x2) << 3);
}

/*
 * Set the show secrets counter
 */
void M_MapSecrets(int option, void *data)
{
    int     op =
        ((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x24;
    cfg.counterCheat |= ((op & 0x1) << 2) | ((op & 0x2) << 4);
}
