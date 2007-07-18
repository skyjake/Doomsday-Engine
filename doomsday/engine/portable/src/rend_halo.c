/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * rend_halo.c: Halos and Flares
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

//#define Z_TEST_BIAS       .00005
#define NUM_FLARES      5

// TYPES -------------------------------------------------------------------

typedef struct flare_s {
    float   offset;
    float   size;
    float   alpha;
    int     texture;            // -1=dlight, 0=flare, 1=brflare, 2=bigflare
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
#if 0
    {0, 0.85f, .6f, -1 /*2 */ },    // Primary flare.
    {1, .35f, .3f, 0},          // Main secondary flare.
    {1.5f, .25f, .2f, 1},
    {-.6f, .2f, .2f, 0},
    {.4f, .25f, .15f, 0}
#endif

    {0, 1, 1, 0},               // Primary flare.
    {1, .41f, .5f, 0},          // Main secondary flare.
    {1.5f, .29f, .333f, 1},
    {-.6f, .24f, .333f, 0},
    {.4f, .29f, .25f, 0}
};

/*  { 0, 1 },       // Primary flare.
   { 1, 2 },        // Main secondary flare.
   { 1.5f, 4 },
   { -.6f, 4 },
   { .4f, 4 }
 */

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void H_Register(void)
{
    cvar_t  cvars[] = {
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
            gl.Disable(DGL_FOG);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Disable(DGL_DEPTH_TEST);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
    }
    else
    {
        if(usingFog)
            gl.Enable(DGL_FOG);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    }
}

/**
 * The caller must check that <code>sourcevis</code> really has a ->light!
 *
 * @param x             X coordinate of the center of the halo.
 * @param y             Y coordinate of the center of the halo.
 * @param z             Z coordinate of the center of the halo.
 * @param lumobj        The lumobj casting the halo.
 * @param primary       If <code>true</code>, we'll draw the primary halo,
 *                      otherwise the secondary ones (which won't be clipped
 *                      or occluded by anything; they're drawn after
 *                      everything else, during a separate pass).
 *                      If <code>false</code>, the caller must setup the
 *                      rendering state.
 *
 * @return              <code>true</code> if a halo was rendered.
 */
boolean H_RenderHalo(float x, float y, float z, lumobj_t *lum, boolean primary)
{
    float   viewpos[3];
    float   viewtocenter[3], mirror[3], normalviewtocenter[3];
    float   leftoff[3], rightoff[3], center[3], radius;
    float   halopos[3], occlusionfactor;
    int     i, k, tex;
    float   color[4], radx, rady, scale, turnangle = 0;
    float   fadefactor = 1, secbold, secdimfactor;
    float   coloraverage, f, distancedim, lum_distance;
    flare_t *fl;

    if(!primary && haloRealistic)
    {
        // In the realistic mode we don't render secondary halos.
        return false;
    }

    if(devNoCulling || P_IsInVoid(viewplayer))
    {
        // Normal visible surface culling has been disabled meaning that this
        // halo should, more than likely, be occluded (at least partially) by
        // something else in the scene.
        // \fixme Therefore we need to check using a line-of-sight method
        // that only checks front facing geometry...?
            return false;
    }

    lum_distance = FIX2FLT(lum->distance);

    if(lum->flags & LUMF_NOHALO || lum_distance == 0 ||
       (haloFadeMax && lum_distance > haloFadeMax))
        return false;

    if(haloFadeMax && haloFadeMax != haloFadeMin && lum_distance < haloFadeMax
       && lum_distance >= haloFadeMin)
    {
        fadefactor =
            (lum_distance - haloFadeMin) / (haloFadeMax - haloFadeMin);
    }

    occlusionfactor = (lum->halofactor & 0x7f) / 127.0f;
    if(occlusionfactor == 0)
        return false;
    occlusionfactor = (1 + occlusionfactor) / 2;

    // viewsidevec is to the left.
    for(i = 0; i < 3; ++i)
    {
        leftoff[i] = viewupvec[i] + viewsidevec[i];
        rightoff[i] = viewupvec[i] - viewsidevec[i];
        // Convert the color to floating point.
        color[i] = lum->rgb[i];
    }

    // Setup the proper DGL state.
    if(primary)
        H_SetupState(true);

    center[VX] = x;
    center[VZ] = y;
    center[VY] = z + lum->zOff;

    // Apply the flare's X offset. (Positive is to the right.)
    for(i = 0; i < 3; i++)
        center[i] -= lum->xOff * viewsidevec[i];

    // Calculate the mirrored position.
    // Project viewtocenter vector onto viewsidevec.
    viewpos[0] = vx;
    viewpos[1] = vy;
    viewpos[2] = vz;
    for(i = 0; i < 3; i++)
        normalviewtocenter[i] = viewtocenter[i] = center[i] - viewpos[i];

    // Calculate the dimming factor for secondary flares.
    M_Normalize(normalviewtocenter);
    secdimfactor = M_DotProduct(normalviewtocenter, viewfrontvec);

    scale =
        M_DotProduct(viewtocenter, viewfrontvec) / M_DotProduct(viewfrontvec,
                                                                viewfrontvec);
    for(i = 0; i < 3; i++)
        halopos[i] = mirror[i] =
            (viewfrontvec[i] * scale - viewtocenter[i]) * 2;
    // Now adding 'mirror' to a position will mirror it.

    // Calculate texture turn angle.
    if(M_Normalize(halopos))
    {
        // Now halopos is a normalized version of the mirror vector.
        // Both vectors are on the view plane.
        if(!(lum->flags & LUMF_DONTTURNHALO))
        {
            turnangle = M_DotProduct(halopos, viewupvec);
            if(turnangle > 1)
                turnangle = 1;
            if(turnangle < -1)
                turnangle = -1;
            turnangle =
                turnangle >= 1 ? 0 : turnangle <= -1 ? PI : acos(turnangle);
            // On which side of the up vector (left or right)?
            if(M_DotProduct(halopos, viewsidevec) < 0)
                turnangle = -turnangle;
        }
        else
            turnangle = 0;
    }

    // Radius is affected by the precalculated 'flaresize' and the
    // distance to the source.

    // Prepare the texture rotation matrix.
    gl.MatrixMode(DGL_TEXTURE);
    gl.PushMatrix();
    gl.LoadIdentity();
    // Rotate around the center of the texture.
    gl.Translatef(0.5f, 0.5f, 0);
    gl.Rotatef(turnangle / PI * 180, 0, 0, 1);
    gl.Translatef(-0.5f, -0.5f, 0);

    // The overall brightness of the flare.
    coloraverage = (color[CR] + color[CG] + color[CB] + 1) / 4;
    //overallbrightness = lum->flareSize/25 * coloraverage;

    // Small flares have stronger dimming.
    f = lum_distance / lum->flareSize;
    if(haloDimStart && haloDimStart < haloDimEnd && f > haloDimStart)
        distancedim = 1 - (f - haloDimStart) / (haloDimEnd - haloDimStart);
    else
        distancedim = 1;

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
            f = minHaloSize * lum->flareSize / lum_distance;
            if(f > 1)
                f = 1;
        }
        f *= distancedim * lum->flareMul;

        // The color & alpha of the flare.
        color[CA] =
            f * (fl->alpha * occlusionfactor * fadefactor +
                 coloraverage * coloraverage / 5);

        radius =
            lum->flareSize * (1 - coloraverage / 3) +
            lum_distance / haloZMagDiv;
        if(radius < haloMinRadius)
            radius = haloMinRadius;
        radius *= occlusionfactor;

        secbold = coloraverage - 8 * (1 - secdimfactor);

        color[CA] *= .8f * haloBright / 100.0f;
        if(i)
        {
            color[CA] *= secbold;   // Secondary flare boldness.
        }
        if(color[CA] <= 0)
        {
            break;              // Not visible.
        }

        // In the realistic mode, halos are slightly dimmer.
        if(haloRealistic)
        {
            color[CA] *= .6f;
        }

        gl.Color4fv(color);

        if(haloRealistic)
        {
            // The 'realistic' halos just use the blurry round
            // texture unless custom.
            if(lum->flareCustom)
                tex = lum->flareTex;
            else
                tex = GL_PrepareLSTexture(LST_DYNAMIC, NULL);
        }
        else
        {
            if(primary && (lum->flareCustom || lum->flareTex))
            {
                tex = lum->flareTex;
            }
            else if(lum->flareSize > 45 ||
                    (coloraverage > .90 && lum->flareSize > 20))
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

        if(renderTextures)
            GL_BindTexture(tex);
        else
            gl.Bind(0);

        // Don't wrap the texture. Evidently some drivers can't just
        // take a hint... (or then something's changing the wrapping
        // mode inadvertently)
        gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

        // In the realistic mode, halos are slightly smaller.
        if(haloRealistic)
        {
            radius *= 0.8f;
        }

        // The final radius.
        radx = radius * fl->size;
        rady = radx / 1.2f;

        // Determine the final position of the halo.
        halopos[VX] = center[VX];
        halopos[VY] = center[VY];
        halopos[VZ] = center[VZ];
        if(i)                   // Secondary halos.
        {
            // Mirror it according to the flare table.
            for(k = 0; k < 3; k++)
                halopos[k] += mirror[k] * fl->offset;
        }

        gl.Begin(DGL_QUADS);
        gl.TexCoord2f(0, 0);
        gl.Vertex3f(halopos[VX] + radx * leftoff[VX],
                    halopos[VY] + rady * leftoff[VY],
                    halopos[VZ] + radx * leftoff[VZ]);
        gl.TexCoord2f(1, 0);
        gl.Vertex3f(halopos[VX] + radx * rightoff[VX],
                    halopos[VY] + rady * rightoff[VY],
                    halopos[VZ] + radx * rightoff[VZ]);
        gl.TexCoord2f(1, 1);
        gl.Vertex3f(halopos[VX] - radx * leftoff[VX],
                    halopos[VY] - rady * leftoff[VY],
                    halopos[VZ] - radx * leftoff[VZ]);
        gl.TexCoord2f(0, 1);
        gl.Vertex3f(halopos[VX] - radx * rightoff[VX],
                    halopos[VY] - rady * rightoff[VY],
                    halopos[VZ] - radx * rightoff[VZ]);
        gl.End();
    }

    gl.MatrixMode(DGL_TEXTURE);
    gl.PopMatrix();

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
    int     i;
    float   val;

    if(argc == 2)
    {
        if(!stricmp(argv[1], "list"))
        {
            for(i = 0; i < NUM_FLARES; i++)
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
