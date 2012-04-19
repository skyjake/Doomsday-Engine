/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Halos and Lens Flares.
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

#define NUM_FLARES          5

// TYPES -------------------------------------------------------------------

typedef struct flare_s {
    float       offset;
    float       size;
    float       alpha;
    int         texture; // 0=round, 1=flare, 2=brflare, 3=bigflare
} flare_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(FlareConfig);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     haloMode = 5, haloBright = 35, haloSize = 80;
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
    cvartemplate_t cvars[] = {
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
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(dosetup)
    {
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        GL_BlendMode(BM_ADD);
    }
    else
    {
        GL_BlendMode(BM_NORMAL);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
}

/**
 * The caller must check that @c sourcevis, really has a ->light!
 *
 * @param x         X coordinate of the center of the halo.
 * @param y         Y coordinate of the center of the halo.
 * @param z         Z coordinate of the center of the halo.
 * @param size      The precalculated radius of the primary halo.
 * @param primary   @c true = we'll draw the primary halo, otherwise the
 *                  secondary ones (which won't be clipped or occluded
 *                  by anything; they're drawn after everything else,
 *                  during a separate pass).
 *                  @c false = the caller must setup the rendering state.
 *
 * @return          @c true, if a halo was rendered.
 */
boolean H_RenderHalo(coord_t x, coord_t y, coord_t z, float size, DGLuint tex,
                     const float color[3], coord_t distanceToViewer,
                     float occlusionFactor, float brightnessFactor,
                     float viewXOffset, boolean primary,
                     boolean viewRelativeRotate)
{
    int i, k;
    float viewPos[3];
    float viewToCenter[3], mirror[3], normalViewToCenter[3];
    float leftOff[3], rightOff[3], center[3], radius;
    float haloPos[3];
    float rgba[4], radX, radY, scale, turnAngle = 0;
    float fadeFactor = 1, secBold, secDimFactor;
    float colorAverage, f, distanceDim;
    flare_t* fl;
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    // In realistic mode we don't render secondary halos.
    if(!primary && haloRealistic)
        return false;

    if(distanceToViewer <= 0 || occlusionFactor == 0 ||
       (haloFadeMax && distanceToViewer > haloFadeMax))
        return false;

    occlusionFactor = (1 + occlusionFactor) / 2;

    if(haloFadeMax && haloFadeMax != haloFadeMin &&
       distanceToViewer < haloFadeMax && distanceToViewer >= haloFadeMin)
    {
        fadeFactor = (distanceToViewer - haloFadeMin) / (haloFadeMax - haloFadeMin);
    }

    // viewSideVec is to the left.
    for(i = 0; i < 3; ++i)
    {
        leftOff[i] = viewData->upVec[i] + viewData->sideVec[i];
        rightOff[i] = viewData->upVec[i] - viewData->sideVec[i];
    }

    rgba[CR] = color[CR];
    rgba[CG] = color[CG];
    rgba[CB] = color[CB];
    rgba[CA] = 1; // Real alpha is set later.

    center[VX] = x;
    center[VZ] = y;
    center[VY] = z;

    // Apply the flare's X offset. (Positive is to the right.)
    for(i = 0; i < 3; i++)
        center[i] -= viewXOffset * viewData->sideVec[i];

    // Calculate the mirrored position.
    // Project viewtocenter vector onto viewSideVec.
    viewPos[VX] = vx;
    viewPos[VY] = vy;
    viewPos[VZ] = vz;

    for(i = 0; i < 3; ++i)
        normalViewToCenter[i] = viewToCenter[i] = center[i] - viewPos[i];
    V3f_Normalize(normalViewToCenter);

    // Calculate the dimming factor for secondary flares.
    secDimFactor = V3f_DotProduct(normalViewToCenter, viewData->frontVec);

    scale = V3f_DotProduct(viewToCenter, viewData->frontVec) / V3f_DotProduct(viewData->frontVec, viewData->frontVec);

    for(i = 0; i < 3; ++i)
        haloPos[i] = mirror[i] = (viewData->frontVec[i] * scale - viewToCenter[i]) * 2;
    // Now adding 'mirror' to a position will mirror it.

    // Calculate texture turn angle.
    if(V3f_Normalize(haloPos))
    {
        // Now halopos is a normalized version of the mirror vector.
        // Both vectors are on the view plane.
        if(viewRelativeRotate)
        {
            turnAngle = V3f_DotProduct(haloPos, viewData->upVec);
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
            if(V3f_DotProduct(haloPos, viewData->sideVec) < 0)
                turnAngle = -turnAngle;
        }
        else
        {
            turnAngle = 0;
        }
    }

    // The overall brightness of the flare.
    colorAverage = (rgba[CR] + rgba[CG] + rgba[CB] + 1) / 4;

    // Small flares have stronger dimming.
    f = distanceToViewer / size;
    if(haloDimStart && haloDimStart < haloDimEnd && f > haloDimStart)
        distanceDim = 1 - (f - haloDimStart) / (haloDimEnd - haloDimStart);
    else
        distanceDim = 1;

    // Setup GL state.
    if(primary)
        H_SetupState(true);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Prepare the texture rotation matrix.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    // Rotate around the center of the texture.
    glTranslatef(0.5f, 0.5f, 0);
    glRotatef(turnAngle / PI * 180, 0, 0, 1);
    glTranslatef(-0.5f, -0.5f, 0);

    for(i = 0, fl = flares; i < haloMode && i < NUM_FLARES; ++i, fl++)
    {
        if(primary && i)
            break;
        if(!primary && !i)
            continue;

        // Calculate the dimming factor.
        if(i > 0)
            // Secondary flares receive additional dimming.
            f = MAX_OF(minHaloSize * size / distanceToViewer, 1);
        else
            f = 1;
        f *= distanceDim * brightnessFactor;

        // The rgba & alpha of the flare.
        rgba[CA] = f * (fl->alpha * occlusionFactor * fadeFactor + colorAverage * colorAverage / 5);

        radius = size * (1 - colorAverage / 3) + distanceToViewer / haloZMagDiv;
        if(radius < haloMinRadius)
            radius = haloMinRadius;
        radius *= occlusionFactor;

        secBold = colorAverage - 8 * (1 - secDimFactor);

        rgba[CA] *= .8f * haloBright / 100.0f;
        if(i)
        {
            rgba[CA] *= secBold; // Secondary flare boldness.
        }

        if(rgba[CA] <= 0)
            break; // Not visible.

        // In the realistic mode, halos are slightly dimmer.
        if(haloRealistic)
        {
            rgba[CA] *= .6f;
        }

        if(haloRealistic)
        {
            // The 'realistic' halos just use the blurry round
            // texture unless custom.
            if(!tex)
                tex = GL_PrepareSysFlareTexture(FXT_ROUND);
        }
        else
        {
            if(!(primary && tex))
            {
                if(size > 45 || (colorAverage > .90 && size > 20))
                {
                    // The "Very Bright" condition.
                    radius *= .65f;
                    if(!i)
                        tex = GL_PrepareSysFlareTexture(FXT_BIGFLARE);
                    else
                        tex = GL_PrepareSysFlareTexture(fl->texture);
                }
                else
                {
                    if(!i)
                        tex = GL_PrepareSysFlareTexture(FXT_ROUND);
                    else
                        tex = GL_PrepareSysFlareTexture(fl->texture);
                }
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

        GL_BindTextureUnmanaged(renderTextures? tex : 0, GL_LINEAR);
        glEnable(GL_TEXTURE_2D);

        // Don't wrap the texture. Evidently some drivers can't just
        // take a hint... (or then something's changing the wrapping
        // mode inadvertently)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glColor4fv(rgba);

        glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex3f(haloPos[VX] + radX * leftOff[VX],
                       haloPos[VY] + radY * leftOff[VY],
                       haloPos[VZ] + radX * leftOff[VZ]);
            glTexCoord2f(1, 0);
            glVertex3f(haloPos[VX] + radX * rightOff[VX],
                       haloPos[VY] + radY * rightOff[VY],
                       haloPos[VZ] + radX * rightOff[VZ]);
            glTexCoord2f(1, 1);
            glVertex3f(haloPos[VX] - radX * leftOff[VX],
                       haloPos[VY] - radY * leftOff[VY],
                       haloPos[VZ] - radX * leftOff[VZ]);
            glTexCoord2f(0, 1);
            glVertex3f(haloPos[VX] - radX * rightOff[VX],
                       haloPos[VY] - radY * rightOff[VY],
                       haloPos[VZ] - radX * rightOff[VZ]);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    // Restore previous GL state.
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
