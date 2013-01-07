/**\file am_map.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "p_mapsetup.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "p_player.h"
#include "hu_automap.h"

static void registerSpecialLine(automapcfg_t* mcfg, int reqAutomapFlags, int reqSpecial,
    int reqSided, int reqNotFlagged, float r, float g, float b, float a, blendmode_t blendmode,
    glowtype_t glowType, float glowStrength, float glowSize, boolean scaleGlowWithView);

static automapcfg_t automapCFG;

automapcfg_t* ST_AutomapConfig(void)
{
    return &automapCFG;
}

void AM_GetMapColor(float* rgb, const float* uColor, int palidx, boolean customPal)
{
    if((!customPal && !cfg.automapCustomColors) ||
       (customPal && cfg.automapCustomColors != 2))
    {
        R_GetColorPaletteRGBf(0, palidx, rgb, false);
        return;
    }

    rgb[0] = uColor[0];
    rgb[1] = uColor[1];
    rgb[2] = uColor[2];
}

const automapcfg_lineinfo_t* AM_GetInfoForLine(automapcfg_t* mcfg, automapcfg_objectname_t name)
{
    assert(mcfg);

    if(name == AMO_NONE)
        return NULL;

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_GetInfoForLine: Unknown object %i.", (int) name);

    switch(name)
    {
    case AMO_UNSEENLINE:
        return &mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];

    case AMO_SINGLESIDEDLINE:
        return &mcfg->mapObjectInfo[MOL_LINEDEF];

    case AMO_TWOSIDEDLINE:
        return &mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];

    case AMO_FLOORCHANGELINE:
        return &mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR];

    case AMO_CEILINGCHANGELINE:
        return &mcfg->mapObjectInfo[MOL_LINEDEF_CEILING];

    default:
        Con_Error("AM_GetInfoForLine: No info for object %i.", (int) name);
    }

    return NULL;
}

const automapcfg_lineinfo_t* AM_GetInfoForSpecialLine(automapcfg_t* mcfg, int special,
    int flags, const Sector* frontsector, const Sector* backsector, int automapFlags)
{
    DENG_ASSERT(mcfg);
{
    if(special > 0)
    {
        automapcfg_lineinfo_t* info;
        uint i;
        for(i = 0, info = mcfg->lineInfo; i < mcfg->lineInfoCount; ++i, info++)
        {
            // Special restriction?
            if(info->reqSpecial != special) continue;

            // Sided restriction?
            if((info->reqSided == 1 && backsector && frontsector) ||
               (info->reqSided == 2 && (!backsector || !frontsector)))
                continue;

            // Line flags restriction?
            if(info->reqNotFlagged && (flags & info->reqNotFlagged)) continue;

            // Automap flags restriction?
            if(info->reqAutomapFlags != 0 && !(automapFlags & info->reqAutomapFlags)) continue;

            // This is the one!
            return info;
        }
    }
    return 0; // Not found.
}}

static void initAutomapConfig(automapcfg_t* mcfg)
{
    assert(mcfg != NULL);
    {
    float rgb[3];

    memset(mcfg, 0, sizeof(automapcfg_t));
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].glow = GLOW_NONE;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowStrength = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowSize = 10;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].blendMode = BM_NORMAL;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].scaleWithView = false;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[0] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[1] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[2] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[3] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF].glow = GLOW_NONE;
    mcfg->mapObjectInfo[MOL_LINEDEF].glowStrength = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF].glowSize = 10;
    mcfg->mapObjectInfo[MOL_LINEDEF].blendMode = BM_NORMAL;
    mcfg->mapObjectInfo[MOL_LINEDEF].scaleWithView = false;
    mcfg->mapObjectInfo[MOL_LINEDEF].rgba[0] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF].rgba[1] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF].rgba[2] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF].rgba[3] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glow = GLOW_NONE;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowStrength = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowSize = 10;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].blendMode = BM_NORMAL;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].scaleWithView = false;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[0] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[1] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[2] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[3] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].glow = GLOW_NONE;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].glowStrength = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].glowSize = 10;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].blendMode = BM_NORMAL;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].scaleWithView = false;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[0] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[1] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[2] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[3] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].glow = GLOW_NONE;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].glowStrength = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].glowSize = 10;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].blendMode = BM_NORMAL;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].scaleWithView = false;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[0] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[1] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[2] = 1;
    mcfg->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[3] = 1;

    // Register lines we want to display in a special way.
#if __JDOOM__ || __JDOOM64__
    // Blue locked door, open.
    registerSpecialLine(mcfg, 0,  32, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Blue locked door, locked.
    registerSpecialLine(mcfg, 0,  26, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0,  99, 0, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 133, 0, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Red locked door, open.
    registerSpecialLine(mcfg, 0,  33, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Red locked door, locked.
    registerSpecialLine(mcfg, 0,  28, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 134, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 135, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow locked door, open.
    registerSpecialLine(mcfg, 0,  34, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow locked door, locked.
    registerSpecialLine(mcfg, 0,  27, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 136, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 137, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Exit switch.
    registerSpecialLine(mcfg, AMF_REND_SPECIALLINES,  11, 1, ML_SECRET, 0, 1, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Exit cross line.
    registerSpecialLine(mcfg, AMF_REND_SPECIALLINES,  52, 2, ML_SECRET, 0, 1, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Secret Exit switch.
    registerSpecialLine(mcfg, AMF_REND_SPECIALLINES,  51, 1, ML_SECRET, 0, 1, 1, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Secret Exit cross line.
    registerSpecialLine(mcfg, AMF_REND_SPECIALLINES, 124, 2, ML_SECRET, 0, 1, 1, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
#elif __JHERETIC__
    // Blue locked door.
    registerSpecialLine(mcfg, 0, 26, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Blue switch?
    registerSpecialLine(mcfg, 0, 32, 0, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow locked door.
    registerSpecialLine(mcfg, 0, 27, 2, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Yellow switch?
    registerSpecialLine(mcfg, 0, 34, 0, ML_SECRET, .905f, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Green locked door.
    registerSpecialLine(mcfg, 0, 28, 2, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Green switch?
    registerSpecialLine(mcfg, 0, 33, 0, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
#elif __JHEXEN__
    // A locked door (all are green).
    registerSpecialLine(mcfg, 0, 13, 0, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 83, 0, ML_SECRET, 0, .9f, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Intra-map teleporters (all are blue).
    registerSpecialLine(mcfg, 0, 70, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    registerSpecialLine(mcfg, 0, 71, 2, ML_SECRET, 0, 0, .776f, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Inter-map teleport.
    registerSpecialLine(mcfg, 0, 74, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
    // Game-winning exit.
    registerSpecialLine(mcfg, 0, 75, 2, ML_SECRET, .682f, 0, 0, 1, BM_NORMAL, GLOW_BOTH, .75f, 5, true);
#endif

    AM_SetVectorGraphic(mcfg, AMO_THING, VG_TRIANGLE);
    AM_SetVectorGraphic(mcfg, AMO_THINGPLAYER, VG_ARROW);

    AM_GetMapColor(rgb, cfg.automapL0, GRAYS+3, customPal);
    AM_SetColorAndOpacity(mcfg, AMO_UNSEENLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL1, WALLCOLORS, customPal);
    AM_SetColorAndOpacity(mcfg, AMO_SINGLESIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL0, TSWALLCOLORS, customPal);
    AM_SetColorAndOpacity(mcfg, AMO_TWOSIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL2, FDWALLCOLORS, customPal);
    AM_SetColorAndOpacity(mcfg, AMO_FLOORCHANGELINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.automapL3, CDWALLCOLORS, customPal);
    AM_SetColorAndOpacity(mcfg, AMO_CEILINGCHANGELINE, rgb[0], rgb[1], rgb[2], 1);
    }
}

void ST_InitAutomapConfig(void)
{
    VERBOSE2( Con_Message("Initializing automap...\n") )
    initAutomapConfig(&automapCFG);
}

void AM_GetColor(automapcfg_t* mcfg, automapcfg_objectname_t name, float* r, float* g, float* b)
{
    assert(NULL != mcfg);
    {
    automapcfg_lineinfo_t* info = NULL;

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColor: Unknown object %i.", (int) name);

    // It must be an object with an info.
    switch(name)
    {
    case AMO_UNSEENLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_GetColor: Object %i does not use color.", (int) name);
        break;
    }

    if(r) *r = info->rgba[0];
    if(g) *g = info->rgba[1];
    if(b) *b = info->rgba[2];
    }
}

void AM_SetColor(automapcfg_t* mcfg, automapcfg_objectname_t name, float r, float g, float b)
{
    automapcfg_lineinfo_t* info;

    if(name == AMO_NONE)
        return; // Ignore.

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColor: Unknown object %i.", (int) name);

    r = MINMAX_OF(0, r, 1);
    g = MINMAX_OF(0, g, 1);
    b = MINMAX_OF(0, b, 1);

    // It must be an object with a name.
    switch(name)
    {
    case AMO_UNSEENLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_SetColor: Object %i does not use color.", (int) name);
        exit(1); // Unreachable.
    }

    info->rgba[0] = r;
    info->rgba[1] = g;
    info->rgba[2] = b;
}

void AM_GetColorAndOpacity(automapcfg_t* mcfg, automapcfg_objectname_t name,
    float* r, float* g, float* b, float* a)
{
    assert(NULL != mcfg);
    {
    automapcfg_lineinfo_t* info = NULL;

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_GetColorAndOpacity: Unknown object %i.", (int) name);

    // It must be an object with an info.
    switch(name)
    {
    case AMO_UNSEENLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_GetColorAndOpacity: Object %i does not use color/alpha.", (int) name);
        break;
    }

    if(r) *r = info->rgba[0];
    if(g) *g = info->rgba[1];
    if(b) *b = info->rgba[2];
    if(a) *a = info->rgba[3];
    }
}

void AM_SetColorAndOpacity(automapcfg_t* mcfg, automapcfg_objectname_t name,
    float r, float g, float b, float a)
{
    assert(NULL != mcfg);
    {
    automapcfg_lineinfo_t* info = NULL;

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_SetColorAndOpacity: Unknown object %i.", (int) name);

    r = MINMAX_OF(0, r, 1);
    g = MINMAX_OF(0, g, 1);
    b = MINMAX_OF(0, b, 1);
    a = MINMAX_OF(0, a, 1);

    // It must be an object with an info.
    switch(name)
    {
    case AMO_UNSEENLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_SetColorAndOpacity: Object %i does not use color/alpha.", (int) name);
        break;
    }

    info->rgba[0] = r;
    info->rgba[1] = g;
    info->rgba[2] = b;
    info->rgba[3] = a;
    }
}

void AM_SetGlow(automapcfg_t* mcfg, automapcfg_objectname_t name, glowtype_t type, float size,
    float alpha, boolean canScale)
{
    assert(NULL != mcfg);
    {
    automapcfg_lineinfo_t* info = NULL;

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_SetGlow: Unknown object %i.", (int) name);

    size = MINMAX_OF(0, size, 100);
    alpha = MINMAX_OF(0, alpha, 1);

    switch(name)
    {
    case AMO_UNSEENLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_UNSEEN];
        break;

    case AMO_SINGLESIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF];
        break;

    case AMO_TWOSIDEDLINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
        break;

    case AMO_FLOORCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_FLOOR];
        break;

    case AMO_CEILINGCHANGELINE:
        info = &mcfg->mapObjectInfo[MOL_LINEDEF_CEILING];
        break;

    default:
        Con_Error("AM_SetGlow: Object %i does not support glow.", (int) name);
        break;
    }

    info->glow = type;
    info->glowStrength = alpha;
    info->glowSize = size;
    info->scaleWithView = canScale;
    }
}

svgid_t AM_GetVectorGraphic(automapcfg_t* mcfg, automapcfg_objectname_t name)
{
    assert(NULL != mcfg);
    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_GetVectorGraphic: Unknown object %i.", (int) name);
    switch(name)
    {
    case AMO_THING:         return mcfg->vectorGraphicForThing;
    case AMO_THINGPLAYER:   return mcfg->vectorGraphicForPlayer;
    default:
        Con_Error("AM_GetVectorGraphic: Object %i does not support vector graphic.", (int) name);
        break;
    }
    return 0;
}

void AM_SetVectorGraphic(automapcfg_t* mcfg, automapcfg_objectname_t name,
    svgid_t svg)
{
    assert(NULL != mcfg);

    if(name < 0 || name >= AMO_NUMOBJECTS)
        Con_Error("AM_SetVectorGraphic: Unknown object %i.", (int) name);

    switch(name)
    {
    case AMO_THING:         mcfg->vectorGraphicForThing  = svg; break;
    case AMO_THINGPLAYER:   mcfg->vectorGraphicForPlayer = svg; break;
    default:
        Con_Error("AM_SetVectorGraphic: Object %i does not support vector  graphic.", (int) name);
    }
}

static automapcfg_lineinfo_t* findLineInfo(automapcfg_t* mcfg, int reqAutomapFlags,
    int reqSpecial, int reqSided, int reqNotFlagged)
{
    DENG_ASSERT(mcfg);
{
    automapcfg_lineinfo_t* info;
    uint i;
    for(i = 0, info = mcfg->lineInfo; i < mcfg->lineInfoCount; ++i, info++)
    {
        if(info->reqSpecial      != reqSpecial) continue;
        if(info->reqAutomapFlags != reqAutomapFlags) continue;
        if(info->reqSided        != reqSided) continue;
        if(info->reqNotFlagged   != reqNotFlagged) continue;

        // This is the one.
        return info;
    }
    return 0; // Not found.
}}

static void registerSpecialLine(automapcfg_t* mcfg, int reqAutomapFlags, int reqSpecial,
    int reqSided, int reqNotFlagged, float r, float g, float b, float a, blendmode_t blendmode,
    glowtype_t glowType, float glowStrength, float glowSize, boolean scaleGlowWithView)
{
    DENG_ASSERT(mcfg);
{
    // Later re-registrations override earlier ones.
    automapcfg_lineinfo_t* info = findLineInfo(mcfg, reqAutomapFlags, reqSpecial, reqSided, reqNotFlagged);
    if(!info)
    {
        // Any room for a new special line?
        if(mcfg->lineInfoCount >= AUTOMAPCFG_MAX_LINEINFO)
            Con_Error("AM_RegisterSpecialLine: No available slot.");
        info = &mcfg->lineInfo[mcfg->lineInfoCount++];
    }

    info->reqAutomapFlags   = reqAutomapFlags;
    info->reqSpecial        = reqSpecial;
    info->reqSided          = reqSided;
    info->reqNotFlagged     = reqNotFlagged;

    info->rgba[CR] = MINMAX_OF(0, r, 1);
    info->rgba[CG] = MINMAX_OF(0, g, 1);
    info->rgba[CB] = MINMAX_OF(0, b, 1);
    info->rgba[CA] = MINMAX_OF(0, a, 1);
    info->glow          = glowType;
    info->glowStrength  = MINMAX_OF(0, glowStrength, 1);
    info->glowSize      = glowSize;
    info->scaleWithView = scaleGlowWithView;
    info->blendMode     = blendmode;
}}

void AM_RegisterSpecialLine(automapcfg_t* mcfg, int reqMapFlags, int reqSpecial,
    int reqSided, int reqNotFlagged, float r, float g, float b, float a, blendmode_t blendmode,
    glowtype_t glowType, float glowStrength, float glowSize, boolean scaleGlowWithView)
{
    if(reqSpecial < 0)
        Con_Error("AM_RegisterSpecialLine: special requirement '%i' negative.", reqSpecial);
    if(reqSided < 0 || reqSided > 2)
        Con_Error("AM_RegisterSpecialLine: sided requirement '%i' invalid.", reqSided);

    registerSpecialLine(mcfg, reqMapFlags, reqSpecial, reqSided, reqNotFlagged,
                        r, g, b, a, blendmode, glowType, glowStrength, glowSize, scaleGlowWithView);
}
