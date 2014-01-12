/** @file rend_halo.cpp Halos and Lens Flares.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "render/rend_halo.h"

#include "render/rend_main.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "con_main.h"
#include "dd_main.h"

#include "world/p_players.h"

#include <cmath>

#define NUM_FLARES          5

using namespace de;

struct flare_t
{
    float offset;
    float size;
    float alpha;
    int   texture; // 0=round, 1=flare, 2=brflare, 3=bigflare
};

D_CMD(FlareConfig);

int     haloMode = 5, haloBright = 45, haloSize = 80;
int     haloRealistic = true;
int     haloOccludeSpeed = 48;
float   haloZMagDiv = 62, haloMinRadius = 20;
float   haloDimStart = 10, haloDimEnd = 100;

float   haloFadeMax = 0, haloFadeMin = 0, minHaloSize = 1;

flare_t flares[NUM_FLARES] = {
    {0, 1, 1, 0},               // Primary flare.
    {1, .41f, .5f, 0},          // Main secondary flare.
    {1.5f, .29f, .333f, 1},
    {-.6f, .24f, .333f, 0},
    {.4f, .29f, .25f, 0}
};

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

TextureVariantSpec const &Rend_HaloTextureSpec()
{
    return App_ResourceSystem().textureSpec(TC_HALO_LUMINANCE,
        TSF_NO_COMPRESSION, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, 1, 0,
        false, false, false, true);
}

void H_SetupState(bool dosetup)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

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

static inline float distanceDimFactorAt(coord_t distToViewer, float size)
{
    if(haloDimStart && haloDimStart < haloDimEnd &&
       distToViewer / size > haloDimStart)
    {
        return 1 - (distToViewer / size - haloDimStart) / (haloDimEnd - haloDimStart);
    }
    return 1;
}

static inline float fadeFactorAt(coord_t distToViewer)
{
    if(haloFadeMax && haloFadeMax != haloFadeMin &&
       distToViewer < haloFadeMax && distToViewer >= haloFadeMin)
    {
        return (distToViewer - haloFadeMin) / (haloFadeMax - haloFadeMin);
    }
    return 1;
}

bool H_RenderHalo(Vector3d const &origin, float size, DGLuint tex,
    Vector3f const &color, coord_t distanceToViewer,
    float occlusionFactor, float brightnessFactor, float viewXOffset,
    bool doPrimary, bool viewRelativeRotate)
{
    // In realistic mode we don't render secondary halos.
    if(!doPrimary && haloRealistic)
        return false;

    if(distanceToViewer <= 0 || occlusionFactor == 0 ||
       (haloFadeMax && distanceToViewer > haloFadeMax))
        return false;

    occlusionFactor = (1 + occlusionFactor) / 2;

    // viewSideVec is to the left.
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    Vector3f const leftOff  = viewData->upVec + viewData->sideVec;
    Vector3f const rightOff = viewData->upVec - viewData->sideVec;

    // Calculate the center of the flare.
    // Apply the flare's X offset. (Positive is to the right.)
    Vector3f const center = Vector3f(origin.x, origin.z, origin.y)
                          - viewData->sideVec * viewXOffset;

    // Calculate the mirrored position.
    // Project viewtocenter vector onto viewSideVec.
    Vector3f const viewToCenter = center - vOrigin;

    // Calculate the 'mirror' vector.
    float const scale = viewToCenter.dot(viewData->frontVec)
                        / viewData->frontVec.dot(viewData->frontVec);
    Vector3f const mirror =
        (viewData->frontVec * scale - viewToCenter) * 2;

    // Calculate dimming factors.
    float const fadeFactor    = fadeFactorAt(distanceToViewer);
    float const secFadeFactor = viewToCenter.normalize().dot(viewData->frontVec);

    // Calculate texture turn angle.
    float turnAngle = 0;
    if(viewRelativeRotate)
    {
        // Normalize the mirror vector so that both are on the view plane.
        Vector3f haloPos = mirror.normalize();
        if(haloPos.length())
        {
            turnAngle = de::clamp<float>(-1, haloPos.dot(viewData->upVec), 1);

            if(turnAngle >= 1)
                turnAngle = 0;
            else if(turnAngle <= -1)
                turnAngle = float(de::PI);
            else
                turnAngle = acos(turnAngle);

            // On which side of the up vector (left or right)?
            if(haloPos.dot(viewData->sideVec) < 0)
                turnAngle = -turnAngle;
        }
    }

    // The overall brightness of the flare (average color).
    float const luminosity = (color.x + color.y + color.z) / 3;

    // Small flares have stronger dimming.
    float const distanceDim = distanceDimFactorAt(distanceToViewer, size);

    // Setup GL state.
    if(doPrimary)
        H_SetupState(true);

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Prepare the texture rotation matrix.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    // Rotate around the center of the texture.
    glTranslatef(0.5f, 0.5f, 0);
    glRotatef(turnAngle / float(de::PI) * 180, 0, 0, 1);
    glTranslatef(-0.5f, -0.5f, 0);

    flare_t *fl = flares;
    for(int i = 0; i < haloMode && i < NUM_FLARES; ++i, fl++)
    {
        bool const secondary = i != 0;

        if(doPrimary && secondary)
            break;
        if(!doPrimary && !secondary)
            continue;

        // Determine visibility.
        float alpha = fl->alpha * occlusionFactor * fadeFactor
                    + luminosity * luminosity / 5;

        // Apply a dimming factor (secondary flares receive stronger dimming).
        alpha *= (!secondary? 1 : de::min<float>(minHaloSize * size / distanceToViewer, 1))
                 * distanceDim * brightnessFactor;

        // Apply the global dimming factor.
        alpha *= .8f * haloBright / 100.0f;

        // Secondary flares are a little bolder.
        if(secondary)
        {
            alpha *= luminosity - 8 * (1 - secFadeFactor);
        }

        // In the realistic mode, halos are slightly dimmer.
        if(haloRealistic)
        {
            alpha *= .6f;
        }

        // Not visible?
        if(alpha <= 0)
            break;

        // Determine radius.
        float radius = size * (1 - luminosity / 3)
                     + distanceToViewer / haloZMagDiv;

        if(radius < haloMinRadius)
            radius = haloMinRadius;

        radius *= occlusionFactor;

        // In the realistic mode, halos are slightly smaller.
        if(haloRealistic)
        {
            radius *= 0.8f;
        }

        if(haloRealistic)
        {
            // The 'realistic' halos just use the blurry round
            // texture unless custom.
            if(!tex)
                tex = GL_PrepareSysFlaremap(FXT_ROUND);
        }
        else
        {
            if(!(doPrimary && tex))
            {
                if(size > 45 || (luminosity > .90 && size > 20))
                {
                    // The "Very Bright" condition.
                    radius *= .65f;
                    if(!secondary)
                        tex = GL_PrepareSysFlaremap(FXT_BIGFLARE);
                    else
                        tex = GL_PrepareSysFlaremap(flaretexid_t(fl->texture));
                }
                else
                {
                    if(!secondary)
                        tex = GL_PrepareSysFlaremap(FXT_ROUND);
                    else
                        tex = GL_PrepareSysFlaremap(flaretexid_t(fl->texture));
                }
            }
        }

        // Determine the final position of the flare.
        Vector3f pos = center;

        // Secondary halos are mirrored according to the flare table.
        if(secondary)
        {
            pos += mirror * fl->offset;
        }

        GL_BindTextureUnmanaged(renderTextures? tex : 0, gl::ClampToEdge, gl::ClampToEdge);
        glEnable(GL_TEXTURE_2D);

        float const radX = radius * fl->size;
        float const radY = radX / 1.2f; // Aspect correction.

        glColor4f(color.x, color.y, color.z, alpha);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex3f(pos.x + radX * leftOff.x,
                       pos.y + radY * leftOff.y,
                       pos.z + radX * leftOff.z);
            glTexCoord2f(1, 0);
            glVertex3f(pos.x + radX * rightOff.x,
                       pos.y + radY * rightOff.y,
                       pos.z + radX * rightOff.z);
            glTexCoord2f(1, 1);
            glVertex3f(pos.x - radX * leftOff.x,
                       pos.y - radY * leftOff.y,
                       pos.z - radX * leftOff.z);
            glTexCoord2f(0, 1);
            glVertex3f(pos.x - radX * rightOff.x,
                       pos.y - radY * rightOff.y,
                       pos.z - radX * rightOff.z);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    // Restore previous GL state.
    if(doPrimary)
        H_SetupState(false);

    return true;
}

/**
 * flareconfig list
 * flareconfig (num) pos/size/alpha/tex (val)
 */
D_CMD(FlareConfig)
{
    DENG2_UNUSED(src);

    int             i;
    float           val;

    if(argc == 2)
    {
        if(!stricmp(argv[1], "list"))
        {
            for(i = 0; i < NUM_FLARES; ++i)
            {
                LOG_MSG("%i: pos:%f s:%.2f a:%.2f tex:%i")
                        << i << flares[i].offset << flares[i].size << flares[i].alpha
                        << flares[i].texture;
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
        LOG_SCR_NOTE("Usage:\n"
                     "  %s list\n"
                     "  %s (num) pos/size/alpha/tex (val)") << argv[0] << argv[0];
    }

    return true;
}
