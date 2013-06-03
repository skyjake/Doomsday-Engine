/**\file hu_automap.c
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "am_map.h"
#include "dmu_lib.h"
#include "hu_automap.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "r_common.h"
#if __JDOOM64__
#  include "p_inventory.h"
#endif
#include "g_common.h"
#include "hu_stuff.h"

#define LERP(start, end, pos)   (end * pos + start * (1 - pos))

// Translate between frame and map distances:
#define FTOM(map, x)            ((x) * (map)->scaleFTOM)
#define MTOF(map, x)            ((x) * (map)->scaleMTOF)

typedef struct {
    player_t* plr;
    // The type of object we want to draw. If @c -1, draw only line specials.
    int objType;
    boolean addToLists;
} uiautomap_rendstate_t;
uiautomap_rendstate_t rs;

boolean freezeMapRLs = false;

// if -1 no background image will be drawn.
#if __JDOOM__ || __JDOOM64__
static int autopageLumpNum = -1;
#elif __JHERETIC__
static int autopageLumpNum = 1;
#else
static int autopageLumpNum = 1;
#endif

static int playerColorPaletteIndices[NUMPLAYERCOLORS] = {
    AM_PLR1_COLOR,
    AM_PLR2_COLOR,
    AM_PLR3_COLOR,
    AM_PLR4_COLOR,
#if __JHEXEN__
    AM_PLR5_COLOR,
    AM_PLR6_COLOR,
    AM_PLR7_COLOR,
    AM_PLR8_COLOR
#endif
};

static DGLuint amMaskTexture = 0; // Used to mask the map primitives.

void UIAutomap_Register(void)
{
    cvartemplate_t cvars[] = {
        { "map-opacity", 0, CVT_FLOAT, &cfg.automapOpacity, 0, 1 },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        { "map-babykeys", 0, CVT_BYTE, &cfg.automapBabyKeys, 0, 1 },
#endif
        { "map-background-r", 0, CVT_FLOAT, &cfg.automapBack[0], 0, 1 },
        { "map-background-g", 0, CVT_FLOAT, &cfg.automapBack[1], 0, 1 },
        { "map-background-b", 0, CVT_FLOAT, &cfg.automapBack[2], 0, 1 },
        { "map-customcolors", 0, CVT_INT, &cfg.automapCustomColors, 0, 1 },
        { "map-line-opacity", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1 },
        { "map-line-width", 0, CVT_FLOAT, &cfg.automapLineWidth, .1f, 2 },
        { "map-mobj-r", 0, CVT_FLOAT, &cfg.automapMobj[0], 0, 1 },
        { "map-mobj-g", 0, CVT_FLOAT, &cfg.automapMobj[1], 0, 1 },
        { "map-mobj-b", 0, CVT_FLOAT, &cfg.automapMobj[2], 0, 1 },
        { "map-wall-r", 0, CVT_FLOAT, &cfg.automapL1[0], 0, 1 },
        { "map-wall-g", 0, CVT_FLOAT, &cfg.automapL1[1], 0, 1 },
        { "map-wall-b", 0, CVT_FLOAT, &cfg.automapL1[2], 0, 1 },
        { "map-wall-unseen-r", 0, CVT_FLOAT, &cfg.automapL0[0], 0, 1 },
        { "map-wall-unseen-g", 0, CVT_FLOAT, &cfg.automapL0[1], 0, 1 },
        { "map-wall-unseen-b", 0, CVT_FLOAT, &cfg.automapL0[2], 0, 1 },
        { "map-wall-floorchange-r", 0, CVT_FLOAT, &cfg.automapL2[0], 0, 1 },
        { "map-wall-floorchange-g", 0, CVT_FLOAT, &cfg.automapL2[1], 0, 1 },
        { "map-wall-floorchange-b", 0, CVT_FLOAT, &cfg.automapL2[2], 0, 1 },
        { "map-wall-ceilingchange-r", 0, CVT_FLOAT, &cfg.automapL3[0], 0, 1 },
        { "map-wall-ceilingchange-g", 0, CVT_FLOAT, &cfg.automapL3[1], 0, 1 },
        { "map-wall-ceilingchange-b", 0, CVT_FLOAT, &cfg.automapL3[2], 0, 1 },
        { "map-door-colors", 0, CVT_BYTE, &cfg.automapShowDoors, 0, 1 },
        { "map-door-glow", 0, CVT_FLOAT, &cfg.automapDoorGlow, 0, 200 },
        { "map-huddisplay", 0, CVT_INT, &cfg.automapHudDisplay, 0, 2 },
        { "map-pan-speed", 0, CVT_FLOAT, &cfg.automapPanSpeed, 0, 1 },
        { "map-pan-resetonopen", 0, CVT_BYTE, &cfg.automapPanResetOnOpen, 0, 1 },
        { "map-rotate", 0, CVT_BYTE, &cfg.automapRotate, 0, 1 },
        { "map-zoom-speed", 0, CVT_FLOAT, &cfg.automapZoomSpeed, 0, 1 },
        { "map-open-timer", CVF_NO_MAX, CVT_FLOAT, &cfg.automapOpenSeconds, 0, 0 },
        { "rend-dev-freeze-map", CVF_NO_ARCHIVE, CVT_BYTE, &freezeMapRLs, 0, 1 },

        // Aliases for old names:
        { "map-alpha-lines", 0, CVT_FLOAT, &cfg.automapLineAlpha, 0, 1 },
        { NULL }
    };
    Con_AddVariableList(cvars);
}

static void rotate2D(coord_t* x, coord_t* y, float angle)
{
    coord_t tmpx = (coord_t) ((*x * cos(angle/180 * PI)) - (*y * sin(angle/180 * PI)));
    *y = (coord_t) ((*x * sin(angle/180 * PI)) + (*y * cos(angle/180 * PI)));
    *x = tmpx;
}

/**
 * Calculate the min/max scaling factors.
 *
 * Take the distance from the bottom left to the top right corners and
 * choose a max scaling factor such that this distance is short than both
 * the automap window width and height.
 */
static void calcViewScaleFactors(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    coord_t dx, dy, dist;
    float a, b, oldMinScale = am->minScaleMTOF;
    assert(obj->type == GUI_AUTOMAP);

    dx = am->bounds[BOXRIGHT] - am->bounds[BOXLEFT];
    dy = am->bounds[BOXTOP]   - am->bounds[BOXBOTTOM];
    dist = (coord_t) sqrt(dx * dx + dy * dy);
    if(dist < 0)
        dist = -dist;

    a = Rect_Width(UIWidget_Geometry(obj))  / dist;
    b = Rect_Height(UIWidget_Geometry(obj)) / dist;

    am->minScaleMTOF = (a < b ? a : b);
    am->maxScaleMTOF = Rect_Height(UIWidget_Geometry(obj)) / am->minScale;

#ifdef _DEBUG
    VERBOSE2( Con_Message("calcViewScaleFactors: dx=%f dy=%f dist=%f w=%i h=%i a=%f b=%f minmtof=%f",
                          dx, dy, dist, Rect_Width(UIWidget_Geometry(obj)),
                          Rect_Height(UIWidget_Geometry(obj)), a, b, am->minScaleMTOF) );
#endif

    // Update previously set view scale accordingly.
    /// @todo  The view scale factor needs to be resolution independent!
    am->targetViewScale = am->viewScale = am->minScaleMTOF/oldMinScale * am->targetViewScale;

    am->updateViewScale = false;
}

void UIAutomap_ClearLists(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    uint i;
    assert(obj->type == GUI_AUTOMAP);

    if(Get(DD_NOVIDEO) || IS_DEDICATED) return; // Nothing to do.

    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        if(am->lists[i])
            DGL_DeleteLists(am->lists[i], 1);
        am->lists[i] = 0;
    }
}

void UIAutomap_LoadResources(void)
{
    if(autopageLumpNum >= 0)
        autopageLumpNum = W_CheckLumpNumForName("autopage");

    if(!amMaskTexture)
    {
        lumpnum_t lumpNum = W_GetLumpNumForName("mapmask");
        if(lumpNum >= 0)
        {
            const uint8_t* pixels = (const uint8_t*) W_CacheLump(lumpNum);
            int width = 256, height = 256;

            amMaskTexture = DGL_NewTextureWithParams(DGL_LUMINANCE, width, height, pixels,
                0x8, DGL_NEAREST, DGL_LINEAR, 0 /*no anisotropy*/, DGL_REPEAT, DGL_REPEAT);

            W_UnlockLump(lumpNum);
        }
    }
}

void UIAutomap_ReleaseResources(void)
{
    if(0 == amMaskTexture) return;
    DGL_DeleteTextures(1, (DGLuint*) &amMaskTexture);
    amMaskTexture = 0;
}

void UIAutomap_Reset(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    UIAutomap_ClearLists(obj);
    am->constructMap = true;
}

/**
 * Draws the given line including any optional extras.
 */
static void rendLine2(uiwidget_t* obj, float x1, float y1, float x2, float y2,
    const float color[4], glowtype_t glowType, float glowStrength, float glowSize,
    boolean glowOnly, boolean scaleGlowWithView, boolean caps, blendmode_t blend,
    boolean drawNormal, boolean addToLists)
{
    //guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const float alpha = uiRendState->pageAlpha;
    //automapcfg_t* mcfg = am->mcfg;
    float a[2], b[2];
    float length, dx, dy;
    float normal[2], unit[2];
    assert(obj->type == GUI_AUTOMAP);

    // Scale into map, screen space units.
    a[VX] = x1;
    a[VY] = y1;
    b[VX] = x2;
    b[VY] = y2;

    dx = b[VX] - a[VX];
    dy = b[VY] - a[VY];
    length = sqrt(dx * dx + dy * dy);
    if(length <= 0) return;

    unit[VX] = dx / length;
    unit[VY] = dy / length;
    normal[VX] = unit[VY];
    normal[VY] = -unit[VX];

    // Is this a glowing line?
    if(glowType != GLOW_NONE)
    {
        int tex;
        float thickness;

        // Scale line thickness relative to zoom level?
        if(scaleGlowWithView)
            thickness = cfg.automapDoorGlow * 2.5f + 3;
        else
            thickness = glowSize;

        tex = Get(DD_DYNLIGHT_TEXTURE);

        if(caps)
        {   // Draw a "cap" at the start of the line.
            float v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX] - unit[VX] * thickness + normal[VX] * thickness;
            v1[VY] = a[VY] - unit[VY] * thickness + normal[VY] * thickness;
            v2[VX] = a[VX] + normal[VX] * thickness;
            v2[VY] = a[VY] + normal[VY] * thickness;
            v3[VX] = a[VX] - normal[VX] * thickness;
            v3[VY] = a[VY] - normal[VY] * thickness;
            v4[VX] = a[VX] - unit[VX] * thickness - normal[VX] * thickness;
            v4[VY] = a[VY] - unit[VY] * thickness - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2], glowStrength * alpha);
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, 0, 0);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, 0, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
        }

        // The middle part of the line.
        switch(glowType)
        {
        case GLOW_BOTH:
            {
            float v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX] + normal[VX] * thickness;
            v1[VY] = a[VY] + normal[VY] * thickness;
            v2[VX] = b[VX] + normal[VX] * thickness;
            v2[VY] = b[VY] + normal[VY] * thickness;
            v3[VX] = b[VX] - normal[VX] * thickness;
            v3[VY] = b[VY] - normal[VY] * thickness;
            v4[VX] = a[VX] - normal[VX] * thickness;
            v4[VY] = a[VY] - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2], glowStrength * alpha);
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
            }
            break;

        case GLOW_BACK:
            {
            float v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX] + normal[VX] * thickness;
            v1[VY] = a[VY] + normal[VY] * thickness;
            v2[VX] = b[VX] + normal[VX] * thickness;
            v2[VY] = b[VY] + normal[VY] * thickness;
            v3[VX] = b[VX];
            v3[VY] = b[VY];
            v4[VX] = a[VX];
            v4[VY] = a[VY];

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2], glowStrength * alpha);
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, 0, .25f);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, 0, .25f);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .5f, .25f);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .5f, .25f);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
            }
            break;

        case GLOW_FRONT:
            {
            float v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX];
            v1[VY] = a[VY];
            v2[VX] = b[VX];
            v2[VY] = b[VY];
            v3[VX] = b[VX] - normal[VX] * thickness;
            v3[VY] = b[VY] - normal[VY] * thickness;
            v4[VX] = a[VX] - normal[VX] * thickness;
            v4[VY] = a[VY] - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2], glowStrength * alpha);
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, .75f, .5f);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, .75f, .5f);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .75f, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .75f, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
            }
            break;

        default:
            break; // Impossible.
        }

        if(caps)
        {
            float v1[2], v2[2], v3[2], v4[2];

            v1[VX] = b[VX] + normal[VX] * thickness;
            v1[VY] = b[VY] + normal[VY] * thickness;
            v2[VX] = b[VX] + unit[VX] * thickness + normal[VX] * thickness;
            v2[VY] = b[VY] + unit[VY] * thickness + normal[VY] * thickness;
            v3[VX] = b[VX] + unit[VX] * thickness - normal[VX] * thickness;
            v3[VY] = b[VY] + unit[VY] * thickness - normal[VY] * thickness;
            v4[VX] = b[VX] - normal[VX] * thickness;
            v4[VY] = b[VY] - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2], glowStrength * alpha);
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, 1, 0);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, 1, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .5, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
        }
    }

    if(!glowOnly)
    {
        if(!addToLists)
        {
            DGL_Color4f(color[0], color[1], color[2], color[3] * alpha);
            DGL_BlendMode(blend);
        }

        DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, a[VX], a[VY]);
            DGL_Vertex2f(a[VX], a[VY]);
            DGL_TexCoord2f(0, b[VX], b[VY]);
            DGL_Vertex2f(b[VX], b[VY]);
        DGL_End();

        if(!addToLists)
            DGL_BlendMode(BM_NORMAL);
    }

    if(drawNormal)
    {
#define NORMTAIL_LENGTH         8

        float center[2];

        center[VX] = a[VX] + (length / 2) * unit[VX];
        center[VY] = a[VY] + (length / 2) * unit[VY];

        a[VX] = center[VX];
        a[VY] = center[VY];
        b[VX] = center[VX] + normal[VX] * NORMTAIL_LENGTH;
        b[VY] = center[VY] + normal[VY] * NORMTAIL_LENGTH;

        if(!addToLists)
        {
            DGL_Color4f(color[0], color[1], color[2], color[3] * alpha);
            DGL_BlendMode(blend);
        }

        DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, a[VX], a[VY]);
            DGL_Vertex2f(a[VX], a[VY]);
            DGL_TexCoord2f(0, b[VX], b[VY]);
            DGL_Vertex2f(b[VX], b[VY]);
        DGL_End();

        if(!addToLists)
            DGL_BlendMode(BM_NORMAL);

#undef NORMTAIL_LENGTH
    }
}

static int rendSeg(void *segment_, void* data)
{
    assert(segment_ != 0 && data != 0 && ((uiwidget_t *)data)->type == GUI_AUTOMAP);
    {
    Segment *segment = (Segment *) segment_;
    uiwidget_t* obj = (uiwidget_t*)data;
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    Sector* frontSector, *backSector;
    const automapcfg_lineinfo_t* info;
    player_t* plr = rs.plr;
    float v1[2], v2[2];
    Line* line;
    xline_t* xLine;

    line = P_GetPtrp(segment, DMU_LINE);
    if(!line) return false;

    xLine = P_ToXLine(line);
    // Already drawn once?
    if(xLine->validCount == VALIDCOUNT) return false;

    // Is this line being drawn?
    if((xLine->flags & ML_DONTDRAW) && !(am->flags & AMF_REND_ALLLINES)) return false;

    // We only want to draw twosided lines once.
    frontSector = P_GetPtrp(line, DMU_FRONT_SECTOR);
    if(frontSector // $degenleaf
       && frontSector != P_GetPtrp(line, DMU_FRONT_SECTOR)) return false;

    info = NULL;
    if((am->flags & AMF_REND_ALLLINES) || xLine->mapped[plr - players])
    {
        backSector = P_GetPtrp(line, DMU_BACK_SECTOR);

        // Perhaps this is a specially colored line?
        info = AM_GetInfoForSpecialLine(UIAutomap_Config(obj), xLine->special, xLine->flags,
                                        frontSector, backSector, UIAutomap_Flags(obj));
        if(rs.objType != -1 && !info)
        {
            // Perhaps a default colored line?
            /// @todo Implement an option which changes the vanilla behavior of always
            ///       coloring non-secret lines with the solid-wall color to instead
            ///       use whichever color it would be if not flagged secret.
            if(!backSector || !P_GetPtrp(line, DMU_BACK) || (xLine->flags & ML_SECRET))
            {
                // solid wall (well probably anyway...)
                info = AM_GetInfoForLine(UIAutomap_Config(obj), AMO_SINGLESIDEDLINE);
            }
            else
            {
                if(!FEQUAL(P_GetDoublep(backSector,  DMU_FLOOR_HEIGHT),
                           P_GetDoublep(frontSector, DMU_FLOOR_HEIGHT)))
                {
                    // Floor level change.
                    info = AM_GetInfoForLine(UIAutomap_Config(obj), AMO_FLOORCHANGELINE);
                }
                else if(!FEQUAL(P_GetDoublep(backSector,  DMU_CEILING_HEIGHT),
                                P_GetDoublep(frontSector, DMU_CEILING_HEIGHT)))
                {
                    // Ceiling level change.
                    info = AM_GetInfoForLine(UIAutomap_Config(obj), AMO_CEILINGCHANGELINE);
                }
                else if(am->flags & AMF_REND_ALLLINES)
                {
                    info = AM_GetInfoForLine(UIAutomap_Config(obj), AMO_UNSEENLINE);
                }
            }
        }
    }
    else if(rs.objType != -1 && UIAutomap_Reveal(obj))
    {
        if(!(xLine->flags & ML_DONTDRAW))
        {
            // An as yet, unseen line.
            info = AM_GetInfoForLine(UIAutomap_Config(obj), AMO_UNSEENLINE);
        }
    }

    if(info && (rs.objType == -1 || info == &am->mcfg->mapObjectInfo[rs.objType]))
    {
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
        P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

        rendLine2((uiwidget_t*)data, v1[VX], v1[VY], v2[VX], v2[VY],
                  info->rgba,
                  (xLine->special && !cfg.automapShowDoors ?
                        GLOW_NONE : info->glow),
                  info->glowStrength,
                  info->glowSize, !rs.addToLists, info->scaleWithView,
                  (info->glow && !(xLine->special && !cfg.automapShowDoors)),
                  (xLine->special && !cfg.automapShowDoors ?
                        BM_NORMAL : info->blendMode),
                  (am->flags & AMF_REND_LINE_NORMALS),
                  rs.addToLists);

        xLine->validCount = VALIDCOUNT; // Mark as drawn this frame.
    }

    return false; // Continue iteration.
    }
}

static int rendBspLeafHEdges(BspLeaf* bspLeaf, void* context)
{
    return P_Iteratep(bspLeaf, DMU_SEGMENT, context, rendSeg);
}

/**
 * Determines visible lines, draws them.
 *
 * @params objType      Type of map object being drawn.
 */
static void renderWalls(uiwidget_t* obj, int objType, boolean addToLists)
{
    //guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    int i;
    assert(obj && obj->type == GUI_AUTOMAP);

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Configure render state:
    rs.objType = objType;
    rs.addToLists = addToLists;

    // Can we use the automap's in-view bounding box to cull out of view objects?
    if(!addToLists)
    {
        AABoxd aaBox;
        UIAutomap_PVisibleAABounds(obj, &aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
        P_BspLeafsBoxIterator(&aaBox, NULL, rendBspLeafHEdges, obj);
    }
    else
    {   // No. As the map lists are considered static we want them to contain all
        // walls, not just those visible *now* (note rotation).
        for(i = 0; i < numbspleafs; ++i)
        {
            P_Iteratep(P_ToPtr(DMU_BSPLEAF, i), DMU_SEGMENT, obj, rendSeg);
        }
    }
}

static void rendLinedef(Line* line, float r, float g, float b, float a,
    blendmode_t blendMode, boolean drawNormal)
{
    float length = P_GetFloatp(line, DMU_LENGTH);

    if(length > 0)
    {
        float v1[2], v2[2];

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

            float normal[2], unit[2], d1[2];

            P_GetFloatpv(line, DMU_DXY, d1);

            unit[VX] = d1[0] / length;
            unit[VY] = d1[1] / length;
            normal[VX] = unit[VY];
            normal[VY] = -unit[VX];

            // The center of the line.
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
 * Rather than draw the instead this will draw the line of which
 * the segment is a part.
 */
int rendPolyobjLine(Line* line, void* context)
{
    uiwidget_t* ob = (uiwidget_t*)context;
    guidata_automap_t* am = (guidata_automap_t*)ob->typedata;
    const float alpha = uiRendState->pageAlpha;
    const automapcfg_lineinfo_t* info;
    automapcfg_objectname_t amo;
    player_t* plr = rs.plr;
    xline_t* xLine;
    assert(ob && ob->type == GUI_AUTOMAP);

    if(!(xLine = P_ToXLine(line))) return false;

    // Already processed this frame?
    if(xLine->validCount == VALIDCOUNT) return false;

    if((xLine->flags & ML_DONTDRAW) && !(am->flags & AMF_REND_ALLLINES)) return false;

    amo = AMO_NONE;
    if((am->flags & AMF_REND_ALLLINES) || xLine->mapped[plr - players])
    {
        amo = AMO_SINGLESIDEDLINE;
    }
    else if(rs.objType != -1 && UIAutomap_Reveal(ob))
    {
        if(!(xLine->flags & ML_DONTDRAW))
        {
            // An as yet, unseen line.
            amo = AMO_UNSEENLINE;
        }
    }

    info = AM_GetInfoForLine(UIAutomap_Config(ob), amo);
    if(info)
    {
        rendLinedef(line, info->rgba[0], info->rgba[1], info->rgba[2],
                    info->rgba[3] * cfg.automapLineAlpha * alpha, info->blendMode,
                    (am->flags & AMF_REND_LINE_NORMALS)? true : false);
    }

    xLine->validCount = VALIDCOUNT; // Mark as processed this frame.

    return false; // Continue iteration.
}

static void rendPolyobjs(uiwidget_t* ob)
{
    //guidata_automap_t* am = (guidata_automap_t*)ob->typedata;
    AABoxd aaBox;
    assert(ob && ob->type == GUI_AUTOMAP);

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Configure render state:
    rs.objType = MOL_LINEDEF;

    // Draw any polyobjects in view.
    UIAutomap_PVisibleAABounds(ob, &aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
    P_PolyobjLinesBoxIterator(&aaBox, rendPolyobjLine, ob);
}

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
int rendXGLinedef(Line* line, void* context)
{
    assert(line && context && ((uiwidget_t*)context)->type == GUI_AUTOMAP);
    {
    guidata_automap_t* am = (guidata_automap_t*)((uiwidget_t*)context)->typedata;
    xline_t* xLine;

    xLine = P_ToXLine(line);
    if(!xLine || xLine->validCount == VALIDCOUNT ||
        ((xLine->flags & ML_DONTDRAW) && !(am->flags & AMF_REND_ALLLINES)))
        return false;

    // Show only active XG lines.
    if(!(xLine->xg && xLine->xg->active && (mapTime & 4)))
        return false;

    rendLinedef(line, .8f, 0, .8f, 1, BM_ADD, (am->flags & AMF_REND_LINE_NORMALS)? true : false);
    xLine->validCount = VALIDCOUNT; // Mark as processed this frame.

    return false; // Continue iteration.
    }
}

static void rendXGLinedefs(uiwidget_t* obj)
{
    //guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    AABoxd aaBox;
    assert(obj->type == GUI_AUTOMAP);

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Configure render state:
    rs.addToLists = false;
    rs.objType = -1;

    UIAutomap_PVisibleAABounds(obj, &aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
    P_LinesBoxIterator(&aaBox, rendXGLinedef, obj);
}
#endif

static void drawVectorGraphic(svgid_t vgId, coord_t x, coord_t y, float angle,
    float scale, const float color[3], float alpha, blendmode_t blendmode)
{
    Point2Rawf origin;

    if(!color) return;

    origin.x = x;
    origin.y = y;
    alpha = MINMAX_OF(0.f, alpha, 1.f);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PushMatrix();
    DGL_Translatef(origin.x, origin.y, 1);

    DGL_Color4f(color[CR], color[CG], color[CB], alpha);
    DGL_BlendMode(blendmode);

    GL_DrawSvg3(vgId, &origin, scale, angle);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();
}

static __inline int playerPaletteColor(int consoleNum)
{
    if(!IS_NETGAME) return WHITE;
    return playerColorPaletteIndices[cfg.playerColor[MAX_OF(0, consoleNum) % MAXPLAYERS]];
}

static void drawPlayerMarker(int consoleNum, automapcfg_t* config)
{
    player_t* player = players + consoleNum;
    mobj_t* mo = player->plr->mo;
    svgid_t svgId;
    coord_t origin[3];
    float color[3], alpha, angle, radius;

    if(!player->plr->inGame || !mo) return;

    svgId  = AM_GetVectorGraphic(config, AMO_THINGPLAYER);
    Mobj_OriginSmoothed(mo, origin);
    /* $unifiedangles */
    angle  = Mobj_AngleSmoothed(mo) / (float) ANGLE_MAX * 360;
    radius = PLAYERRADIUS;

    R_GetColorPaletteRGBf(0, playerPaletteColor(consoleNum), color, false);

    alpha = cfg.automapLineAlpha * uiRendState->pageAlpha;
#if !__JHEXEN__
    if(player->powers[PT_INVISIBILITY])
        alpha *= .125f;
#endif

    drawVectorGraphic(svgId, origin[VX], origin[VY], angle, radius, color, alpha, BM_NORMAL);
}

/**
 * Draws all players on the map using a line character
 */
static void rendPlayerMarkers(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    int i;
    assert(obj->type == GUI_AUTOMAP);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // Do not show markers for other players in deathmatch.
        if(deathmatch && i != UIWidget_Player(obj)) continue;

        drawPlayerMarker(i, am->mcfg);
    }
}

static int getKeyColorForMobjType(int type)
{
    struct keycolor_s {
        int mobjType;
        int palColor;
    } keyColors[] = {
#if __JDOOM__ || __JDOOM64__
        { MT_MISC4, KEY1_COLOR },
        { MT_MISC5, KEY2_COLOR },
        { MT_MISC6, KEY3_COLOR },
        { MT_MISC7, KEY4_COLOR },
        { MT_MISC8, KEY5_COLOR },
        { MT_MISC9, KEY6_COLOR },
#elif __JHERETIC__
        { MT_CKEY, KEY1_COLOR },
        { MT_BKYY, KEY2_COLOR },
        { MT_AKYY, KEY3_COLOR },
#endif
        { -1, -1 }
    };
    uint i;
    for(i = 0; keyColors[i].mobjType != -1; ++i)
    {
        if(keyColors[i].mobjType == type)
            return keyColors[i].palColor;
    }
    return -1; // Not a key.
}

typedef struct {
    int flags; // AMF_* flags.
    svgid_t vgId;
    float rgb[3], alpha;
} renderthing_params_t;

static int rendThingPoint(mobj_t* mo, void* context)
{
    renderthing_params_t* p = (renderthing_params_t*) context;

    // Only sector linked mobjs should be visible in the automap.
    if(!(mo->flags & MF_NOSECTOR))
    {
        svgid_t vgId = p->vgId;
        boolean isVisible = false;
        float* color = p->rgb, angle;
        float keyColorRGB[3];

        if(p->flags & AMF_REND_KEYS)
        {
            int keyColor = getKeyColorForMobjType(mo->type);
            if(keyColor != -1)
            {
                R_GetColorPaletteRGBf(0, keyColor, keyColorRGB, false);
                vgId  = VG_KEY;
                color = keyColorRGB;
                angle = 0;
                isVisible = true;
            }
        }

        // Something else?
        if(!isVisible)
        {
            isVisible = !!(p->flags & AMF_REND_THINGS);
            angle = Mobj_AngleSmoothed(mo) / (float) ANGLE_MAX * 360; // In degrees.
        }

        if(isVisible)
        {
            /* $unifiedangles */
            const coord_t radius = 16;
            coord_t origin[3];
            Mobj_OriginSmoothed(mo, origin);

            drawVectorGraphic(vgId, origin[VX], origin[VY], angle, radius,
                              color, p->alpha, BM_NORMAL);
        }
    }

    return false; // Continue iteration.
}

static void rendThingPoints(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const float alpha = uiRendState->pageAlpha;
    renderthing_params_t params;
    AABoxd aaBox;
    assert(obj->type == GUI_AUTOMAP);

    params.flags = UIAutomap_Flags(obj);
    params.vgId = AM_GetVectorGraphic(am->mcfg, AMO_THING);
    AM_GetMapColor(params.rgb, cfg.automapMobj, THINGCOLORS, customPal);
    params.alpha = MINMAX_OF(0.f, cfg.automapLineAlpha * alpha, 1.f);

    UIAutomap_PVisibleAABounds(obj, &aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
    VALIDCOUNT++;
    P_MobjsBoxIterator(&aaBox, rendThingPoint, &params);
}

static boolean interceptEdge(coord_t point[2], coord_t const startA[2], coord_t const endA[2],
    coord_t const startB[2], coord_t const endB[2])
{
    coord_t directionA[2];
    V2d_Subtract(directionA, endA, startA);
    if(V2d_PointOnLineSide(point, startA, directionA) >= 0)
    {
        coord_t directionB[2];
        V2d_Subtract(directionB, endB, startB);
        V2d_Intersection(startA, directionA, startB, directionB, point);
        return true;
    }
    return false;
}

static void positionPointInView(uiwidget_t* ob, coord_t point[2],
    coord_t const topLeft[2], coord_t const topRight[2], coord_t const bottomRight[2],
    coord_t const bottomLeft[2], coord_t const viewPoint[2])
{
    // Trace a vector from the view location to the marked point and intercept
    // vs the edges of the rotated view window.
    if(!interceptEdge(point, topLeft, bottomLeft, viewPoint, point))
        interceptEdge(point, bottomRight, topRight, viewPoint, point);
    if(!interceptEdge(point, topRight, topLeft, viewPoint, point))
        interceptEdge(point, bottomLeft, bottomRight, viewPoint, point);
}

/**
 * Draws all the points marked by the player.
 */
static void drawMarkedPoints(uiwidget_t* obj, float scale)
{
    //guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const float alpha = uiRendState->pageAlpha;
    coord_t bottomLeft[2], topLeft[2], bottomRight[2], topRight[2], viewPoint[2];
    float angle;
    uint i, pointCount = UIAutomap_PointCount(obj);
    const Point2Raw origin = { 0, 0 };
    assert(obj->type == GUI_AUTOMAP);

    if(!pointCount) return;

    // Calculate final scale factor.
    scale = UIAutomap_FrameToMap(obj, 1) * scale;
#if __JHERETIC__ || __JHEXEN__
    // These games use a larger font, so use a smaller scale.
    scale *= .5f;
#endif

    UIAutomap_CameraOrigin(obj, &viewPoint[0], &viewPoint[1]);
    UIAutomap_VisibleBounds(obj, topLeft, bottomRight, topRight, bottomLeft);

    angle = UIAutomap_CameraAngle(obj);

    for(i = 0; i < pointCount; ++i)
    {
        coord_t point[2];
        char label[10];

        if(!UIAutomap_PointOrigin(obj, i, &point[0], &point[1], NULL)) continue;

        dd_snprintf(label, 10, "%i", i);

        positionPointInView(obj, point, topLeft, topRight, bottomRight, bottomLeft, viewPoint);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(point[0], point[1], 0);
        DGL_Scalef(scale, scale, 1);
        DGL_Rotatef(angle, 0, 0, 1);
        DGL_Scalef(1, -1, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_MAPPOINT));
#if __JDOOM__
        if(gameMode == doom2_hacx)
            FR_SetColorAndAlpha(1, 1, 1, alpha);
        else
            FR_SetColorAndAlpha(.22f, .22f, .22f, alpha);
#else
        FR_SetColorAndAlpha(1, 1, 1, alpha);
#endif
        FR_DrawText3(label, &origin, 0, DTF_ONLY_SHADOW);

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

/**
 * Sets up the state for automap drawing.
 */
static void setupGLStateForMap(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const float alpha = uiRendState->pageAlpha;
    int player = UIWidget_Player(obj);
    float angle, bgColor[3];
    coord_t plx, ply;
    RectRaw geometry;
    assert(obj->type == GUI_AUTOMAP);

    UIAutomap_ParallaxLayerOrigin(obj, &plx, &ply);
    angle = UIAutomap_CameraAngle(obj);

    Rect_Raw(obj->geometry, &geometry);

    // Check for scissor box (to clip the map lines and stuff).
    // Store the old scissor state.
    am->scissorState = DGL_GetInteger(DGL_SCISSOR_TEST);
    DGL_Scissor(&am->scissorRegion);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

#if __JHERETIC__ || __JHEXEN__
    if(W_CheckLumpNumForName("AUTOPAGE") == -1)
    {
        bgColor[CR] = .55f; bgColor[CG] = .45f; bgColor[CB] = .35f;
    }
    else
    {
        AM_GetMapColor(bgColor, cfg.automapBack, WHITE, customPal);
    }
#else
    AM_GetMapColor(bgColor, cfg.automapBack, BACKGROUND, customPal);
#endif

    // Do we want a background texture?
    if(autopageLumpNum != -1)
    {
        // Apply the background texture onto a parallaxing layer which
        // follows the map view target (not player).
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PushMatrix();
        DGL_LoadIdentity();

        DGL_SetRawImage(autopageLumpNum, DGL_REPEAT, DGL_REPEAT);
        DGL_Color4f(bgColor[CR], bgColor[CG], bgColor[CB], cfg.automapOpacity * alpha);

        DGL_Translatef(geometry.origin.x, geometry.origin.y, 0);

        // Apply the parallax scrolling, map rotation and counteract the
        // aspect of the quad (sized to map window dimensions).
        DGL_Translatef(UIAutomap_MapToFrame(obj, plx) + .5f,
                       UIAutomap_MapToFrame(obj, ply) + .5f, 0);
        DGL_Scalef(1, 1.2f/*aspect correct*/, 1);
        DGL_Rotatef(360-angle, 0, 0, 1);
        DGL_Scalef(1, (float)geometry.size.height / geometry.size.width, 1);
        DGL_Translatef(-(.5f), -(.5f), 0);

        DGL_DrawRectf2(0, 0, geometry.size.width, geometry.size.height);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();

        DGL_Disable(DGL_TEXTURE_2D);
    }
    else
    {
        // Nope just a solid color.
        DGL_SetNoMaterial();
        DGL_Color4f(bgColor[CR], bgColor[CG], bgColor[CB], cfg.automapOpacity * alpha);
        DGL_DrawRectf2(0, 0, geometry.size.width, geometry.size.height);
    }

#if __JDOOM64__
    // jd64 > Demon keys
    // If drawn in HUD we don't need them visible in the map too.
    if(!cfg.hudShown[HUD_INVENTORY])
    {
        int i, num = 0;
        player_t* plr = &players[player];
        inventoryitemtype_t items[3] = {
            IIT_DEMONKEY1,
            IIT_DEMONKEY2,
            IIT_DEMONKEY3
        };

        for(i = 0; i < 3; ++i)
            num += P_InventoryCount(player, items[i])? 1 : 0;

        if(num > 0)
        {
            float x, y, w, h, spacing, scale, iconAlpha;
            spriteinfo_t sprInfo;
            int invItemSprites[NUM_INVENTORYITEM_TYPES] = {
                SPR_ART1, SPR_ART2, SPR_ART3
            };

            iconAlpha = MINMAX_OF(.0f, alpha, .5f);

            spacing = geometry.size.height / num;
            y = 0;

            for(i = 0; i < 3; ++i)
            {
                if(P_InventoryCount(player, items[i]))
                {
                    R_GetSpriteInfo(invItemSprites[i], 0, &sprInfo);
                    DGL_SetPSprite(sprInfo.material);
                    DGL_Enable(DGL_TEXTURE_2D);

                    scale = geometry.size.height / (sprInfo.geometry.size.height * num);
                    x = geometry.size.width - sprInfo.geometry.size.width * scale;
                    w = sprInfo.geometry.size.width;
                    h = sprInfo.geometry.size.height;

                    DGL_Color4f(1, 1, 1, iconAlpha);
                    DGL_Begin(DGL_QUADS);
                        DGL_TexCoord2f(0, 0, 0);
                        DGL_Vertex2f(x, y);

                        DGL_TexCoord2f(0, sprInfo.texCoord[0], 0);
                        DGL_Vertex2f(x + w * scale, y);

                        DGL_TexCoord2f(0, sprInfo.texCoord[0], sprInfo.texCoord[1]);
                        DGL_Vertex2f(x + w * scale, y + h * scale);

                        DGL_TexCoord2f(0, 0, sprInfo.texCoord[1]);
                        DGL_Vertex2f(x, y + h * scale);
                    DGL_End();

                    DGL_Disable(DGL_TEXTURE_2D);

                    y += spacing;
                }
            }
        }
    }
    // < d64tc
#endif

    // Setup the scissor clipper.
    /// @todo Do this in the UI module.
    {
    const int border = .5f + UIAUTOMAP_BORDER * aspectScale;
    RectRaw clipRegion;

    Rect_Raw(UIWidget_Geometry(obj), &clipRegion);
    clipRegion.origin.x += border;
    clipRegion.origin.y += border;
    clipRegion.size.width  -= 2 * border;
    clipRegion.size.height -= 2 * border;

    DGL_SetScissor(&clipRegion);
    DGL_Enable(DGL_SCISSOR_TEST);
    }
}

/**
 * Restores the previous GL draw state
 */
static void restoreGLStateFromMap(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    // Restore the previous scissor state.
    if(!am->scissorState)
        DGL_Disable(DGL_SCISSOR_TEST);
    DGL_SetScissor(&am->scissorRegion);
}

static void renderVertexes(uiwidget_t* obj)
{
    //guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const float alpha = uiRendState->pageAlpha;
    float v[2], oldPointSize;
    int i;
    assert(obj->type == GUI_AUTOMAP);

    DGL_Color4f(.2f, .5f, 1, alpha);

    DGL_Enable(DGL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 4 * aspectScale);

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
static void compileObjectLists(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    uint i;
    assert(obj->type == GUI_AUTOMAP);

    UIAutomap_ClearLists(obj);

    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        // Build commands and compile to a display list.
        if(DGL_NewList(0, DGL_COMPILE))
        {
            renderWalls(obj, i, true);
            am->lists[i] = DGL_EndList();
        }
    }

    am->constructMap = false;
}

automapcfg_t* UIAutomap_Config(uiwidget_t* obj)
{
    guidata_automap_t* map = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return map->mcfg;
}

void UIAutomap_Rebuild(uiwidget_t* obj)
{
    guidata_automap_t* map = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    map->constructMap = true;
}

/**
 * Render the automap view window for the specified player.
 */
void UIAutomap_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    static int updateWait = 0; /// @todo should be an instance var of UIAutomap

    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const float alpha = uiRendState->pageAlpha;
    player_t* plr = &players[UIWidget_Player(obj)];
    coord_t vx, vy;
    float angle, oldLineWidth;
    RectRaw geometry;
    int i;
    assert(obj->type == GUI_AUTOMAP);

    if(!plr->plr->inGame) return;

    // Configure render state:
    rs.plr = plr;
    UIAutomap_CameraOrigin(obj, &vx, &vy);
    angle = UIAutomap_CameraAngle(obj);
    Rect_Raw(obj->geometry, &geometry);

    // Freeze the lists if the map is fading out from being open, or for debug.
    if((++updateWait % 10) && am->constructMap && !freezeMapRLs && UIAutomap_Active(obj))
    {
        // Its time to rebuild the automap object display lists.
        compileObjectLists(obj);
    }

    // Setup for frame.
    setupGLStateForMap(obj);

    // Configure the modelview matrix so that we can draw geometry for world
    // objects using their world-space coordinates directly.
    DGL_MatrixMode(DGL_MODELVIEW);
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Translatef(geometry.size.width  / 2,
                   geometry.size.height / 2, 0);
    DGL_Rotatef(angle, 0, 0, 1);
    DGL_Scalef(1, -1, 1); // In the world coordinate space Y+ is up.
    DGL_Scalef(am->scaleMTOF, am->scaleMTOF, 1);
    DGL_Translatef(-vx, -vy, 0);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, MINMAX_OF(.5f, cfg.automapLineWidth * aspectScale, 3.f));

/*#if _DEBUG
    // Draw the rectangle described by the visible bounds.
    {
    coord_t topLeft[2], bottomRight[2], topRight[2], bottomLeft[2];
    UIAutomap_VisibleBounds(obj, topLeft, bottomRight, topRight, bottomLeft);
    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_LINES);
        DGL_Vertex2f(    topLeft[0],     topLeft[1]);
        DGL_Vertex2f(   topRight[0],    topRight[1]);
        DGL_Vertex2f(   topRight[0],    topRight[1]);
        DGL_Vertex2f(bottomRight[0], bottomRight[1]);
        DGL_Vertex2f(bottomRight[0], bottomRight[1]);
        DGL_Vertex2f( bottomLeft[0],  bottomLeft[1]);
        DGL_Vertex2f( bottomLeft[0],  bottomLeft[1]);
        DGL_Vertex2f(    topLeft[0],     topLeft[1]);
    DGL_End();
    }
#endif*/

    if(amMaskTexture)
    {
        const int border = .5f + UIAUTOMAP_BORDER * aspectScale;

        DGL_Bind(amMaskTexture);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_LoadIdentity();

        DGL_PushMatrix();
        DGL_Scalef(1.f / (geometry.size.width  - border*2),
                   1.f / (geometry.size.height - border*2), 1);
        DGL_Translatef(geometry.size.width  /2 - border,
                       geometry.size.height /2 - border, 0);
        DGL_Rotatef(-angle, 0, 0, 1);
        DGL_Scalef(am->scaleMTOF, am->scaleMTOF, 1);
        DGL_Translatef(-vx, -vy, 0);
    }

    // Draw static map geometry.
    for(i = NUM_MAP_OBJECTLISTS-1; i >= 0; i--)
    {
        if(am->lists[i])
        {
            const automapcfg_lineinfo_t* info = &am->mcfg->mapObjectInfo[i];

            // Setup the global list state.
            DGL_Color4f(info->rgba[0], info->rgba[1], info->rgba[2], info->rgba[3] * cfg.automapLineAlpha * alpha);
            DGL_BlendMode(info->blendMode);

            // Draw.
            DGL_CallList(am->lists[i]);
        }
    }

    // Draw dynamic map geometry.
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(UIAutomap_Flags(obj) & AMF_REND_SPECIALLINES)
    {
        rendXGLinedefs(obj);
    }
#endif
    rendPolyobjs(obj);

    // Restore the previous state.
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    if(UIAutomap_Flags(obj) & AMF_REND_VERTEXES)
    {
        renderVertexes(obj);
    }

    // Draw map objects:
    if(UIAutomap_Flags(obj) & (AMF_REND_THINGS|AMF_REND_KEYS))
    {
        rendThingPoints(obj);
    }

    // Sharp player markers.
    DGL_SetFloat(DGL_LINE_WIDTH, 1.f);
    rendPlayerMarkers(obj);

    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);

    if(amMaskTexture)
    {
        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();
    }

    // Draw glows?
    if(cfg.automapShowDoors)
    {
        /// @todo Optimize: Hugely inefficent. Need a new approach.
        DGL_Enable(DGL_TEXTURE_2D);
        renderWalls(obj, -1, false);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    restoreGLStateFromMap(obj);

    drawMarkedPoints(obj, aspectScale);

    // Return to the normal GL state.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

boolean UIAutomap_Open(uiwidget_t* obj, boolean yes, boolean fast)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(G_GameState() != GS_MAP && yes) return false;

    if(yes == am->active) return false; // No change.

    am->targetAlpha = (yes? 1.f : 0.f);
    if(fast)
    {
        am->alpha = am->oldAlpha = am->targetAlpha;
    }
    else
    {
        // Reset the timer.
        am->oldAlpha = am->alpha;
        am->alphaTimer = 0.f;
    }

    am->active = (yes? true : false);

    if(am->active)
    {
        mobj_t* mo = UIAutomap_FollowMobj(obj);
        if(!mo)
        {
            // Set viewer target to the center of the map.
            coord_t aabb[4];
            UIAutomap_PVisibleAABounds(obj, &aabb[BOXLEFT], &aabb[BOXRIGHT], &aabb[BOXBOTTOM], &aabb[BOXTOP]);
            UIAutomap_SetCameraOrigin(obj, (aabb[BOXRIGHT] - aabb[BOXLEFT]) / 2, (aabb[BOXTOP] - aabb[BOXBOTTOM]) / 2);
            UIAutomap_SetCameraAngle(obj, 0);
        }
        else
        {
            // The map's target player is available.
            if(!(am->pan && !cfg.automapPanResetOnOpen))
            {
                coord_t origin[3];
                Mobj_OriginSmoothed(mo, origin);
                UIAutomap_SetCameraOrigin(obj, origin[VX], origin[VY]);
            }

            if(am->pan && cfg.automapPanResetOnOpen)
            {
                /* $unifiedangles */
                float angle = (am->rotate? (mo->angle - ANGLE_90) / (float) ANGLE_MAX * 360 : 0);
                UIAutomap_SetCameraAngle(obj, angle);
            }
        }
    }

    if(am->active)
    {
        DD_Execute(true, "activatebcontext map");
        if(am->pan)
            DD_Execute(true, "activatebcontext map-freepan");
    }
    else
    {
        DD_Execute(true, "deactivatebcontext map");
        DD_Execute(true, "deactivatebcontext map-freepan");
    }
    return true;
}

void UIAutomap_Ticker(uiwidget_t* obj, timespan_t ticLength)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    const int player = UIWidget_Player(obj);
    mobj_t* mo = UIAutomap_FollowMobj(obj);
    float panX[2], panY[2], zoomVel, zoomSpeed, width, height, scale;
    assert(obj->type == GUI_AUTOMAP);

    // Check the state of the controls. Done here so that offsets don't accumulate
    // unnecessarily, as they would, if left unread.
    P_GetControlState(player, CTL_MAP_PAN_X, &panX[0], &panX[1]);
    P_GetControlState(player, CTL_MAP_PAN_Y, &panY[0], &panY[1]);

    if(G_GameState() != GS_MAP) return;

    // Move towards the target alpha level for the automap.
    am->alphaTimer += (cfg.automapOpenSeconds == 0? 1 : 1/cfg.automapOpenSeconds * ticLength);
    if(am->alphaTimer >= 1)
        am->alpha = am->targetAlpha;
    else
        am->alpha = LERP(am->oldAlpha, am->targetAlpha, am->alphaTimer);

    // If the automap is not active do nothing else.
    if(!am->active)
        return;

    // Map view zoom contol.
    zoomSpeed = 1 + (2 * cfg.automapZoomSpeed) * ticLength * TICRATE;
    if(players[player].brain.speed)
        zoomSpeed *= 1.5f;

    P_GetControlState(player, CTL_MAP_ZOOM, &zoomVel, NULL); // ignores rel offset -jk
    if(zoomVel > 0) // zoom in
    {
        UIAutomap_SetScale(obj, am->viewScale * zoomSpeed);
    }
    else if(zoomVel < 0) // zoom out
    {
        UIAutomap_SetScale(obj, am->viewScale / zoomSpeed);
    }

    // Map camera panning control.
    if(am->pan || NULL == mo)
    {
        float panUnitsPerSecond;
        coord_t xy[2] = { 0, 0 }; // deltas

        // DOOM.EXE pans the automap at 140 fixed pixels per second (VGA: 200 pixels tall).
        /// @todo This needs resolution-independent units. (The "frame" units are screen pixels.)
        panUnitsPerSecond = UIAutomap_FrameToMap(obj, 140 * Rect_Height(UIWidget_Geometry(obj))/200.f) * (2 * cfg.automapPanSpeed);
        if(panUnitsPerSecond < 8)
            panUnitsPerSecond = 8;

        /// @todo Fix sensitivity for relative axes.

        xy[VX] = panX[0] * panUnitsPerSecond * ticLength + panX[1];
        xy[VY] = panY[0] * panUnitsPerSecond * ticLength + panY[1];
        V2d_Rotate(xy, am->angle / 360 * 2 * PI);

        if(xy[VX] || xy[VY])
        {
            UIAutomap_TranslateCameraOrigin2(obj, xy[VX], xy[VY], true /*instant change*/);
        }
    }
    else
    {
        /* $unifiedangles */
        const float angle = (am->rotate? (mo->angle - ANGLE_90) / (float) ANGLE_MAX * 360 : 0);
        coord_t origin[3];

        Mobj_OriginSmoothed(mo, origin);
        UIAutomap_SetCameraOrigin(obj, origin[VX], origin[VY]);
        UIAutomap_SetCameraAngle(obj, angle);
    }

    if(am->updateViewScale)
        calcViewScaleFactors(obj);

    // Map viewer location.
    am->viewTimer += (float)(.4 * ticLength * TICRATE);
    if(am->viewTimer >= 1)
    {
        am->viewX = am->targetViewX;
        am->viewY = am->targetViewY;
    }
    else
    {
        am->viewX = LERP(am->oldViewX, am->targetViewX, am->viewTimer);
        am->viewY = LERP(am->oldViewY, am->targetViewY, am->viewTimer);
    }
    // Move the parallax layer.
    am->viewPLX = am->viewX / 4000;
    am->viewPLY = am->viewY / 4000;

    // Map view scale (zoom).
    am->viewScaleTimer += (float)(.4 * ticLength * TICRATE);
    if(am->viewScaleTimer >= 1)
    {
        am->viewScale = am->targetViewScale;
    }
    else
    {
        am->viewScale = LERP(am->oldViewScale, am->targetViewScale, am->viewScaleTimer);
    }

    // Map view rotation.
    am->angleTimer += (float)(.4 * ticLength * TICRATE);
    if(am->angleTimer >= 1)
    {
        am->angle = am->targetAngle;
    }
    else
    {
        float diff, startAngle = am->oldAngle, endAngle = am->targetAngle;

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

        am->angle = LERP(startAngle, endAngle, am->angleTimer);
        if(am->angle < 0)
            am->angle += 360;
        else if(am->angle > 360)
            am->angle -= 360;
    }

    //
    // Activate the new scale, position etc.
    //
    scale = am->viewScale;

    // Scaling multipliers.
    am->scaleMTOF = scale;
    am->scaleFTOM = 1.0f / am->scaleMTOF;

    /**
     * Calculate the coordinates of the rotated view window.
     */
    // Determine fixed to screen space scaling factors.
    {
    coord_t viewPoint[2];
    float rads, viewWidth, viewHeight;
    const int border = .5f + UIAUTOMAP_BORDER * aspectScale;

    viewWidth  = UIAutomap_FrameToMap(obj, Rect_Width(obj->geometry)  - border*2);
    viewHeight = UIAutomap_FrameToMap(obj, Rect_Height(obj->geometry) - border*2);
    am->topLeft[0]     = am->bottomLeft[0] = -viewWidth/2;
    am->topLeft[1]     = am->topRight[1]   =  viewHeight/2;
    am->bottomRight[0] = am->topRight[0]   =  viewWidth/2;
    am->bottomRight[1] = am->bottomLeft[1] = -viewHeight/2;

    // Apply rotation.
    rads = (float)(am->angle / 360 * 2 * PI);
    V2d_Rotate(am->topLeft,     rads);
    V2d_Rotate(am->bottomRight, rads);
    V2d_Rotate(am->bottomLeft,  rads);
    V2d_Rotate(am->topRight,    rads);

    // Translate to the view point.
    UIAutomap_CameraOrigin(obj, &viewPoint[0], &viewPoint[1]);
    V2d_Sum(am->topLeft,     am->topLeft,     viewPoint);
    V2d_Sum(am->bottomRight, am->bottomRight, viewPoint);
    V2d_Sum(am->bottomLeft,  am->bottomLeft,  viewPoint);
    V2d_Sum(am->topRight,    am->topRight,    viewPoint);
    }

    width  = UIAutomap_FrameToMap(obj, Rect_Width(obj->geometry));
    height = UIAutomap_FrameToMap(obj, Rect_Height(obj->geometry));

    // Calculate the in-view, AABB.
    {   // Rotation-aware.
#define ADDTOBOX(b, x, y) if((x) < (b)[BOXLEFT]) \
    (b)[BOXLEFT] = (x); \
    else if((x) > (b)[BOXRIGHT]) \
        (b)[BOXRIGHT] = (x); \
    if((y) < (b)[BOXBOTTOM]) \
        (b)[BOXBOTTOM] = (y); \
    else if((y) > (b)[BOXTOP]) \
        (b)[BOXTOP] = (y);

    float angle;
    coord_t v[2];

    angle = am->angle;

    v[0] = -width / 2;
    v[1] = -height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += am->viewX;
    v[1] += am->viewY;

    am->viewAABB[BOXLEFT] = am->viewAABB[BOXRIGHT]  = v[0];
    am->viewAABB[BOXTOP]  = am->viewAABB[BOXBOTTOM] = v[1];

    v[0] = width / 2;
    v[1] = -height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += am->viewX;
    v[1] += am->viewY;
    ADDTOBOX(am->viewAABB, v[0], v[1]);

    v[0] = -width / 2;
    v[1] = height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += am->viewX;
    v[1] += am->viewY;
    ADDTOBOX(am->viewAABB, v[0], v[1]);

    v[0] = width / 2;
    v[1] = height / 2;
    rotate2D(&v[0], &v[1], angle);
    v[0] += am->viewX;
    v[1] += am->viewY;
    ADDTOBOX(am->viewAABB, v[0], v[1]);

#undef ADDTOBOX
    }
}

float UIAutomap_MapToFrame(uiwidget_t* obj, float val)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return MTOF(am, val);
}

float UIAutomap_FrameToMap(uiwidget_t* obj, float val)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return FTOM(am, val);
}

void UIAutomap_UpdateGeometry(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    RectRaw newGeom;
    assert(obj->type == GUI_AUTOMAP);

    // Determine whether the available space has changed and thus whether
    // the position and/or size of the automap must therefore change too.
    R_ViewWindowGeometry(UIWidget_Player(obj), &newGeom);

/*#ifdef _DEBUG
    Con_Message("UIAutomap_UpdateGeometry: newGeom %i,%i %i,%i current %i,%i %i,%i",
                newGeom.origin.x, newGeom.origin.y,
                newGeom.size.width, newGeom.size.height,
                Rect_X(obj->geometry), Rect_Y(obj->geometry), Rect_Width(obj->geometry), Rect_Height(obj->geometry));
#endif*/

    if(newGeom.origin.x != Rect_X(obj->geometry) ||
       newGeom.origin.y != Rect_Y(obj->geometry) ||
       newGeom.size.width != Rect_Width(obj->geometry) ||
       newGeom.size.height != Rect_Height(obj->geometry))
    {
        Rect_SetXY(obj->geometry, newGeom.origin.x, newGeom.origin.y);
        Rect_SetWidthHeight(obj->geometry, newGeom.size.width, newGeom.size.height);

        // Now the screen dimensions have changed we have to update scaling
        // factors accordingly.
        am->updateViewScale = true;
    }
}

void UIAutomap_CameraOrigin(uiwidget_t* obj, coord_t* x, coord_t* y)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(x) *x = am->viewX;
    if(y) *y = am->viewY;
}

boolean UIAutomap_SetCameraOrigin(uiwidget_t* obj, coord_t x, coord_t y)
{
    return UIAutomap_SetCameraOrigin2(obj, x, y, false);
}

boolean UIAutomap_SetCameraOrigin2(uiwidget_t* obj, coord_t x, coord_t y, boolean forceInstantly)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    boolean instantChange = forceInstantly;
    assert(obj->type == GUI_AUTOMAP);

    // Already at this target?
    if(x == am->targetViewX && y == am->targetViewY)
        return false;

    if(!forceInstantly && am->maxViewPositionDelta > 0)
    {
        coord_t dx, dy, dist;

        dx = am->viewX - x;
        dy = am->viewY - y;
        dist = (coord_t) sqrt(dx * dx + dy * dy);
        if(dist < 0)
            dist = -dist;

        if(dist > am->maxViewPositionDelta)
            instantChange = true;
    }

    if(instantChange)
    {
        am->viewX = am->oldViewX = am->targetViewX = x;
        am->viewY = am->oldViewY = am->targetViewY = y;
    }
    else
    {
        am->oldViewX = am->viewX;
        am->oldViewY = am->viewY;
        am->targetViewX = x;
        am->targetViewY = y;
        // Restart the timer.
        am->viewTimer = 0;
    }
    return true;
}

boolean UIAutomap_TranslateCameraOrigin(uiwidget_t* obj, coord_t x, coord_t y)
{
    return UIAutomap_TranslateCameraOrigin2(obj, x, y, false);
}

boolean UIAutomap_TranslateCameraOrigin2(uiwidget_t* obj, coord_t x, coord_t y, boolean forceInstantly)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return UIAutomap_SetCameraOrigin2(obj, am->viewX + x, am->viewY + y, forceInstantly);
}

void UIAutomap_ParallaxLayerOrigin(uiwidget_t* obj, coord_t* x, coord_t* y)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(x) *x = am->viewPLX;
    if(y) *y = am->viewPLY;
}

float UIAutomap_CameraAngle(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->angle;
}

boolean UIAutomap_SetCameraAngle(uiwidget_t* obj, float angle)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    angle = MINMAX_OF(0, angle, 359.9999f);
    if(angle == am->targetAngle) return false;
    am->oldAngle = am->angle;
    am->targetAngle = angle;
    // Restart the timer.
    am->angleTimer = 0;
    return true;
}

boolean UIAutomap_SetScale(uiwidget_t* obj, float scale)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(am->updateViewScale)
        calcViewScaleFactors(obj);

    scale = MINMAX_OF(am->minScaleMTOF, scale, am->maxScaleMTOF);

    // Already at this target?
    if(scale == am->targetViewScale)
        return false;

    am->oldViewScale = am->viewScale;
    // Restart the timer.
    am->viewScaleTimer = 0;

    am->targetViewScale = scale;
    return true;
}

boolean UIAutomap_Active(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->active;
}

boolean UIAutomap_Reveal(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->reveal;
}

boolean UIAutomap_SetReveal(uiwidget_t* obj, boolean yes)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    boolean oldReveal = am->reveal;
    assert(obj->type == GUI_AUTOMAP);

    am->reveal = yes;
    if(oldReveal != am->reveal)
    {
        UIAutomap_Rebuild(obj);
    }
    return false;
}

void UIAutomap_PVisibleAABounds(const uiwidget_t* obj, coord_t* lowX, coord_t* hiX,
    coord_t* lowY, coord_t* hiY)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(lowX) *lowX = am->viewAABB[BOXLEFT];
    if(hiX)  *hiX  = am->viewAABB[BOXRIGHT];
    if(lowY) *lowY = am->viewAABB[BOXBOTTOM];
    if(hiY)  *hiY  = am->viewAABB[BOXTOP];
}

void UIAutomap_VisibleBounds(const uiwidget_t* obj, coord_t topLeft[2],
    coord_t bottomRight[2], coord_t topRight[2], coord_t bottomLeft[2])
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(topLeft)
    {
        topLeft[0] = am->topLeft[0];
        topLeft[1] = am->topLeft[1];
    }
    if(bottomRight)
    {
        bottomRight[0] = am->bottomRight[0];
        bottomRight[1] = am->bottomRight[1];
    }
    if(topRight)
    {
        topRight[0] = am->topRight[0];
        topRight[1] = am->topRight[1];
    }
    if(bottomLeft)
    {
        bottomLeft[0] = am->bottomLeft[0];
        bottomLeft[1] = am->bottomLeft[1];
    }
}

void UIAutomap_ClearPoints(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    int i;
    assert(obj->type == GUI_AUTOMAP);

    for(i = 0; i < MAX_MAP_POINTS; ++i)
    {
        am->pointsUsed[i] = false;
    }
    am->pointCount = 0;
}

int UIAutomap_PointCount(const uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    int i, used = 0;
    assert(obj->type == GUI_AUTOMAP);

    for(i = 0; i < MAX_MAP_POINTS; ++i)
    {
        if(am->pointsUsed[i])
            ++used;
    }
    return used;
}

int UIAutomap_AddPoint(uiwidget_t* obj, coord_t x, coord_t y, coord_t z)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    unsigned int newIdx;
    guidata_automap_point_t* point;
    assert(obj->type == GUI_AUTOMAP);

    newIdx = am->pointCount;
    point = &am->points[newIdx];
    point->pos[0] = x;
    point->pos[1] = y;
    point->pos[2] = z;
    am->pointsUsed[newIdx] = true;
    ++am->pointCount;

    return newIdx;
}

boolean UIAutomap_PointOrigin(const uiwidget_t* obj, int pointIdx, coord_t* x, coord_t* y, coord_t* z)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(!x && !y && !z) return false;

    if(pointIdx >= 0 && pointIdx < MAX_MAP_POINTS && am->pointsUsed[pointIdx])
    {
        const guidata_automap_point_t* point = &am->points[pointIdx];
        if(x) *x = point->pos[0];
        if(y) *y = point->pos[1];
        if(z) *z = point->pos[2];
        return true;
    }
    return false;
}

boolean UIAutomap_ZoomMax(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->forceMaxScale;
}

boolean UIAutomap_SetZoomMax(uiwidget_t* obj, boolean on)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    boolean oldZoomMax = am->forceMaxScale;
    assert(obj->type == GUI_AUTOMAP);

    if(am->updateViewScale)
        calcViewScaleFactors(obj);

    // When switching to max scale mode, store the old scale.
    if(!am->forceMaxScale)
        am->priorToMaxScale = am->viewScale;

    am->forceMaxScale = on;
    UIAutomap_SetScale(obj, (am->forceMaxScale? 0 : am->priorToMaxScale));
    return (oldZoomMax != am->forceMaxScale);
}

boolean UIAutomap_PanMode(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->pan;
}

boolean UIAutomap_SetPanMode(uiwidget_t* obj, boolean on)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    boolean oldPanMode = am->pan;
    assert(obj->type == GUI_AUTOMAP);

    am->pan = on;
    if(oldPanMode != am->pan)
    {
        DD_Executef(true, "%sactivatebcontext map-freepan", oldPanMode? "de" : "");
        return true;
    }
    return false;
}

mobj_t* UIAutomap_FollowMobj(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    if(players[am->followPlayer].plr->inGame)
    {
        return players[am->followPlayer].plr->mo;
    }
    return NULL;
}

boolean UIAutomap_CameraRotation(uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->rotate;
}

boolean UIAutomap_SetCameraRotation(uiwidget_t* obj, boolean on)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    boolean oldRotate = am->rotate;
    assert(obj->type == GUI_AUTOMAP);

    am->rotate = on;
    return (oldRotate != am->rotate);
}

float UIAutomap_Opacity(const uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->alpha;
}

boolean UIAutomap_SetOpacity(uiwidget_t* obj, float value)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    value = MINMAX_OF(0, value, 1);
    // Already at this target?
    if(value == am->targetAlpha) return false;

    am->oldAlpha = am->alpha;
    // Restart the timer.
    am->alphaTimer = 0;

    am->targetAlpha = value;
    return true;
}

int UIAutomap_Flags(const uiwidget_t* obj)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    return am->flags;
}

void UIAutomap_SetFlags(uiwidget_t* obj, int flags)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    am->flags = flags;
    // We will need to rebuild one or more display lists.
    UIAutomap_Rebuild(obj);
}

void UIAutomap_SetWorldBounds(uiwidget_t* obj, coord_t lowX, coord_t hiX, coord_t lowY, coord_t hiY)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    am->bounds[BOXLEFT]   = lowX;
    am->bounds[BOXTOP]    = hiY;
    am->bounds[BOXRIGHT]  = hiX;
    am->bounds[BOXBOTTOM] = lowY;
    am->updateViewScale = true;

    // Update minScaleMTOF.
    calcViewScaleFactors(obj);

#ifdef _DEBUG
    Con_Message("SetWorldBounds: low=%f,%f hi=%f,%f minScaleMTOF=%f", lowX, lowY, hiX, hiY,
                am->minScaleMTOF);
#endif

    // Choose a default view scale factor.
    UIAutomap_SetScale(obj, am->minScaleMTOF * 2.4f);
}

void UIAutomap_SetMinScale(uiwidget_t* obj, const float scale)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    am->minScale = MAX_OF(1, scale);
    am->updateViewScale = true;
}

void UIAutomap_SetCameraOriginFollowMoveDelta(uiwidget_t* obj, coord_t max)
{
    guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
    assert(obj->type == GUI_AUTOMAP);

    am->maxViewPositionDelta = MINMAX_OF(0, max, 32768*2);
}
