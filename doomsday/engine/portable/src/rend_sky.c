/**\file rend_sky.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Sky Sphere and 3D Models.
 *
 * This version supports only two sky layers.
 * (More would be a waste of resources?)
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "texture.h"
#include "materialvariant.h"

// MACROS ------------------------------------------------------------------

#define SKYVTX_IDX(c, r)    ( (r)*skyColumns + (c)%skyColumns )

// Sky hemispheres.
#define SKYHEMI_UPPER       0x1
#define SKYHEMI_LOWER       0x2
#define SKYHEMI_JUST_CAP    0x4 // Just draw the top or bottom cap.
#define SKYHEMI_FADEOUT_BG  0x8 // Draw the fadeout bg when drawing the cap.

// TYPES -------------------------------------------------------------------

typedef struct skyvertex_s {
    float pos[3];
} skyvertex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float vx, vy, vz;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

skyvertex_t* skyVerts = 0; // Vertices for the upper hemisphere.
int numSkyVerts = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// The texture offset to be applied to the texture coordinates in SkyVertex().
static float maxSideAngle = (float) PI / 3;
static float horizonOffset = 0;
static float skyTexOffset;
static int skyTexWidth, skyTexHeight;
static boolean yflip;
static const fadeout_t* currentFO;

// CODE --------------------------------------------------------------------

void Rend_RenderSkyModels(void)
{
    int                 i, c;
    float               inter;
    skymodel_t         *sky;
    rendmodelparams_t   params;
    float               pos[3];

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Setup basic translation.
    glTranslatef(vx, vy, vz);

    for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; ++i, sky++)
    {
        if(!sky->def)
            continue;

        if(!R_SkyLayerIsEnabled(sky->def->layer))
        {
            // The model has been assigned to a layer, but the layer is
            // not visible.
            continue;
        }

        // Calculate the coordinates for the model.
        pos[0] = vx * -sky->def->coordFactor[0];
        pos[1] = vy * -sky->def->coordFactor[1];
        pos[2] = vz * -sky->def->coordFactor[2];

        inter = (sky->maxTimer > 0 ? sky->timer / (float) sky->maxTimer : 0);

        memset(&params, 0, sizeof(params));

        params.distance = 1;
        params.center[VX] = pos[0];
        params.center[VY] = pos[2];
        params.center[VZ] = params.gzt = pos[1];
        params.extraYawAngle = params.yawAngleOffset = sky->def->rotate[0];
        params.extraPitchAngle = params.pitchAngleOffset = sky->def->rotate[1];
        params.inter = inter;
        params.mf = sky->model;
        params.alwaysInterpolate = true;
        R_SetModelFrame(sky->model, sky->frame);
        params.yaw = sky->yaw;
        for(c = 0; c < 4; ++c)
        {
            params.ambientColor[c] = sky->def->color[c];
        }
        params.vLightListIdx = 0;
        params.shineTranslateWithViewerPos = true;

        Rend_RenderModel(&params);
    }

    // We don't want that anything interferes with what was drawn.
    //glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Calculate the vertex and texture coordinates.
 */
static void SkyVertex(int r, int c)
{
    // The direction must be clockwise.
    skyvertex_t* svtx = skyVerts + SKYVTX_IDX(c, r);

    // And the texture coordinates.
    if(!yflip) // Flipped Y is for the lower hemisphere.
    {
        glTexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns +
                     skyTexOffset / skyTexWidth, r / (float) skyRows);
    }
    else
    {
        glTexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns +
                     skyTexOffset / skyTexWidth, (skyRows - r) / (float) skyRows);
    }

    // Also the color.
    if(currentFO->use)
    {
        if(r == 0)
            glColor4f(1, 1, 1, 0);
        else
            glColor3f(1, 1, 1);
    }
    else
    {
        if(r == 0)
            glColor3f(0, 0, 0);
        else
            glColor3f(1, 1, 1);
    }

    // And finally the vertex itself.
    glVertex3f(svtx->pos[VX], svtx->pos[VY] * (yflip ? -1 : 1), svtx->pos[VZ]);
}

static void CapSideVertex(int r, int c)
{   // Look up the precalculated vertex.
    skyvertex_t *svtx = skyVerts + SKYVTX_IDX(c, r);

    glVertex3f(svtx->pos[VX], svtx->pos[VY] * (yflip ? -1 : 1), svtx->pos[VZ]);
}

/**
 * @param hemi              Upper or Lower. Zero is not acceptable.
 *                          SKYHEMI_JUST_CAP can be used.
 */
void Rend_SkyRenderer(int hemi, const rendskysphereparams_t* params)
{
    int r, c;

    if(hemi & SKYHEMI_LOWER)
        yflip = true;
    else
        yflip = false;

    // The top row (row 0) is the one that's faded out.
    // There must be at least 4 columns. The preferable number is 4n, where
    // n is 1, 2, 3... There should be at least two rows because the first
    // one is always faded.
    if(hemi & SKYHEMI_JUST_CAP)
    {
        // Use the appropriate color.
        if(currentFO->use)
            glColor3fv(currentFO->rgb);
        else
            glColor3f(0, 0, 0);

        // Draw the cap.
        glBegin(GL_TRIANGLE_FAN);
        for(c = 0; c < skyColumns; ++c)
            CapSideVertex(0, c);
        glEnd();

        // If we are doing a colored fadeout...
        if(hemi & SKYHEMI_FADEOUT_BG)
        {
            // We must fill the background for the top row since it'll
            // be partially translucent.
            glBegin(GL_TRIANGLE_STRIP);
            CapSideVertex(0, 0);
            for(c = 0; c < skyColumns; ++c)
            {
                CapSideVertex(1, c); // One step down.
                CapSideVertex(0, c + 1); // And one step right.
            }
            CapSideVertex(1, c);
            glEnd();
        }
        return;
    }

    assert(params != 0);

    if(renderTextures != 0)
    {
        int magMode;
        DGLuint tex;
        
        skyTexOffset = params->offset;

        if(renderTextures == 1 && params->tex)
        {
            tex = params->tex;
            magMode = params->texMagMode;
            skyTexWidth  = params->texWidth;
            skyTexHeight = params->texHeight;
        }
        else
        {
            const materialvariantspecification_t* spec;
            const materialsnapshot_t* ms;
            material_t* mat;
            
            if(renderTextures == 2)
                mat = Materials_MaterialForUriCString(MN_SYSTEM_NAME":gray");
            else
                mat = Materials_MaterialForUriCString(MN_SYSTEM_NAME":missing");

            spec = Materials_VariantSpecificationForContext(MC_SKYSPHERE,
                TSF_NO_COMPRESSION | TSF_ZEROMASK, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                1, 1, 0, false, true, false, false);
            ms = Materials_Prepare(mat, spec, true);

            tex     = MSU(ms, MTU_PRIMARY).tex.glName;
            Texture_Dimensions(MSU(ms, MTU_PRIMARY).tex.texture, &skyTexWidth, &skyTexHeight);
            magMode = MSU(ms, MTU_PRIMARY).magMode;
        }

        GL_BindTexture(tex, magMode);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // The total number of triangles per hemisphere can be calculated
    // as follows: rows * columns * 2 + 2 (for the top cap).
    glEnable(GL_TEXTURE_2D);
    for(r = 0; r < skyRows; ++r)
    {
        if(skySimple)
        {
            glBegin(GL_QUADS);
            for(c = 0; c < skyColumns; ++c)
            {
                SkyVertex(r, c);
                SkyVertex(r + 1, c);
                SkyVertex(r + 1, c + 1);
                SkyVertex(r, c + 1);
            }
            glEnd();
        }
        else
        {
            glBegin(GL_TRIANGLE_STRIP);
            SkyVertex(r, 0);
            SkyVertex(r + 1, 0);
            for(c = 1; c <= skyColumns; ++c)
            {
                SkyVertex(r, c);
                SkyVertex(r + 1, c);
            }
            glEnd();
        }
    }
    glDisable(GL_TEXTURE_2D);
}

void Rend_RenderSkyHemisphere(int whichHemi)
{
    // The current fadeout is the first layer's fadeout.
    currentFO = R_SkyFadeout();

    // First render the cap and the background for fadeouts, if needed.
    // The color for both is the current fadeout color.
    Rend_SkyRenderer(whichHemi | SKYHEMI_JUST_CAP | (currentFO->use ? SKYHEMI_FADEOUT_BG : 0), 0);

    { int i;
    for(i = firstSkyLayer; i < MAXSKYLAYERS; ++i)
    {
        if(R_SkyLayerIsEnabled(i+1))
        {
            rendskysphereparams_t params;
            R_SetupSkySphereParamsForSkyLayer(&params, i+1);
            Rend_SkyRenderer(whichHemi, &params);
        }
    }}

    /// \kludge dj: addresses bug #2982101 - http://sourceforge.net/tracker/?func=detail&aid=2982101&group_id=74815&atid=542099
    glBindTexture(GL_TEXTURE_2D, 0);
    glColor4f(1, 1, 1, 1);
    /// < kludge end
}

void Rend_RenderSky(void)
{
    // IS there a sky to be rendered?
    if(firstSkyLayer == -1)
        return;

    // If sky models have been inited, they will be used.
    if(!skyModelsInited || alwaysDrawSphere)
    {
        // Always render the full sky.
        int hemis = SKYHEMI_UPPER | SKYHEMI_LOWER;

        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        //glDepthMask(GL_FALSE);
        // Disable culling, all triangles face the viewer.
        glDisable(GL_CULL_FACE);
        GL_DisableArrays(true, true, DDMAXINT);

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vx, vy, vz);
        glScalef(skyDist, skyDist, skyDist);

        // Draw the possibly visible hemispheres.
        if(hemis & SKYHEMI_LOWER)
            Rend_RenderSkyHemisphere(SKYHEMI_LOWER);
        if(hemis & SKYHEMI_UPPER)
            Rend_RenderSkyHemisphere(SKYHEMI_UPPER);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Enable the disabled things.
        glEnable(GL_CULL_FACE);
        //glDepthMask(GL_TRUE);
        //glEnable(GL_DEPTH_TEST);
    }

    // How about some 3D models?
    if(skyModelsInited)
    {
        Rend_RenderSkyModels();
    }
}

void Rend_DestroySkySphere(void)
{
    if(skyVerts)
        M_Free(skyVerts);
    skyVerts = 0;
    numSkyVerts = 0;
}

void Rend_CreateSkySphere(int quarterDivs, int rows)
{
    float topAngle, sideAngle, realRadius, scale = 1 /*32 */ ;
    skyvertex_t* svtx;
    int c, r;

    if(quarterDivs < 1)
        quarterDivs = 1;
    if(rows < 1)
        rows = 1;

    skyDetail = quarterDivs;
    skyColumns = 4 * skyDetail;
    skyRows = rows;

    // Calculate the sky vertices.
    numSkyVerts = skyColumns * (skyRows + 1);

    // Allocate memory for it.
    skyVerts = M_Realloc(skyVerts, sizeof(*skyVerts) * numSkyVerts);

    // Calculate the vertices.
    for(r = 0; r <= skyRows; ++r)
        for(c = 0; c < skyColumns; ++c)
        {
            svtx = skyVerts + SKYVTX_IDX(c, r);
            topAngle = ((c / (float) skyColumns) *2) * PI;
            sideAngle =
                horizonOffset + maxSideAngle * (skyRows - r) / (float) skyRows;
            realRadius = scale * cos(sideAngle);
            svtx->pos[VX] = realRadius * cos(topAngle);
            svtx->pos[VY] = scale * sin(sideAngle);   // The height.
            svtx->pos[VZ] = realRadius * sin(topAngle);
        }
}

static void updateLayerStats(void)
{
    // -1 denotes 'no active layers'.
    firstSkyLayer = -1;
    activeSkyLayers = 0;

    { int i;
    for(i = 1; i <= MAXSKYLAYERS; ++i)
    {
        if(!R_SkyLayerIsEnabled(i))
            continue;
        ++activeSkyLayers;
        if(firstSkyLayer == -1)
            firstSkyLayer = i-1;
    }}
}

static void internalSkyParams(int layer, int param, void* data)
{
    switch(param)
    {
    case DD_ENABLE:
        R_SkyLayerEnable(layer, true);
        updateLayerStats();
        break;

    case DD_DISABLE:
        R_SkyLayerEnable(layer, false);
        updateLayerStats();
        break;

    case DD_MASK:
        R_SkyLayerMasked(layer, *((int*)data) == DD_YES);
        break;

    case DD_MATERIAL: {
        material_t* mat = Materials_ToMaterial(*((materialid_t*) data));
        R_SkyLayerSetMaterial(layer, mat);
        break;
      }
    case DD_OFFSET:
        R_SkyLayerSetOffset(layer, *((float*) data));
        break;

    case DD_COLOR_LIMIT:
        R_SkyLayerSetFadeoutLimit(layer, *((float*) data));
        break;

    default:
        Con_Error("R_SkyParams: Bad parameter (%d).\n", param);
        break;
    }
}

void Rend_SkyParams(int layer, int param, void* data)
{
    if(isDedicated)
        return;

    if(layer == DD_SKY) // The whole sky?
    {
        switch(param)
        {
        case DD_COLUMNS:
            Rend_CreateSkySphere(*((int*) data), skyRows);
            break;

        case DD_ROWS:
            Rend_CreateSkySphere(skyDetail, *((int*) data));
            break;

        case DD_HEIGHT:
            maxSideAngle = PI / 2 * *((float*) data);
            // Recalculate the vertices.
            Rend_CreateSkySphere(skyDetail, skyRows);
            break;

        case DD_HORIZON:        // horizon offset angle
            horizonOffset = PI / 2 * *((float*) data);
            // Recalculate the vertices.
            Rend_CreateSkySphere(skyDetail, skyRows);
            break;

        default: // Operate on all layers.
            { int i;
            for(i = 1; i <= MAXSKYLAYERS; ++i)
                internalSkyParams(i, param, data);
            }
        }
    }
    // This is for a specific layer.
    else if(layer >= 0 && layer < MAXSKYLAYERS)
        internalSkyParams(layer+1, param, data);
}
