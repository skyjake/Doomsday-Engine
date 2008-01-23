/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * rend_halo.c: Halos and Flares
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define NUM_FLARES          5

// TYPES -------------------------------------------------------------------

typedef struct flare_s {
    float       offset;
    float       size;
    float       alpha;
    int         texture; // -1=dlight, 0=flare, 1=brflare, 2=bigflare
} flare_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(FlareConfig);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int devNoCulling;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     haloMode = 5, haloBright = 35, haloSize = 50;
int     haloRealistic = true;
int     haloOccludeSpeed = 48;
float   haloZMagDiv = 100, haloMinRadius = 20;
float   haloDimStart = 10, haloDimEnd = 100;

float   haloFadeMax = 0, haloFadeMin = 0, minHaloSize = 1;

flare_t flares[NUM_FLARES] = {
    {0, 1, 1, 0},               // Primary flare.
    {1, .41f, .5f, 0},          // Main secondary flare.
    {1.5f, .29f, .333f, 1},
    {-.6f, .24f, .333f, 0},
    {.4f, .29f, .25f, 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void H_Register(void)
{
    cvar_t          cvars[] = {
        {"rend-halo", 0, CVT_INT, &haloMode, 0, 5},
        {"rend-halo-realistic", 0, CVT_INT, &haloRealistic, 0, 1},
        {"rend-halo-bright", 0, CVT_INT, &haloBright, 0, 100},
        {"rend-halo-occlusion", CVF_NO_MAX, CVT_INT, &haloOccludeSpeed, 0, 0},
        {"rend-halo-size", 0, CVT_INT, &haloSize, 0, 100},
        {"rend-halo-secondary-limit", CVF_NO_MAX, CVT_FLOAT, &minHaloSize, 0, 0},
        {"rend-halo-fade-far", CVF_NO_MAX, CVT_FLOAT, &haloFadeMax, 0, 0},
        {"rend-halo-fade-near", CVF_NO_MAX, CVT_FLOAT, &haloFadeMin, 0, 0},
        {"rend-halo-zmag-div", CVF_NO_MAX, CVT_FLOAT, &haloZMagDiv, 1, 1},
        {"rend-halo-radius-min", CVF_NO_MAX, CVT_FLOAT, &haloMinRadius, 0, 0},
        {"rend-halo-dim-near", CVF_NO_MAX, CVT_FLOAT, &haloDimStart, 0, 0},
        {"rend-halo-dim-far", CVF_NO_MAX, CVT_FLOAT, &haloDimEnd, 0, 0},
        {NULL}
    };
    Con_AddVariableList(cvars);

    C_CMD_FLAGS("flareconfig", NULL, FlareConfig, CMDF_NO_DEDICATED);
}

void H_SetupState(boolean dosetup)
{
    if(dosetup)
    {
        if(usingFog)
            DGL_Disable(DGL_FOG);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        GL_BlendMode(BM_ADD);
    }
    else
    {
        if(usingFog)
            DGL_Enable(DGL_FOG);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        GL_BlendMode(BM_NORMAL);
    }
}

/**
 * The caller must check that @c sourcevis, really has a ->light!
 *
 * @param x         X coordinate of the center of the halo.
 * @param y         Y coordinate of the center of the halo.
 * @param z         Z coordinate of the center of the halo.
 * @param lumobj    The lumobj casting the halo.
 * @param primary   @c true = we'll draw the primary halo, otherwise the
 *                  secondary ones (which won't be clipped or occluded
 *                  by anything; they're drawn after everything else,
 *                  during a separate pass).
 *                  @c false = the caller must setup the rendering state.
 *
 * @return          @c true, if a halo was rendered.
 */
boolean H_RenderHalo(float x, float y, float z, lumobj_t *lum,
                     boolean primary)
{
    int             i, k, tex;
    float           viewPos[3];
    float           viewToCenter[3], mirror[3], normalViewToCenter[3];
    float           leftOff[3], rightOff[3], center[3], radius;
    float           haloPos[3], occlusionFactor;
    float           color[4], radX, radY, scale, turnAngle = 0;
    float           fadeFactor = 1, secBold, secDimFactor;
    float           colorAverage, f, distanceDim;
    flare_t        *fl;

    if(lum->type != LT_OMNI)
        return false; // Only omni lights support halos.

    if(!primary && haloRealistic)
    {
        // In the realistic mode we don't render secondary halos.
        return false;
    }

    if(devNoCulling || P_IsInVoid(viewPlayer))
    {
        // Normal visible surface culling has been disabled meaning that this
        // halo should, more than likely, be occluded (at least partially) by
        // something else in the scene.
        // \fixme Therefore we need to check using a line-of-sight method
        // that only checks front facing geometry...?
        return false;
    }

    if((lum->flags & LUMF_NOHALO) || lum->distanceToViewer <= 0 ||
       (haloFadeMax && lum->distanceToViewer > haloFadeMax))
        return false;

    if(haloFadeMax && haloFadeMax != haloFadeMin &&
       lum->distanceToViewer < haloFadeMax &&
       lum->distanceToViewer >= haloFadeMin)
    {
        fadeFactor = (lum->distanceToViewer - haloFadeMin) /
            (haloFadeMax - haloFadeMin);
    }

    occlusionFactor = (LUM_OMNI(lum)->haloFactor & 0x7f) / 127.0f;
    if(occlusionFactor == 0)
        return false;
    occlusionFactor = (1 + occlusionFactor) / 2;

    // viewSideVec is to the left.
    for(i = 0; i < 3; ++i)
    {
        leftOff[i] = viewUpVec[i] + viewSideVec[i];
        rightOff[i] = viewUpVec[i] - viewSideVec[i];
        // Convert the color to floating point.
        color[i] = lum->color[i];
    }

    // Setup the proper DGL state.
    if(primary)
        H_SetupState(true);

    center[VX] = x;
    center[VZ] = y;
    center[VY] = z + (LUM_OMNI(lum)->zOff - .5f) * viewUpVec[i];

    // Apply the flare's X offset. (Positive is to the right.)
    for(i = 0; i < 3; i++)
        center[i] -= (LUM_OMNI(lum)->xOff + .5f) * viewSideVec[i];

    // Calculate the mirrored position.
    // Project viewtocenter vector onto viewSideVec.
    viewPos[VX] = vx;
    viewPos[VY] = vy;
    viewPos[VZ] = vz;
    for(i = 0; i < 3; i++)
        normalViewToCenter[i] = viewToCenter[i] = center[i] - viewPos[i];

    // Calculate the dimming factor for secondary flares.
    M_Normalize(normalViewToCenter);
    secDimFactor = M_DotProduct(normalViewToCenter, viewFrontVec);

    scale = M_DotProduct(viewToCenter, viewFrontVec) /
                M_DotProduct(viewFrontVec, viewFrontVec);

    for(i = 0; i < 3; i++)
        haloPos[i] = mirror[i] =
            (viewFrontVec[i] * scale - viewToCenter[i]) * 2;
    // Now adding 'mirror' to a position will mirror it.

    // Calculate texture turn angle.
    if(M_Normalize(haloPos))
    {
        // Now halopos is a normalized version of the mirror vector.
        // Both vectors are on the view plane.
        if(!(lum->flags & LUMF_DONTTURNHALO))
        {
            turnAngle = M_DotProduct(haloPos, viewUpVec);
            if(turnAngle > 1)
                turnAngle = 1;
            else if(turnAngle < -1)
                turnAngle = -1;

            if(turnAngle >= 1)
                turnAngle = 0;
            else if(turnAngle <= -1)
                turnAngle = (float) PI;
            else
                turnAngle = acos(turnAngle);

            // On which side of the up vector (left or right)?
            if(M_DotProduct(haloPos, viewSideVec) < 0)
                turnAngle = -turnAngle;
        }
        else
        {
            turnAngle = 0;
        }
    }

    // Radius is affected by the precalculated 'flaresize' and the
    // distance to the source.

    // Prepare the texture rotation matrix.
    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    // Rotate around the center of the texture.
    DGL_Translatef(0.5f, 0.5f, 0);
    DGL_Rotatef(turnAngle / PI * 180, 0, 0, 1);
    DGL_Translatef(-0.5f, -0.5f, 0);

    // The overall brightness of the flare.
    colorAverage = (color[CR] + color[CG] + color[CB] + 1) / 4;

    // Small flares have stronger dimming.
    f = lum->distanceToViewer / LUM_OMNI(lum)->flareSize;
    if(haloDimStart && haloDimStart < haloDimEnd && f > haloDimStart)
        distanceDim = 1 - (f - haloDimStart) / (haloDimEnd - haloDimStart);
    else
        distanceDim = 1;

    for(i = 0, fl = flares; i < haloMode && i < NUM_FLARES; i++, fl++)
    {
        if(primary && i)
            break;
        if(!primary && !i)
            continue;

        f = 1;
        if(i)
        {
            // Secondary flare dimming?
            f = minHaloSize * LUM_OMNI(lum)->flareSize / lum->distanceToViewer;
            if(f > 1)
                f = 1;
        }
        f *= distanceDim * LUM_OMNI(lum)->flareMul;

        // The color & alpha of the flare.
        color[CA] = f * (fl->alpha * occlusionFactor * fadeFactor +
                 colorAverage * colorAverage / 5);

        radius = LUM_OMNI(lum)->flareSize * (1 - colorAverage / 3) +
            lum->distanceToViewer / haloZMagDiv;
        if(radius < haloMinRadius)
            radius = haloMinRadius;
        radius *= occlusionFactor;

        secBold = colorAverage - 8 * (1 - secDimFactor);

        color[CA] *= .8f * haloBright / 100.0f;
        if(i)
        {
            color[CA] *= secBold; // Secondary flare boldness.
        }

        if(color[CA] <= 0)
            break; // Not visible.

        // In the realistic mode, halos are slightly dimmer.
        if(haloRealistic)
        {
            color[CA] *= .6f;
        }

        if(haloRealistic)
        {
            // The 'realistic' halos just use the blurry round
            // texture unless custom.
            if(LUM_OMNI(lum)->flareCustom)
                tex = LUM_OMNI(lum)->flareTex;
            else
                tex = GL_PrepareLSTexture(LST_DYNAMIC, NULL);
        }
        else
        {
            if(primary &&
               (LUM_OMNI(lum)->flareCustom || LUM_OMNI(lum)->flareTex))
            {
                tex = LUM_OMNI(lum)->flareTex;
            }
            else if(LUM_OMNI(lum)->flareSize > 45 ||
                    (colorAverage > .90 && LUM_OMNI(lum)->flareSize > 20))
            {
                // The "Very Bright" condition.
                radius *= .65f;
                if(!i)
                    tex = GL_PrepareFlareTexture(FXT_BIGFLARE, NULL);
                else
                    tex = GL_PrepareFlareTexture(fl->texture, NULL);
            }
            else
            {
                if(!i)
                    tex = GL_PrepareLSTexture(LST_DYNAMIC, NULL);
                else
                    tex = GL_PrepareFlareTexture(fl->texture, NULL);
            }
        }

        // In the realistic mode, halos are slightly smaller.
        if(haloRealistic)
        {
            radius *= 0.8f;
        }

        // The final radius.
        radX = radius * fl->size;
        radY = radX / 1.2f;

        // Determine the final position of the halo.
        haloPos[VX] = center[VX];
        haloPos[VY] = center[VY];
        haloPos[VZ] = center[VZ];
        if(i)
        {   // Secondary halos.
            // Mirror it according to the flare table.
            for(k = 0; k < 3; ++k)
                haloPos[k] += mirror[k] * fl->offset;
        }

        if(renderTextures)
            GL_BindTexture(tex);
        else
            DGL_Bind(0);

        // Don't wrap the texture. Evidently some drivers can't just
        // take a hint... (or then something's changing the wrapping
        // mode inadvertently)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
					    GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);

        DGL_Color4fv(color);

        DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0);
        DGL_Vertex3f(haloPos[VX] + radX * leftOff[VX],
                     haloPos[VY] + radY * leftOff[VY],
                     haloPos[VZ] + radX * leftOff[VZ]);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex3f(haloPos[VX] + radX * rightOff[VX],
                     haloPos[VY] + radY * rightOff[VY],
                     haloPos[VZ] + radX * rightOff[VZ]);
        DGL_TexCoord2f(1, 1);
        DGL_Vertex3f(haloPos[VX] - radX * leftOff[VX],
                     haloPos[VY] - radY * leftOff[VY],
                     haloPos[VZ] - radX * leftOff[VZ]);
        DGL_TexCoord2f(0, 1);
        DGL_Vertex3f(haloPos[VX] - radX * rightOff[VX],
                     haloPos[VY] - radY * rightOff[VY],
                     haloPos[VZ] - radX * rightOff[VZ]);
        DGL_End();
    }

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

    // Undo the changes to the DGL state.
    if(primary)
        H_SetupState(false);

    return true;
}

/**
 * flareconfig list
 * flareconfig (num) pos/size/alpha/tex (val)
 */
D_CMD(FlareConfig)
{
    int             i;
    float           val;

    if(argc == 2)
    {
        if(!stricmp(argv[1], "list"))
        {
            for(i = 0; i < NUM_FLARES; ++i)
            {
                Con_Message("%i: pos:%f s:%.2f a:%.2f tex:%i\n", i,
                            flares[i].offset, flares[i].size, flares[i].alpha,
                            flares[i].texture);
            }
        }
    }
    else if(argc == 4)
    {
        i = atoi(argv[1]);
        val = strtod(argv[3], NULL);
        if(i < 0 || i >= NUM_FLARES)
            return false;

        if(!stricmp(argv[2], "pos"))
        {
            flares[i].offset = val;
        }
        else if(!stricmp(argv[2], "size"))
        {
            flares[i].size = val;
        }
        else if(!stricmp(argv[2], "alpha"))
        {
            flares[i].alpha = val;
        }
        else if(!stricmp(argv[2], "tex"))
        {
            flares[i].texture = (int) val;
        }
    }
    else
    {
        Con_Printf("Usage:\n");
        Con_Printf("  %s list\n", argv[0]);
        Con_Printf("  %s (num) pos/size/alpha/tex (val)\n", argv[0]);
    }

    return true;
}
