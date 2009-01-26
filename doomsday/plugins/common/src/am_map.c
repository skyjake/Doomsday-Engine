/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * am_map.c : Automap, automap menu and related code.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#if __JDOOM__
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

#define AM_LINE_WIDTH       (1.25f)

#define LERP(start, end, pos) (end * pos + start * (1 - pos))

typedef struct mpoint_s {
    float               pos[3];
} mpoint_t;

typedef struct mline_s {
    mpoint_t            a, b;
} vgline_t;

#define R (1.0f)

vgline_t keysquare[] = {
    {{0, 0}, {R / 4, -R / 2}},
    {{R / 4, -R / 2}, {R / 2, -R / 2}},
    {{R / 2, -R / 2}, {R / 2, R / 2}},
    {{R / 2, R / 2}, {R / 4, R / 2}},
    {{R / 4, R / 2}, {0, 0}}, // Handle part type thing.
    {{0, 0}, {-R, 0}}, // Stem.
    {{-R, 0}, {-R, -R / 2}}, // End lockpick part.
    {{-3 * R / 4, 0}, {-3 * R / 4, -R / 4}}
};

vgline_t thintriangle_guy[] = {
    {{-R / 2, R - R / 2}, {R, 0}}, // >
    {{R, 0}, {-R / 2, -R + R / 2}},
    {{-R / 2, -R + R / 2}, {-R / 2, R - R / 2}} // |>
};

#if __JDOOM__ || __JDOOM64__
vgline_t player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}}, // -----
    {{R, 0}, {R - R / 2, R / 4}}, // ----->
    {{R, 0}, {R - R / 2, -R / 4}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 4}}, // >---->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 4}}, // >>--->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4}}
};

vgline_t cheat_player_arrow[] = {
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
vgline_t player_arrow[] = {
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

vgline_t cheat_player_arrow[] = {
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
vgline_t player_arrow[] = {
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

// Translates between frame-buffer and map distances
#define FTOM(map, x) ((x) * (map)->scaleFTOM)
#define MTOF(map, x) ((x) * (map)->scaleMTOF)

#define AM_MAXSPECIALLINES      32

// TYPES -------------------------------------------------------------------

typedef struct mapobjectinfo_s {
    float           rgba[4];
    int             blendMode;
    float           glowAlpha, glowWidth;
    boolean         glow;
    boolean         scaleWithView;
} mapobjectinfo_t;

typedef struct automapwindow_s {
    // Where the window currently is on screen, and the dimensions.
    float           x, y, width, height;

    // Where the window should be on screen, and the dimensions.
    int             targetX, targetY, targetWidth, targetHeight;
    int             oldX, oldY, oldWidth, oldHeight;

    float           posTimer;
} automapwindow_t;

enum {
    MOL_LINEDEF = 0,
    MOL_LINEDEF_TWOSIDED,
    MOL_LINEDEF_FLOOR,
    MOL_LINEDEF_CEILING,
    MOL_LINEDEF_UNSEEN,
    NUM_MAP_OBJECTLISTS
};

typedef struct automapcfg_s {
    float           lineGlowScale;
    boolean         glowingLineSpecials;
    float           backgroundRGBA[4];
    float           panSpeed;
    boolean         panResetOnOpen;
    float           zoomSpeed;

    mapobjectinfo_t mapObjectInfo[NUM_MAP_OBJECTLISTS];
} automapcfg_t;

#define AMF_REND_THINGS         0x01
#define AMF_REND_KEYS           0x02
#define AMF_REND_ALLLINES       0x04
#define AMF_REND_XGLINES        0x08
#define AMF_REND_VERTEXES       0x10
#define AMF_REND_LINE_NORMALS   0x20

typedef struct automapspecialline_s {
    int             special;
    int             sided;
    int             cheatLevel; // minimum cheat level for this special.
    mapobjectinfo_t info;
} automapspecialline_t;

typedef struct automap_s {
// State
    int             flags;
    boolean         active;

    boolean         fullScreenMode; // If the map is currently in fullscreen mode.
    boolean         panMode; // If the map viewer location is currently in free pan mode.
    boolean         rotate;
    uint            followPlayer; // Player id of that to follow.

    boolean         maxScale; // If the map is currently in forced max zoom mode.
    float           priorToMaxScale; // Viewer scale before entering maxScale mode.

    // Used by MTOF to scale from map-to-frame-buffer coords.
    float           scaleMTOF;
    // Used by FTOM to scale from frame-buffer-to-map coords (=1/scaleMTOF).
    float           scaleFTOM;

// Paramaters for render.
    float           alpha;
    float           targetAlpha;

    automapcfg_t cfg;

    automapspecialline_t specialLines[AM_MAXSPECIALLINES];
    uint            numSpecialLines;

    vectorgrapname_t vectorGraphicForPlayer;
    int             scissorState[5];

// DGL Display lists.
    DGLuint         lists[NUM_MAP_OBJECTLISTS]; // Each list contains one or more of given type of automap obj.
    boolean         constructMap; // @c true = force a rebuild of all lists.

// Automap window (screen space).
    automapwindow_t window;

// Viewer location on the map.
    float           viewTimer;
    float           viewX, viewY; // Current.
    float           targetViewX, targetViewY; // Should be at.
    float           oldViewX, oldViewY; // Previous.
    // For the parallax layer.
    float           viewPLX, viewPLY; // Current.

// Viewer frame scale.
    float           viewScaleTimer;
    float           viewScale; // Current.
    float           targetViewScale; // Should be at.
    float           oldViewScale; // Previous.

    float           minScaleMTOF; // Viewer frame scale limits.
    float           maxScaleMTOF; //

// Viewer frame rotation.
    float           angleTimer;
    float           angle; // Current.
    float           targetAngle; // Should be at.
    float           oldAngle; // Previous.

// Viewer frame coordinates on map.
    float           vframe[2][2]; // {TL{x,y}, BR{x,y}}

    float           vbbox[4]; // Clip bbox coordinates on map.

// Misc
    int             cheating;
    boolean         revealed;

    // Marked map points.
    mpoint_t        markpoints[NUMMARKPOINTS];
    boolean         markpointsUsed[NUMMARKPOINTS];
    int             markpointnum; // next point to be assigned.
} automap_t;

typedef struct vectorgrap_s {
    uint            count;
    vgline_t*       lines;
} vectorgrap_t;

typedef struct {
    player_t*       plr;
    automap_t*      map;
    int             objType;
} ssecitervars_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Menu routines.
void            M_DrawMapMenu(void);
void            M_MapPosition(int option, void* data);
void            M_MapWidth(int option, void* data);
void            M_MapHeight(int option, void* data);
void            M_MapLineAlpha(int option, void* data);
void            M_MapDoorColors(int option, void* data);
void            M_MapDoorGlow(int option, void* data);
void            M_MapRotate(int option, void* data);
void            M_MapStatusbar(int option, void* data);
void            M_MapKills(int option, void* data);
void            M_MapItems(int option, void* data);
void            M_MapSecrets(int option, void* data);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     setWindowFullScreenMode(automap_t* map, int value);
static void     setViewTarget(automap_t* map, float x, float y, boolean fast);
static void     setViewScaleTarget(automap_t* map, float scale);
static void     setViewAngleTarget(automap_t* map, float angle, boolean fast);
static void     setViewRotateMode(automap_t* map, boolean on);
static void     clearMarks(automap_t* map);

static void     compileObjectLists(automap_t* map);
static void     findMinMaxBoundaries(void);
static void     renderMapName(void);
static void     rotate2D(float* x, float* y, angle_t a);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean viewActive;

extern int numTexUnits;
extern boolean envModAdd;
extern DGLuint amMaskTexture;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int mapviewplayer;

cvar_t mapCVars[] = {
    {"map-alpha-lines", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static automap_t automaps[MAXPLAYERS];

static vectorgrap_t* vectorGraphs[NUM_VECTOR_GRAPHS];

// if -1 no background image will be drawn.
#if __JDOOM__ || __JDOOM64__
static int autopageLumpNum = -1;
#elif __JHERETIC__
static int autopageLumpNum = 1;
#else
static int autopageLumpNum = 1;
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
    KEY3_COLOR,
    KEY2_COLOR,
    BLOODRED,
    KEY1_COLOR
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

static int scrwidth = 0; // Real screen dimensions.
static int scrheight = 0;

// Map bounds.
static float bounds[2][2]; // {TL{x,y}, BR{x,y}}

static dpatch_t markerPatches[10]; // Numbers used for marking by the automap (lump indices).

// CODE --------------------------------------------------------------------

static __inline automap_t* mapForPlayerId(int id)
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

static vectorgrap_t* getVectorGraphic(uint idx)
{
    vectorgrap_t*       vg;
    vgline_t*           lines;

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

static mapobjectinfo_t* getMapObjectInfo(automap_t* map, int objectname)
{
    if(objectname == AMO_NONE)
        return NULL;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("getMapObjectInfo: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_UNSEENLINE:
        return &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];

    case AMO_SINGLESIDEDLINE:
        return &map->cfg.mapObjectInfo[MOL_LINEDEF];

    case AMO_TWOSIDEDLINE:
        return &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];

    case AMO_FLOORCHANGELINE:
        return &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];

    case AMO_CEILINGCHANGELINE:
        return &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];

    default:
        Con_Error("getMapObjectInfo: No info for object %i.", objectname);
    }

    return NULL;
}

static mapobjectinfo_t* getInfoForSpecialLine(automap_t* map, int special,
                                              sector_t* frontsector,
                                              sector_t* backsector)
{
    uint                i;
    mapobjectinfo_t*    info = NULL;

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
        uint                j;
        automap_t*          map = &automaps[i];

        // Initialize.
        // \fixme Put these values into an array (or read from a lump?).
        map->followPlayer = i;
        map->oldViewScale = 1;
        map->window.oldX = map->window.x = 0;
        map->window.oldY = map->window.y = 0;
        map->window.oldWidth = map->window.width = scrwidth;
        map->window.oldHeight = map->window.height = scrheight;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].glow = NO_GLOW;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].glowAlpha = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].glowWidth = 10;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].blendMode = BM_NORMAL;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].scaleWithView = false;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[0] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[1] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[2] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[3] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF].glow = NO_GLOW;
        map->cfg.mapObjectInfo[MOL_LINEDEF].glowAlpha = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF].glowWidth = 10;
        map->cfg.mapObjectInfo[MOL_LINEDEF].blendMode = BM_NORMAL;
        map->cfg.mapObjectInfo[MOL_LINEDEF].scaleWithView = false;
        map->cfg.mapObjectInfo[MOL_LINEDEF].rgba[0] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF].rgba[1] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF].rgba[2] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF].rgba[3] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].glow = NO_GLOW;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowAlpha = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowWidth = 10;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].blendMode = BM_NORMAL;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].scaleWithView = false;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[0] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[1] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[2] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[3] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].glow = NO_GLOW;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].glowAlpha = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].glowWidth = 10;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].blendMode = BM_NORMAL;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].scaleWithView = false;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[0] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[1] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[2] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[3] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].glow = NO_GLOW;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].glowAlpha = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].glowWidth = 10;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].blendMode = BM_NORMAL;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].scaleWithView = false;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].rgba[0] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].rgba[1] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].rgba[2] = 1;
        map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING].rgba[3] = 1;

        for(j = 0; j < NUM_MAP_OBJECTLISTS; ++j)
            map->lists[j] = 0;
        map->constructMap = true;

        // Register lines we want to display in a special way.
#if __JDOOM__ || __JDOOM64__
        // Blue locked door, open.
        AM_RegisterSpecialLine(i, 0, 32, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Blue locked door, locked.
        AM_RegisterSpecialLine(i, 0, 26, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 99, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 133, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Red locked door, open.
        AM_RegisterSpecialLine(i, 0, 33, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Red locked door, locked.
        AM_RegisterSpecialLine(i, 0, 28, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 134, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 135, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow locked door, open.
        AM_RegisterSpecialLine(i, 0, 34, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow locked door, locked.
        AM_RegisterSpecialLine(i, 0, 27, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 136, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 137, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Exit switch.
        AM_RegisterSpecialLine(i, 1, 11, 1, 0, 1, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Exit cross line.
        AM_RegisterSpecialLine(i, 1, 52, 2, 0, 1, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Secret Exit switch.
        AM_RegisterSpecialLine(i, 1, 51, 1, 0, 1, 1, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Secret Exit cross line.
        AM_RegisterSpecialLine(i, 2, 124, 2, 0, 1, 1, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#elif __JHERETIC__
        // Blue locked door.
        AM_RegisterSpecialLine(i, 0, 26, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Blue switch?
        AM_RegisterSpecialLine(i, 0, 32, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow locked door.
        AM_RegisterSpecialLine(i, 0, 27, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow switch?
        AM_RegisterSpecialLine(i, 0, 34, 0, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Green locked door.
        AM_RegisterSpecialLine(i, 0, 28, 2, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Green switch?
        AM_RegisterSpecialLine(i, 0, 33, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#elif __JHEXEN__
        // A locked door (all are green).
        AM_RegisterSpecialLine(i, 0, 13, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 83, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Intra-map teleporters (all are blue).
        AM_RegisterSpecialLine(i, 0, 70, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        AM_RegisterSpecialLine(i, 0, 71, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Inter-map teleport.
        AM_RegisterSpecialLine(i, 0, 74, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Game-winning exit.
        AM_RegisterSpecialLine(i, 0, 75, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
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
                            cfg.automapL0[2], 1);
        AM_SetColorAndAlpha(i, AMO_SINGLESIDEDLINE,
                            cfg.automapL1[0], cfg.automapL1[1],
                            cfg.automapL1[2], 1);
        AM_SetColorAndAlpha(i, AMO_TWOSIDEDLINE,
                            cfg.automapL0[0], cfg.automapL0[1],
                            cfg.automapL0[2], 1);
        AM_SetColorAndAlpha(i, AMO_FLOORCHANGELINE,
                            cfg.automapL2[0], cfg.automapL2[1],
                            cfg.automapL2[2], 1);
        AM_SetColorAndAlpha(i, AMO_CEILINGCHANGELINE,
                            cfg.automapL3[0], cfg.automapL3[1],
                            cfg.automapL3[2], 1);
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

static void calcViewScaleFactors(automap_t* map)
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
void AM_InitForMap(void)
{
    uint                i;

    if(IS_DEDICATED)
        return; // nothing to do.

    // Find the world boundary points shared by all maps.
    findMinMaxBoundaries();

    // Setup all players' maps.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        uint                j;
        automap_t*          map = &automaps[i];

        for(j = 0; j < NUM_MAP_OBJECTLISTS; ++j)
        {
            if(map->lists[j])
                DGL_DeleteLists(map->lists[j], 1);
            map->lists[j] = 0;
            map->constructMap = true;
        }

        map->revealed = false;

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
        // If the map has been left open; close it.
        AM_Open(i, false, true);

        // Reset position onto the follow player.
        if(players[map->followPlayer].plr->mo)
        {
            mobj_t* mo = players[map->followPlayer].plr->mo;
            setViewTarget(map, mo->pos[VX], mo->pos[VY], true);
        }
    }
}

/**
 * Start the automap.
 */
void AM_Open(int pnum, boolean yes, boolean fast)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // nothing to do.

    if(pnum < 0 || pnum >= MAXPLAYERS || !players[pnum].plr->inGame)
        return;

    if(G_GetGameState() != GS_MAP)
        return;

    map = &automaps[pnum];
    if(yes)
    {
        if(map->active)
            return; // Already active.

        DD_Execute(true, "activatebcontext map");
        if(map->panMode)
            DD_Execute(true, "activatebcontext map-freepan");

        viewActive = false;

        map->active = true;
        map->targetAlpha = 1;
        if(fast)
            map->alpha = 1;

        if(!players[map->followPlayer].plr->inGame)
        {   // Set viewer target to the center of the map.
            setViewTarget(map, (bounds[1][VX] - bounds[0][VX]) / 2,
                               (bounds[1][VY] - bounds[0][VY]) / 2, fast);
            setViewAngleTarget(map, 0, fast);
        }
        else
        {   // The map's target player is available.
            mobj_t*             mo = players[map->followPlayer].plr->mo;

            if(!(map->panMode && !map->cfg.panResetOnOpen))
                setViewTarget(map, mo->pos[VX], mo->pos[VY], fast);

            if(map->panMode && map->cfg.panResetOnOpen)
            {
                float               angle;

                /* $unifiedangles */
                if(map->rotate)
                    angle = mo->angle / (float) ANGLE_MAX * -360 - 90;
                else
                    angle = 0;
                setViewAngleTarget(map, angle, fast);
            }
        }
    }
    else
    {
        if(!map->active)
            return; // Already closed.

        map->active = false;
        map->targetAlpha = 0;
        if(fast)
            map->alpha = 0;

        viewActive = true;

        DD_Execute(true, "deactivatebcontext map");
        DD_Execute(true, "deactivatebcontext map-freepan");
    }
}

/**
 * Translates from map to automap window coordinates.
 */
float AM_MapToFrame(int pid, float val)
{
    automap_t*          map;

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
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_MapToFrame: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;

    return FTOM(map, val);
}

static void setWindowTarget(automap_t *map, int x, int y, int w, int h)
{
    automapwindow_t*    win;

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
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setWindowTarget(map, x, y, w, h);
}

void AM_GetWindow(int pid, float *x, float *y, float *w, float *h)
{
    automap_t*          map;

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
    automap_t*          map;

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
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_IsMapWindowInFullScreenMode: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return false;
    return map->fullScreenMode;
}

static void setViewTarget(automap_t* map, float x, float y, boolean fast)
{
    // Already at this target?
    if(x == map->targetViewX && y == map->targetViewY)
        return;

    x = MINMAX_OF(-32768, x, 32768);
    y = MINMAX_OF(-32768, y, 32768);

    if(fast)
    {   // Change instantly.
        map->viewX = map->oldViewX = map->targetViewX = x;
        map->viewY = map->oldViewY = map->targetViewY = y;
    }
    else
    {
        map->oldViewX = map->viewX;
        map->oldViewY = map->viewY;
        map->targetViewX = x;
        map->targetViewY = y;
        // Restart the timer.
        map->viewTimer = 0;
    }
}

void AM_SetViewTarget(int pid, float x, float y)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewTarget(map, x, y, false);
}

void AM_GetViewPosition(int pid, float* x, float* y)
{
    automap_t*          map;

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
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_ViewAngle: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;

    return map->angle;
}

static void setViewScaleTarget(automap_t* map, float scale)
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
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewScaleTarget(map, scale);
}

static void setViewAngleTarget(automap_t* map, float angle, boolean fast)
{
    // Already at this target?
    if(angle == map->targetAngle)
        return;

    if(fast)
    {   // Change instantly.
        map->angle = map->oldAngle = map->targetAngle = angle;
    }
    else
    {
        map->oldAngle = map->angle;
        map->targetAngle = angle;

        // Restart the timer.
        map->angleTimer = 0;
    }
}

void AM_SetViewAngleTarget(int pid, float angle)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    setViewAngleTarget(map, angle, false);
}

/**
 * @return              True if the specified map is currently active.
 */
boolean AM_IsMapActive(int pid)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return false;

    map = mapForPlayerId(pid);
    if(!map)
        return false;
    return map->active;
}

static void setViewRotateMode(automap_t* map, boolean on)
{
    map->rotate = on;
}

void AM_SetViewRotate(int pid, int offOnToggle)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(offOnToggle == 2)
        cfg.automapRotate = !cfg.automapRotate;
    else
        cfg.automapRotate = (offOnToggle? true : false);

    setViewRotateMode(map, cfg.automapRotate);

    P_SetMessage(&players[pid], (map->rotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF),
                 false);
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
    automap_t*          map;
    xline_t*            xline;

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(lineIdx >= numlines)
        return;

    xline = P_GetXLine(lineIdx);

    // Will we need to rebuild one or more display lists?
    if(xline->mapped[pid] != visible)
        map->constructMap = true;

    xline->mapped[pid] = visible;
}

/**
 * Reveal the whole map.
 */
void AM_RevealMap(int pid, boolean on)
{
    automap_t*          map;

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(map->revealed != on)
    {
        map->revealed = on;
        map->constructMap = true;
    }
}

static void clearMarks(automap_t* map)
{
    uint                i;

    for(i = 0; i < NUMMARKPOINTS; ++i)
        map->markpointsUsed[i] = false;
    map->markpointnum = 0;
}

/**
 * Clears markpoint array.
 */
void AM_ClearMarks(int pid)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    clearMarks(map);

    P_SetMessage(&players[pid], AMSTR_MARKSCLEARED, false);
    Con_Printf("All markers cleared on automap.\n");
}

/**
 * Adds a marker at the current location
 */
static int addMark(automap_t* map, float x, float y)
{
    int             num = map->markpointnum;

    map->markpoints[num].pos[VX] = x;
    map->markpoints[num].pos[VY] = y;
    map->markpoints[num].pos[VZ] = 0;
    map->markpointsUsed[num] = true;
    map->markpointnum = (map->markpointnum + 1) % NUMMARKPOINTS;

/*#if _DEBUG
Con_Message("Added mark point %i to map at X=%g Y=%g\n",
            num, map->markpoints[num].pos[VX],
            map->markpoints[num].pos[VY]);
#endif*/

    return num;
}

/**
 * Adds a marker at the current location
 */
int AM_AddMark(int pid, float x, float y)
{
    static char         buffer[20];
    automap_t*          map;
    int                 newMark;

    if(IS_DEDICATED)
        Con_Error("AM_AddMark: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return -1;

    newMark = addMark(map, x, y);
    if(newMark != -1)
    {
        sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newMark);
        P_SetMessage(&players[pid], buffer, false);
    }

    return newMark;
}

/**
 * Toggles between active and max zoom.
 */
void AM_ToggleZoomMax(int pid)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_ToggleZoomMax: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return;

    // When switching to max scale mode, store the old scale.
    if(!map->maxScale)
        map->priorToMaxScale = map->viewScale;

    map->maxScale = !map->maxScale;
    setViewScaleTarget(map, (map->maxScale? 0 : map->priorToMaxScale));

    Con_Printf("Maximum zoom %s in automap.\n", map->maxScale? "ON":"OFF");
}

/**
 * Toggles follow mode.
 */
void AM_ToggleFollow(int pid)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_ToggleFollow: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return;

    map->panMode = !map->panMode;

    // Enable/disable the pan mode binding class
    DD_Executef(true, "%sactivatebcontext map-freepan", !map->panMode? "de" : "");

    P_SetMessage(&players[pid],
                 (map->panMode ? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON), false);
}

/**
 * Set the alpha level of the automap. Alpha levels below one automatically
 * show the game view in addition to the automap.
 *
 * @param alpha         Alpha level to set the automap too (0...1)
 */
void AM_SetGlobalAlphaTarget(int pid, float alpha)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;
    map->targetAlpha = MINMAX_OF(0, alpha, 1);
}

/**
 * @return              Current alpha level of the automap.
 */
float AM_GlobalAlpha(int pid)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_GlobalAlpha: Not available in dedicated mode.");

    map = mapForPlayerId(pid);
    if(!map)
        return 0;
    return map->alpha;
}

void AM_SetColor(int pid, int objectname, float r, float g, float b)
{
    int                 i;
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(objectname == AMO_NONE)
        return; // Ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColor: Unknown object %i.", objectname);

    r = MINMAX_OF(0, r, 1);
    g = MINMAX_OF(0, g, 1);
    b = MINMAX_OF(0, b, 1);

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
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_SetColor: Object %i does not use color.",
                  objectname);
        break;
    }

    info->rgba[0] = r;
    info->rgba[1] = g;
    info->rgba[2] = b;

    // We will need to rebuild one or more display lists.
    for(i = 0; i < MAXPLAYERS; ++i)
        automaps[i].constructMap = true;
}

void AM_GetColor(int pid, int objectname, float* r, float* g, float* b)
{
    automap_t*          map;
    mapobjectinfo_t*    info;

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
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];
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
    int                 i;
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColorAndAlpha: Unknown object %i.", objectname);

    r = MINMAX_OF(0, r, 1);
    g = MINMAX_OF(0, g, 1);
    b = MINMAX_OF(0, b, 1);
    a = MINMAX_OF(0, a, 1);

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
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];
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

    // We will need to rebuild one or more display lists.
    for(i = 0; i < MAXPLAYERS; ++i)
        automaps[i].constructMap = true;
}

void AM_GetColorAndAlpha(int pid, int objectname, float* r, float* g,
                         float* b, float* a)
{
    automap_t*          map;
    mapobjectinfo_t*    info;

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
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];
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
    uint                i;
    automap_t*          map;
    mapobjectinfo_t*    info;

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
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_SetBlendmode: Object %i does not support blending modes.",
                  objectname);
        break;
    }

    info->blendMode = blendmode;

    // We will need to rebuild one or more display lists.
    for(i = 0; i < MAXPLAYERS; ++i)
        automaps[i].constructMap = true;
}

void AM_SetGlow(int pid, int objectname, glowtype_t type, float size,
                float alpha, boolean canScale)
{
    uint                i;
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    map = mapForPlayerId(pid);
    if(!map)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetGlow: Unknown object %i.", objectname);

    size = MINMAX_OF(0, size, 100);
    alpha = MINMAX_OF(0, alpha, 1);

    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &map->cfg.mapObjectInfo[MOL_LINEDEF_CEILING];
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

    // We will need to rebuild one or more display lists.
    for(i = 0; i < MAXPLAYERS; ++i)
        automaps[i].constructMap = true;
}

void AM_SetVectorGraphic(int pid, int objectname, int vgname)
{
    automap_t*          map;

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
                            int sided, float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView)
{
    uint                i;
    automap_t*          map;
    automapspecialline_t* line, *p;

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

    line->info.rgba[0] = MINMAX_OF(0, r, 1);
    line->info.rgba[1] = MINMAX_OF(0, g, 1);
    line->info.rgba[2] = MINMAX_OF(0, b, 1);
    line->info.rgba[3] = MINMAX_OF(0, a, 1);
    line->info.glow = glowType;
    line->info.glowAlpha = MINMAX_OF(0, glowAlpha, 1);
    line->info.glowWidth = glowWidth;
    line->info.scaleWithView = scaleGlowWithView;
    line->info.blendMode = blendmode;

    // We will need to rebuild one or more display lists.
    map->constructMap = true;
}

void AM_SetCheatLevel(int pid, int level)
{
    automap_t*          map;

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

    // We will need to rebuild one or more display lists.
    map->constructMap = true;
}

void AM_IncMapCheatLevel(int pid)
{
    automap_t*          map;

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

    // We will need to rebuild one or more display lists.
    map->constructMap = true;
}

/**
 * Determines bounding box of all the map's vertexes.
 */
static void findMinMaxBoundaries(void)
{
    uint                i;
    float               pos[2];

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
    int                 i;
    char                namebuf[9];
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

    if(autopageLumpNum != -1)
    {
        if((autopageLumpNum = W_CheckNumForName("AUTOPAGE")) == -1)
        {
#if __JHERETIC__ || __JHEXEN__
            Con_SetFloat("map-background-r", .55f, false);
            Con_SetFloat("map-background-g", .45f, false);
            Con_SetFloat("map-background-b", .35f, false);
            Con_SetFloat("map-background-a", 1, false);
#endif
        }
    }

    if(numTexUnits > 1)
    {   // Great, we can replicate the map fade out effect using multitexture,
        // load the mask texture.
        if(!amMaskTexture && !Get(DD_NOVIDEO))
        {
            amMaskTexture =
                GL_NewTextureWithParams3(DGL_LUMINANCE, 256, 256,
                                         W_CacheLumpName("mapmask", PU_CACHE),
                                         0x8, DGL_NEAREST, DGL_LINEAR,
                                         0 /*no anisotropy*/,
                                         DGL_REPEAT, DGL_REPEAT);
        }
    }
}

/**
 * Unload any resources needed for the automap.
 * Called during shutdown and before a renderer restart.
 */
void AM_UnloadData(void)
{
    int                 i;

    if(Get(DD_NOVIDEO) || IS_DEDICATED)
        return; // Nothing to do.

    // Destroy all display lists.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        uint                j;
        player_t*           pl = &players[i];
        automap_t*          map = &automaps[i];

        if(!((pl->plr->flags & DDPF_LOCAL) && pl->plr->inGame))
            continue;

        for(j = 0; j < NUM_MAP_OBJECTLISTS; ++j)
        {
            if(map->lists[j])
                DGL_DeleteLists(map->lists[j], 1);
            map->lists[j] = 0;
            map->constructMap = true;
        }
    }

    if(amMaskTexture)
        DGL_DeleteTextures(1, (DGLuint*) &amMaskTexture);
    amMaskTexture = 0;
}

/**
 * Animates an automap view window towards the target values.
 */
static void mapWindowTicker(automap_t* map)
{
    float               newX, newY, newWidth, newHeight;
    automapwindow_t*    win;

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
       newWidth != win->width || newHeight != win->height)
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

static void addToBoxf(float* box, float x, float y)
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
static void mapTicker(automap_t* map)
{
#define MAPALPHA_FADE_STEP .07

    int                 playerNum;
    float               diff = 0, width, height, scale, panX[2], panY[2],
                        zoomVel, zoomSpeed;
    player_t*           mapPlayer;
    mobj_t*             mo;

    if(!map)
        return;

    playerNum = map - automaps;
    mapPlayer = &players[playerNum];
    mo = players[map->followPlayer].plr->mo;

    // Check the state of the controls. Done here so that offsets don't accumulate
    // unnecessarily, as they would, if left unread.
    P_GetControlState(playerNum, CTL_MAP_PAN_X, &panX[0], &panX[1]);
    P_GetControlState(playerNum, CTL_MAP_PAN_Y, &panY[0], &panY[1]);

    if(!((mapPlayer->plr->flags & DDPF_LOCAL) && mapPlayer->plr->inGame))
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
            setViewTarget(map, map->viewX + xy[VX], map->viewY + xy[VY], false);
    }
    else  // Camera follows the player
    {
        float       angle;

        setViewTarget(map, mo->pos[VX], mo->pos[VY], false);

        /* $unifiedangles */
        if(map->rotate)
            angle = mo->angle / (float) ANGLE_MAX * 360 - 90;
        else
            angle = 0;
        setViewAngleTarget(map, angle, false);
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
        float               diff;
        float               startAngle = map->oldAngle;
        float               endAngle = map->targetAngle;

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
        mapTicker(&automaps[i]);
    }
}

/**
 * Draws the given line including any optional extras.
 */
static void rendLine2(float x1, float y1, float x2, float y2,
                      glowtype_t glowType, float glowAlpha, float glowWidth,
                      boolean glowOnly, boolean scaleGlowWithView,
                      boolean caps, blendmode_t blend, boolean drawNormal)
{
    float               v1[2], v2[2];
    float               length, dx, dy;
    float               normal[2], unit[2];
    automap_t*          map = &automaps[mapviewplayer];

    // Scale into map, screen space units.
    v1[VX] = x1;
    v1[VY] = y1;
    v2[VX] = x2;
    v2[VY] = y2;

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
            thickness = map->cfg.lineGlowScale * 2.5f + 3;
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
                       //r, g, b, glowAlpha,
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
                       //r, g, b, glowAlpha,
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
                       //r, g, b, glowAlpha,
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
                       //r, g, b, glowAlpha,
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
                       //r, g, b, glowAlpha,
                       tex, false, blend);
        }
    }

    if(!glowOnly)
        AM_AddLine(v1[VX], v1[VY], v2[VX], v2[VY]);

    if(drawNormal)
    {
#define NORMTAIL_LENGTH         8

        float       center[2];

        center[VX] = v1[VX] + (length / 2) * unit[VX];
        center[VY] = v1[VY] + (length / 2) * unit[VY];
        AM_AddLine(center[VX], center[VY],
                   center[VX] + normal[VX] * NORMTAIL_LENGTH,
                   center[VY] + normal[VY] * NORMTAIL_LENGTH);

#undef NORMTAIL_LENGTH
    }
}

int renderWallSeg(void* obj, void* data)
{
    seg_t*          seg = (seg_t*) obj;
    ssecitervars_t* vars = (ssecitervars_t*) data;
    float           v1[2], v2[2];
    linedef_t*      line;
    xline_t*        xLine;
    sector_t*       frontSector, *backSector;
    mapobjectinfo_t* info;
    player_t*       plr = vars->plr;
    automap_t*      map = vars->map;

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

    info = NULL;
    if((map->flags & AMF_REND_ALLLINES) ||
       xLine->mapped[mapviewplayer])
    {
        backSector = P_GetPtrp(line, DMU_BACK_SECTOR);

        // Perhaps this is a specially colored line?
        if(getInfoForSpecialLine(map, xLine->special, frontSector,
                                 backSector))
            return 1; // Continue iteration.

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
    else if(map->revealed)
    {
        if(!(xLine->flags & ML_DONTDRAW))
        {
            // An as yet, unseen line.
            info = getMapObjectInfo(map, AMO_UNSEENLINE);
        }
    }

    if(info && info == &map->cfg.mapObjectInfo[vars->objType])
    {
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

        rendLine2(v1[VX], v1[VY], v2[VX], v2[VY],
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
 * Determines visible lines, draws them.
 *
 * @params objType      Type of map object being drawn.
 */
static void renderWalls(int objType)
{
    uint                i;
    player_t*           plr = &players[mapviewplayer];
    automap_t*          map = &automaps[mapviewplayer];
    ssecitervars_t      data;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    data.plr = plr;
    data.map = map;
    data.objType = objType;

    // Use the automap's viewframe bounding box.
    for(i = 0; i < numsubsectors; ++i)
    {
        P_Iteratep(P_ToPtr(DMU_SUBSECTOR, i), DMU_SEG, &data,
                   renderWallSeg);
    }
}

static void renderLinedef(linedef_t* line, float r, float g, float b,
                          float a, blendmode_t blendMode,
                          boolean drawNormal)
{
    float               length = P_GetFloatp(line, DMU_LENGTH);

    if(length > 0)
    {
        float               v1[2], v2[2];

        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

        DGL_BlendMode(blendMode);
        DGL_Color4f(r, g, b, a);

        DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, v1[VX], v1[VY]);
            DGL_Vertex2f(v1[VX], v1[VY]);

            DGL_TexCoord2f(0, v2[VX], v2[VY]);
            DGL_Vertex2f(v2[VX], v2[VY]);
        DGL_End();

        if(drawNormal)
        {
#define NORMTAIL_LENGTH         8

            float               normal[2], unit[2], d1[2];

            P_GetFloatpv(line, DMU_DXY, d1);

            unit[VX] = d1[0] / length;
            unit[VY] = d1[1] / length;
            normal[VX] = unit[VY];
            normal[VY] = -unit[VX];

            // The center of the linedef.
            v1[VX] += (length / 2) * unit[VX];
            v1[VY] += (length / 2) * unit[VY];

            // Outside point.
            v2[VX] = v1[VX] + normal[VX] * NORMTAIL_LENGTH;
            v2[VY] = v1[VY] + normal[VY] * NORMTAIL_LENGTH;

            DGL_Begin(DGL_LINES);
                DGL_TexCoord2f(0, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                DGL_TexCoord2f(0, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);
            DGL_End();
#undef NORMTAIL_LENGTH
        }

        DGL_BlendMode(BM_NORMAL);
    }
}

/**
 * Rather than draw the segs instead this will draw the linedef of which
 * the seg is a part.
 */
int renderPolyObjSeg(void* obj, void* data)
{
    seg_t*              seg = (seg_t*) obj;
    ssecitervars_t*     vars = (ssecitervars_t*) data;
    linedef_t*          line;
    xline_t*            xLine;
    mapobjectinfo_t*    info;
    player_t*           plr = vars->plr;
    automap_t*          map = vars->map;
    automapobjectname_t amo;

    if(!(line = P_GetPtrp(seg, DMU_LINEDEF)) || !(xLine = P_ToXLine(line)))
        return 1;

    if(xLine->validCount == VALIDCOUNT)
        return 1; // Already processed this frame.

    if((xLine->flags & ML_DONTDRAW) && !(map->flags & AMF_REND_ALLLINES))
        return 1;

    amo = AMO_NONE;
    if((map->flags & AMF_REND_ALLLINES) || xLine->mapped[mapviewplayer])
    {
        amo = AMO_SINGLESIDEDLINE;
    }
    else if(map->revealed && !(xLine->flags & ML_DONTDRAW))
    {   // An as yet, unseen line.
        amo = AMO_UNSEENLINE;
    }

    if((info = getMapObjectInfo(map, amo)))
    {
        renderLinedef(line, info->rgba[0], info->rgba[1], info->rgba[2],
                      info->rgba[3] * cfg.automapLineAlpha * map->alpha,
                      info->blendMode,
                      (map->flags & AMF_REND_LINE_NORMALS)? true : false);
    }

    xLine->validCount = VALIDCOUNT; // Mark as processed this frame.

    return 1; // Continue iteration.
}

boolean drawSegsOfPolyobject(polyobj_t *po, void *data)
{
    seg_t**             segPtr;
    int                 result;

    segPtr = po->segs;
    while(*segPtr && (result = renderPolyObjSeg(*segPtr, data)) != 0)
        *segPtr++;

    return result;
}

static void renderPolyObjs(void)
{
    player_t*           plr = &players[mapviewplayer];
    automap_t*          map = &automaps[mapviewplayer];
    ssecitervars_t      data;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    data.plr = plr;
    data.map = map;
    data.objType = MOL_LINEDEF;

    // Next, draw any polyobjects in view.
    P_PolyobjsBoxIterator(map->vbbox, drawSegsOfPolyobject, &data);
}

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean renderXGLinedef(linedef_t* line, void* data)
{
    ssecitervars_t*     vars = (ssecitervars_t*) data;
    xline_t*            xLine;
    automap_t*          map = vars->map;

    xLine = P_ToXLine(line);
    if(!xLine || xLine->validCount == VALIDCOUNT ||
       (xLine->flags & ML_DONTDRAW) && !(map->flags & AMF_REND_ALLLINES))
        return 1;

    // Show only active XG lines.
    if(!(xLine->xg && xLine->xg->active && (mapTime & 4)))
        return 1;

    renderLinedef(line, .8f, 0, .8f, 1, BM_ADD,
                  (map->flags & AMF_REND_LINE_NORMALS)? true : false);

    xLine->validCount = VALIDCOUNT; // Mark as processed this frame.

    return 1; // Continue iteration.
}
#endif

static void renderXGLinedefs(void)
{
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    player_t*           plr = &players[mapviewplayer];
    automap_t*          map = &automaps[mapviewplayer];
    ssecitervars_t      data;

    if(!(map->flags & AMF_REND_XGLINES))
        return;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    data.plr = plr;
    data.map = map;

    P_LinesBoxIterator(map->vbbox, renderXGLinedef, &data);
#endif
}

/**
 * Rotation in 2D.
 */
static void rotate2D(float* x, float* y, angle_t a)
{
    int                 angle;
    float               tmpx;

    angle = a >> ANGLETOFINESHIFT;
    tmpx = (*x * FIX2FLT(finecosine[angle])) -
                (*y * FIX2FLT(finesine[angle]));

    *y = (*x * FIX2FLT(finesine[angle])) +
                (*y * FIX2FLT(finecosine[angle]));

    *x = tmpx;
}

static void drawLineCharacter(const vectorgrap_t* vg)
{
    uint                i;

    DGL_Begin(DGL_LINES);
    for(i = 0; i < vg->count; ++i)
    {
        vgline_t*           vgl = &vg->lines[i];

        DGL_TexCoord2f(0, vgl->a.pos[VX], vgl->a.pos[VY]);
        DGL_Vertex2f(vgl->a.pos[VX], vgl->a.pos[VY]);
        DGL_TexCoord2f(0, vgl->b.pos[VX], vgl->b.pos[VY]);
        DGL_Vertex2f(vgl->b.pos[VX], vgl->b.pos[VY]);
    }
    DGL_End();
}

/**
 * Draws a line character (eg the player arrow)
 */
static void renderLineCharacter(vectorgrap_t* vg, float x, float y,
                                float angle, float scale, int color,
                                float alpha, blendmode_t blendmode)
{
    automap_t*          map = &automaps[mapviewplayer];
    float               oldLineWidth, rgba[4];

    R_PalIdxToRGB(color, rgba);
    rgba[3] = MINMAX_OF(0.f, alpha, 1.f);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, AM_LINE_WIDTH);

    DGL_Color4fv(rgba);
    DGL_BlendMode(blendmode);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();

    DGL_Translatef(x, y, 1);
    DGL_Rotatef(angle, 0, 0, 1);
    DGL_Scalef(scale, scale, 1);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 1);

    drawLineCharacter(vg);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    // Restore previous line width.
    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
}

/**
 * Draws all players on the map using a line character
 */
static void renderPlayers(void)
{
    int                 i;
    float               size = PLAYERRADIUS;
    vectorgrap_t*       vg;
    automap_t*          map = &automaps[mapviewplayer];

    vg = getVectorGraphic(map->vectorGraphicForPlayer);
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           p = &players[i];
        int                 color;
        float               alpha;
        mobj_t*             mo;

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

        /* $unifiedangles */
        renderLineCharacter(vg, mo->pos[VX], mo->pos[VY],
                            mo->angle / (float) ANGLE_MAX * 360, size,
                            color, alpha * map->alpha, BM_NORMAL);
    }
}

static int getKeyColorForMobjType(int type)
{
    struct keycolor_s {
        int             moType;
        int             color;
    } keyColors[] =
    {
#if __JDOOM__ || __JDOOM64__
        {MT_MISC4, KEY1_COLOR},
        {MT_MISC5, KEY2_COLOR},
        {MT_MISC6, KEY3_COLOR},
        {MT_MISC7, KEY4_COLOR},
        {MT_MISC8, KEY5_COLOR},
        {MT_MISC9, KEY6_COLOR},
#elif __JHERETIC__
        {MT_CKEY, KEY1_COLOR},
        {MT_BKYY, KEY2_COLOR},
        {MT_AKYY, KEY3_COLOR},
#endif
        {-1, -1} // Terminate.
    };
    uint                i;

    for(i = 0; keyColors[i].moType; ++i)
        if(keyColors[i].moType == type)
            return keyColors[i].color;

    return -1; // Not a key.
}

/**
 * Draws all things on the map
 */
static boolean renderThing(thinker_t* th, void* context)
{
    mobj_t*             mo = (mobj_t *) th;
    automap_t*          map = &automaps[mapviewplayer];
    int                 keyColor;

    if(map->flags & AMF_REND_KEYS)
    {
        // Is this a key?
        if((keyColor = getKeyColorForMobjType(mo->type)) != -1)
        {   // This mobj is indeed a key.
            /* $unifiedangles */
            renderLineCharacter(getVectorGraphic(VG_KEYSQUARE),
                                mo->pos[VX], mo->pos[VY],
                                0, PLAYERRADIUS, keyColor,
                                map->alpha * cfg.automapLineAlpha, BM_NORMAL);
            return true; // Continue iteration.
        }
    }

    if(map->flags & AMF_REND_THINGS)
    {   // Something else.
        /* $unifiedangles */
        renderLineCharacter(getVectorGraphic(VG_TRIANGLE),
                            mo->pos[VX], mo->pos[VY],
                            mo->angle / (float) ANGLE_MAX * 360,
                            PLAYERRADIUS, THINGCOLORS,
                            map->alpha * cfg.automapLineAlpha, BM_NORMAL);
    }

    return true; // Continue iteration.
}

/**
 * Draws all the points marked by the player.
 */
static void drawMarks(void)
{
#if !__JDOOM64__
    int                 i;
    float               x, y, w, h;
    automap_t*          map = &automaps[mapviewplayer];
    player_t*           plr = &players[mapviewplayer];
    dpatch_t*           patch;

    for(i = 0; i < NUMMARKPOINTS; ++i)
    {
        if(map->markpointsUsed[i])
        {
            patch = &markerPatches[i];

            w = FIXXTOSCREENX(patch->width) * map->scaleFTOM;
            h = FIXYTOSCREENY(patch->height) * map->scaleFTOM;

            x = map->markpoints[i].pos[VX];
            y = map->markpoints[i].pos[VY];

            DGL_SetPatch(patch->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_Color4f(1, 1, 1, map->alpha);

            DGL_MatrixMode(DGL_PROJECTION);
            DGL_PushMatrix();
            DGL_Translatef(x, y, 0);
            DGL_Rotatef(map->angle, 0, 0, 1);

            DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex2f(-(w / 2), h / 2);

                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex2f(w / 2, h / 2);

                DGL_TexCoord2f(0, 1, 1);
                DGL_Vertex2f(w / 2, -(h / 2));

                DGL_TexCoord2f(0, 0, 1);
                DGL_Vertex2f(-(w / 2), -(h / 2));
            DGL_End();

            DGL_MatrixMode(DGL_PROJECTION);
            DGL_PopMatrix();
        }
    }
#endif
}

/**
 * Sets up the state for automap drawing.
 */
static void setupGLStateForMap(void)
{
    //float extrascale = 0;
    player_t*           plr;
    automap_t*          map;
    automapwindow_t*    win;

    plr = &players[mapviewplayer];
    map = &automaps[mapviewplayer];
    win = &map->window;

    // Check for scissor box (to clip the map lines and stuff).
    // Store the old scissor state.
    DGL_GetIntegerv(DGL_SCISSOR_TEST, map->scissorState);
    DGL_GetIntegerv(DGL_SCISSOR_BOX, map->scissorState + 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, scrwidth, scrheight, -1, 1);

    // Do we want a background texture?
    if(autopageLumpNum != -1)
    {
        // Apply the background texture onto a parallaxing layer which
        // follows the map view target (not player).
        DGL_Enable(DGL_TEXTURING);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PushMatrix();
        DGL_LoadIdentity();

         // We only want the left portion.
        DGL_SetRawImage(autopageLumpNum, false, DGL_REPEAT, DGL_REPEAT);

        DGL_Color4f(map->cfg.backgroundRGBA[0],
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
            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(win->x, win->y);
            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(win->x + win->width, win->y);
            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(win->x + win->width, win->y + win->height);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(win->x, win->y + win->height);
        DGL_End();

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();

        DGL_MatrixMode(DGL_PROJECTION);
    }
    else
    {
        // Nope just a solid color.
        DGL_SetNoMaterial();
        DGL_DrawRect(win->x, win->y, win->width, win->height,
                     map->cfg.backgroundRGBA[0],
                     map->cfg.backgroundRGBA[1],
                     map->cfg.backgroundRGBA[2],
                     map->alpha * map->cfg.backgroundRGBA[3]);
    }

#if __JDOOM64__
    // jd64 > Laser artifacts
    // If drawn in HUD we don't need them visible in the map too.
    if(!cfg.hudShown[HUD_POWER])
    {
        int                 i, num;

        num = 0;
        for(i = 0; i < NUM_ARTIFACT_TYPES; ++i)
            if(plr->artifacts[i])
                num++;

        if(num > 0)
        {
            float               x, y, w, h, spacing, scale, iconAlpha;
            spriteinfo_t        sprInfo;
            int                 artifactSprites[NUM_ARTIFACT_TYPES] = {
                SPR_ART1, SPR_ART2, SPR_ART3
            };

            iconAlpha = MINMAX_OF(.0f, map->alpha, .5f);

            spacing = win->height / num;
            y = 0;

            for(i = 0; i < NUM_ARTIFACT_TYPES; ++i)
            {
                if(plr->artifacts[i])
                {
                    R_GetSpriteInfo(artifactSprites[i], 0, &sprInfo);
                    DGL_SetPSprite(sprInfo.material);

                    scale = win->height / (sprInfo.height * num);
                    x = win->width - sprInfo.width * scale;
                    w = sprInfo.width;
                    h = sprInfo.height;

                    {
                    float               s, t;

                    // Let's calculate texture coordinates.
                    // To remove a possible edge artifact, move the corner a bit up/left.
                    s = (w - 0.4f) / M_CeilPow2(w);
                    t = (h - 0.4f) / M_CeilPow2(h);

                    DGL_Color4f(1, 1, 1, iconAlpha);
                    DGL_Begin(DGL_QUADS);
                        DGL_TexCoord2f(0, 0, 0);
                        DGL_Vertex2f(x, y);

                        DGL_TexCoord2f(0, s, 0);
                        DGL_Vertex2f(x + w * scale, y);

                        DGL_TexCoord2f(0, s, t);
                        DGL_Vertex2f(x + w * scale, y + h * scale);

                        DGL_TexCoord2f(0, 0, t);
                        DGL_Vertex2f(x, y + h * scale);
                    DGL_End();
                    }

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
}

/**
 * Restores the previous gl draw state
 */
static void restoreGLStateFromMap(void)
{
    automap_t*          map = &automaps[mapviewplayer];

    // Return to the normal GL state.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if(!map->scissorState[0])
        DGL_Disable(DGL_SCISSOR_TEST);
    DGL_Scissor(map->scissorState[1], map->scissorState[2], map->scissorState[3],
                map->scissorState[4]);
}

/**
 * Draws the map name into the automap window
 */
static void renderMapName(void)
{
    float               x, y, otherY;
    char*               lname;
    automap_t*          map = &automaps[mapviewplayer];
    automapwindow_t*    win = &map->window;
    dpatch_t*           patch = NULL;
#if __JDOOM__ || __JDOOM64__
    int                 mapNum;
#endif

    lname = P_GetMapNiceName();

    if(lname)
    {
        // Compose the mapnumber used to check the map name patches array.
#if __JDOOM64__
        mapNum = gameMap -1;
        patch = &mapNamePatches[mapNum];
#elif __JDOOM__
        if(gameMode == commercial)
            mapNum = gameMap -1;
        else
            mapNum = ((gameEpisode -1) * 9) + gameMap -1;

        patch = &mapNamePatches[mapNum];
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
        WI_DrawPatch(x, y, 1, 1, 1, map->alpha, patch, lname, false,
                     ALIGN_CENTER);
        Draw_EndZoom();

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();
    }
}

static void renderVertexes(void)
{
    uint                i;
    automap_t*          map = &automaps[mapviewplayer];
    float               v[2], oldPointSize;

    DGL_Color4f(.2f, .5f, 1, map->alpha);

    DGL_Enable(DGL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 4);

    DGL_Begin(DGL_POINTS);
    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, v);
        DGL_TexCoord2f(0, v[VX], v[VY]);
        DGL_Vertex2f(v[VX], v[VY]);
    }
    DGL_End();

    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    DGL_Disable(DGL_POINT_SMOOTH);
}

/**
 * Compile OpenGL commands for drawing the map objects into display lists.
 */
static void compileObjectLists(automap_t* map)
{
    uint                i;

    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        // Construct primitives.
        renderWalls(i);

        if(map->lists[i])
            DGL_DeleteLists(map->lists[i], 1);
        map->lists[i] = 0;

        // Build commands and compile to a display list.
        if(DGL_NewList(0, DGL_COMPILE))
        {
            AM_RenderAllLists();
            map->lists[i] = DGL_EndList();
        }

        // We are now finished with the primitive list.
        AM_ClearAllLists(false);
    }
}

/**
 * Render the automap view window for the specified player.
 */
void AM_Drawer(int viewplayer)
{
    static int          updateWait = 0;

    uint                i;
    automap_t*          map;
    player_t*           mapPlayer;
    automapwindow_t*    win;
    float               oldLineWidth;

    if(IS_DEDICATED)
        return; // Nothing to do.

    if(viewplayer < 0 || viewplayer >= MAXPLAYERS)
        return;

    mapviewplayer = viewplayer;
    mapPlayer = &players[mapviewplayer];

    if(!((mapPlayer->plr->flags & DDPF_LOCAL) && mapPlayer->plr->inGame))
        return;

    map = &automaps[mapviewplayer];
    if(!(map->alpha > 0))
        return;
    win = &map->window;

    // Freeze the lists if the map is fading out from being open or if set
    // to frozen for debug.
    if((++updateWait % 10) && map->constructMap && !freezeMapRLs)
    {   // Its time to rebuild the automap object display lists.
        compileObjectLists(map);
        map->constructMap = false;
    }

    // Setup for frame.
    setupGLStateForMap();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_Translatef(win->x + win->width / 2, win->y + win->height / 2, 0);
    DGL_Rotatef(map->angle, 0, 0, 1);
    DGL_Scalef(1, -1, 1);
    DGL_Scalef(map->scaleMTOF, map->scaleMTOF, 1);
    DGL_Translatef(-map->viewX, -map->viewY, 0);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, AM_LINE_WIDTH);

    if(amMaskTexture)
    {
        DGL_Enable(DGL_TEXTURING);
        DGL_Bind(amMaskTexture);

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_LoadIdentity();

        DGL_PushMatrix();
        DGL_Scalef(1.f / win->width, 1.f / win->height, 1);
        DGL_Translatef(win->width / 2, win->height / 2, 0);
        DGL_Rotatef(-map->angle, 0, 0, 1);
        DGL_Scalef(map->scaleMTOF, map->scaleMTOF, 1);
        DGL_Translatef(-map->viewX , -map->viewY, 0);
    }

    // Draw static map geometry.
    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        if(map->lists[i])
        {
            mapobjectinfo_t*        info = &map->cfg.mapObjectInfo[i];

            // Setup the global list state.
            DGL_Color4f(info->rgba[0], info->rgba[1], info->rgba[2],
                        info->rgba[3] * cfg.automapLineAlpha * map->alpha);
            DGL_BlendMode(info->blendMode);

            // Draw.
            DGL_CallList(map->lists[i]);
        }
    }

    // Restore the previous state.
    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    if(map->flags & AMF_REND_VERTEXES)
        renderVertexes();

    // Draw dynamic map elements:
    renderXGLinedefs();
    renderPolyObjs();

    // Draw the map objects:
    renderPlayers();
    if(map->flags & (AMF_REND_THINGS|AMF_REND_KEYS))
        P_IterateThinkers(P_MobjThinker, renderThing, NULL);

    if(amMaskTexture)
    {
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();
        DGL_Bind(0);
    }

    // Draw any marked points.
    drawMarks();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    renderMapName();

    restoreGLStateFromMap();
}

// ------------------------------------------------------------------------------
// Automap Menu
//-------------------------------------------------------------------------------

menuitem_t MAPItems[] = {
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
    NULL, false,
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
    static char*            hudviewnames[3] = { "NONE", "CURRENT", "STATUSBAR" };
    static char*            yesno[2] = { "NO", "YES" };
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    static char*            countnames[4] = { "NO", "YES", "PERCENT", "COUNT+PCNT" };
#endif
    float                   menuAlpha;
    const menu_t*           menu = &MapDef;

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
