/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "p_automap.h"
#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

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

// TYPES -------------------------------------------------------------------

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

static void registerSpecialLine(automapcfg_t* cfg, int cheatLevel, int lineSpecial,
                            int sided, float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView);
static void setColorAndAlpha(automapcfg_t* cfg, int objectname, float r,
                             float g, float b, float a);

static void     findMinMaxBoundaries(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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
    {"map-open-timer", CVF_NO_MAX, CVT_FLOAT, &cfg.automapOpenSeconds, 0, 0},
    {"rend-dev-freeze-map", CVF_NO_ARCHIVE, CVT_BYTE, &freezeMapRLs, 0, 1},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static automap_t automaps[MAXPLAYERS];
static automapcfg_t automapCFGs[MAXPLAYERS];

static vectorgrap_t* vectorGraphs[NUM_VECTOR_GRAPHS];

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

static __inline automapcfg_t* getAutomapCFG(automapid_t id)
{
    if(id == 0 || id > MAXPLAYERS)
    {
#if _DEBUG
Con_Error("getAutomapCFG: Invalid map id %i.", id);
#endif
        return NULL;
    }

    return &automapCFGs[id-1];
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
    return getAutomapCFG(id);
}

const mapobjectinfo_t* AM_GetMapObjectInfo(automapid_t id, int objectname)
{
    automapcfg_t*           cfg;

    if(objectname == AMO_NONE)
        return NULL;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("getMapObjectInfo: Unknown object %i.", objectname);

    if(!(cfg = getAutomapCFG(id)))
        return NULL;

    switch(objectname)
    {
    case AMO_UNSEENLINE:
        return &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];

    case AMO_SINGLESIDEDLINE:
        return &cfg->mapObjectInfo[MOL_LINEDEF];

    case AMO_TWOSIDEDLINE:
        return &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];

    case AMO_FLOORCHANGELINE:
        return &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];

    case AMO_CEILINGCHANGELINE:
        return &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];

    default:
        Con_Error("AM_GetMapObjectInfo: No info for object %i.", objectname);
    }

    return NULL;
}

const mapobjectinfo_t* AM_GetInfoForSpecialLine(automapid_t id, int special,
                                                const sector_t* frontsector,
                                                const sector_t* backsector)
{
    mapobjectinfo_t*    info = NULL;

    if(special > 0)
    {
        uint                i;
        automapcfg_t*       cfg;

        if(!(cfg = getAutomapCFG(id)))
            return NULL;

        for(i = 0; i < cfg->numSpecialLines && !info; ++i)
        {
            automapspecialline_t *sl = &cfg->specialLines[i];

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
            if(sl->cheatLevel > cfg->cheating)
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

static void initAutomapConfig(int player)
{
    automapcfg_t*       mcfg = &automapCFGs[player];
    float               rgb[3];
    boolean             customPal =
        !W_IsFromIWAD(W_GetNumForName("PLAYPAL"));

    // Initialize.
    // \fixme Put these values into an array (or read from a lump?).
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

    // Register lines we want to display in a special way.
#if __JDOOM__ || __JDOOM64__
    // Blue locked door, open.
    registerSpecialLine(mcfg, 0, 32, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Blue locked door, locked.
    registerSpecialLine(mcfg, 0, 26, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 99, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 133, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Red locked door, open.
    registerSpecialLine(mcfg, 0, 33, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Red locked door, locked.
    registerSpecialLine(mcfg, 0, 28, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 134, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 135, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Yellow locked door, open.
    registerSpecialLine(mcfg, 0, 34, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Yellow locked door, locked.
    registerSpecialLine(mcfg, 0, 27, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 136, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 137, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Exit switch.
    registerSpecialLine(mcfg, 1, 11, 1, 0, 1, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Exit cross line.
    registerSpecialLine(mcfg, 1, 52, 2, 0, 1, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Secret Exit switch.
    registerSpecialLine(mcfg, 1, 51, 1, 0, 1, 1, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Secret Exit cross line.
    registerSpecialLine(mcfg, 2, 124, 2, 0, 1, 1, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#elif __JHERETIC__
    // Blue locked door.
    registerSpecialLine(mcfg, 0, 26, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Blue switch?
    registerSpecialLine(mcfg, 0, 32, 0, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Yellow locked door.
    registerSpecialLine(mcfg, 0, 27, 2, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Yellow switch?
    registerSpecialLine(mcfg, 0, 34, 0, .905f, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Green locked door.
    registerSpecialLine(mcfg, 0, 28, 2, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Green switch?
    registerSpecialLine(mcfg, 0, 33, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#elif __JHEXEN__
    // A locked door (all are green).
    registerSpecialLine(mcfg, 0, 13, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 83, 0, 0, .9f, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Intra-map teleporters (all are blue).
    registerSpecialLine(mcfg, 0, 70, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 71, 2, 0, 0, .776f, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Inter-map teleport.
    registerSpecialLine(mcfg, 0, 74, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
    // Game-winning exit.
    registerSpecialLine(mcfg, 0, 75, 2, .682f, 0, 0, 1, BM_NORMAL, TWOSIDED_GLOW, .75f, 5, true);
#endif

    AM_SetVectorGraphic(mcfg, AMO_THING, VG_TRIANGLE);
    /*AM_GetMapColor(rgb, cfg.automapMobj, THINGCOLORS, customPal);
    setColorAndAlpha(mcfg, AMO_THING, rgb[0], rgb[1], rgb[2], 1);*/
    AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_ARROW);

#if __JHERETIC__ || __JHEXEN__
    if(W_CheckNumForName("AUTOPAGE") == -1)
    {
        setColorAndAlpha(mcfg, AMO_BACKGROUND, .55f, .45f, .35f,
                         cfg.automapOpacity);
    }
    else
    {
        AM_GetMapColor(rgb, cfg.automapBack, WHITE, customPal);
        setColorAndAlpha(mcfg, AMO_BACKGROUND, rgb[0], rgb[1], rgb[2],
                         cfg.automapOpacity);
    }
#else
    AM_GetMapColor(rgb, cfg.automapBack, BACKGROUND, customPal);
    setColorAndAlpha(mcfg, AMO_BACKGROUND, rgb[0], rgb[1], rgb[2],
                        cfg.automapOpacity);
#endif

    AM_GetMapColor(rgb, cfg.automapL0, GRAYS+3, customPal);
    setColorAndAlpha(mcfg, AMO_UNSEENLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL1, WALLCOLORS, customPal);
    setColorAndAlpha(mcfg, AMO_SINGLESIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL0, TSWALLCOLORS, customPal);
    setColorAndAlpha(mcfg, AMO_TWOSIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL2, FDWALLCOLORS, customPal);
    setColorAndAlpha(mcfg, AMO_FLOORCHANGELINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL3, CDWALLCOLORS, customPal);
    setColorAndAlpha(mcfg, AMO_CEILINGCHANGELINE, rgb[0], rgb[1], rgb[2], 1);

    // Setup map config based on player's config.
    // \todo All players' maps work from the same config!
    mcfg->followPlayer = player;
    mcfg->lineGlowScale = cfg.automapDoorGlow;
    mcfg->glowingLineSpecials = cfg.automapShowDoors;
    mcfg->panSpeed = cfg.automapPanSpeed;
    mcfg->panResetOnOpen = cfg.automapPanResetOnOpen;
    mcfg->zoomSpeed = cfg.automapZoomSpeed;
    mcfg->openSeconds = cfg.automapOpenSeconds;
}

/**
 * Called during init.
 */
void AM_Init(void)
{
    uint                i;
    float               scrwidth, scrheight;

    memset(vectorGraphs, 0, sizeof(vectorGraphs));

    scrwidth = Get(DD_WINDOW_WIDTH);
    scrheight = Get(DD_WINDOW_HEIGHT);

    Rend_AutomapInit();
    Rend_AutomapLoadData();

    memset(&automaps, 0, sizeof(automaps));
    memset(&automapCFGs, 0, sizeof(automapCFGs));

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        automap_t*          map = &automaps[i];

        initAutomapConfig(i);

        // Initialize the map.
        map->oldViewScale = 1;
        map->window.oldX = map->window.x = 0;
        map->window.oldY = map->window.y = 0;
        map->window.oldWidth = map->window.width = scrwidth;
        map->window.oldHeight = map->window.height = scrheight;
        map->alpha = map->targetAlpha = map->oldAlpha = 0;

        Automap_SetViewScaleTarget(map, 1);
        Automap_SetViewRotate(map, cfg.automapRotate);
        Automap_SetMaxLocationTargetDelta(map, 128); // In world units.
        Automap_SetWindowTarget(map, 0, 0, scrwidth, scrheight);
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
        automapcfg_t*       mcfg = &automapCFGs[i];

        mcfg->revealed = false;

        Automap_SetWindowFullScreenMode(map, true);

        // Determine the map view scale factors.
        Automap_SetViewScaleTarget(map, map->forceMaxScale? 0 : .45f); // zero clamped to minScaleMTOF
        Automap_ClearMarks(map);

#if !__JHEXEN__
        if(gameSkill == SM_BABY && cfg.automapBabyKeys)
            map->flags |= AMF_REND_KEYS;

        if(!IS_NETGAME && mcfg->cheating)
            AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_CHEATARROW);
#endif
        // If the map has been left open; close it.
        AM_Open(AM_MapForPlayer(i), false, true);

        // Reset position onto the follow player.
        if(players[mcfg->followPlayer].plr->mo)
        {
            mobj_t*             mo = players[mcfg->followPlayer].plr->mo;

            Automap_SetLocationTarget(map, mo->pos[VX], mo->pos[VY]);
        }
    }

    Rend_AutomapInitForMap();
}

/**
 * Start the automap.
 */
void AM_Open(automapid_t id, boolean yes, boolean fast)
{
    automap_t*          map;
    automapcfg_t*       mcfg;

    if(G_GetGameState() != GS_MAP)
        return;

    if(!(mcfg = getAutomapCFG(id)))
        return;

    if(!players[mcfg->followPlayer].plr->inGame)
        return;
    map = getAutomap(id);

    if(yes)
    {
        if(Automap_IsActive(map))
            return; // Already active.

        DD_Execute(true, "activatebcontext map");
        if(map->panMode)
            DD_Execute(true, "activatebcontext map-freepan");
    }
    else
    {
        if(!Automap_IsActive(map))
            return; // Already in-active.

        DD_Execute(true, "deactivatebcontext map");
        DD_Execute(true, "deactivatebcontext map-freepan");
    }

    Automap_Open(map, yes, fast);

    if(yes)
    {
        if(!players[mcfg->followPlayer].plr->inGame)
        {   // Set viewer target to the center of the map.
            float               aabb[4];
            Automap_GetInViewAABB(map, &aabb[BOXLEFT], &aabb[BOXRIGHT],
                                  &aabb[BOXBOTTOM], &aabb[BOXTOP]);
            Automap_SetLocationTarget(map, (aabb[BOXRIGHT] - aabb[BOXLEFT]) / 2,
                                 (aabb[BOXTOP] - aabb[BOXBOTTOM]) / 2);
            Automap_SetViewAngleTarget(map, 0);
        }
        else
        {   // The map's target player is available.
            mobj_t*             mo = players[mcfg->followPlayer].plr->mo;

            if(!(map->panMode && !mcfg->panResetOnOpen))
                Automap_SetLocationTarget(map, mo->pos[0], mo->pos[1]);

            if(map->panMode && mcfg->panResetOnOpen)
            {
                float               angle;

                /* $unifiedangles */
                if(map->rotate)
                    angle = mo->angle / (float) ANGLE_MAX * -360 - 90;
                else
                    angle = 0;
                Automap_SetViewAngleTarget(map, angle);
            }
        }
    }
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

    return Automap_MapToFrame(map, val);
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

    return Automap_FrameToMap(map, val);
}

void AM_SetWindowTarget(automapid_t id, int x, int y, int w, int h)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    Automap_SetWindowTarget(map, x, y, w, h);
}

void AM_GetWindow(automapid_t id, float* x, float* y, float* w, float* h)
{
    automap_t*          map;

    if(IS_DEDICATED)
        Con_Error("AM_GetWindow: Not available in dedicated mode.");

    if(!(map = getAutomap(id)))
        return;

    Automap_GetWindow(map, x, y, w, h);
}

void AM_SetWindowFullScreenMode(automapid_t id, int value)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    Automap_SetWindowFullScreenMode(map, value);
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

void AM_SetViewTarget(automapid_t id, float x, float y)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;

    Automap_SetLocationTarget(map, x, y);
}

void AM_GetViewPosition(automapid_t id, float* x, float* y)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    Automap_GetLocation(map, x, y);
}

void AM_GetViewParallaxPosition(automapid_t id, float* x, float* y)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    Automap_GetViewParallaxPosition(map, x, y);
}

float AM_ViewAngle(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return 0;

    return Automap_GetViewAngle(map);
}

void AM_SetViewScaleTarget(automapid_t id, float scale)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    Automap_SetViewScaleTarget(map, scale);
}

void AM_SetViewAngleTarget(automapid_t id, float angle)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return;

    Automap_SetViewAngleTarget(map, angle);
}

float AM_MapToFrameMultiplier(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return 1;

    return Automap_MapToFrameMultiplier(map);
}

boolean AM_IsActive(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return false; // Never.

    if(!(map = getAutomap(id)))
        return false;

    return Automap_IsActive(map);
}

void AM_SetViewRotate(automapid_t id, int offOnToggle)
{
    automap_t*          map;
    automapcfg_t*       mcfg;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;
    mcfg = getAutomapCFG(id);

    if(offOnToggle == 2)
        cfg.automapRotate = !cfg.automapRotate;
    else
        cfg.automapRotate = (offOnToggle? true : false);

    Automap_SetViewRotate(map, cfg.automapRotate);

    P_SetMessage(&players[mcfg->followPlayer],
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
    automapcfg_t*       mcfg;
    xline_t*            xline;

    if(!(mcfg = getAutomapCFG(id)))
        return;

    if(lineIdx >= numlines)
        return;

    xline = P_GetXLine(lineIdx);

    // Will we need to rebuild one or more display lists?
    if(xline->mapped[mcfg->followPlayer] != visible)
        Rend_AutomapRebuild(id - 1);

    xline->mapped[mcfg->followPlayer] = visible;
}

void AM_RevealMap(automapid_t id, boolean on)
{
    automapcfg_t*       cfg;

    if(!(cfg = getAutomapCFG(id)))
        return;

    if(cfg->revealed != on)
    {
        cfg->revealed = on;
        Rend_AutomapRebuild(id - 1);
    }
}

boolean AM_IsRevealed(automapid_t id)
{
    automapcfg_t*       cfg;

    if(!(cfg = getAutomapCFG(id)))
        return false;

    return cfg->revealed;
}

/**
 * Clears markpoint array.
 */
void AM_ClearMarks(automapid_t id)
{
    automap_t*          map;
    automapcfg_t*       mcfg;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(map = getAutomap(id)))
        return;
    mcfg = getAutomapCFG(id);

    Automap_ClearMarks(map);

    P_SetMessage(&players[mcfg->followPlayer], AMSTR_MARKSCLEARED, false);
    Con_Printf("All markers cleared on automap.\n");
}

/**
 * Adds a marker at the specified X/Y location.
 */
int AM_AddMark(automapid_t id, float x, float y, float z)
{
    static char         buffer[20];
    automap_t*          map;
    int                 newMark;

    if(!(map = getAutomap(id)))
        return -1;

    newMark = Automap_AddMark(map, x, y, z);
    if(newMark != -1)
    {
        automapcfg_t*       mcfg = getAutomapCFG(id);

        sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, newMark);
        P_SetMessage(&players[mcfg->followPlayer], buffer, false);
    }

    return newMark;
}

boolean AM_GetMark(automapid_t id, uint mark, float* x, float* y, float* z)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return false;

    return Automap_GetMark(map, mark, x, y, z);
}

void AM_ToggleZoomMax(automapid_t id)
{
    automap_t*          map;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;

    Automap_ToggleZoomMax(map);

    Con_Printf("Maximum zoom %s in automap.\n", map->forceMaxScale? "ON":"OFF");
}

void AM_ToggleFollow(automapid_t id)
{
    automap_t*          map;
    automapcfg_t*       mcfg;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(map = getAutomap(id)))
        return;
    mcfg = getAutomapCFG(id);

    Automap_ToggleFollow(map);

    // Enable/disable the pan mode binding class
    DD_Executef(true, "%sactivatebcontext map-freepan",
                !map->panMode? "de" : "");

    P_SetMessage(&players[mcfg->followPlayer],
                 (map->panMode ? AMSTR_FOLLOWOFF : AMSTR_FOLLOWON), false);
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

    return Automap_GetOpacity(map);
}

int AM_GetFlags(automapid_t id)
{
    automap_t*          map;

    if(!(map = getAutomap(id)))
        return 0;

    return Automap_GetFlags(map);
}

static void setColor(automapcfg_t* cfg, int objectname, float r, float g,
                     float b)
{
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
        cfg->backgroundRGBA[0] = r;
        cfg->backgroundRGBA[1] = g;
        cfg->backgroundRGBA[2] = b;
        return;
    }

    // It must be an object with a name.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];
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
    Rend_AutomapRebuild(cfg - automapCFGs);
}

void AM_SetColor(automapid_t id, int objectname, float r, float g, float b)
{
    automapcfg_t*       cfg;

    if(IS_DEDICATED)
        return; // Ignore.

    if(!(cfg = getAutomapCFG(id)))
        return;

    setColor(cfg, objectname, r, g, b);
}

void AM_GetColor(automapid_t id, int objectname, float* r, float* g, float* b)
{
    automapcfg_t*       cfg;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        Con_Error("AM_GetColor: Not available in dedicated mode.");

    if(!(cfg = getAutomapCFG(id)))
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColor: Unknown object %i.", objectname);

    // Check special cases first.
    if(objectname == AMO_BACKGROUND)
    {
        if(r) *r = cfg->backgroundRGBA[0];
        if(g) *g = cfg->backgroundRGBA[1];
        if(b) *b = cfg->backgroundRGBA[2];
        return;
    }

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];
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

static void setColorAndAlpha(automapcfg_t* cfg, int objectname, float r,
                             float g, float b, float a)
{
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
        cfg->backgroundRGBA[0] = r;
        cfg->backgroundRGBA[1] = g;
        cfg->backgroundRGBA[2] = b;
        cfg->backgroundRGBA[3] = a;
        return;
    }

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];
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
    Rend_AutomapRebuild(cfg - automapCFGs);
}

void AM_SetColorAndAlpha(automapid_t id, int objectname, float r, float g,
                         float b, float a)
{
    automapcfg_t*       cfg;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(cfg = getAutomapCFG(id)))
        return;

    setColorAndAlpha(cfg, objectname, r, g, b, a);
}

void AM_GetColorAndAlpha(automapid_t id, int objectname, float* r, float* g,
                         float* b, float* a)
{
    automapcfg_t*       cfg;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        Con_Error("AM_GetColorAndAlpha: Not available in dedicated mode.");

    if(!(cfg = getAutomapCFG(id)))
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_GetColorAndAlpha: Unknown object %i.", objectname);

    // Check special cases first.
    if(objectname == AMO_BACKGROUND)
    {
        if(r) *r = cfg->backgroundRGBA[0];
        if(g) *g = cfg->backgroundRGBA[1];
        if(b) *b = cfg->backgroundRGBA[2];
        if(a) *a = cfg->backgroundRGBA[3];
        return;
    }

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];
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
    automapcfg_t*       cfg;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(cfg = getAutomapCFG(id)))
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetBlendmode: Unknown object %i.", objectname);

    // It must be an object with an info.
    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_SetBlendmode: Object %i does not support blending modes.",
                  objectname);
        break;
    }

    info->blendMode = blendmode;

    // We will need to rebuild one or more display lists.
    Rend_AutomapRebuild(id - 1);
}

void AM_SetGlow(automapid_t id, int objectname, glowtype_t type, float size,
                float alpha, boolean canScale)
{
    automapcfg_t*       cfg;
    mapobjectinfo_t*    info;

    if(IS_DEDICATED)
        return; // Just ignore.

    if(!(cfg = getAutomapCFG(id)))
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetGlow: Unknown object %i.", objectname);

    size = MINMAX_OF(0, size, 100);
    alpha = MINMAX_OF(0, alpha, 1);

    switch(objectname)
    {
    case AMO_UNSEENLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &cfg->mapObjectInfo[MOL_LINEDEF_CEILING];
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
    Rend_AutomapRebuild(id - 1);
}

void AM_SetVectorGraphic(automapcfg_t* cfg, int objectname, int vgname)
{
    if(!cfg)
        return;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_SetVectorGraphic: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_THING:
        cfg->vectorGraphicForThing = vgname;
        break;

    case AMO_THINGPLAYER:
        cfg->vectorGraphicForPlayer = vgname;
        break;

    default:
        Con_Error("AM_SetVectorGraphic: Object %i does not support vector "
                  "graphic.", objectname);
        break;
    }
}

vectorgrapname_t AM_GetVectorGraphic(const automapcfg_t* cfg, int objectname)
{
    if(!cfg)
        return VG_NONE;

    if(objectname < 0 || objectname >= AMO_NUMOBJECTS)
        Con_Error("AM_GetVectorGraphic: Unknown object %i.", objectname);

    switch(objectname)
    {
    case AMO_THING:
        return cfg->vectorGraphicForThing;

    case AMO_THINGPLAYER:
        return cfg->vectorGraphicForPlayer;

    default:
        Con_Error("AM_GetVectorGraphic: Object %i does not support vector "
                  "graphic.", objectname);
        break;
    }

    return VG_NONE;
}

static void registerSpecialLine(automapcfg_t* cfg, int cheatLevel, int lineSpecial,
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
    while(i < cfg->numSpecialLines && !line)
    {
        p = &cfg->specialLines[i];
        if(p->special == lineSpecial && p->cheatLevel == cheatLevel)
            line = p;
        else
            i++;
    }

    if(!line) // It must be a new one.
    {
        // Any room for a new special line?
        if(cfg->numSpecialLines >= AM_MAXSPECIALLINES)
            Con_Error("AM_RegisterSpecialLine: No available slot.");

        line = &cfg->specialLines[cfg->numSpecialLines++];
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
    Rend_AutomapRebuild(cfg - automapCFGs);
}

void AM_RegisterSpecialLine(automapid_t id, int cheatLevel, int lineSpecial,
                            int sided, float r, float g, float b, float a,
                            blendmode_t blendmode,
                            glowtype_t glowType, float glowAlpha,
                            float glowWidth, boolean scaleGlowWithView)
{
    automapcfg_t*       cfg;

    if(!(cfg = getAutomapCFG(id)))
        return;

    if(cheatLevel < 0 || cheatLevel > 4)
        Con_Error("AM_RegisterSpecialLine: cheatLevel '%i' out of range {0-4}.",
                  cheatLevel);
    if(lineSpecial < 0)
        Con_Error("AM_RegisterSpecialLine: lineSpecial '%i' is negative.",
                  lineSpecial);
    if(sided < 0 || sided > 2)
        Con_Error("AM_RegisterSpecialLine: sided '%i' is invalid.", sided);

    registerSpecialLine(cfg, cheatLevel, lineSpecial, sided, r, g, b, a,
                        blendmode, glowType, glowAlpha, glowWidth,
                        scaleGlowWithView);
}

void AM_SetCheatLevel(automapid_t id, int level)
{
    int                 flags;
    automap_t*          map;
    automapcfg_t*       mcfg;

    if(!(map = getAutomap(id)))
        return;
    mcfg = getAutomapCFG(id);
    mcfg->cheating = level;

    flags = Automap_GetFlags(map);
    if(mcfg->cheating >= 1)
        flags |= AMF_REND_ALLLINES;
    else
        flags &= ~AMF_REND_ALLLINES;

    if(mcfg->cheating == 2)
        flags |= (AMF_REND_THINGS | AMF_REND_XGLINES);
    else
        flags &= ~(AMF_REND_THINGS | AMF_REND_XGLINES);

    if(mcfg->cheating >= 2)
        flags |= (AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);
    else
        flags &= ~(AMF_REND_VERTEXES | AMF_REND_LINE_NORMALS);

    Automap_SetFlags(map, flags);

    // We will need to rebuild one or more display lists.
    Rend_AutomapRebuild(id - 1);
}

void AM_IncMapCheatLevel(automapid_t id)
{
    int                 flags;
    automap_t*          map;
    automapcfg_t*       mcfg;

    if(!(map = getAutomap(id)))
        return;
    mcfg = getAutomapCFG(id);
    mcfg->cheating = (mcfg->cheating + 1) % 3;

    flags = Automap_GetFlags(map);
    if(mcfg->cheating)
        flags |= AMF_REND_ALLLINES;
    else
        flags &= ~AMF_REND_ALLLINES;

    if(mcfg->cheating == 2)
        flags |= (AMF_REND_THINGS | AMF_REND_XGLINES);
    else
        flags &= ~(AMF_REND_THINGS | AMF_REND_XGLINES);

    Automap_SetFlags(map, flags);

    // We will need to rebuild one or more display lists.
    Rend_AutomapRebuild(id - 1);
}

/**
 * Determines bounding box of all the map's vertexes.
 */
static void findMinMaxBoundaries(void)
{
    uint                i;
    float               pos[2], lowX, hiX, lowY, hiY;

    lowX = lowY = DDMAXFLOAT;
    hiX = hiY = -DDMAXFLOAT;

    for(i = 0; i < numvertexes; ++i)
    {
        P_GetFloatv(DMU_VERTEX, i, DMU_XY, pos);

        if(pos[VX] < lowX)
            lowX =  pos[VX];
        else if(pos[VX] > hiX)
            hiX = pos[VX];

        if(pos[VY] < lowY)
            lowY = pos[VY];
        else if(pos[VY] > hiY)
            hiY = pos[VY];
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        automap_t*          map = &automaps[i];

        Automap_SetMinScale(map, 2 * PLAYERRADIUS);
        Automap_SetWorldBounds(map, lowX, hiX, lowY, hiY);
    }
}

/**
 * Called each tic for each player's automap if they are in-game.
 */
static void mapTicker(automap_t* map)
{
    int                 playerNum;
    float               diff = 0, panX[2], panY[2],
                        zoomVel, zoomSpeed;
    player_t*           mapPlayer;
    mobj_t*             mo;
    automapcfg_t*       mcfg;
    float               scrwidth = DD_GetInteger(DD_WINDOW_WIDTH);
    float               scrheight = DD_GetInteger(DD_WINDOW_HEIGHT);
    float               newX, newY, newWidth, newHeight;

    if(!map)
        return;

    playerNum = map - automaps;
    mapPlayer = &players[playerNum];
    mcfg = &automapCFGs[playerNum];
    mo = players[mcfg->followPlayer].plr->mo;

    // Check the state of the controls. Done here so that offsets don't accumulate
    // unnecessarily, as they would, if left unread.
    P_GetControlState(playerNum, CTL_MAP_PAN_X, &panX[0], &panX[1]);
    P_GetControlState(playerNum, CTL_MAP_PAN_Y, &panY[0], &panY[1]);

    if(!(/*(mapPlayer->plr->flags & DDPF_LOCAL) &&*/ mapPlayer->plr->inGame))
        return;

    // Move towards the target alpha level for the automap.
    if(++map->alphaTimer < mcfg->openSeconds * TICSPERSEC)
        map->alpha = LERP(map->oldAlpha, map->targetAlpha,
                          map->alphaTimer / (mcfg->openSeconds * TICSPERSEC));
    else
        map->alpha = map->targetAlpha;

    // If the automap is not active do nothing else.
    if(!map->active)
        return;

    //
    // Update per tic, driven controls.
    //

    // Map view zoom contol.
    zoomSpeed = (1 + mcfg->zoomSpeed);
    if(players[playerNum].brain.speed)
        zoomSpeed *= 1.5f;
    P_GetControlState(playerNum, CTL_MAP_ZOOM, &zoomVel, NULL); // ignores rel offset -jk
    if(zoomVel > 0)  // zoom in
    {
        Automap_SetViewScaleTarget(map, map->viewScale * zoomSpeed);
    }
    else if(zoomVel < 0) // zoom out
    {
        Automap_SetViewScaleTarget(map, map->viewScale / zoomSpeed);
    }

    // Map viewer location paning control.
    if(map->panMode || !players[mcfg->followPlayer].plr->inGame)
    {
        float               xy[2] = { 0, 0 }; // deltas
        float               scrwidth = Get(DD_WINDOW_WIDTH);
        float               scrheight = Get(DD_WINDOW_HEIGHT);
        // DOOM.EXE pans the automap at 140 fixed pixels per second.
        float       panUnitsPerTic = (Automap_FrameToMap(map, FIXXTOSCREENX(140)) / TICSPERSEC) *
                        (2 * mcfg->panSpeed);

        if(panUnitsPerTic < 8)
            panUnitsPerTic = 8;

        xy[VX] = panX[0] * panUnitsPerTic + panX[1];
        xy[VY] = panY[0] * panUnitsPerTic + panY[1];

        V2_Rotate(xy, map->angle / 360 * 2 * PI);

        if(xy[VX] || xy[VY])
            Automap_SetLocationTarget(map, map->viewX + xy[VX],
                                  map->viewY + xy[VY]);
    }
    else  // Camera follows the player
    {
        float               angle;

        Automap_SetLocationTarget(map, mo->pos[VX], mo->pos[VY]);

        /* $unifiedangles */
        if(map->rotate)
            angle = mo->angle / (float) ANGLE_MAX * 360 - 90;
        else
            angle = 0;
        Automap_SetViewAngleTarget(map, angle);
    }

    // Determine whether the available space has changed and thus whether
    // the position and/or size of the automap must therefore change too.
    R_GetViewWindow(&newX, &newY, &newWidth, &newHeight);
    Automap_UpdateWindow(map, FIXXTOSCREENX(newX), FIXYTOSCREENY(newY),
                         FIXXTOSCREENX(newWidth), FIXYTOSCREENY(newHeight));

    Automap_RunTic(map);
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

void AM_Drawer(int player)
{
    if(IS_DEDICATED)
        return; // Nothing to do.

    if(player < 0 || player > MAXPLAYERS)
        return;

    Rend_Automap(player, getAutomap(AM_MapForPlayer(player)));
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
    GF_FONTA,
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

    idx = 0;
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
    idx++;

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

    Automap_SetViewRotate(getAutomap(AM_MapForPlayer(CONSOLEPLAYER)),
                          cfg.automapRotate);
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
