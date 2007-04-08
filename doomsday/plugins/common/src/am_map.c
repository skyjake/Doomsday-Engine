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
#include "am_rendlist.h"

// TYPES -------------------------------------------------------------------

typedef struct mapline_s {
    float       r, g, b, a, a2, w; // a2 is used in glow mode in case the glow
                                   // needs a different alpha
    boolean     glow;
    boolean     scale;
} mapline_t;

typedef struct mpoint_s {
    float       pos[3];
} mpoint_t;

typedef struct mline_s {
    mpoint_t a, b;
} mline_t;

typedef struct automapwindow_s {
    // Where the window currently is on screen, and the dimensions.
    float       x, y, width, height;

    // Where the window should be on screen, and the dimensions.
    int         targetX, targetY, targetWidth, targetHeight;

    int         oldX, oldY, oldWidth, oldHeight;

    float       posTimer;
} automapwindow_t;

typedef struct automap_s {
// State
    boolean     active;
    boolean     stopped;
    boolean     fullyopen;

    // Used by MTOF to scale from map-to-frame-buffer coords.
    float       scaleMTOF;
    // Used by FTOM to scale from frame-buffer-to-map coords (=1/scaleMTOF).
    float       scaleFTOM;

// Paramaters for render.
    int         rendBlockMapGrid;
    float       alpha;

    boolean     maxScale;
    float       priorToMaxScale; // Viewer scale before entering maxScale mode.
    boolean     followplayer; // specifies whether to follow the player around.
    boolean     rotate;

// Automap window (screen space).
    automapwindow_t window;

// Viewer location on the map.
    float       viewX, viewY; // Current.
    float       targetViewX, targetViewY; // Should be at.
    float       oldViewX, oldViewY; // Previous.
    float       viewTimer;

// Viewer frame scale.
    float       viewScale; // Current.
    float       targetViewScale; // Should be at.
    float       oldViewScale; // Previous.
    float       viewScaleTimer;

    float       minScaleMTOF; // Viewer frame scale limits.
    float       maxScaleMTOF; // 

// Viewer frame rotation.
    float       angle; // Current.
    float       targetAngle; // Should be at.
    float       oldAngle; // Previous.
    float       angleTimer;

// Viewer frame dimensions on map (TODO: does not consider rotation!)
    float       vframe[2][2]; // {TL{x,y}, BR{x,y}}

// Misc stuff.
    int         cheating;
#if __JHEXEN__
    boolean     showKills;
#endif

    // Marked map points.
    mpoint_t    markpoints[AM_NUMMARKPOINTS];
    boolean     markpointsUsed[AM_NUMMARKPOINTS];
    int         markpointnum; // next point to be assigned.
} automap_t;

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

typedef struct automaptex_s {
    int         width, height;
    DGLuint     tex;
} automaptex_t;

typedef enum glowtype_e {
    NO_GLOW = -1,
    TWOSIDED_GLOW,
    BACK_GLOW,
    FRONT_GLOW
} glowtype_t;

// MACROS ------------------------------------------------------------------

#define LERP(start, end, pos) (end * pos + start * (1 - pos))

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

// Translate between fixed screen dimensions to actual, current.
#define FIXXTOSCREENX(x) (scrwidth * (x / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * (y / (float) SCREENHEIGHT))

// translates between frame-buffer and map distances
#define FTOM(map, x) ((x) * (map)->scaleFTOM)
#define MTOF(map, x) ((x) * (map)->scaleMTOF)

// translates between frame-buffer and map coordinates
#define CXMTOF(map, xpos)  ((map)->window.x + MTOF((map), (xpos) - (map)->vframe[0][VX]))
#define CYMTOF(map, ypos)  ((map)->window.y + ((map)->window.height - MTOF((map), (ypos) - (map)->vframe[0][VY])))

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

static void AM_changeWindowScale(automap_t *map);

static int AM_addMark(automap_t *map, float x, float y);
static void AM_clearMarks(automap_t *map);

static void AM_drawLevelName(void);
static void AM_drawWorldTimer(void);
#ifdef __JDOOM__
static void AM_drawFragsTable(void);
#elif __JHEXEN__ || __JSTRIFE__
static void AM_drawDeathmatchStats(void);
#endif

static void AM_drawMline2(mline_t *ml, mapline_t *c, boolean caps,
                          boolean blend);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float menu_alpha;
extern boolean viewactive;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static vectorgrap_t *vectorGraphs[NUM_VECTOR_GRAPHS];

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

static automap_t automaps[MAXPLAYERS];

static int mapviewplayer;

#if __JDOOM__
static int maplumpnum = 0; // if 0 no background image will be drawn
#elif __JHERETIC__
static int maplumpnum = 1; // if 0 no background image will be drawn
#else
static int maplumpnum = 1; // if 0 no background image will be drawn
#endif

#if __JDOOM__
static int their_colors[] = {
    GREENS,
    GRAYS,
    BROWNS,
    REDS
};
#elif __JHERETIC__
static int their_colors[] = {
    KEY3,
    KEY2,
    BLOODRED,
    KEY1
};
#else
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

// Map bounds
static float bounds[2][2]; // {TL{x,y}, BR{x,y}}

static dpatch_t markerPatches[10]; // numbers used for marking by the automap (lump indices)

// Used in subsector debug display.
static mapline_t subColors[10];

// CODE --------------------------------------------------------------------

/**
 * Register cvars and ccmds for the automap
 * Called during the PreInit of each game
 */
void AM_Register(void)
{
    uint        i;

    for(i = 0; mapCVars[i].name; ++i)
        Con_AddVariable(&mapCVars[i]);
    for(i = 0; mapCCmds[i].name; ++i)
        Con_AddCommand(&mapCCmds[i]);

    AM_ListRegister();
}

/**
 * Called during init.
 */
void AM_Init(void)
{
    uint        i;
    memset(vectorGraphs, 0, sizeof(vectorGraphs));

    scrwidth = Get(DD_SCREEN_WIDTH);
    scrheight = Get(DD_SCREEN_HEIGHT);

    AM_ListInit();
    AM_LoadData();

    memset(&automaps, 0, sizeof(automaps));
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        automap_t *map = &automaps[i];

        map->oldViewScale = 1;
        map->window.oldX = map->window.x = 0;
        map->window.oldY = map->window.y = 0;
        map->window.oldWidth = map->window.width = scrwidth;
        map->window.oldHeight = map->window.height = scrheight;
    }

    // Set the colors for the subsector debug display.
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
}

/**
 * Called during shutdown.
 */
void AM_Shutdown(void)
{
    uint        i;

    AM_ListShutdown();
    AM_UnloadData();

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

static void AM_SetMapWindowTarget(automapwindow_t *win, int x, int y,
                                  int w, int h)
{
    if(!win)
        return; // hmm...

    // Already at this target?
    if(x == win->targetX && y == win->targetY &&
       w == win->targetWidth && h == win->targetHeight)
        return;

    win->oldX = win->x;
    win->oldY = win->y;
    win->oldWidth = win->width;
    win->oldHeight = win->height;
    // Restart the timer.
    win->posTimer = 0;

    win->targetX = x;
    win->targetY = y;
    win->targetWidth = w;
    win->targetHeight = h;
}

static void AM_SetMapViewTarget(automap_t *map, float x, float y)
{
    if(!map)
        return; // hmm...

    // Already at this target?
    if(x == map->targetViewX && y == map->targetViewY)
        return;

    map->oldViewX = map->viewX;
    map->oldViewY = map->viewY;
    // Restart the timer.
    map->viewTimer = 0;

    map->targetViewX = x;
    map->targetViewY = y;
}

static void AM_SetMapViewScaleTarget(automap_t *map, float scale)
{
    if(!map)
        return; // hmm...

    scale = CLAMP(scale, 0, 1);

    // Already at this target?
    if(scale == map->targetViewScale)
        return;

    map->oldViewScale = map->viewScale;
    // Restart the timer.
    map->viewScaleTimer = 0;

    map->targetViewScale = scale;
}

static void AM_SetMapAngleTarget(automap_t *map, float angle)
{
    if(!map)
        return; // hmm...

    // Already at this target?
    if(angle == map->targetAngle)
        return;

    map->oldAngle = map->angle;
    // Restart the timer.
    map->angleTimer = 0;

    map->targetAngle = angle;
}

boolean AM_IsMapFullyOpen(int pnum)
{
    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return false;

    return automaps[pnum].fullyopen;
}

boolean AM_IsMapActive(int pnum)
{
    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return false;

    return automaps[pnum].active;
}

void AM_SetMapRotate(int pnum, boolean on)
{
    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return;

    automaps[pnum].rotate = on;
}

void AM_SetMapCheatLevel(int pnum, int level)
{
    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return;

    automaps[pnum].cheating = level;
}

void AM_IncMapCheatLevel(int pnum)
{
    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return;

    automaps[pnum].cheating = (automaps[pnum].cheating + 1) % 4;
}

#if __JHEXEN__
/**
 * @param mode          1= on, 0= off, -1 = toggle
 */
void AM_SetMapShowKills(int pnum, int value)
{
    automap_t  *map;
    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return;

    map = &automaps[pnum];
    if(value == -1)
        map->showKills != map->showKills;
    else if(value == 1)
        map->showKills = true;
    else if(value == 0)
        map->showKills = false;
}
#endif

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
 * Determines bounding box of all vertices,
 * sets global variables controlling zoom range.
 */
static void AM_findMinMaxBoundaries(void)
{
    uint        i;
    float       pos[2];

    bounds[0][0] = bounds[0][1] = (float) DDMAXINT;
    bounds[1][0] = bounds[1][1] = (float) -DDMAXINT;

    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, pos);

        if(pos[VX] < bounds[0][0])
            bounds[0][0] = pos[VX];
        else if(pos[VX] > bounds[1][0])
            bounds[1][0] = pos[VX];

        if(pos[VY] < bounds[0][1])
            bounds[0][1] = pos[VY];
        else if(pos[VY] > bounds[1][1])
            bounds[1][1] = pos[VY];
    }
}

/**
 * Load any resources needed for the automap.
 * Called during startup (post init) and after a renderer restart.
 */
void AM_LoadData(void)
{
#if !__DOOM64TC__
    int         i;
    char        namebuf[9];
#endif

#if !__DOOM64TC__
    // Load the marker patches.
    for(i = 0; i < 10; ++i)
    {
        MARKERPATCHES; // Check the macros eg: "sprintf(namebuf, "AMMNUM%d", i)" for jDoom
        R_CachePatch(&markerPatches[i], namebuf);
    }
#endif

    if(maplumpnum != 0)
    {
        maplumpnum = W_GetNumForName("AUTOPAGE");
    }
}

/**
 * Unload any resources needed for the automap.
 * Called during shutdown and before a renderer restart. 
 */
void AM_UnloadData(void)
{
    // Nothing to do.
}

/**
 * Clears markpoint array.
 */
static void AM_clearMarks(automap_t *map)
{
    uint        i;

    if(!map)
        return;

    for(i = 0; i < AM_NUMMARKPOINTS; ++i)
        map->markpointsUsed[i] = false;
    map->markpointnum = 0;
}

/**
 * Adds a marker at the current location
 */
static int AM_addMark(automap_t *map, float x, float y)
{
    int         num = map->markpointnum;

    if(!map)
        return -1;

    map->markpoints[num].pos[VX] = x;
    map->markpoints[num].pos[VY] = y;
    map->markpoints[num].pos[VZ] = 0;
    map->markpointsUsed[num] = true;
    map->markpointnum = (map->markpointnum + 1) % AM_NUMMARKPOINTS;

#if _DEBUG
Con_Message("Added mark point %i to map at X=%g Y=%g\n",
            num,
            map->markpoints[num].pos[VX],
            map->markpoints[num].pos[VY]);
#endif

    return num;
}

/**
 * Called during the finalization stage of map loading (after all geometry).
 */
void AM_LevelInit(void)
{
    uint        i;
    float       a, b;
    float       max_w;
    float       max_h;
    float       min_w;
    float       min_h;
    automap_t  *map;

    // Find the world boundary points shared by all maps.
    AM_findMinMaxBoundaries();

    // Calculate the min/max scaling factors.
    max_w = bounds[1][0] - bounds[0][0];
    max_h = bounds[1][1] - bounds[0][1];

    min_w = FIX2FLT(2 * PLAYERRADIUS);    // const? never changed?
    min_h = FIX2FLT(2 * PLAYERRADIUS);

    // Setup all players' maps.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->ingame)
            continue;

        map = &automaps[i];

        AM_SetMapWindowTarget(&map->window, 0, 0, Get(DD_SCREEN_WIDTH),
                              Get(DD_SCREEN_HEIGHT));

        // Calculate world to screen space scale based on window width/height
        // divided by the min/max scale factors derived from map boundaries.
        a = (float) map->window.width / max_w;
        b = (float) map->window.height / max_h;

        map->minScaleMTOF = (a < b ? a : b);
        map->maxScaleMTOF = (float) map->window.height / FIX2FLT(2 * PLAYERRADIUS);

        // Change the zoom.
        AM_SetMapViewScaleTarget(map, (map->maxScale? 0 : 0.0125f));

        // Clear any previously marked map points.
        AM_clearMarks(map);

        // If the map has been left open from the previous level; close it.
        AM_Stop(i);
    }
}

/**
 * Stop the automap
 */
void AM_Stop(int pnum)
{
    automap_t *map;

    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return;
    
    map = &automaps[pnum];

    map->active = false;
    map->fullyopen = false;
    map->stopped = true;

    GL_Update(DDUF_BORDER);
}

/**
 * Start the automap.
 */
void AM_Start(int pnum)
{
    player_t   *plr;
    automap_t  *map;

    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->ingame)
        return;

    if(G_GetGameState() != GS_LEVEL)
        return;  // Can't start the automap if player is not in a game!

    map = &automaps[pnum];
    plr = &players[pnum];

    if(!map->stopped)
        AM_Stop(pnum);

    map->active = true;
    map->stopped = false;
    map->alpha = 0;

    AM_SetMapRotate(pnum, cfg.automapRotate);

    AM_SetMapWindowTarget(&map->window, 0, 0, scrwidth, scrheight);

    // Find the player to center on?
    if(map->followplayer && plr->plr->ingame)
        AM_SetMapViewTarget(map, FIX2FLT(plr->plr->mo->pos[VX]),
                                 FIX2FLT(plr->plr->mo->pos[VY]));
}

/**
 * Animates an automap view window towards the target values.
 */
static void AM_MapWindowTicker(automapwindow_t *win)
{
    if(!win)
        return;

    win->posTimer += .4f;
    if(win->posTimer >= 1)
    {
        win->x = win->targetX;
        win->y = win->targetY;
        win->width = win->targetWidth;
        win->height = win->targetHeight;
    }
    else
    {
        win->x = LERP(win->oldX, win->targetX, win->posTimer);
        win->y = LERP(win->oldY, win->targetY, win->posTimer);
        win->width = LERP(win->oldWidth, win->targetWidth, win->posTimer);
        win->height = LERP(win->oldHeight, win->targetHeight, win->posTimer);
    }
}

/**
 * Called each tic for each player's automap if they are in-game.
 */
static void AM_MapTicker(automap_t *map)
{
    float       width, height, scale;

    if(!map)
        return;

    // If the automap is not active do nothing.
    if(!map->active)
        return;

    //
    // Update per tic, driven controls.
    //

    // Map view zoom contol.
    /*
    if(actions[A_MAPZOOMOUT].on)  // zoom out
    {
        AM_SetMapViewScaleTarget(map, map->viewScale + .02f);
    }
    else if(actions[A_MAPZOOMIN].on) // zoom in
    {
        AM_SetMapViewScaleTarget(map, map->viewScale - .02f);
    }
    */

    // Map viewer location paning control.
    if(!map->followplayer)
    {
        float       x = 0, y = 0; // deltas
        // DOOM.EXE used to pan at 140 fixed pixels per second.
        float       panUnitsPerTic =
                        FTOM(map, FIXXTOSCREENX(140)) / TICSPERSEC;
/*
        // X axis.
        if(PLAYER_ACTION(mapplr, A_MAPPANRIGHT)) // pan right?
            x = panUnitsPerTic;
        else if(PLAYER_ACTION(mapplr, A_MAPPANLEFT)) // pan left?
            x = -panUnitsPerTic;
        // Y axis.
        if(PLAYER_ACTION(mapplr, A_MAPPANUP))  // pan up?
            y = panUnitsPerTic;
        else if(PLAYER_ACTION(mapplr, A_MAPPANDOWN))  // pan down?
            y = -panUnitsPerTic;
*/
        if(x || y)
            AM_SetMapViewTarget(map, map->viewX + x, map->viewY + y);
    }
    else  // Camera follows the player
    {
        mobj_t     *mo = players[map - automaps].plr->mo;
        float       angle;

        AM_SetMapViewTarget(map, FIX2FLT(mo->pos[VX]),
                            FIX2FLT(mo->pos[VY]));

        /* $unifiedangles */
        if(map->rotate)
            angle = mo->angle / (float) ANGLE_MAX * 360 - 90;
        else
            angle = 0;

        AM_SetMapAngleTarget(map, angle);
    }

    //
    // Animate map values.
    //

    // Window position and dimensions.
    AM_MapWindowTicker(&map->window);

    // Record whether the window is full open (all targets reached).
    if(map->window.x == map->window.targetX &&
       map->window.y == map->window.targetY && 
       map->window.width == map->window.targetWidth &&
       map->window.height == map->window.targetHeight)
        map->fullyopen = true;
    else
        map->fullyopen = false;

    // Global map opacity.
    if(map->alpha < 1)
        map->alpha += (1 - map->alpha) / 3;

    // Map viewer location.
    map->viewTimer += .4f;
    if(map->viewTimer >= 1)
    {
        map->viewX = map->targetViewX;
        map->viewY = map->targetViewY;
    }
    else
    {
        map->viewX = LERP(map->oldViewX, map->targetViewX, map->viewTimer);
        map->viewY = LERP(map->oldViewY, map->targetViewY, map->viewTimer);
    }

    // Map view scale (zoom).
    map->viewScaleTimer += .4f;
    if(map->viewScaleTimer >= 1)
    {
        map->viewScale = map->targetViewScale;
    }
    else
    {
        map->viewScale =
            LERP(map->oldViewScale, map->targetViewScale, map->viewScaleTimer);
    }

    // Map view rotation.
    map->angleTimer += .05f;
    if(map->angleTimer >= 1)
    {
        map->angle = map->targetAngle;
    }
    else
    {
        float       diff;
        float       startAngle = map->oldAngle;
        float       endAngle = map->targetAngle;

        if(endAngle > startAngle)
        {
            diff = endAngle - startAngle;
            if(diff > 180)
                endAngle = startAngle - (360 - diff);
        }
        else
        {
            diff = startAngle - endAngle;
            if(diff > 180)
                endAngle = startAngle + (360 - diff);
        }


        map->angle = LERP(startAngle, endAngle, map->angleTimer);
    }

    //
    // Activate the new scale, position etc.
    //
    scale = (map->maxScaleMTOF - map->minScaleMTOF) * map->viewScale;

    // Scaling multipliers.
    map->scaleMTOF = map->minScaleMTOF + scale;
    map->scaleFTOM = 1.0f / map->scaleMTOF;

    width = FTOM(map, map->window.width);
    height = FTOM(map, map->window.height);

    // Top Left
    map->vframe[0][VX] = map->viewX - width / 2;
    map->vframe[0][VY] = map->viewY - height / 2;
    // Bottom Right
    map->vframe[1][VX] = map->viewX + width / 2;
    map->vframe[1][VY] = map->viewY + height / 2;
}

/**
 * Updates on Game Tick.
 */
void AM_Ticker(void)
{
    uint        i;

    // All maps get to tick if their player is in-game.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->ingame)
            continue;

        AM_MapTicker(&automaps[i]);
    }
}

/**
 * Draw a border if needed.
 */
static void AM_clearFB(int color)
{
#if !__DOOM64TC__
    float       scaler = cfg.sbarscale / 20.0f;

    GL_Update(DDUF_FULLSCREEN);

    if(cfg.automapHudDisplay != 1)
    {
        GL_SetPatch(W_GetNumForName( BORDERGRAPHIC ));
        GL_DrawCutRectTiled(0, SCREENHEIGHT, 320, BORDEROFFSET, 16, BORDEROFFSET,
                            0, 0, 160 - 160 * scaler + 1, SCREENHEIGHT,
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

/**
 * Draws the given line. No cliping is done!
 */
static void AM_drawMline(mline_t *ml, int color, float alpha)
{
    automap_t  *map = &automaps[mapviewplayer];

    AM_AddLine(CXMTOF(map, ml->a.pos[VX]),
               CYMTOF(map, ml->a.pos[VY]),
               CXMTOF(map, ml->b.pos[VX]),
               CYMTOF(map, ml->b.pos[VY]),
               color,
               alpha);
}

/**
 * Draws the given line including any optional extras.
 */
static void AM_drawMline2(mline_t *ml, mapline_t *c, boolean caps,
                          boolean blend)
{
    float       a[2], b[2];
    float       length, dx, dy;
    automap_t  *map = &automaps[mapviewplayer];

    // Scale into map, screen space units.
    a[VX] = CXMTOF(map, ml->a.pos[VX]);
    a[VY] = CYMTOF(map, ml->a.pos[VY]);
    b[VX] = CXMTOF(map, ml->b.pos[VX]);
    b[VY] = CYMTOF(map, ml->b.pos[VY]);

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
            thickness = cfg.automapDoorGlow * map->scaleMTOF * 2.5f + 3;
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
                       c->r, c->g, c->b, c->a2,
                       tex, false, blend);
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
                       c->r, c->g, c->b, c->a2,
                       tex, false, blend);
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
                       c->r, c->g, c->b, c->a2,
                       tex, false, blend);
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
                       c->r, c->g, c->b, c->a2,
                       tex, false, blend);
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
                       c->r, c->g, c->b, c->a2,
                       tex, false, blend);
        }
    }

    AM_AddLine4f(a[VX], a[VY], b[VX], b[VY],
                 c->r, c->g, c->b, c->a);
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
    automap_t  *map = &automaps[mapviewplayer];

    // Figure out start of vertical gridlines.
    start = map->vframe[0][VX];
    if((int) (start - originx) % MAPBLOCKUNITS)
        start += MAPBLOCKUNITS - ((int) (start - originx) % MAPBLOCKUNITS);
    end = map->vframe[1][VX];

    // Draw vertical gridlines.
    ml.a.pos[VY] = map->vframe[0][VY];
    ml.b.pos[VY] = map->vframe[1][VY];

    for(x = start; x < end; x += MAPBLOCKUNITS)
    {
        ml.a.pos[VX] = x;
        ml.b.pos[VX] = x;
        AM_drawMline(&ml, color, cfg.automapLineAlpha);
    }

    // Figure out start of horizontal gridlines.
    start = map->vframe[0][VY];
    if((int) (start - originy) % MAPBLOCKUNITS)
        start += MAPBLOCKUNITS - ((int) (start - originy) % MAPBLOCKUNITS);
    end = map->vframe[1][VY];

    // Draw horizontal gridlines
    ml.a.pos[VX] = map->vframe[0][VX];
    ml.b.pos[VX] = map->vframe[1][VX];
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
    player_t   *plr = &players[mapviewplayer];
    automap_t  *map = &automaps[mapviewplayer];

    for(s = 0; s < numsubsectors; ++s)
    {
        if(map->cheating == 3)        // Debug cheat, show subsectors
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

            if(map->cheating == 3)        // Debug cheat, show subsectors
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
            if(map->cheating == 2)        // Debug cheat.
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
            if(map->cheating || xline->mapped[mapviewplayer])
            {
                int     specialType;

                if((flags & ML_DONTDRAW) && !map->cheating)
                    continue;

                specialType = AM_checkSpecial(xline->special);

                if(cfg.automapShowDoors &&
                   (specialType == 1 || (specialType > 1 && map->cheating)))
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
                    else if(map->cheating)
                    {
                        AM_getLine(&templine, 0, 0);
                        AM_drawMline2(&l, &templine, false, false);
                    }
                }
            }
            else if(plr->powers[PT_ALLMAP])
            {
                if(!(flags & ML_DONTDRAW))
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

static void addPatchQuad(float x, float y, float w, float h,
                         angle_t angle, uint lumpnum, float r, float g,
                         float b, float a)
{
    float       pos[4][2];
    automap_t  *map = &automaps[mapviewplayer];

    pos[0][VX] = -(w / 2);
    pos[0][VY] = h / 2;
    AM_rotate(&pos[0][VX], &pos[0][VY], angle);
    pos[0][VX] += x;
    pos[0][VY] += y;

    pos[1][VX] = w / 2;
    pos[1][VY] = h / 2;
    AM_rotate(&pos[1][VX], &pos[1][VY], angle);
    pos[1][VX] += x;
    pos[1][VY] += y;

    pos[2][VX] = w / 2;
    pos[2][VY] = -(h / 2);
    AM_rotate(&pos[2][VX], &pos[2][VY], angle);
    pos[2][VX] += x;
    pos[2][VY] += y;


    pos[3][VX] = -(w / 2);
    pos[3][VY] = -(h / 2);
    AM_rotate(&pos[3][VX], &pos[3][VY], angle);
    pos[3][VX] += x;
    pos[3][VY] += y;

    AM_AddQuad(CXMTOF(map, pos[0][VX]), CYMTOF(map, pos[0][VY]),
               CXMTOF(map, pos[1][VX]), CYMTOF(map, pos[1][VY]),
               CXMTOF(map, pos[2][VX]), CYMTOF(map, pos[2][VY]),
               CXMTOF(map, pos[3][VX]), CYMTOF(map, pos[3][VY]),
               0, 0, 1, 0,
               1, 1, 0, 1,
               r, g, b, a,
               lumpnum, true, false);
}

/**
 * Draws all players on the map using a line character
 */
static void renderPlayers(void)
{
    int         i;
    float       size = FIX2FLT(PLAYERRADIUS);
    vectorgrap_t *vg;
    automap_t  *map = &automaps[mapviewplayer];

#if __JDOOM__
    if(!IS_NETGAME && map->cheating)
    {
        vg = AM_GetVectorGraphic(VG_CHEATARROW);
    }
    else
#endif
    {
        vg = AM_GetVectorGraphic(VG_ARROW);
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t   *p = &players[i];
        int         color;
        float       alpha;

        if(!p->plr->ingame)
            continue;

#if __JDOOM__ || __JHERETIC__
        if(deathmatch && p != &players[mapviewplayer])
            continue;
#endif

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
 * Draws all the points marked by the player.
 */
static void AM_drawMarks(void)
{
#if !__DOOM64TC__
    int         i;
    float       x, y, w, h;
    angle_t     angle;
    automap_t  *map = &automaps[mapviewplayer];
    player_t   *plr = &players[mapviewplayer];
    dpatch_t   *patch;

    for(i = 0; i < AM_NUMMARKPOINTS; ++i)
    {
        if(map->markpointsUsed[i])
        {
            patch = &markerPatches[i];

            w = (float) SHORT(patch->width);
            h = (float) SHORT(patch->height);

            x = map->markpoints[i].pos[VX];
            y = map->markpoints[i].pos[VY];

            /* $unifiedangles */ 
            angle = (float) ANGLE_MAX * (map->angle / 360);

            addPatchQuad(x, y, FIXXTOSCREENX(w) * map->scaleFTOM,
                         FIXYTOSCREENY(h) * map->scaleFTOM, angle,
                         patch->lump, 1, 1, 1, 1);
        }
    }
#endif
}

/**
 * Changes the position of the automap window
 */
#if 0
static void AM_setWinPos(automap_t *map)
{
    int         winX, winY, winW, winH;

    if(!map)
        return;

    scrwidth = Get(DD_SCREEN_WIDTH);
    scrheight = Get(DD_SCREEN_HEIGHT);

    winW = (scrwidth/ 1.0f) * 1; //(cfg.automapWidth);
    winH = (scrheight/ 1.0f) * 1; //(cfg.automapHeight);

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
        winX = (scrwidth / 2) - (winW / 2);
        break;

       case 0:
       case 3:
       case 6:
        winX = 0;
        break;

       case 2:
       case 5:
       case 8:
        winX = (scrwidth - winW);
        break;
    }

    // Y Position
    switch (4)//cfg.automapPos)
    {
       case 0:
       case 1:
       case 2:
        winY = 0;
        break;

      case 3:
       case 4:
       case 5:
        winY = (scrheight/2)-(winH/2);
        break;

       case 6:
       case 7:
       case 8:
        winY = (scrheight - winH);
        break;
    }

    AM_SetMapWindowTarget(&map->window, winX, winY, winW, winH);
}
#endif

/**
 * Sets up the state for automap drawing
 */
static void AM_GL_SetupState(void)
{
    //float extrascale = 0;
    player_t       *plr;
    automap_t      *map;
    automapwindow_t *win;

    plr = &players[mapviewplayer];
    map = &automaps[mapviewplayer];
    win = &map->window;

    // Update the window scale vars?
/*
    if(win->oldWidth != cfg.automapWidth || win->oldHeight != cfg.automapHeight)
        AM_setWinPos(map);
*/
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
                            (map->alpha - (1- cfg.automapBack[3])));
        GL_SetRawImage(maplumpnum, 0);        // We only want the left portion.
        GL_DrawRectTiled(win->x, win->y, win->width, win->height, 128, 100);
    }
    else
    {
        // nope just a solid color
        GL_SetNoTexture();
        GL_DrawRect(win->x, win->y, win->width, win->height,
                    cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2],
                    (map->alpha - (1- cfg.automapBack[3])));
    }

    // How about an outside border?
/*    if(win->width < srcwidth || win->height < scrheight)
    {
        gl.Begin(DGL_LINES);
        gl.Color4f(0.5f, 1, 0.5f, map->alpha - (1 - (cfg.automapLineAlpha /2)) );

        if(win->height < srcwidth)
        {
            gl.Vertex2f(win->x + win->width + 1, win->y - 1);
            gl.Vertex2f(win->x + win->width + 1, win->y + win->height + 1);

            gl.Vertex2f(win->x - 1, win->y + win->height + 1);
            gl.Vertex2f(win->x - 1, win->y - 1);
        }
        if(cfg.automapHeight != 1)
        {
            gl.Vertex2f(win->x - 1, win->y - 1);
            gl.Vertex2f(win->X + win->width + 1, win->y - 1);

            gl.Vertex2f(win->x + win->width + 1, win->y + win->height + 1);
            gl.Vertex2f(win->x - 1, win->y + win->height + 1);
        }
        gl.End();
    }*/

    // Setup the scissor clipper.
    gl.Scissor(win->x, win->y, win->width, win->height);
    gl.Enable(DGL_SCISSOR_TEST);

    // Rotate map?
    gl.Translatef(win->x + (win->width / 2.0f),
                  win->y + (win->height / 2.0f), 0);
    gl.Rotatef(map->angle, 0, 0, 1);
    gl.Translatef(-(win->x + (win->width / 2.0f)),
                  -(win->y + (win->height /2.0f)), 0);
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
    player_t   *plr;
#if __JDOOM__ || __JHERETIC__
    char        buf[40], tmp[20];
    int         x = 5, y = LINEHEIGHT_A * 3;
#endif

    plr = &players[mapviewplayer];

#if __JDOOM__ || __JHERETIC__
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
    if(IS_NETGAME)
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
    int         x, y, otherY;
    const char *lname = "";
    automap_t  *map = &automaps[mapviewplayer];
    automapwindow_t *win = &map->window;

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

        x = (win->x + (win->width * .5f) - (M_StringWidth((char*)lname, hu_font_a) * .5f));
        y = (win->y + win->height);
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

        M_WriteText2(x, y, (char*)lname, hu_font_a, 1, 1, 1, map->alpha);

        gl.MatrixMode(DGL_PROJECTION);
        gl.PopMatrix();
    }
}

/**
 * Render the automap view window for the specified player.
 */
void AM_Drawer(int viewplayer)
{
    automap_t *map;

    if(viewplayer < 0 || viewplayer >= MAXPLAYERS ||
       !players[viewplayer].plr->ingame)
        return;

    mapviewplayer = viewplayer;
    map = &automaps[mapviewplayer];

    // If the automap isn't active, draw nothing
    if(!map->active)
        return;

    // Setup for frame.
    AM_clearFB(BACKGROUND);
    AM_GL_SetupState();

    if(!freezeMapRLs)
    {
        AM_ClearAllLists(false);

        // Draw.
        if(map->rendBlockMapGrid)
            renderGrid(GRIDCOLORS);

        renderWalls();
        renderPlayers();

        if(map->cheating == 2)
            renderThings(THINGCOLORS, THINGRANGE);

#if !__JHEXEN__
        if(gameskill == SM_BABY && cfg.automapBabyKeys)
            renderKeys();
#endif
        // Draw any marked points.
        AM_drawMarks();
    }
    AM_RenderAllLists(map->alpha);

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

    worldTimer = players[mapviewplayer].worldTimer;

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

    AM_SetMapRotate(consoleplayer, cfg.automapRotate);
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
        Con_Printf("The automap is only available when in-game.\n");
        return false;
    }

    if(!stricmp(argv[0], "automap")) // open/close the map.
    {
        automap_t   *map = &automaps[consoleplayer];

        if(map->active) // Is open, now close it.
        {
            viewactive = true;

            // Disable the automap binding classes
            DD_SetBindClass(GBC_CLASS1, false);

            if(!map->followplayer)
                DD_SetBindClass(GBC_CLASS2, false);

            AM_Stop(consoleplayer);
        }
        else // Is closed, now open it.
        {
            AM_Start(consoleplayer);
            // Enable/disable the automap binding classes
            DD_SetBindClass(GBC_CLASS1, true);
            if(!map->followplayer)
                DD_SetBindClass(GBC_CLASS2, true);

            viewactive = false;
        }

        return true;
    }
    else if(!stricmp(argv[0], "follow"))  // follow mode toggle
    {
        player_t    *plr = &players[consoleplayer];
        automap_t   *map = &automaps[consoleplayer];

        if(map->active)
        {
            map->followplayer = !map->followplayer;

            // Enable/disable the follow mode binding class
            if(!map->followplayer)
                DD_SetBindClass(GBC_CLASS2, true);
            else
                DD_SetBindClass(GBC_CLASS2, false);

            P_SetMessage(plr, (map->followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), false);
            Con_Printf("Follow mode toggle.\n");
            return true;
        }
    }
    else if(!stricmp(argv[0], "rotate"))  // rotate mode toggle
    {
        player_t    *plr = &players[consoleplayer];
        automap_t   *map = &automaps[consoleplayer];

        if(map->active)
        {
            cfg.automapRotate = !cfg.automapRotate;
            AM_SetMapRotate(consoleplayer, cfg.automapRotate);

            P_SetMessage(plr, (cfg.automapRotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF), false);
            Con_Printf("Rotate mode toggle.\n");
            return true;
        }
    }
    else if(!stricmp(argv[0], "zoommax"))  // max zoom
    {
        player_t    *plr = &players[consoleplayer];
        automap_t   *map = &automaps[consoleplayer];

        if(map->active)
        {
            // If switching to maXScale mode, store the old scale.
            if(!map->maxScale)
                map->priorToMaxScale = map->viewScale;

            map->maxScale = !map->maxScale;
            AM_SetMapViewScaleTarget(map, (map->maxScale? 0 : map->priorToMaxScale));

            Con_Printf("Maximum zoom toggle in automap.\n");
            return true;
        }
    }
    else if(!stricmp(argv[0], "addmark"))  // add a mark
    {
        player_t   *plr = &players[consoleplayer];
        automap_t  *map = &automaps[consoleplayer];
        uint        num;

        if(map->active)
        {
            num = AM_addMark(map, FIX2FLT(plr->plr->mo->pos[VX]),
                                  FIX2FLT(plr->plr->mo->pos[VY]));
            if(num != -1)
            {
                sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, num);
                P_SetMessage(plr, buffer, false);
            }
            return true;
        }
    }
    else if(!stricmp(argv[0], "clearmarks"))  // clear all marked points
    {
        player_t    *plr = &players[consoleplayer];
        automap_t   *map = &automaps[consoleplayer];

        if(map->active)
        {
            AM_clearMarks(map);

            P_SetMessage(plr, AMSTR_MARKSCLEARED, false);
            Con_Printf("All markers cleared on automap.\n");
            return true;
        }
    }
    else if(!stricmp(argv[0], "grid")) // toggle drawing of grid
    {
        player_t    *plr = &players[consoleplayer];
        automap_t   *map = &automaps[consoleplayer];

        if(map->active)
        {
            map->rendBlockMapGrid = !map->rendBlockMapGrid;
            P_SetMessage(plr, (map->rendBlockMapGrid ? AMSTR_GRIDON : AMSTR_GRIDOFF), false);
            Con_Printf("Grid toggled in automap.\n");
            return true;
        }
    }

    return false;
}
