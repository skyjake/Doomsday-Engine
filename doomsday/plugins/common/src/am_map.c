/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * am_map.c : Automap, automap menu and related code.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "p_player.h"
#include "g_controls.h"
#include "am_rendlist.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#define AM_LINE_WIDTH           (1.25f)

#define LERP(start, end, pos) (end * pos + start * (1 - pos))

//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//

typedef struct mpoint_s {
    float       pos[3];
} mpoint_t;

typedef struct mline_s {
    mpoint_t a, b;
} vgline_t;

#define R (1.0f)

vgline_t         keysquare[] = {
    {{0, 0}, {R / 4, -R / 2}},
    {{R / 4, -R / 2}, {R / 2, -R / 2}},
    {{R / 2, -R / 2}, {R / 2, R / 2}},
    {{R / 2, R / 2}, {R / 4, R / 2}},
    {{R / 4, R / 2}, {0, 0}},       // handle part type thing
    {{0, 0}, {-R, 0}},               // stem
    {{-R, 0}, {-R, -R / 2}},       // end lockpick part
    {{-3 * R / 4, 0}, {-3 * R / 4, -R / 4}}
};

vgline_t         thintriangle_guy[] = {
    {{-R / 2, R - R / 2}, {R, 0}}, // >
    {{R, 0}, {-R / 2, -R + R / 2}},
    {{-R / 2, -R + R / 2}, {-R / 2, R - R / 2}} // |>
};

#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
vgline_t         player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}},    // -----
    {{R, 0}, {R - R / 2, R / 4}},    // ----->
    {{R, 0}, {R - R / 2, -R / 4}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 4}},    // >---->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 4}},    // >>--->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4}}
};

vgline_t         cheat_player_arrow[] = {
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
vgline_t         player_arrow[] = {
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

vgline_t         cheat_player_arrow[] = {
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
vgline_t         player_arrow[] = {
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
#define FIXXTOSCREENX(x) (scrwidth * ((x) / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * ((y) / (float) SCREENHEIGHT))
#define SCREENXTOFIXX(x) ((float) SCREENWIDTH * ((x) / scrwidth))
#define SCREENYTOFIXY(y) ((float) SCREENHEIGHT * ((y) / scrheight))

// translates between frame-buffer and map distances
#define FTOM(map, x) ((x) * (map)->scaleFTOM)
#define MTOF(map, x) ((x) * (map)->scaleMTOF)

// translates between frame-buffer and map coordinates
#define CXMTOF(map, xpos)  ((map)->window.x + MTOF((map), (xpos) - (map)->vframe[0][VX]))
#define CYMTOF(map, ypos)  ((map)->window.y + ((map)->window.height - MTOF((map), (ypos) - (map)->vframe[0][VY])))

#define AM_MAXSPECIALLINES      32

// TYPES -------------------------------------------------------------------

typedef struct mapobjectinfo_s {
    float       rgba[4];
    int         blendMode;

    float       glowAlpha;
    float       glowWidth;
    boolean     glow;

    boolean     scaleWithView;
} mapobjectinfo_t;

typedef struct automapwindow_s {
    // Where the window currently is on screen, and the dimensions.
    float       x, y, width, height;

    // Where the window should be on screen, and the dimensions.
    int         targetX, targetY, targetWidth, targetHeight;

    int         oldX, oldY, oldWidth, oldHeight;

    float       posTimer;
} automapwindow_t;

typedef struct automapcfg_s {
    float       lineGlowScale;
    boolean     glowingLineSpecials;
    float       backgroundRGBA[4];
    float       panSpeed;
    boolean     panResetOnOpen;
    float       zoomSpeed;

    mapobjectinfo_t unseenLine;
    mapobjectinfo_t singleSidedLine;
    mapobjectinfo_t twoSidedLine;
    mapobjectinfo_t floorChangeLine;
    mapobjectinfo_t ceilingChangeLine;
} automapcfg_t;

#define AMF_REND_THINGS         0x01
#define AMF_REND_KEYS           0x02
#define AMF_REND_ALLLINES       0x04
#define AMF_REND_XGLINES        0x08
#define AMF_REND_VERTEXES       0x10
#define AMF_REND_LINE_NORMALS   0x20

typedef struct automapspecialline_s {
    int         special;
    int         sided;
    int         cheatLevel; // minimum cheat level for this special.
    mapobjectinfo_t info;
} automapspecialline_t;

typedef struct automap_s {
// State
    int         flags;
    boolean     active;

    boolean     fullScreenMode; // If the map is currently in fullscreen mode.
    boolean     panMode; // If the map viewer location is currently in free pan mode.
    boolean     rotate;
    uint        followPlayer; // Player id of that to follow.

    boolean     maxScale; // If the map is currently in forced max zoom mode.
    float       priorToMaxScale; // Viewer scale before entering maxScale mode.

    // Used by MTOF to scale from map-to-frame-buffer coords.
    float       scaleMTOF;
    // Used by FTOM to scale from frame-buffer-to-map coords (=1/scaleMTOF).
    float       scaleFTOM;

// Paramaters for render.
    float       alpha;
    float       targetAlpha;

    automapcfg_t cfg;

    automapspecialline_t specialLines[AM_MAXSPECIALLINES];
    uint        numSpecialLines;

    vectorgrapname_t vectorGraphicForPlayer;

// Automap window (screen space).
    automapwindow_t window;

// Viewer location on the map.
    float       viewTimer;
    float       viewX, viewY; // Current.
    float       targetViewX, targetViewY; // Should be at.
    float       oldViewX, oldViewY; // Previous.
    // For the parallax layer.
    float       viewPLX, viewPLY; // Current.

// Viewer frame scale.
    float       viewScaleTimer;
    float       viewScale; // Current.
    float       targetViewScale; // Should be at.
    float       oldViewScale; // Previous.

    float       minScaleMTOF; // Viewer frame scale limits.
    float       maxScaleMTOF; //

// Viewer frame rotation.
    float       angleTimer;
    float       angle; // Current.
    float       targetAngle; // Should be at.
    float       oldAngle; // Previous.

// Viewer frame coordinates on map.
    float       vframe[2][2]; // {TL{x,y}, BR{x,y}}

    float       vbbox[4]; // Clip bbox coordinates on map.

// Misc
    int         cheating;

    // Marked map points.
    mpoint_t    markpoints[NUMMARKPOINTS];
    boolean     markpointsUsed[NUMMARKPOINTS];
    int         markpointnum; // next point to be assigned.
} automap_t;

typedef struct vectorgrap_s {
    uint        count;
    vgline_t    *lines;
} vectorgrap_t;

typedef struct automaptex_s {
    int         width, height;
    DGLuint     tex;
} automaptex_t;

typedef struct {
    player_t   *plr;
    automap_t  *map;
} ssecitervars_t;

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

static void setWindowFullScreenMode(automap_t *map, int value);
static void setViewTarget(automap_t *map, float x, float y);
static void setViewScaleTarget(automap_t *map, float scale);
static void setViewAngleTarget(automap_t *map, float angle);
static void setViewRotateMode(automap_t *map, boolean on);
static void clearMarks(automap_t *map);

static void findMinMaxBoundaries(void);
static void drawLevelName(void);
static void drawWorldTimer(void);
#if __JDOOM__ || __JDOOM64__
static void drawFragsTable(void);
#elif __JHEXEN__ || __JSTRIFE__
static void drawDeathmatchStats(void);
#endif

static void rotate2D(float *x, float *y, angle_t a);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean viewActive;

extern int numTexUnits;
extern boolean envModAdd;
extern DGLuint amMaskTexture;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int mapviewplayer;

cvar_t  mapCVars[] = {
    {"map-alpha-lines", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __WOLFTC__
    {"map-babykeys", 0, CVT_BYTE, &cfg.automapBabyKeys, 0, 1},
#endif
    {"map-background-r", 0, CVT_FLOAT, &cfg.automapBack[0], 0, 1},
    {"map-background-g", 0, CVT_FLOAT, &cfg.automapBack[1], 0, 1},
    {"map-background-b", 0, CVT_FLOAT, &cfg.automapBack[2], 0, 1},
    {"map-background-a", 0, CVT_FLOAT, &cfg.automapBack[3], 0, 1},
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
    {"map-door-colors", 0, CVT_BYTE, &cfg.automapShowDoors, 0, 1},
    {"map-door-glow", 0, CVT_FLOAT, &cfg.automapDoorGlow, 0, 200},
    {"map-huddisplay", 0, CVT_INT, &cfg.automapHudDisplay, 0, 2},
    {"map-pan-speed", 0, CVT_FLOAT, &cfg.automapPanSpeed, 0, 1},
    {"map-pan-resetonopen", 0, CVT_BYTE, &cfg.automapPanResetOnOpen, 0, 1},
    {"map-rotate", 0, CVT_BYTE, &cfg.automapRotate, 0, 1},
    {"map-zoom-speed", 0, CVT_FLOAT, &cfg.automapZoomSpeed, 0, 1},
    {NULL}
};

ccmd_t  mapCCmds[] = {
    {"automap",     "", CCmdMapAction, CMDF_NO_DEDICATED},
    {"follow",      "", CCmdMapAction, CMDF_NO_DEDICATED},
    {"rotate",      "", CCmdMapAction, CMDF_NO_DEDICATED},
    {"addmark",     "", CCmdMapAction, CMDF_NO_DEDICATED},
    {"clearmarks",  "", CCmdMapAction, CMDF_NO_DEDICATED},
    {"zoommax",     "", CCmdMapAction, CMDF_NO_DEDICATED},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static automap_t automaps[MAXPLAYERS];

static vectorgrap_t *vectorGraphs[NUM_VECTOR_GRAPHS];

#if __JDOOM__ || __JDOOM64__
static int maplumpnum = 0; // if 0 no background image will be drawn
#elif __JHERETIC__
static int maplumpnum = 1; // if 0 no background image will be drawn
#else
static int maplumpnum = 1; // if 0 no background image will be drawn
#endif

#if __JDOOM__ || __JDOOM64__
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

// CODE --------------------------------------------------------------------

static automap_t __inline *mapForPlayerId(int id)
{
    if(id < 0 || id >= MAXPLAYERS)
    {
#if _DEBUG
Con_Error("mapForPlayerId: Invalid player id %i.", id);
#endif
        return NULL;
    }

    return &automaps[id];
}

static vectorgrap_t *getVectorGraphic(uint idx)
{
    vectorgrap_t       *vg;
    vgline_t           *lines;

    if(idx > NUM_VECTOR_GRAPHS - 1)
        return NULL;

    if(vectorGraphs[idx])
        return vectorGraphs[idx];

    // Not loaded yet.
    {
    uint                i, linecount;

    vg = vectorGraphs[idx] = malloc(sizeof(*vg));

    switch(idx)
    {
    case VG_KEYSQUARE:
        lines = keysquare;
        linecount = sizeof(keysquare) / sizeof(vgline_t);
        break;

    case VG_TRIANGLE:
        lines = thintriangle_guy;
        linecount = sizeof(thintriangle_guy) / sizeof(vgline_t);
        break;

    case VG_ARROW:
        lines = player_arrow;
        linecount = sizeof(player_arrow) / sizeof(vgline_t);
        break;

#if !__JHEXEN__
    case VG_CHEATARROW:
        lines = cheat_player_arrow;
        linecount = sizeof(cheat_player_arrow) / sizeof(vgline_t);
        break;
#endif

    default:
        Con_Error("getVectorGraphic: Unknown idx %i.", idx);
        break;
    }

    vg->lines = malloc(linecount * sizeof(vgline_t));
    vg->count = linecount;
    for(i = 0; i < linecount; ++i)
        memcpy(&vg->lines[i], &lines[i], sizeof(vgline_t));
    }

    return vg;
}

static mapobjectinfo_t *getMapObjectInfo(automap_t *map, int objectname)
{
    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("getMapObjectInfo: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_UNSEENLINE:
        return &map->cfg.unseenLine;

    case AMO_SINGLESIDEDLINE:
        return &map->cfg.singleSidedLine;

    case AMO_TWOSIDEDLINE:
        return &map->cfg.twoSidedLine;

    case AMO_FLOORCHANGELINE:
        return &map->cfg.floorChangeLine;

    case AMO_CEILINGCHANGELINE:
        return &map->cfg.ceilingChangeLine;

    default:
        Con_Error("getMapObjectInfo: No info for object %i.", objectname);
    }

    return NULL;
}

static mapobjectinfo_t *getInfoForSpecialLine(automap_t *map, int special,
                                              sector_t *frontsector,
                                              sector_t *backsector)
{
    uint                i;
    mapobjectinfo_t    *info = NULL;

    if(special > 0)
    {
        for(i = 0; i < map->numSpecialLines && !info; ++i)
        {
            automapspecialline_t *sl = &map->specialLines[i];

            // Is there a line special restriction?
            if(sl->special)
            {
                if(sl->special != special)
                    continue;
            }

            // Is there a sided restriction?
            if(sl->sided)
            {
                if(sl->sided == 1 && backsector && frontsector)
                    continue;
                else if(sl->sided == 2 && (!backsector || !frontsector))
                    continue;
            }

            // Is there a cheat level restriction?
            if(sl->cheatLevel > map->cheating)
                continue;

            // This is the one!
            info = &sl->info;
        }
    }

    return info;
}

/**
 * Register cvars and ccmds for the automap
 * Called during the PreInit of each game
 */
void AM_Register(void)
{
    uint                i;

    for(i = 0; mapCVars[i].name; ++i)
        Con_AddVariable(&mapCVars[i]);
    for(i = 0; mapCCmds[i].name; ++i)
        Con_AddCommand(&mapCCmds[i]);

    if(!IS_DEDICATED)
        AM_ListRegister();
}

/**
 * Called during init.
 */
void AM_Init(void)
{
    uint                i;

    if(IS_DEDICATED)
        return;

    memset(vectorGraphs, 0, sizeof(vectorGraphs));

    scrwidth = Get(DD_WINDOW_WIDTH);
    scrheight = Get(DD_WINDOW_HEIGHT);

    AM_ListInit();
    AM_LoadData();

    memset(&automaps, 0, sizeof(automaps));
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        automap_t *map = &automaps[i];

        // Initialize.
        // \fixme Put these values into an array (or read from a lump?).
        map->followPlayer = i;
        map->oldViewScale = 1;
        map->window.oldX = map->window.x = 0;
        map->window.oldY = map->window.y = 0;
        map->window.oldWidth = map->window.width = scrwidth;
        map->window.oldHeight = map->window.height = scrheight;
        map->cfg.unseenLine.glow = NO_GLOW;
        map->cfg.unseenLine.glowAlpha = 1;
        map->cfg.unseenLine.glowWidth = 10;
        map->cfg.unseenLine.blendMode = BM_NORMAL;
        map->cfg.unseenLine.scaleWithView = false;
        map->cfg.unseenLine.rgba[0] = 1;
        map->cfg.unseenLine.rgba[1] = 1;
        map->cfg.unseenLine.rgba[2] = 1;
        map->cfg.unseenLine.rgba[3] = 1;
        map->cfg.singleSidedLine.glow = NO_GLOW;
        map->cfg.singleSidedLine.glowAlpha = 1;
        map->cfg.singleSidedLine.glowWidth = 10;
        map->cfg.singleSidedLine.blendMode = BM_NORMAL;
        map->cfg.singleSidedLine.scaleWithView = false;
        map->cfg.singleSidedLine.rgba[0] = 1;
        map->cfg.singleSidedLine.rgba[1] = 1;
        map->cfg.singleSidedLine.rgba[2] = 1;
        map->cfg.singleSidedLine.rgba[3] = 1;
        map->cfg.twoSidedLine.glow = NO_GLOW;
        map->cfg.twoSidedLine.glowAlpha = 1;
        map->cfg.twoSidedLine.glowWidth = 10;
        map->cfg.twoSidedLine.blendMode = BM_NORMAL;
        map->cfg.twoSidedLine.scaleWithView = false;
        map->cfg.twoSidedLine.rgba[0] = 1;
        map->cfg.twoSidedLine.rgba[1] = 1;
        map->cfg.twoSidedLine.rgba[2] = 1;
        map->cfg.twoSidedLine.rgba[3] = 1;
        map->cfg.floorChangeLine.glow = NO_GLOW;
        map->cfg.floorChangeLine.glowAlpha = 1;
        map->cfg.floorChangeLine.glowWidth = 10;
        map->cfg.floorChangeLine.blendMode = BM_NORMAL;
        map->cfg.floorChangeLine.scaleWithView = false;
        map->cfg.floorChangeLine.rgba[0] = 1;
        map->cfg.floorChangeLine.rgba[1] = 1;
        map->cfg.floorChangeLine.rgba[2] = 1;
        map->cfg.floorChangeLine.rgba[3] = 1;
        map->cfg.ceilingChangeLine.glow = NO_GLOW;
        map->cfg.ceilingChangeLine.glowAlpha = 1;
        map->cfg.ceilingChangeLine.glowWidth = 10;
        map->cfg.ceilingChangeLine.blendMode = BM_NORMAL;
        map->cfg.ceilingChangeLine.scaleWithView = false;
        map->cfg.ceilingChangeLine.rgba[0] = 1;
        map->cfg.ceilingChangeLine.rgba[1] = 1;
        map->cfg.ceilingChangeLine.rgba[2] = 1;
        map->cfg.ceilingChangeLine.rgba[3] = 1;

        // Register lines we want to display in a special way.
#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
        // Blue locked door, open.
        AM_RegisterSpecialLine(i, 0, 32, 2, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Blue locked door, locked.
        AM_RegisterSpecialLine(i, 0, 32, 2, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 99, 0, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 133, 0, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Red locked door, open.
        AM_RegisterSpecialLine(i, 0, 33, 2, .682f, 0, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Red locked door, locked.
        AM_RegisterSpecialLine(i, 0, 28, 2, .682f, 0, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 134, 2, .682f, 0, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 135, 2, .682f, 0, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Yellow locked door, open.
        AM_RegisterSpecialLine(i, 0, 34, 2, .905f, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Yellow locked door, locked.
        AM_RegisterSpecialLine(i, 0, 27, 2, .905f, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 136, 2, .905f, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 137, 2, .905f, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Exit switch.
        AM_RegisterSpecialLine(i, 1, 11, 1, 0, 1, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Exit cross line.
        AM_RegisterSpecialLine(i, 1, 52, 2, 0, 1, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Secret Exit switch.
        AM_RegisterSpecialLine(i, 1, 51, 1, 0, 1, 1, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Secret Exit cross line.
        AM_RegisterSpecialLine(i, 2, 124, 2, 0, 1, 1, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
#elif __JHERETIC__
        // Blue locked door.
        AM_RegisterSpecialLine(i, 0, 26, 2, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Blue switch?
        AM_RegisterSpecialLine(i, 0, 32, 0, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Yellow locked door.
        AM_RegisterSpecialLine(i, 0, 27, 2, .905f, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Yellow switch?
        AM_RegisterSpecialLine(i, 0, 34, 0, .905f, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Green locked door.
        AM_RegisterSpecialLine(i, 0, 28, 2, 0, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Green switch?
        AM_RegisterSpecialLine(i, 0, 33, 0, 0, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
#elif __JHEXEN__
        // A locked door (all are green).
        AM_RegisterSpecialLine(i, 0, 13, 0, 0, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 83, 0, 0, .9f, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Intra-level teleporters (all are blue).
        AM_RegisterSpecialLine(i, 0, 70, 2, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        AM_RegisterSpecialLine(i, 0, 71, 2, 0, 0, .776f, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Inter-level teleport.
        AM_RegisterSpecialLine(i, 0, 74, 2, .682f, 0, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
        // Game-winning exit.
        AM_RegisterSpecialLine(i, 0, 75, 2, .682f, 0, 0, cfg.automapLineAlpha/2, BM_NORMAL, TWOSIDED_GLOW, cfg.automapLineAlpha/1.5, 5, true);
#endif

        // Setup map based on player's config.
        // \todo All players' maps work from the same config!
        map->cfg.lineGlowScale = cfg.automapDoorGlow;
        map->cfg.glowingLineSpecials = cfg.automapShowDoors;
        map->cfg.panSpeed = cfg.automapPanSpeed;
        map->cfg.panResetOnOpen = cfg.automapPanResetOnOpen;
        map->cfg.zoomSpeed = cfg.automapZoomSpeed;
        setViewRotateMode(map, cfg.automapRotate);

        AM_SetVectorGraphic(i, AMO_THINGPLAYER, VG_ARROW);
        AM_SetColorAndAlpha(i, AMO_BACKGROUND,
                            cfg.automapBack[0], cfg.automapBack[1],
                            cfg.automapBack[2], cfg.automapBack[3]);
        AM_SetColorAndAlpha(i, AMO_UNSEENLINE,
                            cfg.automapL0[0], cfg.automapL0[1],
                            cfg.automapL0[2], cfg.automapLineAlpha);
        AM_SetColorAndAlpha(i, AMO_SINGLESIDEDLINE,
                            cfg.automapL1[0], cfg.automapL1[1],
                            cfg.automapL1[2], cfg.automapLineAlpha);
        AM_SetColorAndAlpha(i, AMO_TWOSIDEDLINE,
                            cfg.automapL0[0], cfg.automapL0[1],
                            cfg.automapL0[2], cfg.automapLineAlpha);
        AM_SetColorAndAlpha(i, AMO_FLOORCHANGELINE,
                            cfg.automapL2[0], cfg.automapL2[1],
                            cfg.automapL2[2], cfg.automapLineAlpha);
        AM_SetColorAndAlpha(i, AMO_CEILINGCHANGELINE,
                            cfg.automapL3[0], cfg.automapL3[1],
                            cfg.automapL3[2], cfg.automapLineAlpha);
    }
}

/**
 * Called during shutdown.
 */
void AM_Shutdown(void)
{
    uint                i;

    if(IS_DEDICATED)
        return; // nothing to do.

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

static void calcViewScaleFactors(automap_t *map)
{
    float               a, b;
    float               maxWidth;
    float               maxHeight;
    float               minWidth;
    float               minHeight;

    if(!map)
        return; // hmm...

    // Calculate the min/max scaling factors.
    maxWidth  = bounds[1][0] - bounds[0][0];
    maxHeight = bounds[1][1] - bounds[0][1];

    minWidth  = 2 * PLAYERRADIUS; // const? never changed?
    minHeight = 2 * PLAYERRADIUS;

    // Calculate world to screen space scale based on window width/height
    // divided by the min/max scale factors derived from map boundaries.
    a = (float) map->window.width / maxWidth;
    b = (float) map->window.height / maxHeight;

    map->minScaleMTOF = (a < b ? a : b);
    map->maxScaleMTOF =
        (float) map->window.height / (2 * PLAYERRADIUS);
}

/**
 * Called during the finalization stage of map loading (after all geometry).
 */
void AM_InitForLevel(void)
{
    uint                i;
    automap_t          *map;

    if(IS_DEDICATED)
        return; // nothing to do.

    // Find the world boundary points shared by all maps.
    findMinMaxBoundaries();

    // Setup all players' maps.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        map = &automaps[i];

        setWindowFullScreenMode(map, true);

        // Determine the map view scale factors.
        calcViewScaleFactors(map);

        // Change the zoom.
        setViewScaleTarget(map, map->maxScale? 0 : .45f); // zero clamped to minScaleMTOF

        // Clear any previously marked map points.
        clearMarks(map);

#if !__JHEXEN__
        if(gameSkill == SM_BABY && cfg.automapBabyKeys)
            map->flags |= AMF_REND_KEYS;

        if(!IS_NETGAME && map->cheating)
            AM_SetVectorGraphic(i, AMO_THINGPLAYER, VG_CHEATARROW);
#endif
        // If the map has been left open from the previous level; close it.
        AM_Stop(i);

        // Reset position onto the follow player.
        if(players[map->followPlayer].plr->mo)
        {
            mobj_t* mo = players[map->followPlayer].plr->mo;
            setViewTarget(map, mo->pos[VX], mo->pos[VY]);
        }
    }
}

/**
 * Start the automap.
 */
void AM_Start(int pnum)
{
    automap_t          *map;

    if(IS_DEDICATED)
        return; // nothing to do.

    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->inGame)
        return;

    if(G_GetGameState() != GS_LEVEL)
        return;  // Can't start the automap if player is not in a game!

    map = &automaps[pnum];
    if(map->active == true)
        return; // Already active.

    map->active = true;
    AM_SetGlobalAlphaTarget(pnum, 1);

    if(!players[map->followPlayer].plr->inGame)
    {   // Set viewer target to the center of the map.
        setViewTarget(map, (bounds[1][VX] - bounds[0][VX]) / 2,
                            (bounds[1][VY] - bounds[0][VY]) / 2);
        setViewAngleTarget(map, 0);
    }
    else
    {   // The map's target player is available.
        mobj_t *mo = players[map->followPlayer].plr->mo;

        if(!(map->panMode && !map->cfg.panResetOnOpen))
            setViewTarget(map, mo->pos[VX], mo->pos[VY]);

        if(map->panMode && map->cfg.panResetOnOpen)
        {
            float       angle;

            /* $unifiedangles */
            if(map->rotate)
                angle = mo->angle / (float) ANGLE_MAX * 360 - 90;
            else
                angle = 0;
            setViewAngleTarget(map, angle);
        }
    }
}

/**
 * Stop the automap
 */
void AM_Stop(int pnum)
{
    automap_t          *map;

    if(IS_DEDICATED)
        return; // nothing to do.

    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->inGame)
        return;

    map = &automaps[pnum];

    map->active = false;
    AM_SetGlobalAlphaTarget(pnum, 0);
}

/**
 * Translates from map to automap window coordinates.
 */
float AM_MapToFrame(int pid, float val)
{
    automap_t          *map;

    if(IS_DEDICATED)
        Con_Error("AM_MapToFrame: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;

    return MTOF(map, val);
}

/**
 * Translates from automap window to map coordinates.
 */
float AM_FrameToMap(int pid, float val)
{
    automap_t          *map;

    if(IS_DEDICATED)
        Con_Error("AM_MapToFrame: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;

    return FTOM(map, val);
}

static void setWindowTarget(automap_t *map, int x, int y, int w, int h)
{
    automapwindow_t    *win;

    // Are we in fullscreen mode?
    // If so, setting the window size is not allowed.
    if(map->fullScreenMode)
        return;

    win = &map->window;

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

void AM_SetWindowTarget(int pid, int x, int y, int w, int h)
{
    automap_t          *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setWindowTarget(map, x, y, w, h);
}

void AM_GetWindow(int pid, float *x, float *y, float *w, float *h)
{
    automap_t          *map;

    if(IS_DEDICATED)
        Con_Error("AM_GetWindow: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(x) *x = map->window.x;
    if(y) *y = map->window.y;
    if(w) *w = map->window.width;
    if(h) *h = map->window.height;
}

static void setWindowFullScreenMode(automap_t *map, int value)
{
    if(value == 2) // toggle
        map->fullScreenMode = !map->fullScreenMode;
    else
        map->fullScreenMode = value;
}

void AM_SetWindowFullScreenMode(int pid, int value)
{
    automap_t          *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(value < 0 || value > 2)
    {
#if _DEBUG
Con_Error("AM_SetFullScreenMode: Unknown value %i.", value);
#endif
        return;
    }
    setWindowFullScreenMode(map, value);
}

boolean AM_IsMapWindowInFullScreenMode(int pid)
{
    automap_t          *map;

    if(IS_DEDICATED)
        Con_Error("AM_IsMapWindowInFullScreenMode: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return false;
    return map->fullScreenMode;
}

static void setViewTarget(automap_t *map, float x, float y)
{
    // Already at this target?
    if(x == map->targetViewX && y == map->targetViewY)
        return;

    x = CLAMP(x, -32768, 32768);
    y = CLAMP(y, -32768, 32768);

    map->oldViewX = map->viewX;
    map->oldViewY = map->viewY;
    // Restart the timer.
    map->viewTimer = 0;

    map->targetViewX = x;
    map->targetViewY = y;
}

void AM_SetViewTarget(int pid, float x, float y)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewTarget(map, x, y);
}

void AM_GetViewPosition(int pid, float *x, float *y)
{
    automap_t *map;

    if(IS_DEDICATED)
        Con_Error("AM_GetViewPosition: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(x) *x = map->viewX;
    if(y) *y = map->viewY;
}

/**
 * @return              Current alpha level of the automap.
 */
float AM_ViewAngle(int pid)
{
    automap_t *map;

    if(IS_DEDICATED)
        Con_Error("AM_ViewAngle: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;

    return map->angle;
}

static void setViewScaleTarget(automap_t *map, float scale)
{
    scale = MINMAX_OF(map->minScaleMTOF, scale, map->maxScaleMTOF);

    // Already at this target?
    if(scale == map->targetViewScale)
        return;

    map->oldViewScale = map->viewScale;
    // Restart the timer.
    map->viewScaleTimer = 0;

    map->targetViewScale = scale;
}

void AM_SetViewScaleTarget(int pid, float scale)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewScaleTarget(map, scale);
}

static void setViewAngleTarget(automap_t *map, float angle)
{
    // Already at this target?
    if(angle == map->targetAngle)
        return;

    map->oldAngle = map->angle;
    // Restart the timer.
    map->angleTimer = 0;

    map->targetAngle = angle;
}

void AM_SetViewAngleTarget(int pid, float angle)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewAngleTarget(map, angle);
}

/**
 * @return              True if the specified map is currently active.
 */
boolean AM_IsMapActive(int pid)
{
    automap_t *map;

    if(IS_DEDICATED)
        return false;

    map = mapForPlayerId(pid);
    if(!map)
        return false;
    return map->active;
}

static void setViewRotateMode(automap_t *map, boolean on)
{
    map->rotate = on;
}

void AM_SetViewRotateMode(int pid, boolean on)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewRotateMode(map, on);
}

/**
 * Update the specified player's automap.
 *
 * @param pid           Idx of the player whose map is being updated.
 * @param lineIdx       Idx of the line being added to the map.
 * @param visible       @c true= mark the line as visible, else hidden.
 */
void AM_UpdateLinedef(int pid, uint lineIdx, boolean visible)
{
    xline_t            *xline;

    if(pid < 0 || pid >= MAXPLAYERS)
        return;

    if(lineIdx >= numlines)
        return;

    xline = P_GetXLine(lineIdx);
    xline->mapped[pid] = visible;
}

static void clearMarks(automap_t *map)
{
    uint        i;

    for(i = 0; i < NUMMARKPOINTS; ++i)
        map->markpointsUsed[i] = false;
    map->markpointnum = 0;
}

/**
 * Clears markpoint array.
 */
void AM_ClearMarks(int pid)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    clearMarks(map);
}

/**
 * Adds a marker at the current location
 */
static int addMark(automap_t *map, float x, float y)
{
    int         num = map->markpointnum;

    map->markpoints[num].pos[VX] = x;
    map->markpoints[num].pos[VY] = y;
    map->markpoints[num].pos[VZ] = 0;
    map->markpointsUsed[num] = true;
    map->markpointnum = (map->markpointnum + 1) % NUMMARKPOINTS;

#if _DEBUG
Con_Message("Added mark point %i to map at X=%g Y=%g\n",
            num,
            map->markpoints[num].pos[VX],
            map->markpoints[num].pos[VY]);
#endif

    return num;
}

/**
 * Adds a marker at the current location
 */
int AM_AddMark(int pid, float x, float y)
{
    automap_t *map;

    if(IS_DEDICATED)
        Con_Error("AM_AddMark: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return -1;
    return addMark(map, x, y);
}

/**
 * Set the alpha level of the automap. Alpha levels below one automatically
 * show the game view in addition to the automap.
 *
 * @param alpha         Alpha level to set the automap too (0...1)
 */
void AM_SetGlobalAlphaTarget(int pid, float alpha)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    map->targetAlpha = CLAMP(alpha, 0, 1);
}

/**
 * @return              Current alpha level of the automap.
 */
float AM_GlobalAlpha(int pid)
{
    automap_t *map;

    if(IS_DEDICATED)
        Con_Error("AM_GlobalAlpha: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;
    return map->alpha;
}

void AM_SetColor(int pid, int objectname, float r, float g, float b)
{
    automap_t *map;
    mapobjectinfo_t *info;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColor: Unknown object %i.", objectname);

    r = CLAMP(r, 0, 1);
    g = CLAMP(g, 0, 1);
    b = CLAMP(b, 0, 1);

    // Check special cases first,
    if(objectname == AMO_BACKGROUND)
    {
        map->cfg.backgroundRGBA[0] = r;
        map->cfg.backgroundRGBA[1] = g;
        map->cfg.backgroundRGBA[2] = b;
        return;
    }

    // It must be an object with a name.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.unseenLine;
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.singleSidedLine;
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.twoSidedLine;
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.floorChangeLine;
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.ceilingChangeLine;
        break;

    default:
        Con_Error("AM_SetColor: Object %i does not use color.",
                  objectname);
        break;
    }

    info->rgba[0] = r;
    info->rgba[1] = g;
    info->rgba[2] = b;
}

void AM_GetColor(int pid, int objectname, float *r, float *g, float *b)
{
    automap_t *map;
    mapobjectinfo_t *info;

    if(IS_DEDICATED)
        Con_Error("AM_GetColor: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColor: Unknown object %i.", objectname);

    // Check special cases first.
    if(objectname == AMO_BACKGROUND)
    {
        if(r) *r = map->cfg.backgroundRGBA[0];
        if(g) *g = map->cfg.backgroundRGBA[1];
        if(b) *b = map->cfg.backgroundRGBA[2];
        return;
    }

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.unseenLine;
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.singleSidedLine;
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.twoSidedLine;
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.floorChangeLine;
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.ceilingChangeLine;
        break;

    default:
        Con_Error("AM_GetColor: Object %i does not use color.",
                  objectname);
        break;
    }

    if(r) *r = info->rgba[0];
    if(g) *g = info->rgba[1];
    if(b) *b = info->rgba[2];
}

void AM_SetColorAndAlpha(int pid, int objectname, float r, float g, float b,
                         float a)
{
    automap_t *map;
    mapobjectinfo_t *info;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColorAndAlpha: Unknown object %i.", objectname);

    r = CLAMP(r, 0, 1);
    g = CLAMP(g, 0, 1);
    b = CLAMP(b, 0, 1);
    a = CLAMP(a, 0, 1);

    // Check special cases first.
    if(objectname == AMO_BACKGROUND)
    {
        map->cfg.backgroundRGBA[0] = r;
        map->cfg.backgroundRGBA[1] = g;
        map->cfg.backgroundRGBA[2] = b;
        map->cfg.backgroundRGBA[3] = a;
        return;
    }

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.unseenLine;
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.singleSidedLine;
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.twoSidedLine;
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.floorChangeLine;
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.ceilingChangeLine;
        break;

    default:
        Con_Error("AM_SetColorAndAlpha: Object %i does not use color/alpha.",
                  objectname);
        break;
    }

    info->rgba[0] = r;
    info->rgba[1] = g;
    info->rgba[2] = b;
    info->rgba[3] = a;
}

void AM_GetColorAndAlpha(int pid, int objectname, float *r, float *g,
                         float *b, float *a)
{
    automap_t *map;
    mapobjectinfo_t *info;

    if(IS_DEDICATED)
        Con_Error("AM_GetColorAndAlpha: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_GetColorAndAlpha: Unknown object %i.", objectname);

    // Check special cases first.
    if(objectname == AMO_BACKGROUND)
    {
        if(r) *r = map->cfg.backgroundRGBA[0];
        if(g) *g = map->cfg.backgroundRGBA[1];
        if(b) *b = map->cfg.backgroundRGBA[2];
        if(a) *a = map->cfg.backgroundRGBA[3];
        return;
    }

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.unseenLine;
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.singleSidedLine;
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.twoSidedLine;
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.floorChangeLine;
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.ceilingChangeLine;
        break;

    default:
        Con_Error("AM_GetColorAndAlpha: Object %i does not use color/alpha.",
                  objectname);
        break;
    }

    if(r) *r = info->rgba[0];
    if(g) *g = info->rgba[1];
    if(b) *b = info->rgba[2];
    if(a) *a = info->rgba[3];
}

void AM_SetBlendmode(int pid, int objectname, blendmode_t blendmode)
{
    automap_t *map;
    mapobjectinfo_t *info;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetBlendmode: Unknown object %i.", objectname);

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.unseenLine;
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.singleSidedLine;
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.twoSidedLine;
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.floorChangeLine;
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.ceilingChangeLine;
        break;

    default:
        Con_Error("AM_SetBlendmode: Object %i does not support blending modes.",
                  objectname);
        break;
    }

    info->blendMode = blendmode;
}

void AM_SetGlow(int pid, int objectname, glowtype_t type, float size,
                float alpha, boolean canScale)
{
    automap_t *map;
    mapobjectinfo_t *info;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetGlow: Unknown object %i.", objectname);

    size = CLAMP(size, 0, 100);
    alpha = CLAMP(alpha, 0, 1);

    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.unseenLine;
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.singleSidedLine;
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.twoSidedLine;
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.floorChangeLine;
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.ceilingChangeLine;
        break;

    default:
        Con_Error("AM_SetGlow: Object %i does not support glow.",
                  objectname);
        break;
    }

    info->glow = type;
    info->glowAlpha = alpha;
    info->glowWidth = size;
    info->scaleWithView = canScale;
}

void AM_SetVectorGraphic(int pid, int objectname, int vgname)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetVectorGraphic: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_THINGPLAYER:
        map->vectorGraphicForPlayer = vgname;
        break;

    default:
        Con_Error("AM_SetVectorGraphic: Object %i does not support vector graphic.",
                  objectname);
        break;
    }
}

void AM_RegisterSpecialLine(int pid, int cheatLevel, int lineSpecial,
                            int sided,
                            float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView)
{
    uint        i;
    automap_t  *map;
    automapspecialline_t *line, *p;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(cheatLevel < 0 || cheatLevel > 4)
        Con_Error("AM_RegisterSpecialLine: cheatLevel '%i' out of range {0-4}.",
                  cheatLevel);
    if(lineSpecial < 0)
        Con_Error("AM_RegisterSpecialLine: lineSpecial '%i' is negative.",
                  lineSpecial);
    if(sided < 0 || sided > 2)
        Con_Error("AM_RegisterSpecialLine: sided '%i' is invalid.", sided);

    // Later re-registrations override earlier ones.
    i = 0;
    line = NULL;
    while(i < map->numSpecialLines && !line)
    {
        p = &map->specialLines[i];
        if(p->special == lineSpecial && p->cheatLevel == cheatLevel)
            line = p;
        else
            i++;
    }

    if(!line) // It must be a new one.
    {
        // Any room for a new special line?
        if(map->numSpecialLines >= AM_MAXSPECIALLINES)
            Con_Error("AM_RegisterSpecialLine: No available slot.");

        line = &map->specialLines[map->numSpecialLines++];
    }

    line->cheatLevel = cheatLevel;
    line->special = lineSpecial;
    line->sided = sided;

    line->info.rgba[0] = CLAMP(r, 0, 1);
    line->info.rgba[1] = CLAMP(g, 0, 1);
    line->info.rgba[2] = CLAMP(b, 0, 1);
    line->info.rgba[3] = CLAMP(a, 0, 1);
    line->info.glow = glowType;
    line->info.glowAlpha = CLAMP(glowAlpha, 0, 1);
    line->info.glowWidth = glowWidth;
    line->info.scaleWithView = scaleGlowWithView;
    line->info.blendMode = blendmode;
}

void AM_SetCheatLevel(int pid, int level)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    map->cheating = level;

    if(map->cheating >= 1)
        map->flags |= AMF_REND_ALLLINES;
    else
        map->flags &= ~AMF_REND_ALLLINES;

    if(map->cheating == 2)
        map->flags |= (AMF_REND_THINGS | AMF_REND_XGLINES);
    else
        map->flags &= ~(AMF_REND_THINGS | AMF_REND_XGLINES);

    if(map->cheating >= 2)
        map->flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    else
        map->flags &= ~(AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
}

void AM_IncMapCheatLevel(int pid)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    map->cheating = (map->cheating + 1) % 4;

    if(map->cheating)
        map->flags |= AMF_REND_ALLLINES;
    else
        map->flags &= ~AMF_REND_ALLLINES;

    if(map->cheating == 2)
        map->flags |= (AMF_REND_THINGS | AMF_REND_XGLINES);
    else
        map->flags &= ~(AMF_REND_THINGS | AMF_REND_XGLINES);
}

/**
 * Determines bounding box of all the map's vertexes.
 */
static void findMinMaxBoundaries(void)
{
    uint        i;
    float       pos[2];

    bounds[0][0] = bounds[0][1] = DDMAXFLOAT;
    bounds[1][0] = bounds[1][1] = -DDMAXFLOAT;

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
#if !__JDOOM64__
    int         i;
    char        namebuf[9];
#endif

    if(IS_DEDICATED)
        return; // Nothing to do.

#if !__JDOOM64__
    // Load the marker patches.
    for(i = 0; i < 10; ++i)
    {
        MARKERPATCHES; // Check the macros eg: "sprintf(namebuf, "AMMNUM%d", i)" for jDoom
        R_CachePatch(&markerPatches[i], namebuf);
    }
#endif

    if(maplumpnum != 0)
        maplumpnum = W_GetNumForName("AUTOPAGE");

    if(numTexUnits > 1)
    {   // Great, we can replicate the map fade out effect using multitexture,
        // load the mask texture.
        if(!amMaskTexture && !Get(DD_NOVIDEO))
        {
            amMaskTexture =
                GL_NewTextureWithParams2(DGL_LUMINANCE, 256, 256,
                                         W_CacheLumpName("mapmask", PU_CACHE),
                                         0x8, DGL_NEAREST, DGL_LINEAR,
                                         0 /*no anisotropy*/,
                                         DGL_CLAMP, DGL_CLAMP);
        }
    }
}

/**
 * Unload any resources needed for the automap.
 * Called during shutdown and before a renderer restart.
 */
void AM_UnloadData(void)
{
    if(Get(DD_NOVIDEO) || IS_DEDICATED)
        return; // Nothing to do.

    if(amMaskTexture)
        DGL_DeleteTextures(1, (DGLuint*) &amMaskTexture);
    amMaskTexture = 0;
}

/**
 * Animates an automap view window towards the target values.
 */
static void mapWindowTicker(automap_t *map)
{
    float       newX, newY, newWidth, newHeight;
    automapwindow_t *win;

    if(!map)
        return; // hmm...

    win = &map->window;

    // Get the view window dimensions.
    R_GetViewWindow(&newX, &newY, &newWidth, &newHeight);
    // Scale to screen space.
    newX = FIXXTOSCREENX(newX);
    newY = FIXYTOSCREENY(newY);
    newWidth = FIXXTOSCREENX(newWidth);
    newHeight = FIXYTOSCREENY(newHeight);

    if(newX != win->x || newY != win->y ||
       newWidth != win->height || newHeight != win->width)
    {
        if(map->fullScreenMode)
        {
            // In fullscreen mode we always snap straight to the
            // new dimensions.
            win->x = win->oldX = win->targetX = newX;
            win->y = win->oldY = win->targetY = newY;
            win->width = win->oldWidth = win->targetWidth = newWidth;
            win->height = win->oldHeight = win->targetHeight = newHeight;
        }
        else
        {
            // Snap dimensions if new scale is smaller.
            if(newX > win->x)
                win->x = win->oldX = win->targetX = newX;
            if(newY > win->y)
                win->y = win->oldY = win->targetY = newY;
            if(newWidth < win->width)
                win->width = win->oldWidth = win->targetWidth = newWidth;
            if(newHeight < win->height)
                win->height = win->oldHeight = win->targetHeight = newHeight;
        }

        // Now the screen dimensions have changed we have to update scaling
        // factors accordingly.
        calcViewScaleFactors(map);
    }

    if(map->fullScreenMode)
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

static void addToBoxf(float *box, float x, float y)
{
    if(x < box[BOXLEFT])
        box[BOXLEFT] = x;
    else if(x > box[BOXRIGHT])
        box[BOXRIGHT] = x;
    if(y < box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    else if(y > box[BOXTOP])
        box[BOXTOP] = y;
}

/**
 * Called each tic for each player's automap if they are in-game.
 */
static void mapTicker(automap_t *map)
{
#define MAPALPHA_FADE_STEP .07

    int         playerNum = map - automaps;
    float       diff = 0;
    float       width, height, scale;
    float       panX[2], panY[2];
    float       zoomVel;
    float       zoomSpeed;
    mobj_t     *mo = players[map->followPlayer].plr->mo;

    // Check the state of the controls. Done here so that offsets don't accumulate
    // unnecessarily, as they would, if left unread.
    P_GetControlState(playerNum, CTL_MAP_PAN_X, &panX[0], &panX[1]);
    P_GetControlState(playerNum, CTL_MAP_PAN_Y, &panY[0], &panY[1]);

    if(!map)
        return;

    // Move towards the target alpha level for the automap.
    if(map->alpha != map->targetAlpha)
    {
        diff = map->targetAlpha - map->alpha;
        if(fabs(diff) > MAPALPHA_FADE_STEP)
        {
            map->alpha += MAPALPHA_FADE_STEP * (diff > 0? 1 : -1);
        }
        else
        {
            map->alpha = map->targetAlpha;
        }
    }

    // If the automap is not active do nothing else.
    if(!map->active)
        return;

    //
    // Update per tic, driven controls.
    //

    // Map view zoom contol.
    zoomSpeed = (1 + map->cfg.zoomSpeed);
    if(players[playerNum].brain.speed)
        zoomSpeed *= 1.5f;
    P_GetControlState(playerNum, CTL_MAP_ZOOM, &zoomVel, NULL); // ignores rel offset -jk
    if(zoomVel > 0)  // zoom in
    {
        setViewScaleTarget(map, map->viewScale * zoomSpeed);
    }
    else if(zoomVel < 0) // zoom out
    {
        setViewScaleTarget(map, map->viewScale / zoomSpeed);
    }

    // Map viewer location paning control.
    if(map->panMode || !players[map->followPlayer].plr->inGame)
    {
        float       xy[2] = { 0, 0 }; // deltas

        // DOOM.EXE used to pan at 140 fixed pixels per second.
        float       panUnitsPerTic = (FTOM(map, FIXXTOSCREENX(140)) / TICSPERSEC) *
                        (2 * map->cfg.panSpeed);

        if(panUnitsPerTic < 8)
            panUnitsPerTic = 8;

        xy[VX] = panX[0] * panUnitsPerTic + panX[1];
        xy[VY] = panY[0] * panUnitsPerTic + panY[1];

        V2_Rotate(xy, map->angle / 360 * 2 * PI);

        if(xy[VX] || xy[VY])
            setViewTarget(map, map->viewX + xy[VX], map->viewY + xy[VY]);
    }
    else  // Camera follows the player
    {
        float       angle;

        setViewTarget(map, mo->pos[VX], mo->pos[VY]);

        /* $unifiedangles */
        if(map->rotate)
            angle = mo->angle / (float) ANGLE_MAX * 360 - 90;
        else
            angle = 0;
        setViewAngleTarget(map, angle);
    }

    //
    // Animate map values.
    //

    // Window position and dimensions.
    mapWindowTicker(map);

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
    // Move the parallax layer.
    map->viewPLX = map->viewX / 4000;
    map->viewPLY = map->viewY / 4000;

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
    map->angleTimer += .4f;
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
    scale = /*(map->maxScaleMTOF - map->minScaleMTOF)* */ map->viewScale;

    // Scaling multipliers.
    map->scaleMTOF = /*map->minScaleMTOF + */ scale;
    map->scaleFTOM = 1.0f / map->scaleMTOF;

    width = FTOM(map, map->window.width);
    height = FTOM(map, map->window.height);

    // Calculate the viewframe.
    // Top Left
    map->vframe[0][VX] = map->viewX - width / 2;
    map->vframe[0][VY] = map->viewY - height / 2;
    // Bottom Right
    map->vframe[1][VX] = map->viewX + width / 2;
    map->vframe[1][VY] = map->viewY + height / 2;

    // Calculate the view clipbox (rotation aware).
    {
    angle_t     angle;
    float       v[2];

    /* $unifiedangles */
    angle = (float) ANGLE_MAX * (map->angle / 360);

    v[VX] = -width / 2;
    v[VY] = -height / 2;
    rotate2D(&v[VX], &v[VY], angle);
    v[VX] += map->viewX;
    v[VY] += map->viewY;

    map->vbbox[BOXLEFT] = map->vbbox[BOXRIGHT] = v[VX];
    map->vbbox[BOXTOP] = map->vbbox[BOXBOTTOM] = v[VY];

    v[VX] = width / 2;
    v[VY] = -height / 2;
    rotate2D(&v[VX], &v[VY], angle);
    v[VX] += map->viewX;
    v[VY] += map->viewY;
    addToBoxf(map->vbbox, v[VX], v[VY]);

    v[VX] = -width / 2;
    v[VY] = height / 2;
    rotate2D(&v[VX], &v[VY], angle);
    v[VX] += map->viewX;
    v[VY] += map->viewY;
    addToBoxf(map->vbbox, v[VX], v[VY]);

    v[VX] = width / 2;
    v[VY] = height / 2;
    rotate2D(&v[VX], &v[VY], angle);
    v[VX] += map->viewX;
    v[VY] += map->viewY;
    addToBoxf(map->vbbox, v[VX], v[VY]);
    }

#undef MAPALPHA_FADE_STEP
}

/**
 * Updates on Game Tick.
 */
void AM_Ticker(void)
{
    uint        i;

    if(IS_DEDICATED)
        return; // Nothing to do.

    // We need to respond right away if the screen dimensions change, so
    // keep a copy locally.
    scrwidth = Get(DD_WINDOW_WIDTH);
    scrheight = Get(DD_WINDOW_HEIGHT);

    // All maps get to tick if their player is in-game.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        mapTicker(&automaps[i]);
    }
}

/**
 * Is the given point within the viewframe of the automap?
 *
 * @return          @c true, if the point is visible.
 */
static boolean isPointVisible(automap_t *map, float x, float y)
{
    // First check if the vector is completely outside the view bbox.
    if(x < map->vbbox[BOXLEFT] || x > map->vbbox[BOXRIGHT] ||
       y > map->vbbox[BOXTOP]  || y < map->vbbox[BOXBOTTOM])
       return false;

    // \todo The point is within the view bbox but it is not necessarily
    // within view. Implement a more accurate test.
    return true;
}

/**
 * Is the given vector within the viewframe of the automap?
 *
 * @return          @c true, if the vector is at least partially
 *                  visible.
 */
static boolean isVectorVisible(automap_t *map,
                               float x1, float y1, float x2, float y2)
{
    // First check if the vector is completely outside the view bbox.
    if((x1 < map->vbbox[BOXLEFT]   && x2 < map->vbbox[BOXLEFT]) ||
       (x1 > map->vbbox[BOXRIGHT]  && x2 > map->vbbox[BOXRIGHT]) ||
       (y1 > map->vbbox[BOXTOP]    && y2 > map->vbbox[BOXTOP]) ||
       (y1 < map->vbbox[BOXBOTTOM] && y2 < map->vbbox[BOXBOTTOM]))
       return false;

    /** \todo The vector is within the view bbox but it is not necessarily
    * within view. Implement a more accurate test. Should we even bother
    * with determining the intersection and actively clip primitives in
    * the window?
    */
    return true;
}

/**
 * Draws the given line. Basic cliping is done.
 */
static void rendLine(float x1, float y1, float x2, float y2, float width,
                     int color, float alpha, blendmode_t blendmode)
{
    automap_t  *map = &automaps[mapviewplayer];

    if(!isVectorVisible(map, x1, y1, x2, y2))
        return;

    AM_AddLine(CXMTOF(map, x1), CYMTOF(map, y1),
               CXMTOF(map, x2), CYMTOF(map, y2),
               width, color, alpha, blendmode);
}

/**
 * Draws the given line including any optional extras.
 */
static void rendLine2(float x1, float y1, float x2, float y2, float width,
                      float r, float g, float b, float a,
                      glowtype_t glowType, float glowAlpha, float glowWidth,
                      boolean glowOnly, boolean scaleGlowWithView,
                      boolean caps, blendmode_t blend, boolean drawNormal)
{
    float       v1[2], v2[2];
    float       length, dx, dy;
    float       normal[2], unit[2];
    automap_t  *map = &automaps[mapviewplayer];

    // Scale into map, screen space units.
    v1[VX] = CXMTOF(map, x1);
    v1[VY] = CYMTOF(map, y1);
    v2[VX] = CXMTOF(map, x2);
    v2[VY] = CYMTOF(map, y2);

    if(!isVectorVisible(map, x1, y1, x2, y2))
        return;

    dx = v2[VX] - v1[VX];
    dy = v2[VY] - v1[VY];
    length = sqrt(dx * dx + dy * dy);
    if(length <= 0)
        return;

    unit[VX] = dx / length;
    unit[VY] = dy / length;
    normal[VX] = unit[VY];
    normal[VY] = -unit[VX];

    // Is this a glowing line?
    if(glowType != NO_GLOW)
    {
        int         tex;
        float       thickness;

        // Scale line thickness relative to zoom level?
        if(scaleGlowWithView)
            thickness = map->cfg.lineGlowScale * map->scaleMTOF * 2.5f + 3;
        else
            thickness = glowWidth;

        tex = Get(DD_DYNLIGHT_TEXTURE);

        if(caps)
        {
            // Draw a "cap" at the start of the line
            AM_AddQuad(v1[VX] - unit[VX] * thickness + normal[VX] * thickness,
                       v1[VY] - unit[VY] * thickness + normal[VY] * thickness,
                       v1[VX] + normal[VX] * thickness,
                       v1[VY] + normal[VY] * thickness,
                       v1[VX] - normal[VX] * thickness,
                       v1[VY] - normal[VY] * thickness,
                       v1[VX] - unit[VX] * thickness - normal[VX] * thickness,
                       v1[VY] - unit[VY] * thickness - normal[VY] * thickness,
                       0, 0,
                       0.5f, 0,
                       0.5f, 1,
                       0, 1,
                       r, g, b, glowAlpha,
                       tex, false, blend);
        }

        // The middle part of the line.
        switch(glowType)
        {
        case TWOSIDED_GLOW:
            AM_AddQuad(v1[VX] + normal[VX] * thickness,
                       v1[VY] + normal[VY] * thickness,
                       v2[VX] + normal[VX] * thickness,
                       v2[VY] + normal[VY] * thickness,
                       v2[VX] - normal[VX] * thickness,
                       v2[VY] - normal[VY] * thickness,
                       v1[VX] - normal[VX] * thickness,
                       v1[VY] - normal[VY] * thickness,
                       0.5f, 0,
                       0.5f, 0,
                       0.5f, 1,
                       0.5f, 1,
                       r, g, b, glowAlpha,
                       tex, false, blend);
            break;

        case BACK_GLOW:
            AM_AddQuad(v1[VX] + normal[VX] * thickness,
                       v1[VY] + normal[VY] * thickness,
                       v2[VX] + normal[VX] * thickness,
                       v2[VY] + normal[VY] * thickness,
                       v2[VX], v2[VY],
                       v1[VX], v1[VY],
                       0, 0.25f,
                       0, 0.25f,
                       0.5f, 0.25f,
                       0.5f, 0.25f,
                       r, g, b, glowAlpha,
                       tex, false, blend);
            break;

        case FRONT_GLOW:
            AM_AddQuad(v1[VX], v1[VY],
                       v2[VX], v2[VY],
                       v2[VX] - normal[VX] * thickness,
                       v2[VY] - normal[VY] * thickness,
                       v1[VX] - normal[VX] * thickness,
                       v1[VY] - normal[VY] * thickness,
                       0.75f, 0.5f,
                       0.75f, 0.5f,
                       0.75f, 1,
                       0.75f, 1,
                       r, g, b, glowAlpha,
                       tex, false, blend);
            break;

        default:
            break; // Impossible.
        }

        if(caps)
        {
            // Draw a "cap" at the end of the line.
            AM_AddQuad(v2[VX] + normal[VX] * thickness,
                       v2[VY] + normal[VY] * thickness,
                       v2[VX] + unit[VX] * thickness + normal[VX] * thickness,
                       v2[VY] + unit[VY] * thickness + normal[VY] * thickness,
                       v2[VX] + unit[VX] * thickness - normal[VX] * thickness,
                       v2[VY] + unit[VY] * thickness - normal[VY] * thickness,
                       v2[VX] - normal[VX] * thickness,
                       v2[VY] - normal[VY] * thickness,
                       0.5f, 0,
                       1, 0,
                       1, 1,
                       0.5f, 1,
                       r, g, b, glowAlpha,
                       tex, false, blend);
        }
    }

    if(!glowOnly)
        AM_AddLine4f(v1[VX], v1[VY], v2[VX], v2[VY], width,
                     r, g, b, a, blend);

    if(drawNormal)
    {
#define NORMTAIL_LENGTH         8

        float       center[2];

        center[VX] = v1[VX] + (length / 2) * unit[VX];
        center[VY] = v1[VY] + (length / 2) * unit[VY];
        AM_AddLine4f(center[VX], center[VY],
                     center[VX] - normal[VX] * NORMTAIL_LENGTH,
                     center[VY] - normal[VY] * NORMTAIL_LENGTH, 1,
                     r, g, b, a, blend);

#undef NORMTAIL_LENGTH
    }
}

int renderWallSeg(seg_t *seg, void *data)
{
    ssecitervars_t *vars = (ssecitervars_t*) data;
    float       v1[2], v2[2];
    linedef_t     *line;
    xline_t    *xLine;
    sector_t   *frontSector, *backSector;
    mapobjectinfo_t *info;
    player_t   *plr = vars->plr;
    automap_t  *map = vars->map;

    line = P_GetPtrp(seg, DMU_LINEDEF);
    if(!line)
        return 1;

    xLine = P_ToXLine(line);
    if(xLine->validCount == VALIDCOUNT)
        return 1; // Already drawn once.

    if((xLine->flags & ML_DONTDRAW) && !(map->flags & AMF_REND_ALLLINES))
        return 1;

    frontSector = P_GetPtrp(line, DMU_FRONT_SECTOR);
    if(frontSector != P_GetPtrp(line, DMU_SIDEDEF0_OF_LINE | DMU_SECTOR))
        return 1; // We only want to draw twosided lines once.

#if !__JHEXEN__
#if !__JSTRIFE__
    if(map->flags & AMF_REND_XGLINES) // Debug cheat.
    {
        // Show active XG lines.
        if(xLine->xg && xLine->xg->active && (levelTime & 4))
        {
            P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
            P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

            rendLine2(v1[VX], v1[VY], v2[VX], v2[VY], AM_LINE_WIDTH,
                      .8f, 0, .8f, 1,
                      TWOSIDED_GLOW, 1, 5, false, true, false, BM_ADD,
                      (map->flags & AMF_REND_LINE_NORMALS));

            xLine->validCount = VALIDCOUNT;  // Mark as drawn this frame.
            return 1; // Continue iteration.
        }
    }
#endif
#endif

    info = NULL;
    if((map->flags & AMF_REND_ALLLINES) ||
       xLine->mapped[mapviewplayer])
    {
        backSector = P_GetPtrp(line, DMU_BACK_SECTOR);

        // Perhaps this is a specially colored line?
        info = getInfoForSpecialLine(map, xLine->special,
                                     frontSector, backSector);

        if(!info)
        {   // Perhaps a default colored line?
            if(!(frontSector && backSector) || (xLine->flags & ML_SECRET))
            {
                // solid wall (well probably anyway...)
                info = getMapObjectInfo(map, AMO_SINGLESIDEDLINE);
            }
            else
            {
                if(P_GetFloatp(backSector, DMU_FLOOR_HEIGHT) !=
                   P_GetFloatp(frontSector, DMU_FLOOR_HEIGHT))
                {
                    // Floor level change.
                    info = getMapObjectInfo(map, AMO_FLOORCHANGELINE);
                }
                else if(P_GetFloatp(backSector, DMU_CEILING_HEIGHT) !=
                        P_GetFloatp(frontSector, DMU_CEILING_HEIGHT))
                {
                    // Ceiling level change.
                    info = getMapObjectInfo(map, AMO_CEILINGCHANGELINE);
                }
                else if(map->flags & AMF_REND_ALLLINES)
                {
                    info = getMapObjectInfo(map, AMO_UNSEENLINE);
                }
            }
        }
    }
    else if(plr->powers[PT_ALLMAP])
    {
        if(!(xLine->flags & ML_DONTDRAW))
        {
            // An as yet, unseen line.
            info = getMapObjectInfo(map, AMO_UNSEENLINE);
        }
    }

    if(info)
    {
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

        rendLine2(v1[VX], v1[VY], v2[VX], v2[VY], AM_LINE_WIDTH,
                  info->rgba[0], info->rgba[1], info->rgba[2], info->rgba[3],
                  (xLine->special && !map->cfg.glowingLineSpecials ?
                        NO_GLOW : info->glow),
                  info->glowAlpha,
                  info->glowWidth, false, info->scaleWithView,
                  (info->glow && !(xLine->special && !map->cfg.glowingLineSpecials)),
                  (xLine->special && !map->cfg.glowingLineSpecials ?
                        BM_NORMAL : info->blendMode),
                  (map->flags & AMF_REND_LINE_NORMALS));

        xLine->validCount = VALIDCOUNT; // Mark as drawn this frame.
    }

    return 1; // Continue iteration.
}

/**
 * Sub-routine of renderWalls().
 */
boolean drawSegsOfSubsector(subsector_t *s, void *data)
{
    return P_Iteratep(s, DMU_SEG, data, renderWallSeg);
}

/**
 * Sub-routine of renderWalls().
 */
boolean drawSegsOfPolyobject(polyobj_t *po, void *data)
{
    return P_Iteratep(po, DMU_SEG, data, renderWallSeg);
}

/**
 * Determines visible lines, draws them.
 */
static void renderWalls(void)
{
    player_t   *plr = &players[mapviewplayer];
    automap_t  *map = &automaps[mapviewplayer];
    ssecitervars_t data;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    data.plr = plr;
    data.map = map;

    // Use the automap's viewframe bounding box.
    P_SubsectorsBoxIterator(map->vbbox, NULL, drawSegsOfSubsector, &data);

    // Next, draw any polyobjects in view.
    P_PolyobjsBoxIterator(map->vbbox, drawSegsOfPolyobject, &data);
}

/**
 * Rotation in 2D. Used to rotate player arrow line character.
 */
static void rotate2D(float *x, float *y, angle_t a)
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
static void addLineCharacter(vectorgrap_t *vg, float x, float y, angle_t angle,
                             float scale, int color, float alpha,
                             blendmode_t blendmode)
{
    uint        i;
    float       v1[2], v2[2];

    for(i = 0; i < vg->count; ++i)
    {
        v1[VX] = vg->lines[i].a.pos[VX];
        v1[VY] = vg->lines[i].a.pos[VY];

        v1[VX] *= scale;
        v1[VY] *= scale;

        rotate2D(&v1[VX], &v1[VY], angle);

        v1[VX] += x;
        v1[VY] += y;

        v2[VX] = vg->lines[i].b.pos[VX];
        v2[VY] = vg->lines[i].b.pos[VY];

        v2[VX] *= scale;
        v2[VY] *= scale;

        rotate2D(&v2[VX], &v2[VY], angle);

        v2[VX] += x;
        v2[VY] += y;

        rendLine(v1[VX], v1[VY], v2[VX], v2[VY], AM_LINE_WIDTH,
                 color, alpha, blendmode);
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
    rotate2D(&pos[0][VX], &pos[0][VY], angle);
    pos[0][VX] += x;
    pos[0][VY] += y;

    pos[1][VX] = w / 2;
    pos[1][VY] = h / 2;
    rotate2D(&pos[1][VX], &pos[1][VY], angle);
    pos[1][VX] += x;
    pos[1][VY] += y;

    pos[2][VX] = w / 2;
    pos[2][VY] = -(h / 2);
    rotate2D(&pos[2][VX], &pos[2][VY], angle);
    pos[2][VX] += x;
    pos[2][VY] += y;

    pos[3][VX] = -(w / 2);
    pos[3][VY] = -(h / 2);
    rotate2D(&pos[3][VX], &pos[3][VY], angle);
    pos[3][VX] += x;
    pos[3][VY] += y;

    AM_AddQuad(CXMTOF(map, pos[0][VX]), CYMTOF(map, pos[0][VY]),
               CXMTOF(map, pos[1][VX]), CYMTOF(map, pos[1][VY]),
               CXMTOF(map, pos[2][VX]), CYMTOF(map, pos[2][VY]),
               CXMTOF(map, pos[3][VX]), CYMTOF(map, pos[3][VY]),
               0, 0, 1, 0,
               1, 1, 0, 1,
               r, g, b, a,
               lumpnum, true, BM_NORMAL);
}

/**
 * Draws all players on the map using a line character
 */
static void renderPlayers(void)
{
    int         i;
    float       size = PLAYERRADIUS;
    vectorgrap_t *vg;
    automap_t  *map = &automaps[mapviewplayer];

    vg = getVectorGraphic(map->vectorGraphicForPlayer);
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t   *p = &players[i];
        int         color;
        float       alpha;
        mobj_t     *mo;

        if(!p->plr->inGame)
            continue;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        if(deathmatch && p != &players[mapviewplayer])
            continue;
#endif

        color = their_colors[cfg.playerColor[i]];
        alpha = cfg.automapLineAlpha;

#if !__JHEXEN__
        if(p->powers[PT_INVISIBILITY])
            alpha *= 0.125;
#endif
        mo = p->plr->mo;
        if(!isPointVisible(map, mo->pos[VX], mo->pos[VY]))
            continue;

        /* $unifiedangles */
        addLineCharacter(vg, mo->pos[VX], mo->pos[VY], mo->angle, size,
                         color, alpha, BM_NORMAL);
    }
}

/**
 * Draws all things on the map
 */
static void renderThings(int color, int colorrange)
{
    uint        i;
    mobj_t     *iter;
    automap_t  *map = &automaps[mapviewplayer];
    float       size = PLAYERRADIUS;
    vectorgrap_t *vg = getVectorGraphic(VG_TRIANGLE);

    for(i = 0; i < numsectors; ++i)
    {
        for(iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); iter;
            iter = iter->sNext)
        {
            if(!isPointVisible(map, iter->pos[VX], iter->pos[VY]))
                continue;

            addLineCharacter(vg, iter->pos[VX], iter->pos[VY], iter->angle,
                             size, color, cfg.automapLineAlpha, BM_NORMAL);
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
    float       size = PLAYERRADIUS;
    automap_t  *map = &automaps[mapviewplayer];

    for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) th;
#if __JDOOM__ || __JDOOM64__
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

        if(!isPointVisible(map, mo->pos[VX], mo->pos[VY]))
            continue;

        addLineCharacter(getVectorGraphic(VG_KEYSQUARE),
                         mo->pos[VX], mo->pos[VY], 0, size,
                         keyColor, cfg.automapLineAlpha, BM_NORMAL);
    }
}

/**
 * Draws all the points marked by the player.
 */
static void drawMarks(void)
{
#if !__JDOOM64__
    int         i;
    float       x, y, w, h;
    angle_t     angle;
    automap_t  *map = &automaps[mapviewplayer];
    player_t   *plr = &players[mapviewplayer];
    dpatch_t   *patch;

    for(i = 0; i < NUMMARKPOINTS; ++i)
    {
        if(map->markpointsUsed[i])
        {
            patch = &markerPatches[i];

            w = (float) patch->width;
            h = (float) patch->height;

            x = map->markpoints[i].pos[VX];
            y = map->markpoints[i].pos[VY];

            if(!isPointVisible(map, x, y))
                continue;

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
static void setWinPos(automap_t *map)
{
    int         winX, winY, winW, winH;

    if(!map)
        return;

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

    setWindowTarget(map, winX, winY, winW, winH);
}
#endif

/**
 * Sets up the state for automap drawing.
 */
static void setupGLStateForMap(void)
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
        setWinPos(map);
*/
    // check for scissor box (to clip the map lines and stuff).
    // Store the old scissor state.
    DGL_GetIntegerv(DGL_SCISSOR_TEST, scissorState);
    DGL_GetIntegerv(DGL_SCISSOR_BOX, scissorState + 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, scrwidth, scrheight, -1, 1);

    // Do we want a background texture?
    if(maplumpnum)
    {
        // Apply the background texture onto a parallaxing layer which
        // follows the map view target (not player).
        DGL_Enable(DGL_TEXTURING);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PushMatrix();
        DGL_LoadIdentity();

         // We only want the left portion.
        GL_SetRawImage(maplumpnum, false, DGL_REPEAT, DGL_REPEAT);

        GL_SetColorAndAlpha(map->cfg.backgroundRGBA[0],
                            map->cfg.backgroundRGBA[1],
                            map->cfg.backgroundRGBA[2],
                            map->alpha * map->cfg.backgroundRGBA[3]);

        // Scale from texture to window space
        DGL_Translatef(win->x, win->y, 0);

        // Apply the parallax scrolling, map rotation and counteract the
        // aspect of the quad (sized to map window dimensions).
        DGL_Translatef(MTOF(map, map->viewPLX) + .5f,
                      MTOF(map, map->viewPLY) + .5f, 0);
        DGL_Rotatef(map->angle, 0, 0, 1);
        DGL_Scalef(1, win->height / win->width, 1);
        DGL_Translatef(-(.5f), -(.5f), 0);

        DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 1);
        DGL_Vertex2f(win->x, win->y);
        DGL_TexCoord2f(1, 1);
        DGL_Vertex2f(win->x + win->width, win->y);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex2f(win->x + win->width, win->y + win->height);
        DGL_TexCoord2f(0, 0);
        DGL_Vertex2f(win->x, win->y + win->height);
        DGL_End();

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();

        DGL_MatrixMode(DGL_PROJECTION);
    }
    else
    {
        // Nope just a solid color.
        GL_SetNoTexture();
        GL_DrawRect(win->x, win->y, win->width, win->height,
                    map->cfg.backgroundRGBA[0],
                    map->cfg.backgroundRGBA[1],
                    map->cfg.backgroundRGBA[2],
                    map->alpha * map->cfg.backgroundRGBA[3]);
    }

    // How about an outside border?
/*    if(win->width < srcwidth || win->height < scrheight)
    {
        DGL_Begin(DGL_LINES);
        DGL_Color4f(0.5f, 1, 0.5f, map->alpha - (1 - (cfg.automapLineAlpha /2)) );

        if(win->height < srcwidth)
        {
            DGL_Vertex2f(win->x + win->width + 1, win->y - 1);
            DGL_Vertex2f(win->x + win->width + 1, win->y + win->height + 1);

            DGL_Vertex2f(win->x - 1, win->y + win->height + 1);
            DGL_Vertex2f(win->x - 1, win->y - 1);
        }
        if(cfg.automapHeight != 1)
        {
            DGL_Vertex2f(win->x - 1, win->y - 1);
            DGL_Vertex2f(win->X + win->width + 1, win->y - 1);

            DGL_Vertex2f(win->x + win->width + 1, win->y + win->height + 1);
            DGL_Vertex2f(win->x - 1, win->y + win->height + 1);
        }
        DGL_End();
    }*/

#if __JDOOM64__
    // jd64 > Laser artifacts
    // If drawn in HUD we don't need them visible in the map too.
    if(!cfg.hudShown[HUD_POWER])
    {
        int                 i, num;

        num = 0;
        for(i = 0; i < NUMARTIFACTS; ++i)
            if(plr->artifacts[i])
                num++;

        if(num > 0)
        {
            float               x, y, spacing, scale, iconAlpha;
            spriteinfo_t        sprInfo;
            int                 artifactSprites[NUMARTIFACTS] = {
                SPR_POW1, SPR_POW2, SPR_POW3
            };

            iconAlpha = map->alpha;
            CLAMP(iconAlpha, 0.0f, 0.5f);

            spacing = win->height / num;

            x = win->width;
            y = 0;

            for(i = 0; i < NUMARTIFACTS; ++i)
            {
                if(plr->artifacts[i])
                {
                    R_GetSpriteInfo(artifactSprites[i], 0, &sprInfo);

                    scale = win->height / (sprInfo.height * num);
                    DGL_Color4f(1, 1, 1, iconAlpha);
                    GL_DrawPSprite(x - sprInfo.width * scale, y, scale, false,
                                   sprInfo.idx);
                    y += spacing;
                }
            }
        }
    }
    // < d64tc
#endif

    // Setup the scissor clipper.
    DGL_Scissor(win->x, win->y, win->width, win->height);
    DGL_Enable(DGL_SCISSOR_TEST);

    // Rotate map?
    DGL_Translatef(win->x + (win->width / 2.0f),
                  win->y + (win->height / 2.0f), 0);
    DGL_Rotatef(map->angle, 0, 0, 1);
    DGL_Translatef(-(win->x + (win->width / 2.0f)),
                  -(win->y + (win->height /2.0f)), 0);
}

/**
 * Restores the previous gl draw state
 */
static void restoreGLStateFromMap(void)
{
    // Return to the normal GL state.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if(!scissorState[0])
        DGL_Disable(DGL_SCISSOR_TEST);
    DGL_Scissor(scissorState[1], scissorState[2], scissorState[3],
               scissorState[4]);
}

/**
 * Draws the level name into the automap window
 */
static void drawLevelName(void)
{
    float               x, y, otherY;
    char               *lname;
    automap_t          *map = &automaps[mapviewplayer];
    automapwindow_t    *win = &map->window;
    int                 lumpNum = -1;
#if __JDOOM__ || __JDOOM64__
    int                 mapNum;
#endif

    lname = P_GetMapNiceName();

    if(lname)
    {
        // Compose the mapnumber used to check the map name patches array.
#if __JDOOM64__
        mapNum = gameMap -1;
        lumpNum = levelNamePatches[mapNum].lump;
#elif __JDOOM__
        if(gameMode == commercial)
            mapNum = gameMap -1;
        else
            mapNum = ((gameEpisode -1) * 9) + gameMap -1;

        lumpNum = levelNamePatches[mapNum].lump;
#endif

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PushMatrix();
        DGL_LoadIdentity();
        DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

        x = SCREENXTOFIXX(win->x + (win->width * .5f));
        y = SCREENYTOFIXY(win->y + win->height);
        if(cfg.setBlocks < 13)
        {
#if !__JDOOM64__
            if(cfg.setBlocks == 12)
#endif
            {   // We may need to adjust for the height of the HUD icons.
                otherY = y;
                otherY += -(y * (cfg.hudScale / 10.0f));

                if(y > otherY)
                    y = otherY;
            }
#if !__JDOOM64__
            else if(cfg.setBlocks <= 11 || cfg.automapHudDisplay == 2)
            {   // We may need to adjust for the height of the statusbar
                otherY = ST_Y;
                otherY += ST_HEIGHT * (1 - (cfg.statusbarScale / 20.0f));

                if(y > otherY)
                    y = otherY;
            }
#endif
        }

        Draw_BeginZoom(.4f, x, y);
        y -= 24; // border
        WI_DrawPatch(x, y, 1, 1, 1, map->alpha, lumpNum,
                     lname, false, ALIGN_CENTER);
        Draw_EndZoom();

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();
    }
}

static void renderVertexes(void)
{
    uint        i;
    automap_t  *map = &automaps[mapviewplayer];
    float       v[2];

    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, v);

        if(!isPointVisible(map, v[VX], v[VY]))
            continue;

        addPatchQuad(v[VX], v[VY], FIXXTOSCREENX(.75f) * map->scaleFTOM,
             FIXYTOSCREENY(.75f) * map->scaleFTOM, 0,
             0, .2f, .5f, 1, 1);
    }
}

/**
 * Render the automap view window for the specified player.
 */
void AM_Drawer(int viewplayer)
{
    automap_t *map;

    if(IS_DEDICATED)
        return; // Nothing to do.

    if(viewplayer < 0 || viewplayer >= MAXPLAYERS ||
       !players[viewplayer].plr->inGame)
        return;

    mapviewplayer = viewplayer;
    map = &automaps[mapviewplayer];

    if(!(map->alpha > 0))
        return;

    // Setup for frame.
    setupGLStateForMap();

    // Freeze the lists if the map is fading out from being open
    // or if set to frozen for debug.
    if(!(freezeMapRLs || !map->active))
    {
        AM_ClearAllLists(false);

        // Draw.
        renderWalls();

        if(map->flags & AMF_REND_VERTEXES)
            renderVertexes();

        renderPlayers();

        if(map->flags & AMF_REND_THINGS)
            renderThings(THINGCOLORS, THINGRANGE);

        if(map->flags & AMF_REND_KEYS)
            renderKeys();

        // Draw any marked points.
        drawMarks();
    }
    AM_RenderAllLists(map->alpha);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    drawLevelName();
    restoreGLStateFromMap();
}

// ------------------------------------------------------------------------------
// Automap Menu
//-------------------------------------------------------------------------------

menuitem_t MAPItems[] = {
/*
    {ITT_LRFUNC, 0, "window position : ",       M_MapPosition, 0 },
    {ITT_LRFUNC, 0, "window width :       ",    M_MapWidth, 0 },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "window height :     ",     M_MapHeight, 0 },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
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
#if __JDOOM__ || __JDOOM64__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
    {ITT_EFUNC, 0, "door colors :        ",     M_MapDoorColors, 0 },
    {ITT_LRFUNC, 0, "door glow : ",             M_MapDoorGlow, 0 },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "line alpha :          ",   M_MapLineAlpha, 0 },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
    {ITT_EMPTY, 0, NULL,                        NULL, 0 },
#endif
};

menu_t MapDef = {
    0,
#if __JDOOM__ || __JDOOM64__
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
    huFontA,
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
    float           menuAlpha;
    const menu_t *menu = &MapDef;
    static char *hudviewnames[3] = { "NONE", "CURRENT", "STATUSBAR" };
    static char *yesno[2] = { "NO", "YES" };
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __WOLFTC__
    static char *countnames[4] = { "NO", "YES", "PERCENT", "COUNT+PCNT" };
#endif

    menuAlpha = Hu_MenuAlpha();
    M_DrawTitle("Automap OPTIONS", menu->y - 26);

#if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, 0, hudviewnames[cfg.automapHudDisplay]);
    M_WriteMenuText(menu, 1, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 2, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 3, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    MN_DrawColorBox(menu, 5, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menuAlpha);
    MN_DrawColorBox(menu, 6, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menuAlpha);
    MN_DrawColorBox(menu, 7, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menuAlpha);
    MN_DrawColorBox(menu, 8, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menuAlpha);
    MN_DrawColorBox(menu, 9, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menuAlpha);
    M_WriteMenuText(menu, 11, yesno[cfg.automapShowDoors]);
    MN_DrawSlider(menu, 12, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    MN_DrawSlider(menu, 13, 11, cfg.automapLineAlpha * 10 + .5f);
#else
    M_WriteMenuText(menu, 0, hudviewnames[cfg.automapHudDisplay]);
# if __JHERETIC__
    M_WriteMenuText(menu, 1, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    M_WriteMenuText(menu, 2, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    M_WriteMenuText(menu, 3, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
    MN_DrawColorBox(menu, 5, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menuAlpha);
    MN_DrawColorBox(menu, 6, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menuAlpha);
    MN_DrawColorBox(menu, 7, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menuAlpha);
    MN_DrawColorBox(menu, 8, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menuAlpha);
    MN_DrawColorBox(menu, 9, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menuAlpha);
    M_WriteMenuText(menu, 10, yesno[cfg.automapShowDoors]);
    MN_DrawSlider(menu, 12, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
    MN_DrawSlider(menu, 15, 11, cfg.automapLineAlpha * 10 + .5f);
# else
    MN_DrawColorBox(menu, 2, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], menuAlpha);
    MN_DrawColorBox(menu, 3, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], menuAlpha);
    MN_DrawColorBox(menu, 4, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], menuAlpha);
    MN_DrawColorBox(menu, 5, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], menuAlpha);
    MN_DrawColorBox(menu, 6, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], menuAlpha);
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

    setViewRotateMode(&automaps[CONSOLEPLAYER], cfg.automapRotate);
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
        automap_t   *map = &automaps[CONSOLEPLAYER];

        if(map->active) // Is open, now close it.
        {
            viewActive = true;

            // Disable the automap binding classes
            //DD_SetBindClass(GBC_CLASS1, false);
            //if(map->panMode)
            //    DD_SetBindClass(GBC_CLASS2, false);

            DD_Execute(true, "deactivatebclass map");
            DD_Execute(true, "deactivatebclass map-freepan");

            AM_Stop(CONSOLEPLAYER);
        }
        else // Is closed, now open it.
        {
            AM_Start(CONSOLEPLAYER);
            // Enable/disable the automap binding classes
            //DD_SetBindClass(GBC_CLASS1, true);
            //if(map->panMode)
            //    DD_SetBindClass(GBC_CLASS2, true);

            DD_Execute(true, "activatebclass map");
            if(map->panMode)
                DD_Execute(true, "activatebclass map-freepan");

            viewActive = false;
        }

        return true;
    }
    else if(!stricmp(argv[0], "follow"))  // follow mode toggle
    {
        player_t    *plr = &players[CONSOLEPLAYER];
        automap_t   *map = &automaps[CONSOLEPLAYER];

        if(map->active)
        {
            map->panMode = !map->panMode;

            // Enable/disable the pan mode binding class
            //DD_SetBindClass(GBC_CLASS2, map->panMode);
            DD_Executef(true, "%sactivatebclass map-freepan", !map->panMode? "de" : "");

            P_SetMessage(plr, (map->panMode ? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON), false);
            Con_Printf("Follow mode toggle.\n");
            return true;
        }
    }
    else if(!stricmp(argv[0], "rotate"))  // rotate mode toggle
    {
        player_t    *plr = &players[CONSOLEPLAYER];
        automap_t   *map = &automaps[CONSOLEPLAYER];

        if(map->active)
        {
            cfg.automapRotate = !cfg.automapRotate;
            setViewRotateMode(map, cfg.automapRotate);

            P_SetMessage(plr, (map->rotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF),
                         false);
            Con_Printf("Rotate mode toggle.\n");
            return true;
        }
    }
    else if(!stricmp(argv[0], "zoommax"))  // max zoom
    {
        player_t    *plr = &players[CONSOLEPLAYER];
        automap_t   *map = &automaps[CONSOLEPLAYER];

        if(map->active)
        {
            // When switching to max scale mode, store the old scale.
            if(!map->maxScale)
                map->priorToMaxScale = map->viewScale;

            map->maxScale = !map->maxScale;
            setViewScaleTarget(map, (map->maxScale? 0 : map->priorToMaxScale));

            Con_Printf("Maximum zoom %s in automap.\n", map->maxScale? "ON":"OFF");
            return true;
        }
    }
    else if(!stricmp(argv[0], "addmark"))  // add a mark
    {
        player_t   *plr = &players[CONSOLEPLAYER];
        automap_t  *map = &automaps[CONSOLEPLAYER];
        uint        num;

        if(map->active)
        {
            num = addMark(map, plr->plr->mo->pos[VX],
                               plr->plr->mo->pos[VY]);
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
        player_t    *plr = &players[CONSOLEPLAYER];
        automap_t   *map = &automaps[CONSOLEPLAYER];

        if(map->active)
        {
            clearMarks(map);

            P_SetMessage(plr, AMSTR_MARKSCLEARED, false);
            Con_Printf("All markers cleared on automap.\n");
            return true;
        }
    }

    return false;
}
