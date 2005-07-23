/*
 *
 * AM_MAP.C
 *
 * Basen on the DOOM Automap. Modified GL version of the original.
 *
 * "commonised" by DaniJ - Based on code by ID and Raven, heavily modified by Skyjake
 *
 *  TODO: Clean up!
 *
 *    Baby mode Keys in jHexen!
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <math.h>
#include <ctype.h>

#ifdef __JDOOM__
# include "jDoom/doomdef.h"
# include "jDoom/d_config.h"
# include "jDoom/st_stuff.h"
# include "jDoom/p_local.h"
# include "jDoom/m_cheat.h"
# include "jDoom/D_Action.h"
# include "jDoom/doomstat.h"
# include "jDoom/r_state.h"
# include "jDoom/dstrings.h"
# include "jDoom/Mn_def.h"
# include "jDoom/m_menu.h"
# include "jDoom/wi_stuff.h"
#elif __JHERETIC__
# include "jHeretic/H_Action.h"
# include "jHeretic/Doomdef.h"
# include "jHeretic/Mn_def.h"
# include "jHeretic/P_local.h"
# include "h_config.h"
# include "jHeretic/Dstrings.h"
#elif __JHEXEN__
# include "jHexen/h2_actn.h"
# include "jHexen/mn_def.h"
# include "jHexen/h2def.h"
# include "x_config.h"
#elif __JSTRIFE__
# include "jStrife/h2_actn.h"
# include "jStrife/Mn_def.h"
# include "jStrife/h2def.h"
# include "jStrife/d_config.h"
#endif

#include "Common/hu_stuff.h"
#include "Common/am_map.h"

// TYPES -------------------------------------------------------------------

#ifdef __JDOOM__

typedef struct {
    fixed_t x, y;
} mpoint_t;

#endif

typedef struct {
    float r, g, b, a, a2, w;    // a2 is used in glow mode in case the glow needs a different alpha
    boolean glow;
    boolean scale;
} mapline_t;

typedef struct {
    int     x, y;
} fpoint_t;

typedef struct {
    fpoint_t a, b;
} fline_t;

typedef struct {
    mpoint_t a, b;
} mline_t;

typedef struct {
    fixed_t slp, islp;
} islope_t;

enum {
    NO_GLOW = -1,
    TWOSIDED_GLOW,
    BACK_GLOW,
    FRONT_GLOW
};

// MACROS ------------------------------------------------------------------

#define DEFCC(name)        int name(int argc, char **argv)

#define thinkercap    (*gi.thinkercap)

//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//

#define R ((8*PLAYERRADIUS)/7)
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
#define NUMKEYSQUARELINES (sizeof(keysquare)/sizeof(mline_t))

#define R (FRACUNIT)
mline_t         triangle_guy[] = {
    {{-.867 * R, -.5 * R}, {.867 * R, -.5 * R}},
    {{.867 * R, -.5 * R}, {0, R}},
    {{0, R}, {-.867 * R, -.5 * R}}
};
#undef R
#define NUMTRIANGLEGUYLINES (sizeof(triangle_guy)/sizeof(mline_t))

#define R (FRACUNIT)
mline_t         thintriangle_guy[] = {
    {{-.5 * R, -.7 * R}, {R, 0}},
    {{R, 0}, {-.5 * R, .7 * R}},
    {{-.5 * R, .7 * R}, {-.5 * R, -.7 * R}}
};
#undef R
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))


// JDOOM Stuff
#ifdef __JDOOM__

#define R ((8*PLAYERRADIUS)/7)
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

#define R ((8*PLAYERRADIUS)/7)
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

#define R ((8*PLAYERRADIUS)/7)

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

#define R ((8*PLAYERRADIUS)/7)
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

#define R ((8*PLAYERRADIUS)/7)

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
#define NUMKEYSQUARELINES (sizeof(keysquare)/sizeof(mline_t))

#endif

// JSTRIFE Stuff ------------------------------------------------------------
#if __JSTRIFE__

#define R ((8*PLAYERRADIUS)/7)

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
#define NUMKEYSQUARELINES (sizeof(keysquare)/sizeof(mline_t))

#endif

// Used for Baby mode
vertex_t KeyPoints[NUMBEROFKEYS];

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

void    AM_drawMline2(mline_t * ml, mapline_t c, boolean caps, \
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
    {"map-position", 0, CVT_INT, &cfg.automapPos, 0, 8,
        "Relative position of the automap.\n0-8 Left to right, top to bottom"},
    {"map-width", 0, CVT_FLOAT, &cfg.automapWidth, 0, 1,
        "Automap width scale factor."},
    {"map-height", 0, CVT_FLOAT, &cfg.automapHeight, 0, 1,
        "Automap height scale factor."},
    {"map-color-unseen-r", 0, CVT_FLOAT, &cfg.automapL0[0], 0, 1,
        "Automap unseen areas, red component."},
    {"map-color-unseen-g", 0, CVT_FLOAT, &cfg.automapL0[1], 0, 1,
        "Automap unseen areas, green component."},
    {"map-color-unseen-b", 0, CVT_FLOAT, &cfg.automapL0[2], 0, 1,
        "Automap unseen areas, blue component."},
    {"map-color-wall-r", 0, CVT_FLOAT, &cfg.automapL1[0], 0, 1,
        "Automap walls, red component."},
    {"map-color-wall-g", 0, CVT_FLOAT, &cfg.automapL1[1], 0, 1,
        "Automap walls, green component."},
    {"map-color-wall-b", 0, CVT_FLOAT, &cfg.automapL1[2], 0, 1,
        "Automap walls, blue component."},
    {"map-color-floor-r", 0, CVT_FLOAT, &cfg.automapL2[0], 0, 1,
        "Automap floor height difference lines, red component."},
    {"map-color-floor-g", 0, CVT_FLOAT, &cfg.automapL2[1], 0, 1,
        "Automap floor height difference lines, green component."},
    {"map-color-floor-b", 0, CVT_FLOAT, &cfg.automapL2[2], 0, 1,
        "Automap floor height difference lines, blue component."},
    {"map-color-ceiling-r", 0, CVT_FLOAT, &cfg.automapL3[0], 0, 1,
        "Automap ceiling height difference lines, red component."},
    {"map-color-ceiling-g", 0, CVT_FLOAT, &cfg.automapL3[1], 0, 1,
        "Automap ceiling height difference lines, green component."},
    {"map-color-ceiling-b", 0, CVT_FLOAT, &cfg.automapL3[2], 0, 1,
        "Automap ceiling height difference lines, blue component."},
    {"map-background-r", 0, CVT_FLOAT, &cfg.automapBack[0], 0, 1,
        "Automap background color, red component."},
    {"map-background-g", 0, CVT_FLOAT, &cfg.automapBack[1], 0, 1,
        "Automap background color, green component."},
    {"map-background-b", 0, CVT_FLOAT, &cfg.automapBack[2], 0, 1,
        "Automap background color, blue component."},
    {"map-background-a", 0, CVT_FLOAT, &cfg.automapBack[3], 0, 1,
        "Alpha level of the automap background."},
    {"map-alpha-lines", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1,
        "Alpha level of automap lines."},
    {"map-rotate", 0, CVT_BYTE, &cfg.automapRotate, 0, 1,
        "1=Automap turns with player, up=forward."},
    {"map-huddisplay", 0, CVT_INT, &cfg.automapHudDisplay, 0, 2,
        "0=No HUD when in the automap\n1=Current HUD display shown when in the automap\n2=Always show Status Bar when in the automap"},
    {"map-door-colors", 0, CVT_BYTE, &cfg.automapShowDoors, 0, 1,
        "1=Show door colors in automap."},
    {"map-door-glow", 0, CVT_FLOAT, &cfg.automapDoorGlow, 0, 200,
        "Door glow thickness in the automap (with map-door-colors)."},
#ifndef __JHEXEN__
#ifndef __JSTRIFE__
    {"map-cheat-counter", 0, CVT_BYTE, &cfg.counterCheat, 0, 63,
        "6-bit bitfield. Show kills, items and secret counters in automap."},
    {"map-cheat-counter-scale", 0, CVT_FLOAT, &cfg.counterCheatScale, .1f, 1,
        "Size factor for the counters in the automap."},
    {"map-babykeys", 0, CVT_BYTE, &cfg.automapBabyKeys, 0, 1,
        "1=Show keys in automap (easy skill mode only)."},
#endif
#endif
    {NULL}
};

ccmd_t  mapCCmds[] = {
    {"automap", CCmdMapAction, "Show automap."},
    {"follow", CCmdMapAction, "Toggle Follow mode in the automap."},
    {"rotate", CCmdMapAction, "Toggle Rotate mode in the automap."},
    {"addmark", CCmdMapAction, "Add a mark in the automap."},
    {"clearmarks", CCmdMapAction, "Clear all marks in the automap."},
    {"grid", CCmdMapAction, "Toggle the grid in the automap."},
    {"zoommax", CCmdMapAction, "Zoom out to the max in the automap."},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifdef __JDOOM__

static int keycolors[] = { KEY1, KEY2, KEY3, KEY4, KEY5, KEY6 };

static unsigned char cheat_amap_seq[] = { 0xb2, 0x26, 0x26, 0x2e, 0xff };
static cheatseq_t cheat_amap = { cheat_amap_seq, 0 };

static int maplumpnum = 0;    // pointer to the raw data for the automap background.
                // if 0 no background image will be drawn

#elif __JHERETIC__

static int keycolors[] = { KEY1, KEY2, KEY3 };

static char cheat_amap[] = { 'r', 'a', 'v', 'm', 'a', 'p' };

static byte cheatcount = 0;

static int maplumpnum = 1;    // pointer to the raw data for the automap background.
                // if 0 no background image will be drawn

#else

static int keycolors[] = { KEY1, KEY2, KEY3 };

static char cheat_kills[] = { 'k', 'i', 'l', 'l', 's' };
static boolean ShowKills = 0;
static unsigned ShowKillsCount = 0;

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
static int cheatstate = 0;
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
int CCmdMapAction(int argc, char **argv)
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
            Con_Execute("enablebindclass map 0", true);

            if(!followplayer)
                Con_Execute("enablebindclass mapfollowoff 0", true);

            AM_Stop();
            return true; 
        }

        if(!stricmp(argv[0], "follow"))  // follow mode toggle
        {
            followplayer = !followplayer;
            f_oldloc.x = DDMAXINT;
            
            // Enable/disable the follow mode binding class
            if(!followplayer)
                Con_Execute("enablebindclass mapfollowoff 1", true);
            else
                Con_Execute("enablebindclass mapfollowoff 0", true);

            P_SetMessage(plr, followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF  );
            Con_Printf("Follow mode toggle.\n");
            return true; 
        }

        if(!stricmp(argv[0], "rotate"))  // rotate mode toggle
        {
            cfg.automapRotate = !cfg.automapRotate;
            P_SetMessage(plr, cfg.automapRotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF  );
            Con_Printf("Rotate mode toggle.\n");
            return true;
        }

        if(!stricmp(argv[0], "addmark"))  // add a mark
        {
            sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
            P_SetMessage(plr, buffer  );
            AM_addMark();
            Con_Printf("Marker added at current location.\n");
            return true;
        }

        if(!stricmp(argv[0], "clearmarks"))  // clear all marked points
        {
            AM_clearMarks();
            P_SetMessage(plr, AMSTR_MARKSCLEARED  );
            Con_Printf("All markers cleared on automap.\n");
            return true;
        }

        if(!stricmp(argv[0], "grid")) // toggle drawing of grid
        {
            grid = !grid;
            P_SetMessage(plr, grid ? AMSTR_GRIDON : AMSTR_GRIDOFF  );
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
            Con_Execute("enablebindclass map 1", true);
            if(!followplayer)
                Con_Execute("enablebindclass mapfollowoff 1", true);

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

    dy = ml->a.y - ml->b.y;
    dx = ml->b.x - ml->a.x;

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
        m_x = plr->plr->mo->x - m_w / 2;
        m_y = plr->plr->mo->y - m_h / 2;
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

    markpoints[markpointnum].x = m_x + m_w / 2;
    markpoints[markpointnum].y = m_y + m_h / 2;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;

}

/*
 * Determines bounding box of all vertices,
 * sets global variables controlling zoom range.
 */
void AM_findMinMaxBoundaries(void)
{
    int     i;
    fixed_t a;
    fixed_t b;

    min_x = min_y = DDMAXINT;
    max_x = max_y = -DDMAXINT;

    for(i = 0; i < numvertexes; i++)
    {
        if(vertexes[i].x < min_x)
            min_x = vertexes[i].x;
        else if(vertexes[i].x > max_x)
            max_x = vertexes[i].x;

        if(vertexes[i].y < min_y)
            min_y = vertexes[i].y;
        else if(vertexes[i].y > max_y)
            max_y = vertexes[i].y;
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
    if(m_paninc.x || m_paninc.y)
    {
        followplayer = 0;

        f_oldloc.x = DDMAXINT;
    }

    m_x += m_paninc.x;
    m_y += m_paninc.y;

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
    int     pnum;
#ifdef __JDOOM__    
    static event_t st_notify = { ev_keyup, AM_MSGENTERED };
#endif

    thinker_t *think;
    mobj_t *mo;

    automapactive = true;

    f_oldloc.x = DDMAXINT;

    amclock = 0;
    lightlev = 0;

    m_paninc.x = m_paninc.y = 0;

    if(cfg.automapWidth == 1 && cfg.automapHeight == 1)
    {
        winx = 0;
        winy = 0;
        winw = scrwidth;
        winh = scrheight;
    } 
    else 
    { 
        // smooth scale/move from center
        winx = 160;
        winy = 100;
        winw = 0;
        winh = 0;
    }

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
    m_x = plr->plr->mo->x - m_w / 2;
    m_y = plr->plr->mo->y - m_h / 2;
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    AM_setWinPos();

    memset(KeyPoints, 0, sizeof(vertex_t) * NUMBEROFKEYS );

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
                KeyPoints[0].x = mo->x;
                KeyPoints[0].y = mo->y;
            }
            if(mo->type == MT_MISC5)
            {
                KeyPoints[1].x = mo->x;
                KeyPoints[1].y = mo->y;
            }
            if(mo->type == MT_MISC6)
            {
                KeyPoints[2].x = mo->x;
                KeyPoints[2].y = mo->y;
            }
            if(mo->type == MT_MISC7)
            {
                KeyPoints[3].x = mo->x;
                KeyPoints[3].y = mo->y;
            }
            if(mo->type == MT_MISC8)
            {
                KeyPoints[4].x = mo->x;
                KeyPoints[4].y = mo->y;
            }
            if(mo->type == MT_MISC9)
            {
                KeyPoints[5].x = mo->x;
                KeyPoints[5].y = mo->y;
            }
#elif __JHERETIC__              // NB - Should really put the keys into a struct for neatness.
                                // name of the vector object, object keyname, colour etc.
                                // could easily display other objects on the map then...
            if(mo->type == MT_CKEY)
            {
                KeyPoints[0].x = mo->x;
                KeyPoints[0].y = mo->y;
            }
            else if(mo->type == MT_BKYY)
            {
                KeyPoints[1].x = mo->x;
                KeyPoints[1].y = mo->y;
            }
            else if(mo->type == MT_AKYY)
            {
                KeyPoints[2].x = mo->x;
                KeyPoints[2].y = mo->y;
            }
#endif                                // FIXME: Keys in jHexen!
        }
    }

#ifdef __JDOOM__
    // inform the status bar of the change
    ST_Responder(&st_notify);
#endif
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
 */
void AM_clearMarks(void)
{
    int     i;

    for(i = 0; i < AM_NUMMARKPOINTS; i++)
        markpoints[i].x = -1;    // means empty
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
#ifdef __JDOOM__
    static event_t st_notify = { 0, ev_keyup, AM_MSGEXITED };
#endif    

    AM_unloadPics();
    automapactive = false;
    amap_fullyopen = false;
    am_alpha = 0;

#ifdef __JDOOM__
    ST_Responder(&st_notify);
#endif
    stopped = true;

    GL_Update(DDUF_BORDER);
}

/*
 * Start the automap
 */
void AM_Start(void)
{
    static int lastlevel = -1, lastepisode = -1;

    if(!stopped)
        AM_Stop();

    stopped = false;

    if(gamestate != GS_LEVEL)
        return;                    // don't show automap if we aren't in a game!

    if(lastlevel != gamemap || lastepisode != gameepisode)
    {
        AM_LevelInit();
        lastlevel = gamemap;
        lastepisode = gameepisode;
    }
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
 * Handle input events in the automap.
 * Shouldn't really be here anyway...
 */
boolean AM_Responder(event_t *ev)
{
    int     rc;
    //static int cheatstate = 0;
    //static int bigstate = 0;
    //static char buffer[20];

    rc = false;

    if(automapactive)
    {
        if(ev->type == ev_keydown )
        {
            cheatstate = 0;
            rc = false;

#ifdef __JDOOM__
            if(!deathmatch && cht_CheckCheat(&cheat_amap, (char) ev->data1))
            {
                rc = false;
                cheating = (cheating + 1) % 3;
            }
#elif __JHERETIC__
            if(cheat_amap[cheatcount] == ev->data1 && !IS_NETGAME)
                cheatcount++;
            else
                cheatcount = 0;
            if(cheatcount == 6)
            {
                cheatcount = 0;
                rc = false;
                cheating = (cheating + 1) % 3;
            }
#else
            if(cheat_kills[ShowKillsCount] == ev->data1 && IS_NETGAME && deathmatch)
            {
                ShowKillsCount++;
                if(ShowKillsCount == 5)
                {
                    ShowKillsCount = 0;
                    rc = false;
                    ShowKills ^= 1;
                }
            }
            else
            {
                ShowKillsCount = 0;
            }
#endif
        }
        else if(ev->type == ev_keyup)
        {
            rc = false;
        }
        else if(ev->type == ev_keyrepeat)
            return true;
    }

    return rc;

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
    if(f_oldloc.x != plr->plr->mo->x || f_oldloc.y != plr->plr->mo->y)
    {
        // Now that a high resolution is used (compared to 320x200!)
        // there is no need to quantify map scrolling. -jk
        m_x =  plr->plr->mo->x - m_w / 2;
        m_y =  plr->plr->mo->y  - m_h / 2;
        m_x2 = m_x + m_w;
        m_y2 = m_y + m_h;
        f_oldloc.x = plr->plr->mo->x;
        f_oldloc.y = plr->plr->mo->y;
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
            m_paninc.x = FTOM(F_PANINC);
        else if(actions[A_MAPPANLEFT].on) // pan left?
            m_paninc.x = -FTOM(F_PANINC);
        else
            m_paninc.x = 0;

        // Y axis pan
        if(actions[A_MAPPANUP].on)  // pan up?
            m_paninc.y = FTOM(F_PANINC);
        else if(actions[A_MAPPANDOWN].on)  // pan down?
            m_paninc.y = -FTOM(F_PANINC);
        else
            m_paninc.y = 0;
    }
    else  // Camera follows the player
        AM_doFollowPlayer();

    // Change the zoom
    AM_changeWindowScale();

    // Change window location
    if(m_paninc.x || m_paninc.y || oldwin_w != cfg.automapWidth || oldwin_h != cfg.automapHeight)
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

    gl.Vertex2f(FIX2FLT(CXMTOFX(ml->a.x)), FIX2FLT(CYMTOFX(ml->a.y)));
    gl.Vertex2f(FIX2FLT(CXMTOFX(ml->b.x)), FIX2FLT(CYMTOFX(ml->b.y)));
}

/*
 * Draws the given line including any optional extras 
 */
void AM_drawMline2(mline_t * ml, mapline_t c, boolean caps, boolean glowmode, boolean blend)
{

    float   a[2], b[2], normal[2], unit[2], thickness;
    float   length, dx, dy;

    // scale line thickness relative to zoom level?
    if(c.scale)
        thickness = cfg.automapDoorGlow * FIX2FLT(scale_mtof) * 2.5f + 3;
    else
        thickness = c.w;

    // get colour from the passed line. if the line glows and this is glow mode - use alpha 2 else alpha 1
    GL_SetColorAndAlpha(c.r, c.g, c.b, (glowmode && c.glow) ? (am_alpha - ( 1 - c.a2)) : (am_alpha - ( 1 - c.a)) );

    // If we are in glow mode and the line has glow - draw it
    if(glowmode && c.glow > NO_GLOW){
        gl.Enable(DGL_TEXTURING);

        if(blend)
            gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);

        gl.Bind(Get(DD_DYNLIGHT_TEXTURE));

        gl.Begin(DGL_QUADS);

        a[VX] = FIX2FLT(CXMTOFX(ml->a.x));
        a[VY] = FIX2FLT(CYMTOFX(ml->a.y));
        b[VX] = FIX2FLT(CXMTOFX(ml->b.x));
        b[VY] = FIX2FLT(CYMTOFX(ml->b.y));

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
        if(c.glow == TWOSIDED_GLOW )
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
        else if (c.glow == BACK_GLOW)
        {
            // back side only
            gl.TexCoord2f(0, 0.25f);
            gl.Vertex2f(a[VX] + normal[VX] * thickness,
                    a[VY] + normal[VY] * thickness);
            gl.Vertex2f(b[VX] + normal[VX] * thickness,
                    b[VY] + normal[VY] * thickness);

            gl.TexCoord2f(0.5f, 0.25f);
            gl.Vertex2f(b[VX],
                    b[VY]);
            gl.Vertex2f(a[VX],
                    a[VY]);

        }
        else
        {
            // front side only
            gl.TexCoord2f(0.75f, 0.5f);
            gl.Vertex2f(a[VX],
                    a[VY]);
            gl.Vertex2f(b[VX],
                    b[VY]);

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

        gl.Vertex2f(FIX2FLT(CXMTOFX(ml->a.x)), FIX2FLT(CYMTOFX(ml->a.y)));
        gl.Vertex2f(FIX2FLT(CXMTOFX(ml->b.x)), FIX2FLT(CYMTOFX(ml->b.y)));
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
    mline_t ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if((start - bmaporgx) % (MAPBLOCKUNITS << FRACBITS))
        start +=
            (MAPBLOCKUNITS << FRACBITS) -
            ((start - bmaporgx) % (MAPBLOCKUNITS << FRACBITS));
    end = m_x + m_w;

    // draw vertical gridlines
    ml.a.y = m_y;
    ml.b.y = m_y + m_h;

    gl.Begin(DGL_LINES);
    for(x = start; x < end; x += (MAPBLOCKUNITS << FRACBITS))
    {
        ml.a.x = x;
        ml.b.x = x;
        AM_drawMline(&ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if((start - bmaporgy) % (MAPBLOCKUNITS << FRACBITS))
        start +=
            (MAPBLOCKUNITS << FRACBITS) -
            ((start - bmaporgy) % (MAPBLOCKUNITS << FRACBITS));
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.x = m_x;
    ml.b.x = m_x + m_w;
    for(y = start; y < end; y += (MAPBLOCKUNITS << FRACBITS))
    {
        ml.a.y = y;
        ml.b.y = y;
        AM_drawMline(&ml, color);
    }
    gl.End();
}

/*
 * Determines visible lines, draws them. This is LineDef based, not LineSeg based.
 */
void AM_drawWalls(boolean glowmode)
{
    int     i;
    static mline_t l;
    mapline_t templine;
    boolean withglow = false;

    for(i = 0; i < numlines; i++)
    {
        l.a.x = lines[i].v1->x;
        l.a.y = lines[i].v1->y;
        l.b.x = lines[i].v2->x;
        l.b.y = lines[i].v2->y;

#if !__JHEXEN__
#if !__JSTRIFE__
        if(cheating == 2)        // Debug cheat.
        {
            // Show active XG lines.
            if(lines[i].xg && lines[i].xg->active && (leveltime & 4))
            {
                templine = AM_getLine( 1, 0);
                AM_drawMline2(&l, templine, false, glowmode, true);
            }
        }
#endif
#endif
        if(cheating || (lines[i].flags & ML_MAPPED))
        {
            if((lines[i].flags & LINE_NEVERSEE) && !cheating)
                continue;
            if(!lines[i].backsector)        // solid wall (well probably anyway...)
            {
                templine = AM_getLine( 1, 0);

                AM_drawMline2(&l, templine, false, glowmode, false);
            }
            else
            {
                if(lines[i].flags & ML_SECRET)    // secret door
                {
                    templine = AM_getLine( 1, 0);

                    AM_drawMline2(&l, templine, false, glowmode, false);
                }
                else if(cfg.automapShowDoors && AM_checkSpecial(lines[i].special) > 0 )    // some sort of special
                {
                    if(cfg.automapDoorGlow > 0 && glowmode) withglow = true;

                    templine = AM_getLine( 2, lines[i].special);

                    AM_drawMline2(&l, templine, withglow, glowmode, withglow);
                }
                else if(lines[i].backsector->floorheight !=
                        lines[i].frontsector->floorheight)        // floor level change
                {
                    templine = AM_getLine( 3, 0);

                    AM_drawMline2(&l, templine, false, glowmode, false);
                }
                else if(lines[i].backsector->ceilingheight !=
                        lines[i].frontsector->ceilingheight )        // ceiling level change
                {
                    templine = AM_getLine( 4, 0);

                    AM_drawMline2(&l, templine, false, glowmode, false);
                }
                else if(cheating)
                {
                    templine = AM_getLine( 0, 0);

                    AM_drawMline2(&l, templine, false, glowmode, false);
                }
            }
        }
        else if(plr->powers[pw_allmap])
        {
            if(!(lines[i].flags & LINE_NEVERSEE))            // as yet unseen line
            {
                templine = AM_getLine( 0, 0);
                AM_drawMline2(&l, templine, false, glowmode, false);
            }
        }

    }
}

/*
 * Rotation in 2D. Used to rotate player arrow line character.
 */
void AM_rotate(fixed_t *x, fixed_t *y, angle_t a)
{
    fixed_t tmpx;

    tmpx = FixedMul(*x, finecosine[a >> ANGLETOFINESHIFT]) - FixedMul(*y, finesine[a >> ANGLETOFINESHIFT]);

    *y = FixedMul(*x, finesine[a >> ANGLETOFINESHIFT]) + FixedMul(*y, finecosine[a >> ANGLETOFINESHIFT]);

    *x = tmpx;
}

/*
 * Draws a line character (eg the player arrow)
 */
void AM_drawLineCharacter(mline_t * lineguy, int lineguylines, fixed_t scale,
                          angle_t angle, int color, fixed_t x, fixed_t y)
{
    int     i;
    mline_t l;

    gl.Begin(DGL_LINES);
    for(i = 0; i < lineguylines; i++)
    {
        l.a.x = lineguy[i].a.x;
        l.a.y = lineguy[i].a.y;

        if(scale)
        {
            l.a.x = FixedMul(scale, l.a.x);
            l.a.y = FixedMul(scale, l.a.y);
        }

        if(angle)
            AM_rotate(&l.a.x, &l.a.y, angle);

        l.a.x += x;
        l.a.y += y;

        l.b.x = lineguy[i].b.x;
        l.b.y = lineguy[i].b.y;

        if(scale)
        {
            l.b.x = FixedMul(scale, l.b.x);
            l.b.y = FixedMul(scale, l.b.y);
        }

        if(angle)
            AM_rotate(&l.b.x, &l.b.y, angle);

        l.b.x += x;
        l.b.y += y;

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
    angle_t ang;

    if(!IS_NETGAME)
    {
        ang = plr->plr->clAngle;
#if __JDOOM__
        if(cheating)
            AM_drawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, 0, ang,
                                 WHITE, plr->plr->mo->x, plr->plr->mo->y);
        else
            AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, ang, WHITE,
                                 plr->plr->mo->x, plr->plr->mo->y);
#else
        AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, plr->plr->clAngle,    //mo->angle,
                             WHITE, plr->plr->mo->x, plr->plr->mo->y);
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

        AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0,
                             consoleplayer ==
                             i ? p->plr->clAngle : p->plr->mo->angle, color,
                             p->plr->mo->x, p->plr->mo->y);
    }
}

/*
 * Draws all things on the map
 */
void AM_drawThings(int colors, int colorrange)
{
    int     i;
    mobj_t *t;

    for(i = 0; i < numsectors; i++)
    {
        t = sectors[i].thinglist;
        while(t)
        {
            AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,
                                 16 << FRACBITS, t->angle, colors + lightlev,
                                 t->x, t->y);

            t = t->snext;
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
        if(markpoints[i].x != -1)
        {
            w = 5;                // because something's wrong with the wad, i guess
            h = 6;                // because something's wrong with the wad, i guess

            fx = FIX2FLT( CXMTOF( markpoints[i].x ) << FRACBITS );
            fy = FIX2FLT( CYMTOF( markpoints[i].y ) << FRACBITS );

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

    gl.Begin(DGL_LINES);

    for (i=0; i< NUMBEROFKEYS; i++){
        if(KeyPoints[i].x != 0 || KeyPoints[i].y != 0)
        {
            AM_drawLineCharacter(keysquare, NUMKEYSQUARELINES, 0, 0, keycolors[i],
                             KeyPoints[i].x, KeyPoints[i].y);
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

    winw = f_w = (scrwidth/ 1.0f) * (cfg.automapWidth);
    winh = f_h = (scrheight/ 1.0f) * (cfg.automapHeight);

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
    switch (cfg.automapPos)
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
    switch (cfg.automapPos)
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

    oldwin_w = cfg.automapWidth;
    oldwin_h = cfg.automapHeight;
}

/*
 * Sets up the state for automap drawing
 */
int     scissorState[5];

void AM_GL_SetupState()
{
    float extrascale = 0;

    // Update the window scale vars
    if(oldwin_w != cfg.automapWidth || oldwin_h != cfg.automapHeight)
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

        GL_SetColorAndAlpha(cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], (am_alpha - (1- cfg.automapBack[3])));
        GL_SetRawImage(maplumpnum, 0);        // We only want the left portion.
        GL_DrawRectTiled(winx, winy, winw, winh, 128, 100);
    } 
    else 
    {
        // nope just a solid color
        GL_SetNoTexture();
        GL_DrawRect(winx, winy, winw, winh, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], (am_alpha - (1- cfg.automapBack[3])));
    }

    // How about an outside border?
    if (cfg.automapHeight != 1 || cfg.automapWidth != 1 )
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
    }

    // setup the scissor clipper
    gl.Scissor( winx, winy, winw, winh);
    //gl.Enable(DGL_SCISSOR_TEST);

    gl.Translatef(winx+(winw / 2.0f), winy+(winh /2.0f), 0);

    // apply some extra zoom to counteract the size of the window
    extrascale = ((cfg.automapWidth + cfg.automapHeight)/2);
    gl.Scalef( extrascale, extrascale , 0);

    if(cfg.automapRotate && followplayer)    // Rotate map?
        gl.Rotatef(plr->plr->clAngle / (float) ANGLE_MAX * 360 - 90, 0, 0, 1);

    gl.Translatef(-(winx+(winw / 2.0f)), -(winy+(winh /2.0f)), 0);
}

/*
 * Restores the previous gl draw state
 */
void AM_GL_RestoreState()
{
    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();

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

#if !__JHEXEN__
#if !__JSTRIFE__
    char    buf[40], tmp[20];

    int     x = 5, y = LINEHEIGHT_A * 3;

    gl.Color3f(1, 1, 1);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
#endif
#endif

    gl.Enable(DGL_TEXTURING);

    AM_drawWorldTimer();

#if !__JHEXEN__
#if !__JSTRIFE__

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
#endif

#if !__JHERETIC__
#if !__JDOOM__
    if(IS_NETGAME)                    //  if(ShowKills && netgame && deathmatch)
    {
        // Always draw deathmatch stats in a netgame, even in coop
        AM_drawDeathmatchStats();
    }
#endif
#endif
}

/*
 * Draws the level name into the automap window
 */
void AM_drawLevelName(void)
{
    int    x;
    int     y ;
    char   *lname;

#ifdef __JHEXEN__
    lname =  (char *) Get(DD_MAP_NAME);
#elif __JSTRIFE__
    lname =  (char *) Get(DD_MAP_NAME);

#elif __JHERETIC__
    if((gameepisode < (ExtendedWAD ? 6 : 4)) && gamemap < 10)
    {
        lname =  (char *) Get(DD_MAP_NAME);
    }
#elif __JDOOM__

    lname = (char *) Get(DD_MAP_NAME);

    // Plutonia and TNT are special cases.
    if(gamemission == pack_plut)
    {
        lname = mapnamesp[gamemap - 1];
    }
    else if(gamemission == pack_tnt)
    {
        lname = mapnamest[gamemap - 1];
    }

#endif

    if(lname)
    {
        gl.PopMatrix();

        gl.MatrixMode(DGL_PROJECTION);
        gl.PushMatrix();
        gl.LoadIdentity();
        gl.Ortho(0, 0, scrwidth, scrheight, -1, 1);

        while(*lname && isspace(*lname))
                lname++;

        x = (sx0+(sx1/2) - (M_StringWidth(lname, hu_font_a)/2));
        y = (sy0+sy1 - 32);

        M_WriteText2(x, y, lname, hu_font_a, 1, 1, 1, am_alpha);
    }
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
    int     max, choose;
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
        M_WriteText2(320 - w, y, tmp, hu_font_a, 1, 1, 1, -1);
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
    {ITT_LRFUNC, "window position : ",    M_MapPosition, 0 },
    {ITT_LRFUNC, "window width :       ",    M_MapWidth, 0 },
    {ITT_LRFUNC, "window height :     ",    M_MapHeight, 0 },
    {ITT_LRFUNC, "hud display :        ",    M_MapStatusbar, 0 },
    {ITT_LRFUNC, "kills count :         ", M_MapKills, 0 },
    {ITT_LRFUNC, "items count :         ", M_MapItems, 0 },
    {ITT_LRFUNC, "secrets count :    ",    M_MapSecrets, 0 },
    {ITT_EMPTY,  "automap colours",        NULL, 0 },
    {ITT_EFUNC,  "   walls",        SCColorWidget, 1 },
    {ITT_EFUNC,  "   floor height changes", SCColorWidget, 2 },
    {ITT_EFUNC,  "   ceiling height changes",SCColorWidget, 3 },
    {ITT_EFUNC,  "   unseen areas",        SCColorWidget, 0 },
    {ITT_EFUNC,  "   background",        SCColorWidget, 4 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_EFUNC,  "door colors :        ",    M_MapDoorColors, 0 },
    {ITT_LRFUNC, "door glow : ",        M_MapDoorGlow, 0 },
    {ITT_LRFUNC, "line alpha :          ",    M_MapLineAlpha, 0 },
};

Menu_t MapDef = {
    70, 40,
    M_DrawMapMenu,
    17, MAPItems,
    0, MENU_OPTIONS,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 17
};

#else

MenuItem_t MAPItems[] = {
    {ITT_LRFUNC, "window position : ", M_MapPosition, 0 },
    {ITT_LRFUNC, "window width :       ", M_MapWidth, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_LRFUNC, "window height :     ", M_MapHeight, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_LRFUNC,  "hud display :      ", M_MapStatusbar, 0 },
#ifdef __JHERETIC__
    {ITT_LRFUNC, "kills count :           ", M_MapKills, 0 },
    {ITT_LRFUNC, "items count :          ", M_MapItems, 0 },
    {ITT_LRFUNC, "secrets count :     ", M_MapSecrets, 0 },
#endif
    {ITT_INERT, "automap colours", NULL, 0 },
#ifndef __JHERETIC__
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
#endif
    {ITT_INERT, "automap colours", NULL, 0 },
    {ITT_EFUNC, "   walls", SCColorWidget, 1 },
    {ITT_EFUNC, "   floor height changes", SCColorWidget, 2 },
    {ITT_EFUNC, "   ceiling height changes", SCColorWidget, 3 },
    {ITT_EFUNC, "   unseen areas", SCColorWidget, 0 },
    {ITT_EFUNC, "   background", SCColorWidget, 4 },
    {ITT_EFUNC,  "door colors :        ", M_MapDoorColors, 0 },
    {ITT_LRFUNC, "door glow :", M_MapDoorGlow, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_EMPTY, NULL, NULL, 0 },
    {ITT_LRFUNC, "line alpha :         ", M_MapLineAlpha, 0 }
};

Menu_t MapDef = {
    64, 30,
    M_DrawMapMenu,
    23, MAPItems,
    0, MENU_OPTIONS,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 12
};
#endif

extern short    itemOn;

/*
 * Draws the automap options menu
 * Uses a bit of a kludge for multi pages with jHeretic/jHexen. 
 */
void M_DrawMapMenu(void)
{
    const Menu_t *menu = &MapDef;
#if !defined( __JDOOM__ ) && !defined( __JHERETIC__ )
    const MenuItem_t *item = menu->items + menu->firstItem;
#endif

    char   *posnames[9] = { "TOP LEFT", "TOP CENTER", "TOP RIGHT", "CENTER LEFT", "CENTER", \
                            "CENTER RIGHT", "BOTTOM LEFT", "BOTTOM CENTER", "BOTTOM RIGHT" };
    char   *hudviewnames[3] = { "NONE", "CURRENT", "STATUSBAR" };
    char *yesno[2] = { "NO", "YES" };
#if !defined(__JHEXEN__) && !defined(__JSTRIFE__)
    char   *countnames[4] = { "NO", "YES", "PERCENT", "COUNT+PCNT" };
#endif

#ifndef __JDOOM__
    char  *token;
#endif

    M_DrawTitle("Automap OPTIONS", menu->y - 28);

#ifdef __JDOOM__

    M_WriteMenuText(menu, 0, posnames[cfg.automapPos]);
    M_DrawSlider(menu, 1, 11, cfg.automapWidth * 10 + .25f);
    M_DrawSlider(menu, 2, 11, cfg.automapHeight * 10 + .25f);
    M_WriteMenuText(menu, 3, hudviewnames[cfg.automapHudDisplay]);
    M_WriteMenuText(menu, 4, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 5, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 6, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    M_DrawColorBox(menu, 8, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    M_DrawColorBox(menu, 9, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    M_DrawColorBox(menu, 10, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    M_DrawColorBox(menu, 11, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    M_DrawColorBox(menu, 12, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 14, yesno[cfg.automapShowDoors]);
    M_DrawSlider(menu, 15, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    M_DrawSlider(menu, 16, 11, cfg.automapLineAlpha * 10 + .5f);

#else

    // Draw the page arrows.
    gl.Color4f( 1, 1, 1, menu_alpha);
    token = (!menu->firstItem || MenuTime & 8) ? "invgeml2" : "invgeml1";
    GL_DrawPatch_CS(menu->x -20, menu->y - 16, W_GetNumForName(token));
    token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
             MenuTime & 8) ? "invgemr2" : "invgemr1";
    GL_DrawPatch_CS(312 - (menu->x -20), menu->y - 16, W_GetNumForName(token));

#ifdef __JHERETIC__
    if(menu->firstItem < menu->numVisItems){
        M_WriteMenuText(menu, 0, posnames[cfg.automapPos]);
        M_DrawSlider(menu, 2, 11, cfg.automapWidth * 10 + .25f);
        M_DrawSlider(menu, 5, 11, cfg.automapHeight * 10 + .25f);
        M_WriteMenuText(menu, 7, hudviewnames[cfg.automapHudDisplay]);
        M_WriteMenuText(menu, 8, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
        M_WriteMenuText(menu, 9, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
        M_WriteMenuText(menu, 10, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    } 
    else 
    {
        M_DrawColorBox(menu, 13, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
        M_DrawColorBox(menu, 14, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
        M_DrawColorBox(menu, 15, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
        M_DrawColorBox(menu, 16, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
        M_DrawColorBox(menu, 17, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
        M_WriteMenuText(menu, 18, yesno[cfg.automapShowDoors]);
        M_DrawSlider(menu, 20, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
        M_DrawSlider(menu, 23, 11, cfg.automapLineAlpha * 10 + .5f);
    }
#else
    if(menu->firstItem < menu->numVisItems){
        M_WriteMenuText(menu, 0, posnames[cfg.automapPos]);
        M_DrawSlider(menu, 2, 11, cfg.automapWidth * 10 + .25f);
        M_DrawSlider(menu, 5, 11, cfg.automapHeight * 10 + .25f);
        M_WriteMenuText(menu, 7, hudviewnames[cfg.automapHudDisplay]);
    } 
    else 
    {
        M_DrawColorBox(menu, 13, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
        M_DrawColorBox(menu, 14, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
        M_DrawColorBox(menu, 15, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
        M_DrawColorBox(menu, 16, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
        M_DrawColorBox(menu, 17, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
        M_WriteMenuText(menu, 18, yesno[cfg.automapShowDoors]);
        M_DrawSlider(menu, 20, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
        M_DrawSlider(menu, 23, 11, cfg.automapLineAlpha * 10 + .5f);
    }
#endif

#endif

}

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
