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

#include "p_mapsetup.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "p_player.h"
#include "g_controls.h"
#include "p_tick.h"
#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

#define LERP(start, end, pos) (end * pos + start * (1 - pos))

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

// Translates between frame-buffer and map distances
#define FTOM(map, x) ((x) * (map)->scaleFTOM)
#define MTOF(map, x) ((x) * (map)->scaleMTOF)

#define AM_MAXSPECIALLINES      32

// TYPES -------------------------------------------------------------------

typedef struct automapwindow_s {
    // Where the window currently is on screen, and the dimensions.
    float           x, y, width, height;

    // Where the window should be on screen, and the dimensions.
    int             targetX, targetY, targetWidth, targetHeight;
    int             oldX, oldY, oldWidth, oldHeight;

    float           posTimer;
} automapwindow_t;

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
    vectorgrapname_t vectorGraphicForThing;

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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Menu routines.
void            M_DrawMapMenu(void);
void            M_MapPosition(int option, void* data);
void            M_MapWidth(int option, void* data);
void            M_MapHeight(int option, void* data);
void            M_MapOpacity(int option, void* data);
void            M_MapLineAlpha(int option, void* data);
void            M_MapDoorColors(int option, void* data);
void            M_MapDoorGlow(int option, void* data);
void            M_MapRotate(int option, void* data);
void            M_MapStatusbar(int option, void* data);
void            M_MapCustomColors(int option, void* data);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     setWindowFullScreenMode(automap_t* map, int value);
static void     setViewTarget(automap_t* map, float x, float y, boolean fast);
static void     setViewScaleTarget(automap_t* map, float scale);
static void     setViewAngleTarget(automap_t* map, float angle, boolean fast);
static void     setViewRotateMode(automap_t* map, boolean on);
static void     clearMarks(automap_t* map);
static void registerSpecialLine(automap_t* map, int cheatLevel, int lineSpecial,
                            int sided, float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView);
static void setVectorGraphic(automap_t* map, int objectname, int vgname);
static void setColorAndAlpha(automap_t* map, int objectname, float r,
                             float g, float b, float a);
static void openMap(automap_t* map, boolean yes, boolean fast);

static void     findMinMaxBoundaries(void);
static void     rotate2D(float* x, float* y, angle_t a);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean viewActive;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int mapviewplayer;

cvar_t mapCVars[] = {
    {"map-opacity", 0, CVT_FLOAT, &cfg.automapOpacity, 0, 1},
    {"map-alpha-lines", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {"map-babykeys", 0, CVT_BYTE, &cfg.automapBabyKeys, 0, 1},
#endif
    {"map-background-r", 0, CVT_FLOAT, &cfg.automapBack[0], 0, 1},
    {"map-background-g", 0, CVT_FLOAT, &cfg.automapBack[1], 0, 1},
    {"map-background-b", 0, CVT_FLOAT, &cfg.automapBack[2], 0, 1},
    {"map-customcolors", 0, CVT_INT, &cfg.automapCustomColors, 0, 1},
    {"map-mobj-r", 0, CVT_FLOAT, &cfg.automapMobj[0], 0, 1},
    {"map-mobj-g", 0, CVT_FLOAT, &cfg.automapMobj[1], 0, 1},
    {"map-mobj-b", 0, CVT_FLOAT, &cfg.automapMobj[2], 0, 1},
    {"map-wall-r", 0, CVT_FLOAT, &cfg.automapL1[0], 0, 1},
    {"map-wall-g", 0, CVT_FLOAT, &cfg.automapL1[1], 0, 1},
    {"map-wall-b", 0, CVT_FLOAT, &cfg.automapL1[2], 0, 1},
    {"map-wall-unseen-r", 0, CVT_FLOAT, &cfg.automapL0[0], 0, 1},
    {"map-wall-unseen-g", 0, CVT_FLOAT, &cfg.automapL0[1], 0, 1},
    {"map-wall-unseen-b", 0, CVT_FLOAT, &cfg.automapL0[2], 0, 1},
    {"map-wall-floorchange-r", 0, CVT_FLOAT, &cfg.automapL2[0], 0, 1},
    {"map-wall-floorchange-g", 0, CVT_FLOAT, &cfg.automapL2[1], 0, 1},
    {"map-wall-floorchange-b", 0, CVT_FLOAT, &cfg.automapL2[2], 0, 1},
    {"map-wall-ceilingchange-r", 0, CVT_FLOAT, &cfg.automapL3[0], 0, 1},
    {"map-wall-ceilingchange-g", 0, CVT_FLOAT, &cfg.automapL3[1], 0, 1},
    {"map-wall-ceilingchange-b", 0, CVT_FLOAT, &cfg.automapL3[2], 0, 1},
    {"map-door-colors", 0, CVT_BYTE, &cfg.automapShowDoors, 0, 1},
    {"map-door-glow", 0, CVT_FLOAT, &cfg.automapDoorGlow, 0, 200},
    {"map-huddisplay", 0, CVT_INT, &cfg.automapHudDisplay, 0, 2},
    {"map-pan-speed", 0, CVT_FLOAT, &cfg.automapPanSpeed, 0, 1},
    {"map-pan-resetonopen", 0, CVT_BYTE, &cfg.automapPanResetOnOpen, 0, 1},
    {"map-rotate", 0, CVT_BYTE, &cfg.automapRotate, 0, 1},
    {"map-zoom-speed", 0, CVT_FLOAT, &cfg.automapZoomSpeed, 0, 1},
    {"rend-dev-freeze-map", CVF_NO_ARCHIVE, CVT_BYTE, &freezeMapRLs, 0, 1},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static automap_t automaps[MAXPLAYERS];

static vectorgrap_t* vectorGraphs[NUM_VECTOR_GRAPHS];

// Map bounds.
static float bounds[2][2]; // {TL{x,y}, BR{x,y}}

// CODE --------------------------------------------------------------------

static __inline automap_t* getAutomap(automapid_t id)
{
    if(id == 0 || id > MAXPLAYERS)
    {
#if _DEBUG
Con_Error("getAutomap: Invalid map id %i.", id);
#endif
        return NULL;
    }

    return &automaps[id-1];
}

automapid_t AM_MapForPlayer(int plrnum)
{
    if(plrnum < 0 || plrnum >= MAXPLAYERS)
    {
#if _DEBUG
Con_Error("AM_MapForPlayer: Invalid player num %i.", plrnum);
#endif
        return 0;
    }

    return ((automapid_t) plrnum) + 1; // 1-based index.
}

void AM_GetMapColor(float* rgb, const float* uColor, int palidx,
                    boolean customPal)
{
    if((!customPal && !cfg.automapCustomColors) ||
       (customPal && cfg.automapCustomColors != 2))
    {
        R_PalIdxToRGB(rgb, palidx, false);
        return;
    }

    rgb[0] = uColor[0];
    rgb[1] = uColor[1];
    rgb[2] = uColor[2];
}

vectorgrap_t* AM_GetVectorGraph(vectorgrapname_t id)
{
    vectorgrap_t*       vg;
    vgline_t*           lines;

    if(id > NUM_VECTOR_GRAPHS - 1)
        return NULL;

    if(vectorGraphs[id])
        return vectorGraphs[id];

    // Not loaded yet.
    {
    uint                i, linecount;

    vg = vectorGraphs[id] = malloc(sizeof(*vg));

    switch(id)
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
        Con_Error("AM_GetVectorGraph: Unknown id %i.", id);
        break;
    }

    vg->lines = malloc(linecount * sizeof(vgline_t));
    vg->count = linecount;
    vg->dlist = 0;
    for(i = 0; i < linecount; ++i)
        memcpy(&vg->lines[i], &lines[i], sizeof(vgline_t));
    }

    return vg;
}

const automapcfg_t* AM_GetMapConfig(automapid_t id)
{
    automap_t*              map;

    if(!(map = getAutomap(id)))
        return NULL;

    return &map->cfg;
}

const mapobjectinfo_t* AM_GetMapObjectInfo(automapid_t id, int objectname)
{
    automap_t*              map;

    if(objectname == AMO_NONE)
        return NULL;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("getMapObjectInfo: Unknown object %i.", objectname);

    if(!(map = getAutomap(id)))
        return NULL;

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

const mapobjectinfo_t* AM_GetInfoForSpecialLine(automapid_t id, int special,
                                                const sector_t* frontsector,
                                                const sector_t* backsector)
{
    uint                i;
    automap_t*          map;
    mapobjectinfo_t*    info = NULL;

    if(!(map = getAutomap(id)))
        return NULL;

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
}

/**
 * Called during init.
 */
void AM_Init(void)
{
    uint                i;
    float               scrwidth, scrheight;
    boolean             customPal;

    if(IS_DEDICATED)
        return;

    memset(vectorGraphs, 0, sizeof(vectorGraphs));

    scrwidth = Get(DD_WINDOW_WIDTH);
    scrheight = Get(DD_WINDOW_HEIGHT);

    Rend_AutomapInit();
    Rend_AutomapLoadData();

    customPal = !W_IsFromIWAD(W_GetNumForName("PLAYPAL"));

    memset(&automaps, 0, sizeof(automaps));
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        automap_t*          map = &automaps[i];
        automapcfg_t*       mcfg = &map->cfg;
        float               rgb[3];

        // Initialize.
        // \fixme Put these values into an array (or read from a lump?).
        map->followPlayer = i;
        map->oldViewScale = 1;
        map->window.oldX = map->window.x = 0;
        map->window.oldY = map->window.y = 0;
        map->window.oldWidth = map->window.width = scrwidth;
        map->window.oldHeight = map->window.height = scrheight;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].glow = NO_GLOW;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowAlpha = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowWidth = 10;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].blendMode = BM_NORMAL;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].scaleWithView = false;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[0] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[1] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[2] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[3] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF].glow = NO_GLOW;
        mcfg->mapObjectInfo[MOL_LINEDEF].glowAlpha = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF].glowWidth = 10;
        mcfg->mapObjectInfo[MOL_LINEDEF].blendMode = BM_NORMAL;
        mcfg->mapObjectInfo[MOL_LINEDEF].scaleWithView = false;
        mcfg->mapObjectInfo[MOL_LINEDEF].rgba[0] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF].rgba[1] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF].rgba[2] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF].rgba[3] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glow = NO_GLOW;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowAlpha = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowWidth = 10;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].blendMode = BM_NORMAL;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].scaleWithView = false;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[0] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[1] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[2] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[3] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].glow = NO_GLOW;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].glowAlpha = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].glowWidth = 10;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].blendMode = BM_NORMAL;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].scaleWithView = false;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[0] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[1] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[2] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[3] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].glow = NO_GLOW;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].glowAlpha = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].glowWidth = 10;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].blendMode = BM_NORMAL;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].scaleWithView = false;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[0] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[1] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[2] = 1;
        mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[3] = 1;

        Rend_AutomapRebuild(AM_MapForPlayer(i));

        // Register lines we want to display in a special way.
#if __JDOOM__ || __JDOOM64__
        // Blue locked door, open.
        registerSpecialLine(map, 0, 32, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Blue locked door, locked.
        registerSpecialLine(map, 0, 26, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 99, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 133, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Red locked door, open.
        registerSpecialLine(map, 0, 33, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Red locked door, locked.
        registerSpecialLine(map, 0, 28, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 134, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 135, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow locked door, open.
        registerSpecialLine(map, 0, 34, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow locked door, locked.
        registerSpecialLine(map, 0, 27, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 136, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 137, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Exit switch.
        registerSpecialLine(map, 1, 11, 1, 0, 1, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Exit cross line.
        registerSpecialLine(map, 1, 52, 2, 0, 1, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Secret Exit switch.
        registerSpecialLine(map, 1, 51, 1, 0, 1, 1, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Secret Exit cross line.
        registerSpecialLine(map, 2, 124, 2, 0, 1, 1, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#elif __JHERETIC__
        // Blue locked door.
        registerSpecialLine(map, 0, 26, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Blue switch?
        registerSpecialLine(map, 0, 32, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow locked door.
        registerSpecialLine(map, 0, 27, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Yellow switch?
        registerSpecialLine(map, 0, 34, 0, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Green locked door.
        registerSpecialLine(map, 0, 28, 2, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Green switch?
        registerSpecialLine(map, 0, 33, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#elif __JHEXEN__
        // A locked door (all are green).
        registerSpecialLine(map, 0, 13, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 83, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Intra-map teleporters (all are blue).
        registerSpecialLine(map, 0, 70, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        registerSpecialLine(map, 0, 71, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Inter-map teleport.
        registerSpecialLine(map, 0, 74, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
        // Game-winning exit.
        registerSpecialLine(map, 0, 75, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#endif

        // Setup map based on player's config.
        // \todo All players' maps work from the same config!
        mcfg->lineGlowScale = cfg.automapDoorGlow;
        mcfg->glowingLineSpecials = cfg.automapShowDoors;
        mcfg->panSpeed = cfg.automapPanSpeed;
        mcfg->panResetOnOpen = cfg.automapPanResetOnOpen;
        mcfg->zoomSpeed = cfg.automapZoomSpeed;
        setViewRotateMode(map, cfg.automapRotate);

        setVectorGraphic(map, AMO_THING, VG_TRIANGLE);
        /*AM_GetMapColor(rgb, cfg.automapMobj, THINGCOLORS, customPal);
        setColorAndAlpha(map, AMO_THING, rgb[0], rgb[1], rgb[2], 1);*/
        setVectorGraphic(map, AMO_THINGPLAYER, VG_ARROW);

#if __JHERETIC__ || __JHEXEN__
        if(W_CheckNumForName("AUTOPAGE") == -1)
        {
            setColorAndAlpha(map, AMO_BACKGROUND, .55f, .45f, .35f,
                             cfg.automapOpacity);
        }
        else
        {
            AM_GetMapColor(rgb, cfg.automapBack, WHITE, customPal);
            setColorAndAlpha(map, AMO_BACKGROUND, rgb[0], rgb[1], rgb[2],
                             cfg.automapOpacity);
        }
#else
        AM_GetMapColor(rgb, cfg.automapBack, BACKGROUND, customPal);
        setColorAndAlpha(map, AMO_BACKGROUND, rgb[0], rgb[1], rgb[2],
                            cfg.automapOpacity);
#endif

        AM_GetMapColor(rgb, cfg.automapL0, GRAYS+3, customPal);
        setColorAndAlpha(map, AMO_UNSEENLINE, rgb[0], rgb[1], rgb[2], 1);

        AM_GetMapColor(rgb, cfg.automapL1, WALLCOLORS, customPal);
        setColorAndAlpha(map, AMO_SINGLESIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

        AM_GetMapColor(rgb, cfg.automapL0, TSWALLCOLORS, customPal);
        setColorAndAlpha(map, AMO_TWOSIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

        AM_GetMapColor(rgb, cfg.automapL2, FDWALLCOLORS, customPal);
        setColorAndAlpha(map, AMO_FLOORCHANGELINE, rgb[0], rgb[1], rgb[2], 1);

        AM_GetMapColor(rgb, cfg.automapL3, CDWALLCOLORS, customPal);
        setColorAndAlpha(map, AMO_CEILINGCHANGELINE, rgb[0], rgb[1], rgb[2], 1);
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

    Rend_AutomapUnloadData();

    // Vector graphics.
    for(i = 0; i < NUM_VECTOR_GRAPHS; ++i)
    {
        vectorgrap_t *vg = vectorGraphs[i];

        if(vg)
        {
            if(vg->dlist)
                DGL_DeleteLists(vg->dlist, 1);
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
        automap_t*          map = &automaps[i];

        Rend_AutomapRebuild(AM_MapForPlayer(i));

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
            setVectorGraphic(map, AMO_THINGPLAYER, VG_CHEATARROW);
#endif
        // If the map has been left open; close it.
        openMap(map, false, true);

        // Reset position onto the follow player.
        if(players[map->followPlayer].plr->mo)
        {
            mobj_t*             mo = players[map->followPlayer].plr->mo;

            setViewTarget(map, mo->pos[VX], mo->pos[VY], true);
        }
    }
}

static void openMap(automap_t* map, boolean yes, boolean fast)
{
    if(G_GetGameState() != GS_MAP)
        return;

    if(!players[map->followPlayer].plr->inGame)
        return;

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
 * Start the automap.
 */
void AM_Open(automapid_t id, boolean yes, boolean fast)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    openMap(map, yes, fast);
}

/**
 * Translates from map to automap window coordinates.
 */
float AM_MapToFrame(automapid_t id, float val)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_MapToFrame: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
        return 0;

    return MTOF(map, val);
}

/**
 * Translates from automap window to map coordinates.
 */
float AM_FrameToMap(automapid_t id, float val)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_MapToFrame: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
        return 0;

    return FTOM(map, val);
}

static void setWindowTarget(automap_t* map, int x, int y, int w, int h)
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

void AM_SetWindowTarget(automapid_t id, int x, int y, int w, int h)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    setWindowTarget(map, x, y, w, h);
}

void AM_GetWindow(automapid_t id, float* x, float* y, float* w, float* h)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_GetWindow: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
        return;

    if(x) *x = map->window.x;
    if(y) *y = map->window.y;
    if(w) *w = map->window.width;
    if(h) *h = map->window.height;
}

static void setWindowFullScreenMode(automap_t* map, int value)
{
    if(value == 2) // toggle
        map->fullScreenMode = !map->fullScreenMode;
    else
        map->fullScreenMode = value;
}

void AM_SetWindowFullScreenMode(automapid_t id, int value)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
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

boolean AM_IsMapWindowInFullScreenMode(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_IsMapWindowInFullScreenMode: Not available in "
                  "dedicated mode.");

    if(!(map = getAutomap(id)))
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

void AM_SetViewTarget(automapid_t id, float x, float y)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    setViewTarget(map, x, y, false);
}

void AM_GetViewPosition(automapid_t id, float* x, float* y)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    if(x) *x = map->viewX;
    if(y) *y = map->viewY;
}

void AM_GetViewParallaxPosition(automapid_t id, float* x, float* y)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    if(x) *x = map->viewPLX;
    if(y) *y = map->viewPLY;
}

/**
 * @return              Current alpha level of the automap.
 */
float AM_ViewAngle(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
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

void AM_SetViewScaleTarget(automapid_t id, float scale)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
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

void AM_SetViewAngleTarget(automapid_t id, float angle)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    setViewAngleTarget(map, angle, false);
}

float AM_MapToFrameMultiplier(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return 1;

    return map->scaleMTOF;
}

/**
 * @return              True if the specified map is currently active.
 */
boolean AM_IsActive(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return false; // Never.

    if(!(map = getAutomap(id)))
        return false;

    return map->active;
}

static void setViewRotateMode(automap_t* map, boolean on)
{
    map->rotate = on;
}

void AM_SetViewRotate(automapid_t id, int offOnToggle)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;

    if(offOnToggle == 2)
        cfg.automapRotate = !cfg.automapRotate;
    else
        cfg.automapRotate = (offOnToggle? true : false);

    setViewRotateMode(map, cfg.automapRotate);

    P_SetMessage(&players[map->followPlayer],
                 (map->rotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF), false);
}

/**
 * Update the specified player's automap.
 *
 * @param id            Id of the map being updated.
 * @param lineIdx       Idx of the line being added to the map.
 * @param visible       @c true= mark the line as visible, else hidden.
 */
void AM_UpdateLinedef(automapid_t id, uint lineIdx, boolean visible)
{
    automap_t*          map;
    xline_t*            xline;

    if(!(map = getAutomap(id)))
        return;

    if(lineIdx >= numlines)
        return;

    xline = P_GetXLine(lineIdx);

    // Will we need to rebuild one or more display lists?
    if(xline->mapped[map->followPlayer] != visible)
        Rend_AutomapRebuild(id);

    xline->mapped[map->followPlayer] = visible;
}

void AM_GetMapBBox(automapid_t id, float vbbox[4])
{
    automap_t*          map;

    if(!vbbox)
        return;

    if(!(map = getAutomap(id)))
        return;

   vbbox[0] = map->vbbox[0];
   vbbox[1] = map->vbbox[1];
   vbbox[2] = map->vbbox[2];
   vbbox[3] = map->vbbox[3];
}

/**
 * Reveal the whole map.
 */
void AM_RevealMap(automapid_t id, boolean on)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    if(map->revealed != on)
    {
        map->revealed = on;
        Rend_AutomapRebuild(id);
    }
}

boolean AM_IsRevealed(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return false;

    return map->revealed;
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
void AM_ClearMarks(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    clearMarks(map);

    P_SetMessage(&players[map->followPlayer], AMSTR_MARKSCLEARED, false);
    Con_Printf("All markers cleared on automap.\n");
}

/**
 * Adds a marker at the current location
 */
static int addMark(automap_t* map, float x, float y)
{
    int                 num = map->markpointnum;

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
 * Adds a marker at the specified X/Y location.
 */
int AM_AddMark(automapid_t id, float x, float y)
{
    static char         buffer[20];
    automap_t*          map;
    int                 newMark;

    if(!(map = getAutomap(id)))
        return -1;

    newMark = addMark(map, x, y);
    if(newMark != -1)
    {
        sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newMark);
        P_SetMessage(&players[map->followPlayer], buffer, false);
    }

    return newMark;
}

boolean AM_GetMark(automapid_t id, uint mark, float* x, float* y)
{
    automap_t*          map;

    if(!x && !y)
        return false;

    if(!(map = getAutomap(id)))
        return false;

    if(mark < NUMMARKPOINTS && map->markpointsUsed[mark])
    {
        if(x) *x = map->markpoints[mark].pos[0];
        if(y) *y = map->markpoints[mark].pos[1];

        return true;
    }

    return false;
}

/**
 * Toggles between active and max zoom.
 */
void AM_ToggleZoomMax(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
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
void AM_ToggleFollow(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;

    map->panMode = !map->panMode;

    // Enable/disable the pan mode binding class
    DD_Executef(true, "%sactivatebcontext map-freepan",
                !map->panMode? "de" : "");

    P_SetMessage(&players[map->followPlayer],
                 (map->panMode ? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON), false);
}

/**
 * Set the alpha level of the automap. Alpha levels below one automatically
 * show the game view in addition to the automap.
 *
 * @param alpha         Alpha level to set the automap too (0...1)
 */
void AM_SetGlobalAlphaTarget(automapid_t id, float alpha)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;

    map->targetAlpha = MINMAX_OF(0, alpha, 1);
}

/**
 * @return              Current alpha level of the automap.
 */
float AM_GlobalAlpha(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_GlobalAlpha: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
        return 0;

    return map->alpha;
}

int AM_GetFlags(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return 0;

    return map->flags;
}

static void setColor(automap_t* map, int objectname, float r, float g,
                     float b)
{
    int                 i;
    mapobjectinfo_t*    info;

    if(objectname == AMO_NONE)
        return; // Ignore.

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
        Rend_AutomapRebuild(AM_MapForPlayer(i));
}

void AM_SetColor(automapid_t id, int objectname, float r, float g, float b)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;

    setColor(map, objectname, r, g, b);
}

void AM_GetColor(automapid_t id, int objectname, float* r, float* g, float* b)
{
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        Con_Error("AM_GetColor: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
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

static void setColorAndAlpha(automap_t* map, int objectname, float r,
                             float g, float b, float a)
{
    int                 i;
    mapobjectinfo_t*    info;

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
        Rend_AutomapRebuild(AM_MapForPlayer(i));
}

void AM_SetColorAndAlpha(automapid_t id, int objectname, float r, float g,
                         float b, float a)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    setColorAndAlpha(map, objectname, r, g, b, a);
}

void AM_GetColorAndAlpha(automapid_t id, int objectname, float* r, float* g,
                         float* b, float* a)
{
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        Con_Error("AM_GetColorAndAlpha: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
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

void AM_SetBlendmode(automapid_t id, int objectname, blendmode_t blendmode)
{
    uint                i;
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
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
        Rend_AutomapRebuild(AM_MapForPlayer(i));
}

void AM_SetGlow(automapid_t id, int objectname, glowtype_t type, float size,
                float alpha, boolean canScale)
{
    uint                i;
    automap_t*          map;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
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
        Rend_AutomapRebuild(AM_MapForPlayer(i));
}

static void setVectorGraphic(automap_t* map, int objectname, int vgname)
{
    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetVectorGraphic: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_THING:
        map->vectorGraphicForThing = vgname;
        break;

    case AMO_THINGPLAYER:
        map->vectorGraphicForPlayer = vgname;
        break;

    default:
        Con_Error("AM_SetVectorGraphic: Object %i does not support vector "
                  "graphic.", objectname);
        break;
    }
}

static vectorgrapname_t getVectorGraphic(automap_t* map, int objectname)
{
    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_GetVectorGraphic: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_THING:
        return map->vectorGraphicForThing;

    case AMO_THINGPLAYER:
        return map->vectorGraphicForPlayer;

    default:
        Con_Error("AM_GetVectorGraphic: Object %i does not support vector "
                  "graphic.", objectname);
        break;
    }

    return VG_NONE;
}

void AM_SetVectorGraphic(automapid_t id, int objectname, int vgname)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    setVectorGraphic(map, objectname, vgname);
}

vectorgrapname_t AM_GetVectorGraphic(automapid_t id, int objectname)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return VG_NONE;

    return getVectorGraphic(map, objectname);
}

static void registerSpecialLine(automap_t* map, int cheatLevel, int lineSpecial,
                            int sided, float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView)
{
    uint                i;
    automapspecialline_t* line, *p;

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
    Rend_AutomapRebuild(AM_MapForPlayer(map->followPlayer));
}

void AM_RegisterSpecialLine(automapid_t id, int cheatLevel, int lineSpecial,
                            int sided, float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    if(cheatLevel < 0 || cheatLevel > 4)
        Con_Error("AM_RegisterSpecialLine: cheatLevel '%i' out of range {0-4}.",
                  cheatLevel);
    if(lineSpecial < 0)
        Con_Error("AM_RegisterSpecialLine: lineSpecial '%i' is negative.",
                  lineSpecial);
    if(sided < 0 || sided > 2)
        Con_Error("AM_RegisterSpecialLine: sided '%i' is invalid.", sided);

    registerSpecialLine(map, cheatLevel, lineSpecial, sided, r, g, b, a,
                        blendmode, glowType, glowAlpha, glowWidth,
                        scaleGlowWithView);
}

void AM_SetCheatLevel(automapid_t id, int level)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
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
    Rend_AutomapRebuild(id);
}

void AM_IncMapCheatLevel(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
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
    Rend_AutomapRebuild(id);
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
 * Animates an automap view window towards the target values.
 */
static void mapWindowTicker(automap_t* map)
{
    float               newX, newY, newWidth, newHeight;
    automapwindow_t*    win;
    float               scrwidth = Get(DD_WINDOW_WIDTH);
    float               scrheight = Get(DD_WINDOW_HEIGHT);

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
        float               xy[2] = { 0, 0 }; // deltas
        float               scrwidth = Get(DD_WINDOW_WIDTH);
        float               scrheight = Get(DD_WINDOW_HEIGHT);
        // DOOM.EXE pans the automap at 140 fixed pixels per second.
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
        float               angle;

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
    uint                i;

    if(IS_DEDICATED)
        return; // Nothing to do.

    // All maps get to tick if their player is in-game.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        mapTicker(&automaps[i]);
    }
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

// ------------------------------------------------------------------------------
// Automap Menu
//-------------------------------------------------------------------------------

menuitem_t MAPItems[] = {
    {ITT_LRFUNC, 0, "opacity :", M_MapOpacity, 0},
#if __JHERETIC__ || __JHEXEN__
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "line alpha :", M_MapLineAlpha, 0 },
#if __JHERETIC__ || __JHEXEN__
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "hud display :", M_MapStatusbar, 0 },
    {ITT_EFUNC, 0, "door colors :", M_MapDoorColors, 0 },
    {ITT_LRFUNC, 0, "door glow : ", M_MapDoorGlow, 0 },
#if __JHERETIC__ || __JHEXEN__
    {ITT_EMPTY, 0, NULL, NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0 },
#endif
#if __JDOOM__ || __JDOOM64__
    {ITT_EMPTY, 0, NULL, NULL, 0 },
#endif
    {ITT_LRFUNC, 0, "use custom colors :", M_MapCustomColors, 0 },
    {ITT_EFUNC, 0, "   wall", SCColorWidget, 1 },
    {ITT_EFUNC, 0, "   floor height change", SCColorWidget, 2 },
    {ITT_EFUNC, 0, "   ceiling height change", SCColorWidget, 3 },
    {ITT_EFUNC, 0, "   unseen", SCColorWidget, 0 },
    {ITT_EFUNC, 0, "   thing", SCColorWidget, 6 },
    {ITT_EFUNC, 0, "   background", SCColorWidget, 4 },
};

menu_t MapDef = {
    0,
#if __JDOOM__ || __JDOOM64__
    70, 40,
#else
    64, 28,
#endif
    M_DrawMapMenu,
#if __JHERETIC__ || __JHEXEN__
    18, MAPItems,
#else
    13, MAPItems,
#endif
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
#if __JHERETIC__ || __JHEXEN__
    0, 11
#else
    0, 13
#endif
};

/**
 * Draws the automap options menu
 */
void M_DrawMapMenu(void)
{
    static char*        hudviewnames[3] = { "NONE", "CURRENT", "STATUSBAR" };
    static char*        yesno[2] = { "NO", "YES" };
    static char*        customColors[3] = { "NEVER", "AUTO", "ALWAYS" };
    float               menuAlpha;
    uint                idx;
#if __JHERETIC__ || __JHEXEN__
    char*               token;
    int                 page;
#endif
    const menu_t*       menu = &MapDef;

    menuAlpha = Hu_MenuAlpha();

    M_DrawTitle("Automap OPTIONS", menu->y - 26);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());

    // Draw the page arrows.
    token = (!menu->firstItem || menuTime & 8) ? "invgeml2" : "invgeml1";
    GL_DrawPatch_CS(menu->x, menu->y - 22, W_GetNumForName(token));
    token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
             menuTime & 8) ? "invgemr2" : "invgemr1";
    GL_DrawPatch_CS(312 - menu->x, menu->y - 22, W_GetNumForName(token));
#endif

    idx = menu->firstItem;
#if __JHERETIC__ || __JHEXEN__
    page = menu->firstItem / menu->numVisItems + 1;
    if(page == 2)
        goto page2;
#endif

#if __JHERETIC__ || __JHEXEN__
    idx++;
#endif
    MN_DrawSlider(menu, idx++, 11, cfg.automapOpacity * 10 + .5f);
#if __JHERETIC__ || __JHEXEN__
    idx+= 2;
#endif
    MN_DrawSlider(menu, idx++, 11, cfg.automapLineAlpha * 10 + .5f);
#if __JHERETIC__ || __JHEXEN__
    idx++;
#endif
    M_WriteMenuText(menu, idx++, hudviewnames[cfg.automapHudDisplay % 3]);
    M_WriteMenuText(menu, idx++, yesno[cfg.automapShowDoors]);
#if __JHERETIC__ || __JHEXEN__
    idx++;
#endif
    MN_DrawSlider(menu, idx++, 21, (cfg.automapDoorGlow - 1) / 10 + .5f );
#if __JHERETIC__ || __JHEXEN__
    idx++;
#endif
    idx++;
#if __JHERETIC__ || __JHEXEN__
    return;

page2:
#endif
    M_WriteMenuText(menu, idx++, customColors[cfg.automapCustomColors % 3]);
    MN_DrawColorBox(menu, idx++, cfg.automapL1[0], cfg.automapL1[1], cfg.automapL1[2], 1);
    MN_DrawColorBox(menu, idx++, cfg.automapL2[0], cfg.automapL2[1], cfg.automapL2[2], 1);
    MN_DrawColorBox(menu, idx++, cfg.automapL3[0], cfg.automapL3[1], cfg.automapL3[2], 1);
    MN_DrawColorBox(menu, idx++, cfg.automapL0[0], cfg.automapL0[1], cfg.automapL0[2], 1);
    MN_DrawColorBox(menu, idx++, cfg.automapMobj[0], cfg.automapMobj[1], cfg.automapMobj[2], 1);
    MN_DrawColorBox(menu, idx, cfg.automapBack[0], cfg.automapBack[1], cfg.automapBack[2], 1);
}

/**
 * Set automap line alpha
 */
void M_MapOpacity(int option, void* data)
{
    M_FloatMod10(&cfg.automapOpacity, option);
}

/**
 * Set automap line alpha
 */
void M_MapLineAlpha(int option, void* data)
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

void M_MapCustomColors(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.automapCustomColors < 2)
            cfg.automapCustomColors++;
    }
    else if(cfg.automapCustomColors > 0)
        cfg.automapCustomColors--;
}
