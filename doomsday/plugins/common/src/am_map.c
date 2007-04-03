/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
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
#include "g_controls.h"

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

typedef enum vectorgraphname_e {
    VG_KEYSQUARE,
    VG_TRIANGLE,
    VG_ARROW,
    VG_CHEATARROW,
    NUM_VECTOR_GRAPHS
} vectorgrapname_t;

typedef struct vectorgrap_s {
    uint        count;
    mline_t    *lines;
} vectorgrap_t;

typedef enum glowtype_e {
    NO_GLOW = -1,
    TWOSIDED_GLOW,
    BACK_GLOW,
    FRONT_GLOW
} glowtype_t;

// Types used for automap rendering lists.
#define AMLT_PALCOL     1
#define AMLT_RGBA       2

typedef struct amrline_s {
    byte    type;
    struct {
        float   pos[2];
    } a, b;
    union {
        struct {
            int     color;
            float   alpha;
        } palcolor;
        struct {
            float rgba[4];
        } f4color;
    } coldata;
} amrline_t;

typedef struct amrquad_s {
    float   rgba[4];
    struct {
        float   pos[2];
        float   tex[2];
    } verts[4];
} amrquad_t;

typedef struct amprim_s {
    union {
        amrquad_t quad;
        amrline_t line;
    } data;
    struct amprim_s *next;
} amprim_t;

typedef struct amprimlist_s {
    int         type; // DGL_QUAD or DGL_LINES
    amprim_t *head, *tail, *unused;
} amprimlist_t;

typedef struct amquadlist_s {
    int         tex;
    boolean     blend;
    amprimlist_t primlist;
    struct amquadlist_s *next;
} amquadlist_t;

// MACROS ------------------------------------------------------------------

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

mline_t         thintriangle_guy[] = {
    {{-R / 2, R - R / 2}, {R, 0}}, // >
    {{R, 0}, {-R / 2, -R + R / 2}},
    {{-R / 2, -R + R / 2}, {-R / 2, R - R / 2}} // |>
};

#if __JDOOM__
mline_t         player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},    // -----
    {{R, 0}, {R - R / 2, R / 4}},    // ----->
    {{R, 0}, {R - R / 2, -R / 4}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 4}},    // >---->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 4}},    // >>--->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4}}
};

mline_t         cheat_player_arrow[] = {
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

#elif __JHERETIC__
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

#elif __JHEXEN__
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
#endif

#undef R

// scale on entry
#define INITSCALEMTOF (.2f)

// how much the automap moves window per tic in frame-buffer coordinates
#define F_PANINC    4                   // moves 140 pixels in 1 second

// how much zoom-in per tic
#define M_ZOOMIN        (1.02f)        // goes to 2x in 1 second

// how much zoom-out per tic
#define M_ZOOMOUT       (1.02f)        // pulls out to 0.5x in 1 second

// translates between frame-buffer and map distances
#define FTOM(x) ((x) * scale_ftom)
#define MTOF(x) ((x) * scale_mtof)
#define MTOFX(x) ((x) * scale_mtof)

// translates between frame-buffer and map coordinates
#define CXMTOF(x)  (f_x + MTOF((x) - m_x))
#define CYMTOF(y)  (f_y + (f_h - MTOF((y) - m_y)))
#define CXMTOFX(x)  (f_x + MTOFX((x) - m_x))
#define CYMTOFX(y)  (f_y + (f_h - MTOFX((y) - m_y)))

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

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

DEFCC(CCmdMapAction);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void AM_changeWindowScale(void);
static void AM_drawLevelName(void);
static void AM_drawWorldTimer(void);
static void AM_addMark(void);
static void AM_clearMarks(void);
static void AM_saveScaleAndLoc(void);
static void AM_minOutWindowScale(void);
static void AM_restoreScaleAndLoc(void);
static void AM_setWinPos(void);
static void AM_drawMline2(mline_t *ml, mapline_t *c, boolean caps,
                          boolean blend);

#ifdef __JDOOM__
static void AM_drawFragsTable(void);
#elif __JHEXEN__ || __JSTRIFE__
static void AM_drawDeathmatchStats(void);
#endif

static void AM_ClearAllLists(boolean destroy);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float menu_alpha;
extern boolean viewactive;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static vectorgrap_t *vectorGraphs[NUM_VECTOR_GRAPHS];

int     cheating = 0;
boolean automapactive = false;
boolean amap_fullyopen = false;

boolean freezeMapRLs = false;

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
    {"rend-dev-freeze-map", CVF_NO_ARCHIVE, CVT_BYTE, &freezeMapRLs, 0, 1},
    {NULL}
};

ccmd_t  mapCCmds[] = {
    {"automap",     "", CCmdMapAction},
    {"follow",      "", CCmdMapAction},
    {"rotate",      "", CCmdMapAction},
    {"addmark",     "", CCmdMapAction},
    {"clearmarks",  "", CCmdMapAction},
    {"grid",        "", CCmdMapAction},
    {"zoommax",     "", CCmdMapAction},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JDOOM__
static int maplumpnum = 0; // if 0 no background image will be drawn
#elif __JHERETIC__
static int maplumpnum = 1; // if 0 no background image will be drawn
#else
static int maplumpnum = 1; // if 0 no background image will be drawn
#endif

#if __JHEXEN__
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
#endif

static int scissorState[5];

static int scrwidth = 0;    // real screen dimensions
static int scrheight = 0;

static int finit_height = SCREENHEIGHT;

static float am_alpha = 0;

static int bigstate = 0;
static int grid = 0;
static int followplayer = 1;        // specifies whether to follow the player around

// location of window on screen
static int f_x, f_y;

// size of window on screen
static int f_w, f_h;

static mpoint_t m_paninc;        // how far the window pans each tic (map coords)
static float mtof_zoommul;        // how far the window zooms in each tic (map coords)
static float ftom_zoommul;        // how far the window zooms in each tic (fb coords)

static float m_x, m_y;        // LL x,y where the window is on the map (map coords)
static float m_x2, m_y2;        // UR x,y where the window is on the map (map coords)

// width/height of window on map (map coords)
static float m_w, m_h;

// based on level size
static float min_x;
static float min_y;
static float max_x;
static float max_y;

static float min_scale_mtof;        // used to tell when to stop zooming out
static float max_scale_mtof;        // used to tell when to stop zooming in

// old stuff for recovery later
static float old_m_w, old_m_h;
static float old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static float scale_mtof = INITSCALEMTOF;

// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static float scale_ftom;

static player_t *plr;                // the player represented by an arrow

static int markpnums[10];            // numbers used for marking by the automap (lump indices)
static mpoint_t markpoints[AM_NUMMARKPOINTS];    // where the points are
static int markpointnum = 0;            // next point to be assigned

static boolean stopped = true;

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

//
// Rendering lists.
//
static amquadlist_t *amQuadListHead;

// Lines.
static amprimlist_t amLineList;

// CODE --------------------------------------------------------------------

/**
 * Register cvars and ccmds for the automap
 * Called during the PreInit of each game
 */
void AM_Register(void)
{
    int         i;

    for(i = 0; mapCVars[i].name; ++i)
        Con_AddVariable(&mapCVars[i]);
    for(i = 0; mapCCmds[i].name; ++i)
        Con_AddCommand(&mapCCmds[i]);
}

/**
 * Called during init.
 */
void AM_Init(void)
{
    memset(vectorGraphs, 0, sizeof(vectorGraphs));

    amLineList.type = DGL_LINES;
    amLineList.head = amLineList.tail = amLineList.unused = NULL;
}

/**
 * Called during shutdown.
 */
void AM_Shutdown(void)
{
    uint        i;
    amquadlist_t *list, *listp;
    amprim_t   *n, *np;
    amprim_t   *nq, *npq;

    AM_ClearAllLists(true);

    // Empty the free node lists too.
    // Lines.
    n = amLineList.unused;
    while(n)
    {
        np = n->next;
        free(n);
        n = np;
    }
    amLineList.unused = NULL;

    // Quads.
    list = amQuadListHead;
    while(list)
    {
        listp = list->next;

        nq = list->primlist.unused;
        while(nq)
        {
            npq = nq->next;
            free(nq);
            nq = npq;
        }
        list->primlist.unused = NULL;
        free(list);

        list = listp;
    }

    // Vector graphics.
    for(i = 0; i < NUM_VECTOR_GRAPHS; ++i)
    {
        vectorgrap_t *vg = vectorGraphs[i];

        if(vg)
        {
            free(vg->lines);
            free(vg);
        }
    }
}

static vectorgrap_t *AM_GetVectorGraphic(uint idx)
{
    vectorgrap_t *vg;
    mline_t    *lines;

    if(idx > NUM_VECTOR_GRAPHS - 1)
        return NULL;

    if(vectorGraphs[idx])
        return vectorGraphs[idx];

    // Not loaded yet.
    {
    uint        i, linecount;

    vg = vectorGraphs[idx] = malloc(sizeof(*vg));

    switch(idx)
    {
    case VG_KEYSQUARE:
        lines = keysquare;
        linecount = sizeof(keysquare) / sizeof(mline_t);
        break;

    case VG_TRIANGLE:
        lines = thintriangle_guy;
        linecount = sizeof(thintriangle_guy) / sizeof(mline_t);
        break;

    case VG_ARROW:
        lines = player_arrow;
        linecount = sizeof(player_arrow) / sizeof(mline_t);
        break;

#if !__JHEXEN__
    case VG_CHEATARROW:
        lines = cheat_player_arrow;
        linecount = sizeof(cheat_player_arrow) / sizeof(mline_t);
        break;
#endif

    default:
        Con_Error("AM_GetVectorGraphic: Unknown idx %i.", idx);
        break;
    }

    vg->lines = malloc(linecount * sizeof(mline_t));
    vg->count = linecount;
    for(i = 0; i < linecount; ++i)
        memcpy(&vg->lines[i], &lines[i], sizeof(mline_t));
    }

    return vg;
}

/**
 * Activate the new scale
 */
static void AM_activateNewScale(void)
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

/**
 * Save the current scale/location
 */
static void AM_saveScaleAndLoc(void)
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

/**
 * Restore the scale/location from last automap viewing
 */
static void AM_restoreScaleAndLoc(void)
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
        m_x = FIX2FLT(plr->plr->mo->pos[VX]) - m_w / 2.0f;
        m_y = FIX2FLT(plr->plr->mo->pos[VY]) - m_h / 2.0f;
    }
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = (float) f_w / m_w;
    scale_ftom = 1.0f / scale_mtof;
}

/**
 * Adds a marker at the current location
 */
static void AM_addMark(void)
{
#if _DEBUG
Con_Message("Adding mark point %i at X=%g Y=%g\n",
            (markpointnum + 1) % AM_NUMMARKPOINTS, m_x + m_w / 2.0f, m_y + m_h / 2.0f);
#endif

    markpoints[markpointnum].pos[VX] = m_x + m_w / 2.0f;
    markpoints[markpointnum].pos[VY] = m_y + m_h / 2.0f;
    markpoints[markpointnum].pos[VZ] = 0;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
}

/**
 * Determines bounding box of all vertices,
 * sets global variables controlling zoom range.
 */
static void AM_findMinMaxBoundaries(void)
{
    uint        i;
    float       pos[2];
    float       a, b;
    float       max_w;
    float       max_h;
    float       min_w;
    float       min_h;

    min_x = min_y = (float) DDMAXINT;
    max_x = max_y = (float) -DDMAXINT;

    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, pos);

        if(pos[VX] < min_x)
            min_x = pos[VX];
        else if(pos[VX] > max_x)
            max_x = pos[VX];

        if(pos[VY] < min_y)
            min_y = pos[VY];
        else if(pos[VY] > max_y)
            max_y = pos[VY];
    }

    max_w = max_x - min_x;
    max_h = max_y - min_y;

    min_w = FIX2FLT(2 * PLAYERRADIUS);    // const? never changed?
    min_h = FIX2FLT(2 * PLAYERRADIUS);

    a = (float) f_w / max_w;
    b = (float) f_h / max_h;

    min_scale_mtof = (a < b ? a : b);
    max_scale_mtof = (float) f_h / FIX2FLT(2 * PLAYERRADIUS);
}

/**
 * Change the window scale
 */
static void AM_changeWindowLoc(void)
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

/**
 * Init variables used in automap
 */
static void AM_initVariables(void)
{
    int         i, pnum;

    automapactive = true;

    f_oldloc.pos[VX] = (float) DDMAXINT;
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
        for(pnum = 0; pnum < MAXPLAYERS; ++pnum)
            if(players[pnum].plr->ingame)
                break;
    }

    plr = &players[pnum];
    m_x = FIX2FLT(plr->plr->mo->pos[VX]) - m_w / 2.0f;
    m_y = FIX2FLT(plr->plr->mo->pos[VY]) - m_h / 2.0f;
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    AM_setWinPos();
}

/**
 * Load any graphics needed for the automap
 */
static void AM_loadPics(void)
{
    int         i;
    char        namebuf[9];

#if !__DOOM64TC__
    // FIXME >
    for(i = 0; i < 10; ++i)
    {
        MARKERPATCHES; // Check the macros eg: "sprintf(namebuf, "AMMNUM%d", i)" for jDoom

        markpnums[i] = W_GetNumForName(namebuf);
    }
    // < FIXME
#endif

    if(maplumpnum != 0)
    {
        maplumpnum = W_GetNumForName("AUTOPAGE");
    }
}

/**
 * Clears markpoint array.
 */
static void AM_clearMarks(void)
{
    int         i;

    for(i = 0; i < AM_NUMMARKPOINTS; ++i)
        markpoints[i].pos[VX] = -1;    // means empty
    markpointnum = 0;
}

/**
 * Should be called at the start of every level, right now, i figure it out myself.
 */
void AM_LevelInit(void)
{
    f_x = f_y = 0;

    f_w = Get(DD_SCREEN_WIDTH);
    f_h = Get(DD_SCREEN_HEIGHT);

    AM_clearMarks();

    AM_findMinMaxBoundaries();
    scale_mtof = min_scale_mtof / 0.7f;
    if(scale_mtof > max_scale_mtof)
        scale_mtof = min_scale_mtof;
    scale_ftom = 1.0f / scale_mtof;
}

/**
 * Stop the automap
 */
void AM_Stop(void)
{
    automapactive = false;
    amap_fullyopen = false;
    am_alpha = 0;

    stopped = true;

    GL_Update(DDUF_BORDER);
}

/**
 * Start the automap
 */
void AM_Start(void)
{
    if(!stopped)
        AM_Stop();

    stopped = false;

    if(G_GetGameState() != GS_LEVEL)
        return;  // don't show automap if we aren't in a game!

    AM_initVariables();
    AM_loadPics();
}

/**
 * Set the window scale to the minimum size.
 */
static void AM_minOutWindowScale(void)
{
    scale_mtof = min_scale_mtof;
    scale_ftom = 1.0f / scale_mtof;
    AM_activateNewScale();
}

/**
 * Set the window scale to the maximum size.
 */
static void AM_maxOutWindowScale(void)
{
    scale_mtof = max_scale_mtof;
    scale_ftom = 1.0f / scale_mtof;
    AM_activateNewScale();
}

/**
 * Changes the map scale values
 */
static void AM_changeWindowScale(void)
{
    // Change the scaling multipliers
    scale_mtof = scale_mtof * mtof_zoommul;
    scale_ftom = 1.0f / scale_mtof;

    if(scale_mtof < min_scale_mtof)
        AM_minOutWindowScale();
    else if(scale_mtof > max_scale_mtof)
        AM_maxOutWindowScale();
    else
        AM_activateNewScale();
}

/**
 * Align the map camera to the players position
 */
static void AM_doFollowPlayer(void)
{
    mobj_t *mo = plr->plr->mo;

    if(f_oldloc.pos[VX] != FIX2FLT(mo->pos[VX]) || f_oldloc.pos[VY] != FIX2FLT(mo->pos[VY]))
    {
        // Now that a high resolution is used (compared to 320x200!)
        // there is no need to quantify map scrolling. -jk
        m_x = FIX2FLT(mo->pos[VX]) - m_w / 2.0f;
        m_y = FIX2FLT(mo->pos[VY]) - m_h / 2.0f;
        m_x2 = m_x + m_w;
        m_y2 = m_y + m_h;
        f_oldloc.pos[VX] = FIX2FLT(mo->pos[VX]);
        f_oldloc.pos[VY] = FIX2FLT(mo->pos[VY]);
    }
}

/**
 * Updates on Game Tick.
 */
void AM_Ticker(void)
{
    int         mapplr = consoleplayer;

    // If the automap is not active do nothing
    if(!automapactive)
        return;

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
/*
    if(PLAYER_ACTION(mapplr, A_MAPZOOMOUT))  // zoom out
    {
        mtof_zoommul = M_ZOOMOUT;
        ftom_zoommul = M_ZOOMIN;
    }
    else if(PLAYER_ACTION(mapplr, A_MAPZOOMIN)) // zoom in
    {
        mtof_zoommul = M_ZOOMIN;
        ftom_zoommul = M_ZOOMOUT;
    }
    else
*/
    {
        mtof_zoommul = 1;
        ftom_zoommul = 1;
    }

/*
    // Camera location paning
    if(!followplayer)
    {
        // X axis pan
        if(PLAYER_ACTION(mapplr, A_MAPPANRIGHT)) // pan right?
            m_paninc.pos[VX] = FTOM(F_PANINC);
        else if(PLAYER_ACTION(mapplr, A_MAPPANLEFT)) // pan left?
            m_paninc.pos[VX] = -FTOM(F_PANINC);
        else
            m_paninc.pos[VX] = 0;

        // Y axis pan
        if(PLAYER_ACTION(mapplr, A_MAPPANUP))  // pan up?
            m_paninc.pos[VY] = FTOM(F_PANINC);
        else if(PLAYER_ACTION(mapplr, A_MAPPANDOWN))  // pan down?
            m_paninc.pos[VY] = -FTOM(F_PANINC);
        else
            m_paninc.pos[VY] = 0;
    }
    else  // Camera follows the player
*/
        AM_doFollowPlayer();

    // Change the zoom
    AM_changeWindowScale();

    // Change window location
    if(m_paninc.pos[VX] || m_paninc.pos[VY]
       /*|| oldwin_w != cfg.automapWidth || oldwin_h != cfg.automapHeight*/)
        AM_changeWindowLoc();
}

/**
 * Draw a border if needed.
 */
static void AM_clearFB(int color)
{
#if !__DOOM64TC__
    float       scaler = cfg.sbarscale / 20.0f;

    finit_height = SCREENHEIGHT;

    GL_Update(DDUF_FULLSCREEN);

    if(cfg.automapHudDisplay != 1)
    {
        GL_SetPatch(W_GetNumForName( BORDERGRAPHIC ));
        GL_DrawCutRectTiled(0, finit_height, 320, BORDEROFFSET, 16, BORDEROFFSET,
                            0, 0, 160 - 160 * scaler + 1, finit_height,
                            320 * scaler - 2, BORDEROFFSET);
    }
#endif
}

/**
 * Returns the settings for the given type of line.
 * Not a very pretty routine. Will do this with an array when I rewrite the map...
 */
static void AM_getLine(mapline_t *l, int type, int special)
{
    if(!l)
        return;

    switch(type)
    {
    default:
        // An unseen line (got the computer map)
        l->r = cfg.automapL0[0];
        l->g = cfg.automapL0[1];
        l->b = cfg.automapL0[2];
        l->a = cfg.automapLineAlpha;
        l->a2 =  cfg.automapLineAlpha;
        l->glow = NO_GLOW;
        l->w = 0;
        l->scale = false;
        break;

    case 1:
        // onesided linedef (regular wall)
        l->r = cfg.automapL1[0];
        l->g = cfg.automapL1[1];
        l->b = cfg.automapL1[2];
        l->a = cfg.automapLineAlpha;
        l->a2 = cfg.automapLineAlpha/3;
        l->glow = NO_GLOW;
        l->w = 0;
        l->scale = false;
        break;

    case 2:
        // a linedef with some sort of special
        // could be a door or teleport or something...
        switch(special)
        {
        default:
            // nope nothing special about it
            l->r = cfg.automapL0[0];
            l->g = cfg.automapL0[1];
            l->b = cfg.automapL0[2];
            l->a = 1;
            l->a2 = 1;
            l->glow = NO_GLOW;
            l->w = 0;
            l->scale = false;
            break;

#ifdef __JDOOM__
            case 32:                    // Blue locked door open
            case 26:                    // Blue Door/Locked
            case 99:
            case 133:
                l->r = 0;
                l->g = 0;
                l->b = 0.776f;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;

                l->w = 5.0f;
                l->scale = true;
                break;

            case 33:                    // Red locked door open
            case 28:                    // Red Door /Locked
            case 134:
            case 135:
                l->r = 0.682f;
                l->g = 0;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;

            case 34:                    // Yellow locked door open
            case 27:                    // Yellow Door /Locked
            case 136:
            case 137:
                l->r = 0.905f;
                l->g = 0.9f;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;

            case 11:                    // Exit switch
            case 52:                    // Exit walkover
                l->r = 0;
                l->g = 1;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;

                l->w = 5.0f;
                l->scale = true;
                break;

            case 51:                    // Exit switch (secret)
            case 124:                   // Exit walkover (secret)
                l->r = 0;
                l->g = 1;
                l->b = 1;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;

                l->w = 5.0f;
                l->scale = true;
                break;
#elif __JHERETIC__
            case 26:                    // Blue
            case 32:
                l->r = 0;
                l->g = 0;
                l->b = 0.776f;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;

            case 27:                    // Yellow
            case 34:
                l->r = 0.905f;
                l->g = 0.9f;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;

            case 28:                    // Green
            case 33:
                l->r = 0;
                l->g = 0.9f;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;
#else
            case 13:                    // Locked door line -- all locked doors are green
            case 83:
                l->r = 0;
                l->g = 0.9f;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;

            case 70:                    // intra-level teleports are blue
            case 71:
                l->r = 0;
                l->g = 0;
                l->b = 0.776f;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;

            case 74:                    // inter-level teleport/game-winning exit -- both are red
            case 75:
                l->r = 0.682f;
                l->g = 0;
                l->b = 0;
                l->a = cfg.automapLineAlpha/2;
                l->a2 = cfg.automapLineAlpha/1.5;
                if(cfg.automapDoorGlow > 0)
                    l->glow = TWOSIDED_GLOW;
                else
                    l->glow = NO_GLOW;
                l->w = 5.0f;
                l->scale = true;
                break;
#endif
        }
        break;

    case 3: // twosided linedef, floor change
        l->r = cfg.automapL2[0];
        l->g = cfg.automapL2[1];
        l->b = cfg.automapL2[2];
        l->a = cfg.automapLineAlpha;
        l->a2 = cfg.automapLineAlpha/2;
        l->glow = NO_GLOW;
        l->w = 0;
        l->scale = false;
        break;

    case 4: // twosided linedef, ceiling change
        l->r = cfg.automapL3[0];
        l->g = cfg.automapL3[1];
        l->b = cfg.automapL3[2];
        l->a = cfg.automapLineAlpha;
        l->a2 = cfg.automapLineAlpha/2;
        l->glow = NO_GLOW;
        l->w = 0;
        l->scale = false;
        break;
    }
}

/**
 * Returns a value > 0 if the line is some sort of special that we
 * might want to add a glow to, or change the colour of etc.
 */
static int AM_checkSpecial(int special)
{
    switch (special)
    {
    default:
        // If it's not a special, zero is returned.
        break;

#ifdef __JDOOM__
    case 32:                    // Blue locked door open
    case 26:                    // Blue Door/Locked
    case 99:                    // Blue door switch
    case 133:                   // Blue door switch
    case 33:                    // Red locked door open
    case 28:                    // Red Door /Locked
    case 134:                   // Red door switch
    case 135:                   // Red door switch
    case 34:                    // Yellow locked door open
    case 27:                    // Yellow Door /Locked
    case 136:                   // Yellow door switch
    case 137:                   // Yellow door switch
        return 1; // its a door

    case 11:                    // Exit switch
    case 51:                    // Exit switch (secret)
    case 52:                    // Exit walkover
    case 124:                   // Exit walkover (secret)
        return 3; // some form of exit
#elif __JHERETIC__
    case 26:
    case 32:
    case 27:
    case 34:
    case 28:
    case 33:
        // its a door
        return 1;
#else
    case 13:
    case 83:
        // its a door
        return 1;

    case 70:
    case 71:
        // intra-level teleports
        return 2;

    case 74:
    case 75:
        // inter-level teleport/game-winning exit
        return 3;

#endif
    }
    return 0;
}

static amprim_t *AM_NewPrimitive(amprimlist_t *list)
{
    amprim_t   *p;

    // Any primitives available in a passed primlist's, unused list?
    if(list && list->unused)
    {   // Yes, unlink from the unused list and use it.
        p = list->unused;
        list->unused = p->next;
    }
    else
    {   // No, allocate another.
        p = malloc(sizeof(*p));
    }

    return p;
}

static void AM_LinkPrimitiveToList(amprimlist_t *list, amprim_t *p)
{
    p->next = list->head;
    if(!list->tail)
        list->tail = p;
    list->head = p;
}

static amrline_t *AM_AllocateLine(void)
{
    amprimlist_t *list = &amLineList;
    amprim_t   *p = AM_NewPrimitive(list);

    AM_LinkPrimitiveToList(list, p);
    return &(p->data.line);
}

static amrquad_t *AM_AllocateQuad(int tex, boolean blend)
{
    amquadlist_t *list;
    amprim_t   *p;
    boolean     found;

    // Find a suitable primitive list (matching texture & blend).
    list = amQuadListHead;
    found = false;
    while(list && !found)
    {
        if(list->tex == tex && list->blend == blend)
            found = true;
        else
            list = list->next;
    }

    if(!found)
    {   // Allocate a new list.
        list = malloc(sizeof(*list));

        list->tex = tex;
        list->blend = blend;
        list->primlist.type = DGL_QUADS;
        list->primlist.head = list->primlist.tail =
            list->primlist.unused = NULL;

        // Link it in.
        list->next = amQuadListHead;
        amQuadListHead = list;
    }

    p = AM_NewPrimitive(&list->primlist);
    AM_LinkPrimitiveToList(&list->primlist, p);

    return &(p->data.quad);
}

static void AM_DeleteList(amprimlist_t *list, boolean destroy)
{
    amprim_t *n, *np;

    // Are we destroying the lists?
    if(destroy)
    {   // Yes, free the nodes.
        n = list->head;
        while(n)
        {
            np = n->next;
            free(n);
            n = np;
        }
    }
    else
    {   // No, move all nodes to the free list.
        n = list->tail;
        if(list->tail && list->unused)
            n->next = list->unused;
    }

    list->head = list->tail = NULL;
}

static void AM_ClearAllLists(boolean destroy)
{
    amquadlist_t *list;

    // Lines.
    AM_DeleteList(&amLineList, destroy);

    // Quads.
    list = amQuadListHead;
    while(list)
    {
        AM_DeleteList(&list->primlist, destroy);
        list = list->next;
    }
}

static void AM_AddLine(float x, float y, float x2, float y2, int color,
                       float alpha)
{
    amrline_t *l;

    l = AM_AllocateLine();

    l->a.pos[0] = x;
    l->a.pos[1] = y;
    l->b.pos[0] = x2;
    l->b.pos[1] = y2;

    l->type = AMLT_PALCOL;
    l->coldata.palcolor.color = color;
    l->coldata.palcolor.alpha = CLAMP(alpha, 0.0, 1.0f);
}

static void AM_AddLine4f(float x, float y, float x2, float y2,
                         float r, float g, float b, float a)
{
    amrline_t *l;

    l = AM_AllocateLine();

    l->a.pos[0] = x;
    l->a.pos[1] = y;
    l->b.pos[0] = x2;
    l->b.pos[1] = y2;

    l->type = AMLT_RGBA;
    l->coldata.f4color.rgba[0] = r;
    l->coldata.f4color.rgba[1] = g;
    l->coldata.f4color.rgba[2] = b;
    l->coldata.f4color.rgba[3] = CLAMP(a, 0.0, 1.0f);
}

static void AM_AddQuad(float x1, float y1, float x2, float y2,
                       float x3, float y3, float x4, float y4,
                       float tc1st1, float tc1st2,
                       float tc2st1, float tc2st2,
                       float tc3st1, float tc3st2,
                       float tc4st1, float tc4st2,
                       float r, float g, float b, float a,
                       int tex, boolean blend)
{
    // Vertex layout.
    // 1--2
    // | /|
    // |/ |
    // 3--4
    //
    amrquad_t *q;

    q = AM_AllocateQuad(tex, blend);

    q->rgba[0] = r;
    q->rgba[1] = g;
    q->rgba[2] = b;
    q->rgba[3] = a;

    // V1
    q->verts[0].pos[0] = x1;
    q->verts[0].tex[0] = tc1st1;
    q->verts[0].pos[1] = y1;
    q->verts[0].tex[1] = tc1st2;

    // V2
    q->verts[1].pos[0] = x2;
    q->verts[1].tex[0] = tc2st1;
    q->verts[1].pos[1] = y2;
    q->verts[1].tex[1] = tc2st2;

    // V3
    q->verts[2].pos[0] = x3;
    q->verts[2].tex[0] = tc3st1;
    q->verts[2].pos[1] = y3;
    q->verts[2].tex[1] = tc3st2;

    // V4
    q->verts[3].pos[0] = x4;
    q->verts[3].tex[0] = tc4st1;
    q->verts[3].pos[1] = y4;
    q->verts[3].tex[1] = tc4st2;
}

/**
 * Draws the given line. No cliping is done!
 */
static void AM_drawMline(mline_t *ml, int color, float alpha)
{
    AM_AddLine(CXMTOFX(ml->a.pos[VX]),
               CYMTOFX(ml->a.pos[VY]),
               CXMTOFX(ml->b.pos[VX]),
               CYMTOFX(ml->b.pos[VY]),
               color,
               (am_alpha - (1 - alpha)));
}

/**
 * Draws the given line including any optional extras
 */
static void AM_drawMline2(mline_t *ml, mapline_t *c, boolean caps,
                          boolean blend)
{
    float       a[2], b[2];
    float       length, dx, dy;

    // Scale into map, screen space units.
    a[VX] = CXMTOFX(ml->a.pos[VX]);
    a[VY] = CYMTOFX(ml->a.pos[VY]);
    b[VX] = CXMTOFX(ml->b.pos[VX]);
    b[VY] = CYMTOFX(ml->b.pos[VY]);

    dx = b[VX] - a[VX];
    dy = b[VY] - a[VY];
    length = sqrt(dx * dx + dy * dy);
    if(length <= 0)
        return;

    // Is this a glowing line?
    if(c->glow > NO_GLOW)
    {
        int         tex;
        float       normal[2], unit[2], thickness;

        // Scale line thickness relative to zoom level?
        if(c->scale)
            thickness = cfg.automapDoorGlow * scale_mtof * 2.5f + 3;
        else
            thickness = c->w;

        tex = Get(DD_DYNLIGHT_TEXTURE);

        unit[VX] = dx / length;
        unit[VY] = dy / length;
        normal[VX] = unit[VY];
        normal[VY] = -unit[VX];

        if(caps)
        {
            // Draw a "cap" at the start of the line
            AM_AddQuad(a[VX] - unit[VX] * thickness + normal[VX] * thickness,
                       a[VY] - unit[VY] * thickness + normal[VY] * thickness,
                       a[VX] + normal[VX] * thickness,
                       a[VY] + normal[VY] * thickness,
                       a[VX] - normal[VX] * thickness,
                       a[VY] - normal[VY] * thickness,
                       a[VX] - unit[VX] * thickness - normal[VX] * thickness,
                       a[VY] - unit[VY] * thickness - normal[VY] * thickness,
                       0, 0,
                       0.5f, 0,
                       0.5f, 1,
                       0, 1,
                       c->r, c->g, c->b, am_alpha - (1 - c->a2),
                       tex, blend);
        }

        // The middle part of the line.
        switch(c->glow)
        {
        case TWOSIDED_GLOW:
            AM_AddQuad(a[VX] + normal[VX] * thickness,
                       a[VY] + normal[VY] * thickness,
                       b[VX] + normal[VX] * thickness,
                       b[VY] + normal[VY] * thickness,
                       b[VX] - normal[VX] * thickness,
                       b[VY] - normal[VY] * thickness,
                       a[VX] - normal[VX] * thickness,
                       a[VY] - normal[VY] * thickness,
                       0.5f, 0,
                       0.5f, 0,
                       0.5f, 1,
                       0.5f, 1,
                       c->r, c->g, c->b, am_alpha - (1 - c->a2),
                       tex, blend);
            break;

        case BACK_GLOW:
            AM_AddQuad(a[VX] + normal[VX] * thickness,
                       a[VY] + normal[VY] * thickness,
                       b[VX] + normal[VX] * thickness,
                       b[VY] + normal[VY] * thickness,
                       b[VX], b[VY],
                       a[VX], a[VY],
                       0, 0.25f,
                       0, 0.25f,
                       0.5f, 0.25f,
                       0.5f, 0.25f,
                       c->r, c->g, c->b, am_alpha - (1 - c->a2),
                       tex, blend);
            break;

        case FRONT_GLOW:
            AM_AddQuad(a[VX], a[VY],
                       b[VX], b[VY],
                       b[VX] - normal[VX] * thickness,
                       b[VY] - normal[VY] * thickness,
                       a[VX] - normal[VX] * thickness,
                       a[VY] - normal[VY] * thickness,
                       0.75f, 0.5f,
                       0.75f, 0.5f,
                       0.75f, 1,
                       0.75f, 1,
                       c->r, c->g, c->b, am_alpha - (1 - c->a2),
                       tex, blend);
            break;

        default:
            break; // Impossible.
        }

        if(caps)
        {
            // Draw a "cap" at the end of the line.
            AM_AddQuad(b[VX] + normal[VX] * thickness,
                       b[VY] + normal[VY] * thickness,
                       b[VX] + unit[VX] * thickness + normal[VX] * thickness,
                       b[VY] + unit[VY] * thickness + normal[VY] * thickness,
                       b[VX] + unit[VX] * thickness - normal[VX] * thickness,
                       b[VY] + unit[VY] * thickness - normal[VY] * thickness,
                       b[VX] - normal[VX] * thickness,
                       b[VY] - normal[VY] * thickness,
                       0.5f, 0,
                       1, 0,
                       1, 1,
                       0.5f, 1,
                       c->r, c->g, c->b, am_alpha - (1 - c->a2),
                       tex, blend);
        }
    }

    AM_AddLine4f(a[VX], a[VY], b[VX], b[VY],
                 c->r, c->g, c->b, am_alpha - (1 - c->a));
}

/**
 * Draws blockmap aligned grid lines.
 */
static void renderGrid(int color)
{
    float       x, y;
    float       start, end;
    float       originx = FIX2FLT(*((fixed_t*) DD_GetVariable(DD_BLOCKMAP_ORIGIN_X)));
    float       originy = FIX2FLT(*((fixed_t*) DD_GetVariable(DD_BLOCKMAP_ORIGIN_Y)));
    mline_t     ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if((int) (start - originx) % MAPBLOCKUNITS)
        start += MAPBLOCKUNITS - ((int) (start - originx) % MAPBLOCKUNITS);
    end = m_x + m_w;

    // Draw vertical gridlines
    ml.a.pos[VY] = m_y;
    ml.b.pos[VY] = m_y + m_h;

    for(x = start; x < end; x += MAPBLOCKUNITS)
    {
        ml.a.pos[VX] = x;
        ml.b.pos[VX] = x;
        AM_drawMline(&ml, color, cfg.automapLineAlpha);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if((int) (start - originy) % MAPBLOCKUNITS)
        start += MAPBLOCKUNITS - ((int) (start - originy) % MAPBLOCKUNITS);
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.pos[VX] = m_x;
    ml.b.pos[VX] = m_x + m_w;
    for(y = start; y < end; y += MAPBLOCKUNITS)
    {
        ml.a.pos[VY] = y;
        ml.b.pos[VY] = y;
        AM_drawMline(&ml, color, cfg.automapLineAlpha);
    }
}

/**
 * Determines visible lines, draws them.
 */
static void renderWalls(void)
{
    int         i, segcount, flags;
    uint        s, subColor = 0;
    line_t     *line;
    xline_t    *xline;
    mline_t     l;
    sector_t   *frontsector, *backsector;
    seg_t      *seg;
    mapline_t   templine;
    boolean     withglow = false;

    for(s = 0; s < numsubsectors; ++s)
    {
        if(cheating == 3)        // Debug cheat, show subsectors
        {
            subColor++;
            if(subColor == 10)
                subColor = 0;
        }

        segcount = P_GetInt(DMU_SUBSECTOR, s, DMU_SEG_COUNT);
        for(i = 0; i < segcount; ++i)
        {
            seg = P_GetPtr(DMU_SUBSECTOR, s, DMU_SEG_OF_SUBSECTOR | i);
            line = P_GetPtrp(seg, DMU_LINE);
            if(!line)
                continue;

            P_GetFloatpv(seg, DMU_VERTEX1_XY, l.a.pos);
            P_GetFloatpv(seg, DMU_VERTEX2_XY, l.b.pos);

            if(cheating == 3)        // Debug cheat, show subsectors
            {
                AM_drawMline2(&l, &subColors[subColor], false, true);
                continue;
            }

            frontsector = P_GetPtrp(seg, DMU_FRONT_SECTOR);
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
                    AM_getLine(&templine, 1, 0);
                    AM_drawMline2(&l, &templine, false, true);
                    continue;
                }
            }
#endif
#endif
            backsector = P_GetPtrp(seg, DMU_BACK_SECTOR);
            flags = P_GetIntp(line, DMU_FLAGS);
            if(cheating || (flags & ML_MAPPED))
            {
                int     specialType;

                if((flags & LINE_NEVERSEE) && !cheating)
                    continue;

                specialType = AM_checkSpecial(xline->special);

                if(cfg.automapShowDoors &&
                   (specialType == 1 || (specialType > 1 && cheating)))
                {
                    // some sort of special

                    if(cfg.automapDoorGlow > 0)
                        withglow = true;

                    AM_getLine(&templine, 2, xline->special);
                    AM_drawMline2(&l, &templine, withglow, withglow);
                }
                else if(!backsector)
                {
                    // solid wall (well probably anyway...)
                    AM_getLine(&templine, 1, 0);
                    AM_drawMline2(&l, &templine, false, false);
                }
                else
                {
                    if(flags & ML_SECRET)
                    {
                        // secret door
                        AM_getLine(&templine, 1, 0);
                        AM_drawMline2(&l, &templine, false, false);
                    }
                    else if(P_GetFloatp(backsector, DMU_FLOOR_HEIGHT) !=
                            P_GetFloatp(frontsector, DMU_FLOOR_HEIGHT))
                    {
                        // floor level change
                        AM_getLine(&templine, 3, 0);
                        AM_drawMline2(&l, &templine, false, false);
                    }
                    else if(P_GetFloatp(backsector, DMU_CEILING_HEIGHT) !=
                            P_GetFloatp(frontsector, DMU_CEILING_HEIGHT))
                    {
                        // ceiling level change
                        AM_getLine(&templine, 4, 0);
                        AM_drawMline2(&l, &templine, false, false);
                    }
                    else if(cheating)
                    {
                        AM_getLine(&templine, 0, 0);
                        AM_drawMline2(&l, &templine, false, false);
                    }
                }
            }
            else if(plr->powers[PT_ALLMAP])
            {
                if(!(flags & LINE_NEVERSEE))
                {
                    // as yet unseen line
                    AM_getLine(&templine, 0, 0);
                    AM_drawMline2(&l, &templine, false, false);
                }
            }
        }
    }
}

/**
 * Rotation in 2D. Used to rotate player arrow line character.
 */
static void AM_rotate(float *x, float *y, angle_t a)
{
    int         angle;
    float       tmpx;

    angle = a >> ANGLETOFINESHIFT;
    tmpx = (*x * FIX2FLT(finecosine[angle])) -
                (*y * FIX2FLT(finesine[angle]));

    *y = (*x * FIX2FLT(finesine[angle])) +
                (*y * FIX2FLT(finecosine[angle]));

    *x = tmpx;
}

/**
 * Draws a line character (eg the player arrow)
 */
static void addLineCharacter(vectorgrap_t *vg, float scale,
                             angle_t angle, int color, float alpha,
                             float x, float y)
{
    uint        i;
    mline_t     l;

    for(i = 0; i < vg->count; ++i)
    {
        l.a.pos[VX] = vg->lines[i].a.pos[VX];
        l.a.pos[VY] = vg->lines[i].a.pos[VY];

        l.a.pos[VX] *= scale;
        l.a.pos[VY] *= scale;

        AM_rotate(&l.a.pos[VX], &l.a.pos[VY], angle);

        l.a.pos[VX] += x;
        l.a.pos[VY] += y;

        l.b.pos[VX] = vg->lines[i].b.pos[VX];
        l.b.pos[VY] = vg->lines[i].b.pos[VY];

        l.b.pos[VX] *= scale;
        l.b.pos[VY] *= scale;

        AM_rotate(&l.b.pos[VX], &l.b.pos[VY], angle);

        l.b.pos[VX] += x;
        l.b.pos[VY] += y;

        AM_drawMline(&l, color, alpha);
    }
}

/**
 * Draws all players on the map using a line character
 */
static void renderPlayers(void)
{
    int         i;
#if __JDOOM__
    static int  their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
#elif __JHERETIC__
    static int  their_colors[] = { KEY3, KEY2, BLOODRED, KEY1 };
#endif
    float       size = FIX2FLT(PLAYERRADIUS);
    vectorgrap_t *vg;

#if __JDOOM__
    if(!IS_NETGAME && cheating)
    {
        vg = AM_GetVectorGraphic(VG_CHEATARROW);
    }
    else
#endif
    {
        vg = AM_GetVectorGraphic(VG_ARROW);
    }

    if(!IS_NETGAME)
    {
        /* $unifiedangles */
        addLineCharacter(vg, size, plr->plr->mo->angle, WHITE,
                         cfg.automapLineAlpha,
                         FIX2FLT(plr->plr->mo->pos[VX]),
                         FIX2FLT(plr->plr->mo->pos[VY]));
        return;
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t   *p = &players[i];
        int         color;
        float       alpha;

#if __JDOOM__ || __JHERETIC__
        if(deathmatch && p != &players[consoleplayer])
            continue;
#endif
        if(!p->plr->ingame)
            continue;

        color = their_colors[cfg.playerColor[i]];
        alpha = cfg.automapLineAlpha;

#if !__JHEXEN__
        if(p->powers[PT_INVISIBILITY])
            alpha *= 0.125;
#endif

        /* $unifiedangles */
        addLineCharacter(vg, size, p->plr->mo->angle, color, alpha,
                         FIX2FLT(p->plr->mo->pos[VX]),
                         FIX2FLT(p->plr->mo->pos[VY]));
    }
}

/**
 * Draws all things on the map
 */
static void renderThings(int color, int colorrange)
{
    uint        i;
    mobj_t     *iter;
    float       size = FIX2FLT(PLAYERRADIUS);
    vectorgrap_t *vg = AM_GetVectorGraphic(VG_TRIANGLE);

    for(i = 0; i < numsectors; ++i)
    {
        for(iter = P_GetPtr(DMU_SECTOR, i, DMU_THINGS); iter;
            iter = iter->snext)
        {
            addLineCharacter(vg, size, iter->angle,
                             color, cfg.automapLineAlpha,
                             FIX2FLT(iter->pos[VX]), FIX2FLT(iter->pos[VY]));
        }
    }
}

/**
 * Draws all the points marked by the player
 * FIXME: marks could be drawn without rotation
 */
static void AM_drawMarks(void)
{
#if !__DOOM64TC__
    int         i;

    gl.Color4f(1, 1, 1, 1);

    // FIXME >
    for(i = 0; i < AM_NUMMARKPOINTS; ++i)
    {
        if(markpoints[i].pos[VX] != -1)
        {
            GL_DrawPatch_CS(CXMTOF(markpoints[i].pos[VX]),
                            CYMTOF(markpoints[i].pos[VY]), markpnums[i]);
        }
    }
    // < FIXME
#endif
}

/**
 * Draws all the keys on the map using the keysquare line character.
 */
static void renderKeys(void)
{
    int         keyColor;
    thinker_t  *th;
    mobj_t     *mo;
    float       size = FIX2FLT(PLAYERRADIUS);

    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)           
            continue;

        mo = (mobj_t *) th;
#if __JDOOM__
        if(mo->type == MT_MISC4)
            keyColor = KEY1;
        else if(mo->type == MT_MISC5)
            keyColor = KEY2;
        else if(mo->type == MT_MISC6)
            keyColor = KEY3;
        else if(mo->type == MT_MISC7)
            keyColor = KEY4;
        else if(mo->type == MT_MISC8)
            keyColor = KEY5;
        else if(mo->type == MT_MISC9)
            keyColor = KEY6;
        else
#elif __JHERETIC__
        if(mo->type == MT_CKEY)
            keyColor = KEY1;
        else if(mo->type == MT_BKYY)
            keyColor = KEY2;
        else if(mo->type == MT_AKYY)
            keyColor = KEY3;
        else
#endif
            continue; // not a key.

        addLineCharacter(AM_GetVectorGraphic(VG_KEYSQUARE), size, 0,
                         keyColor, cfg.automapLineAlpha,
                         FIX2FLT(mo->pos[VX]), FIX2FLT(mo->pos[VY]));
    }
}

/**
 * Changes the position of the automap window
 */
static void AM_setWinPos(void)
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

/**
 * Sets up the state for automap drawing
 */
static void AM_GL_SetupState(void)
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
    if(maplumpnum)
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
        gl.Rotatef(plr->plr->mo->angle / (float) ANGLE_MAX * 360 - 90, 0, 0, 1); /* $unifiedangles */

    gl.Translatef(-(winx+(winw / 2.0f)), -(winy+(winh /2.0f)), 0);
}

/**
 * Restores the previous gl draw state
 */
static void AM_GL_RestoreState(void)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    if(!scissorState[0])
        gl.Disable(DGL_SCISSOR_TEST);
    gl.Scissor(scissorState[1], scissorState[2], scissorState[3],
               scissorState[4]);
}

/**
 * Handles what counters to draw eg title, timer, dm stats etc
 */
static void AM_drawCounters(void)
{
#if __JDOOM__ || __JHERETIC__
    char        buf[40], tmp[20];
    int         x = 5, y = LINEHEIGHT_A * 3;

    gl.Color3f(1, 1, 1);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
#endif

    gl.Enable(DGL_TEXTURING);

    AM_drawWorldTimer();

#if __JDOOM__ || __JHERETIC__
    Draw_BeginZoom(cfg.counterCheatScale, x, y);

    if(cfg.counterCheat)
    {
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

/**
 * Draws the level name into the automap window
 */
static void AM_drawLevelName(void)
{
#define FIXXTOSCREENX(x) (scrwidth * (x / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * (y / (float) SCREENHEIGHT))
    int         x, y, otherY;
    const char *lname = "";

    lname = DD_GetVariable(DD_MAP_NAME);

#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name
    if(!lname)
        lname = P_GetMapName(gamemap);
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
#if !__DOOM64TC__
        if(cfg.setblocks <= 11 || cfg.automapHudDisplay == 2)
        {   // We may need to adjust for the height of the statusbar
            otherY = FIXYTOSCREENY(ST_Y);
            otherY += FIXYTOSCREENY(ST_HEIGHT) * (1 - (cfg.sbarscale / 20.0f));

            if(y > otherY)
                y = otherY;
        }
        else if(cfg.setblocks == 12)
#endif
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

static void AM_RenderList(int tex, boolean blend, amprimlist_t *list)
{
    amprim_t *p;

    // Change render state for this list?
    if(tex)
    {
        gl.Enable(DGL_TEXTURING);
        gl.Bind(tex);
    }
    else
    {
        gl.Disable(DGL_TEXTURING);
    }

    if(blend)
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);

    // Write commands.
    p = list->head;
    gl.Begin(list->type);
    switch(list->type)
    {
    case DGL_QUADS:
        while(p)
        {
            gl.Color4fv(p->data.quad.rgba);

            // V1
            gl.TexCoord2f(p->data.quad.verts[0].tex[0],
                          p->data.quad.verts[0].tex[1]);
            gl.Vertex2f(p->data.quad.verts[0].pos[0],
                        p->data.quad.verts[0].pos[1]);
            // V2
            gl.TexCoord2f(p->data.quad.verts[1].tex[0],
                          p->data.quad.verts[1].tex[1]);
            gl.Vertex2f(p->data.quad.verts[1].pos[0],
                        p->data.quad.verts[1].pos[1]);
            // V3
            gl.TexCoord2f(p->data.quad.verts[2].tex[0],
                          p->data.quad.verts[2].tex[1]);
            gl.Vertex2f(p->data.quad.verts[2].pos[0],
                        p->data.quad.verts[2].pos[1]);
            // V4
            gl.TexCoord2f(p->data.quad.verts[3].tex[0],
                          p->data.quad.verts[3].tex[1]);
            gl.Vertex2f(p->data.quad.verts[3].pos[0],
                        p->data.quad.verts[3].pos[1]);

            p = p->next;
        }
        break;

    case DGL_LINES:
        while(p)
        {
            if(p->data.line.type == AMLT_PALCOL)
            {
                GL_SetColor2(p->data.line.coldata.palcolor.color,
                             p->data.line.coldata.palcolor.alpha);
            }
            else
            {
                gl.Color4fv(p->data.line.coldata.f4color.rgba);
            }

            gl.Vertex2f(p->data.line.a.pos[0], p->data.line.a.pos[1]);
            gl.Vertex2f(p->data.line.b.pos[0], p->data.line.b.pos[1]);

            p = p->next;
        }
        break;

    default:
        break;
    }
    gl.End();

    // Restore previous state.
    if(!tex)
        gl.Enable(DGL_TEXTURING);
    if(blend)
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}

static void AM_RenderAllLists(void)
{
    amquadlist_t *list;

    // Quads.
    list = amQuadListHead;
    while(list)
    {
        AM_RenderList(list->tex, list->blend, &list->primlist);
        list = list->next;
    }

    // Lines.
    AM_RenderList(0, 0, &amLineList);
}

void AM_Drawer(void)
{
    // If the automap isn't active, draw nothing
    if(!automapactive)
        return;

    // Setup for frame.
    AM_clearFB(BACKGROUND);
    AM_GL_SetupState();

    if(!freezeMapRLs)
    {
        AM_ClearAllLists(false);

        // Draw.
        if(grid)
            renderGrid(GRIDCOLORS);

        renderWalls();
        renderPlayers();

        if(cheating == 2)
            renderThings(THINGCOLORS, THINGRANGE);

#if !__JHEXEN__
        if(gameskill == SM_BABY && cfg.automapBabyKeys)
            renderKeys();
#endif
    }
    AM_RenderAllLists();

    // Draw any marked points
    if(!freezeMapRLs)
        AM_drawMarks();

    gl.PopMatrix();

    AM_drawLevelName();
    AM_GL_RestoreState();
    AM_drawCounters();
}

#if __JDOOM__
/**
 * Draws a sorted frags list in the lower right corner of the screen.
 */
static void AM_drawFragsTable(void)
{
#define FRAGS_DRAWN    -99999
    int         i, k, y, inCount = 0;    // How many players in the game?
    int         totalFrags[MAXPLAYERS];
    int         max, choose = 0;
    int         w = 30;
    char       *name, tmp[40];

    memset(totalFrags, 0, sizeof(totalFrags));
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->ingame)
            continue;

        inCount++;
        for(k = 0; k < MAXPLAYERS; ++k)
            totalFrags[i] += players[i].frags[k] * (k != i ? 1 : -1);
    }

    // Start drawing from the top.
# if __DOOM64TC__
    y = HU_TITLEY + 32 * (inCount - 1) * LINEHEIGHT_A;
# else
    y = HU_TITLEY + 32 * (20 - cfg.sbarscale) / 20 - (inCount - 1) * LINEHEIGHT_A;
#endif
    for(i = 0; i < inCount; ++i, y += LINEHEIGHT_A)
    {
        // Find the largest.
        for(max = FRAGS_DRAWN + 1, k = 0; k < MAXPLAYERS; ++k)
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
        switch(cfg.playerColor[choose])
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

/**
 * Draws the deathmatch stats
 * TODO: Merge with AM_drawFragsTable()
 */
#ifndef __JHERETIC__
#ifndef __JDOOM__
static void AM_drawDeathmatchStats(void)
{
    int         i, j, k, m;
    int         fragCount[MAXPLAYERS];
    int         order[MAXPLAYERS];
    char        textBuffer[80];
    int         yPosition;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        fragCount[i] = 0;
        order[i] = -1;
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->ingame)
            continue;

        for(j = 0; j < MAXPLAYERS; ++j)
        {
            if(players[i].plr->ingame)
            {
                fragCount[i] += players[i].frags[j];
            }
        }

        for(k = 0; k < MAXPLAYERS; ++k)
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

    yPosition = 15;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(order[i] < 0 || !players[order[i]].plr ||
           !players[order[i]].plr->ingame)
        {
            continue;
        }
        else
        {
            GL_SetColor(their_colors[cfg.playerColor[order[i]]]);
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

/**
 * Draws the world time in the top right corner of the screen
 */
static void AM_drawWorldTimer(void)
{
#ifndef __JDOOM__
#ifndef __JHERETIC__
    int     days, hours, minutes, seconds;
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

menuitem_t MAPItems[] = {
/*
    {ITT_LRFUNC, 0, "window position : ",       M_MapPosition, 0 },
    {ITT_LRFUNC, 0, "window width :       ",    M_MapWidth, 0 },
#if !__JDOOM__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "window height :     ",     M_MapHeight, 0 },
#if !__JDOOM__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
*/
    {ITT_LRFUNC, 0, "hud display :        ",    M_MapStatusbar, 0 },
#if !__JHEXEN__
    {ITT_LRFUNC, 0, "kills count :         ",   M_MapKills, 0 },
    {ITT_LRFUNC, 0, "items count :         ",   M_MapItems, 0 },
    {ITT_LRFUNC, 0, "secrets count :    ",      M_MapSecrets, 0 },
#endif
    {ITT_NAVLEFT, 0, "automap colours",         NULL, 0 },
    {ITT_EFUNC, 0, "   walls",                  SCColorWidget, 1 },
    {ITT_EFUNC, 0, "   floor height changes",   SCColorWidget, 2 },
    {ITT_EFUNC, 0, "   ceiling height changes", SCColorWidget, 3 },
    {ITT_EFUNC, 0, "   unseen areas",           SCColorWidget, 0 },
    {ITT_EFUNC, 0, "   background",             SCColorWidget, 4 },
#if __JDOOM__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
    {ITT_EFUNC, 0, "door colors :        ",     M_MapDoorColors, 0 },
    {ITT_LRFUNC, 0, "door glow : ",             M_MapDoorGlow, 0 },
#if !__JDOOM__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "line alpha :          ",   M_MapLineAlpha, 0 },
#if !__JDOOM__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
};

menu_t MapDef = {
    0,
#if __JDOOM__
    70, 40,
#else
    64, 28,
#endif
    M_DrawMapMenu,
#if __JHERETIC__
    17, MAPItems,
#else
    14, MAPItems,
#endif
    0, MENU_OPTIONS,
    hu_font_a,
    cfg.menuColor2,
    NULL,
    LINEHEIGHT_A,
#if __JHERETIC__
    0, 17
#else
    0, 14
#endif
};

/**
 * Draws the automap options menu
 */
void M_DrawMapMenu(void)
{
    const menu_t *menu = &MapDef;
    static char *hudviewnames[3] = { "NONE", "CURRENT", "STATUSBAR" };
    static char *yesno[2] = { "NO", "YES" };
#if __JDOOM__ || __JHERETIC__ || __DOOM64TC__ || __WOLFTC__
    static char *countnames[4] = { "NO", "YES", "PERCENT", "COUNT+PCNT" };
#endif

    M_DrawTitle("Automap OPTIONS", menu->y - 26);

#if __JDOOM__
    M_WriteMenuText(menu, 0, hudviewnames[cfg.automapHudDisplay]);
    M_WriteMenuText(menu, 1, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 2, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 3, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    MN_DrawColorBox(menu, 5, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    MN_DrawColorBox(menu, 6, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    MN_DrawColorBox(menu, 7, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    MN_DrawColorBox(menu, 8, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    MN_DrawColorBox(menu, 9, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 11, yesno[cfg.automapShowDoors]);
    MN_DrawSlider(menu, 12, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    MN_DrawSlider(menu, 13, 11, cfg.automapLineAlpha * 10 + .5f);
#else
    M_WriteMenuText(menu, 0, hudviewnames[cfg.automapHudDisplay]);
# if __JHERETIC__
    M_WriteMenuText(menu, 1, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 2, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 3, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    MN_DrawColorBox(menu, 5, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    MN_DrawColorBox(menu, 6, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    MN_DrawColorBox(menu, 7, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    MN_DrawColorBox(menu, 8, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    MN_DrawColorBox(menu, 9, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 10, yesno[cfg.automapShowDoors]);
    MN_DrawSlider(menu, 12, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    MN_DrawSlider(menu, 15, 11, cfg.automapLineAlpha * 10 + .5f);
# else
    MN_DrawColorBox(menu, 2, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menu_alpha);
    MN_DrawColorBox(menu, 3, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menu_alpha);
    MN_DrawColorBox(menu, 4, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menu_alpha);
    MN_DrawColorBox(menu, 5, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menu_alpha);
    MN_DrawColorBox(menu, 6, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menu_alpha);
    M_WriteMenuText(menu, 7, yesno[cfg.automapShowDoors]);
    MN_DrawSlider(menu, 9, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    MN_DrawSlider(menu, 12, 11, cfg.automapLineAlpha * 10 + .5f);
# endif
#endif
}

/**
 * Set automap window width
 */
#if 0 // unused atm
void M_MapWidth(int option, void *data)
{
    M_FloatMod10(&cfg.automapWidth, option);
}
#endif

/**
 * Set automap window height
 */
#if 0 // unused atm
void M_MapHeight(int option, void *data)
{
    M_FloatMod10(&cfg.automapHeight, option);
}
#endif

/**
 * Set map window position
 */
#if 0 // unused atm
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

/**
 * Set automap line alpha
 */
void M_MapLineAlpha(int option, void *data)
{
    M_FloatMod10(&cfg.automapLineAlpha, option);
}

/**
 * Set show line/teleport lines in different color
 */
void M_MapDoorColors(int option, void *data)
{
    cfg.automapShowDoors = !cfg.automapShowDoors;
}

/**
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

/**
 * Set rotate mode
 */
void M_MapRotate(int option, void *data)
{
    cfg.automapRotate = !cfg.automapRotate;
}

/**
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

/**
 * Set the show kills counter
 */
void M_MapKills(int option, void *data)
{
    int         op = (cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x9;
    cfg.counterCheat |= (op & 0x1) | ((op & 0x2) << 2);
}

/**
 * Set the show items counter
 */
void M_MapItems(int option, void *data)
{
    int         op =
        ((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x12;
    cfg.counterCheat |= ((op & 0x1) << 1) | ((op & 0x2) << 3);
}

/**
 * Set the show secrets counter
 */
void M_MapSecrets(int option, void *data)
{
    int         op =
        ((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x24;
    cfg.counterCheat |= ((op & 0x1) << 2) | ((op & 0x2) << 4);
}

/**
 * Handle the console commands for the automap
 */
DEFCC(CCmdMapAction)
{
    static char buffer[20];

    if(G_GetGameState() != GS_LEVEL)
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
