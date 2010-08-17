/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_sky.c: Sky Sphere and 3D Models
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

// MACROS ------------------------------------------------------------------

#define MAXSKYLAYERS        2

#define SKYVTX_IDX(c, r)    ( (r)*skyColumns + (c)%skyColumns )

// Sky hemispheres.
#define SKYHEMI_UPPER       0x1
#define SKYHEMI_LOWER       0x2
#define SKYHEMI_JUST_CAP    0x4 // Just draw the top or bottom cap.
#define SKYHEMI_FADEOUT_BG  0x8 // Draw the fadeout bg when drawing the cap.

// TYPES -------------------------------------------------------------------

typedef struct skyvertex_s {
    float           pos[3];
} skyvertex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(SkyDetail);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float vx, vy, vz;
extern byte topLineRGB[3];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

skylayer_t skyLayers[MAXSKYLAYERS];
int firstLayer, activeLayers;

skyvertex_t *skyVerts = NULL; // Vertices for the upper hemisphere.
int numSkyVerts = 0;

int skyDetail = 6, simpleSky;
int skyColumns, skyRows = 3;
float skyDist = 1600;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// The texture offset to be applied to the texture coordinates in SkyVertex().
static float maxSideAngle = (float) PI / 3;
static float horizonOffset = 0;
static float skyTexOff;
static int skyTexWidth, skyTexHeight;
static boolean yflip;
static fadeout_t *currentFO;

// CODE --------------------------------------------------------------------

void Rend_SkyRegister(void)
{
    // Cvars
    C_VAR_INT("rend-sky-detail", &skyDetail, CVF_PROTECTED, 3, 7);
    C_VAR_INT("rend-sky-rows", &skyRows, CVF_PROTECTED, 1, 8);
    C_VAR_FLOAT("rend-sky-distance", &skyDist, CVF_NO_MAX, 1, 0);
    C_VAR_INT("rend-sky-simple", &simpleSky, 0, 0, 2);

    // Ccmds
    C_CMD_FLAGS("skydetail", "i", SkyDetail, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("skyrows", "i", SkyDetail, CMDF_NO_DEDICATED);
}

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

        if(sky->def->layer > 0 && sky->def->layer <= MAXSKYLAYERS &&
           !(skyLayers[sky->def->layer - 1].flags & SLF_ENABLED))
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
    skyvertex_t *svtx = skyVerts + SKYVTX_IDX(c, r);

    // And the texture coordinates.
    if(!yflip)                  // Flipped Y is for the lower hemisphere.
    {
        glTexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns +
                     skyTexOff / skyTexWidth, r / (float) skyRows);
    }
    else
    {
        glTexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns +
                     skyTexOff / skyTexWidth, (skyRows - r) / (float) skyRows);
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
 * \note                    The current texture is used.
* @param hemi               Upper or Lower. Zero is not acceptable.
 *                          SKYHEMI_JUST_CAP can be used.
 */
void Rend_SkyRenderer(int hemi)
{
    int                 r, c;

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
        glDisable(GL_TEXTURE_2D);
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

        glEnable(GL_TEXTURE_2D);
        return;
    }

    // The total number of triangles per hemisphere can be calculated
    // as follows: rows * columns * 2 + 2 (for the top cap).
    for(r = 0; r < skyRows; ++r)
    {
        if(simpleSky)
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
}

static void setupFadeout(skylayer_t* slayer)
{
    if(slayer->mat)
    {
        int             i;
        material_load_params_t params;
        material_snapshot_t ms;

        // Ensure we have up to date info on the material.
        memset(&params, 0, sizeof(params));
        params.flags = MLF_LOAD_AS_SKY;
        params.tex.flags = GLTF_NO_COMPRESSION;
        if(slayer->flags & SLF_MASKED)
            params.tex.flags |= GLTF_ZEROMASK;

        Materials_Prepare(&ms, slayer->mat, true, &params);
        slayer->fadeout.rgb[CR] = ms.topColor[CR];
        slayer->fadeout.rgb[CG] = ms.topColor[CG];
        slayer->fadeout.rgb[CB] = ms.topColor[CB];

        // Determine if it should be used.
        for(slayer->fadeout.use = false, i = 0; i < 3; ++i)
            if(slayer->fadeout.rgb[i] > slayer->fadeout.limit)
            {
                // Colored fadeout is needed.
                slayer->fadeout.use = true;
                break;
            }

        return;
    }

    // An invalid texture, default to black.
    slayer->fadeout.rgb[CR] = slayer->fadeout.rgb[CG] =
        slayer->fadeout.rgb[CB] = 0;
    slayer->fadeout.use = true;
}

void Rend_RenderSkyHemisphere(int whichHemi)
{
    skylayer_t* slayer;
    int i;

    // The current fadeout is the first layer's fadeout.
    currentFO = &skyLayers[firstLayer].fadeout;

    // First render the cap and the background for fadeouts, if needed.
    // The color for both is the current fadeout color.
    Rend_SkyRenderer(whichHemi | SKYHEMI_JUST_CAP | (currentFO->use ? SKYHEMI_FADEOUT_BG : 0));

    for(i = firstLayer, slayer = &skyLayers[firstLayer]; i < MAXSKYLAYERS; ++i, slayer++)
    {
        if(slayer->flags & SLF_ENABLED)
        {
            material_t* mat = (renderTextures? (renderTextures == 2? Materials_ToMaterial(Materials_NumForName("gray", MN_SYSTEM)) : (slayer->mat? slayer->mat : Materials_ToMaterial(Materials_NumForName("missing", MN_SYSTEM)))) : 0);
            byte result = 0;
 
            // The texture is actually loaded when an update is done.
            if(mat)
            {
                material_load_params_t params;
                material_snapshot_t ms;

                memset(&params, 0, sizeof(params));
                params.flags = MLF_LOAD_AS_SKY;
                params.tex.flags = GLTF_NO_COMPRESSION;
                if(slayer->flags & SLF_MASKED)
                    params.tex.flags |= GLTF_ZEROMASK;

                result = Materials_Prepare(&ms, mat, true, &params);
                skyTexWidth = GLTexture_GetWidth(ms.units[MTU_PRIMARY].texInst->tex);
                skyTexHeight = GLTexture_GetHeight(ms.units[MTU_PRIMARY].texInst->tex);

                if(result)
                {   // Texture was reloaded.
                    setupFadeout(slayer);
                }

                GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id, ms.units[MTU_PRIMARY].magMode);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, 0);
                skyTexWidth = skyTexHeight = 64;
            }

            skyTexOff = slayer->offset;

            Rend_SkyRenderer(whichHemi);
        }
    }
}

void Rend_RenderSky(void)
{
    // IS there a sky to be rendered?
    if(firstLayer == -1)
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

/**
 * Calculate sky vertices.
 */
void Rend_InitSky(void)
{
    int                 i;

    firstLayer = 0;

    Rend_SkyDetail(skyDetail, skyRows);

    // Initialize the layers.
    for(i = 0; i < MAXSKYLAYERS; ++i)
    {
        skylayer_t*         slayer = &skyLayers[i];

        slayer->mat = NULL; // No material.
        slayer->fadeout.limit = .3f;
    }
}

void Rend_ShutdownSky(void)
{
    M_Free(skyVerts);
    skyVerts = NULL;
    numSkyVerts = 0;
}

void Rend_SkyDetail(int quarterDivs, int rows)
{
    float               topAngle, sideAngle, realRadius, scale = 1 /*32 */ ;
    int                 c, r;
    skyvertex_t        *svtx;

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
    int                 i = 0;

    // -1 denotes 'no active layers'.
    firstLayer = -1;
    activeLayers = 0;
    for(i = 0; i < MAXSKYLAYERS; ++i)
    {
        if(skyLayers[i].flags & SLF_ENABLED)
        {
            activeLayers++;
            if(firstLayer == -1)
                firstLayer = i;
        }
    }
}

static void internalSkyParams(skylayer_t* slayer, int param, void* data)
{
    switch(param)
    {
    case DD_ENABLE:
        slayer->flags |= SLF_ENABLED;
        updateLayerStats();
        break;

    case DD_DISABLE:
        slayer->flags &= ~SLF_ENABLED;
        updateLayerStats();
        break;

    case DD_MASK:
        {
        boolean             deleteTextures = false;
        if(*((int*)data) == DD_YES)
        {
            // Invalidate the loaded texture, if necessary.
            if(slayer->mat && !(slayer->flags & SLF_MASKED))
                deleteTextures = true;
            slayer->flags |= SLF_MASKED;
        }
        else
        {
            // Invalidate the loaded texture, if necessary.
            if(slayer->mat && (slayer->flags & SLF_MASKED))
                deleteTextures = true;
            slayer->flags &= ~SLF_MASKED;
        }

        if(deleteTextures)
            Material_DeleteTextures(slayer->mat);
        }
        break;

    case DD_MATERIAL:
        if((slayer->mat = Materials_ToMaterial(*(materialnum_t*) data)))
        {
            material_load_params_t params;

            memset(&params, 0, sizeof(params));
            params.flags = MLF_LOAD_AS_SKY;
            params.tex.flags = GLTF_NO_COMPRESSION;
            if(slayer->flags & SLF_MASKED)
                params.tex.flags |= GLTF_ZEROMASK;

            Materials_Prepare(NULL, slayer->mat, true, &params);
        }

        setupFadeout(slayer);
        break;

    case DD_OFFSET:
        slayer->offset = *((float*) data);
        break;

    case DD_COLOR_LIMIT:
        slayer->fadeout.limit = *((float*) data);
        setupFadeout(slayer);
        break;

    default:
        Con_Error("R_SkyParams: Bad parameter (%d).\n", param);
        break;
    }
}

void Rend_SkyParams(int layer, int param, void* data)
{
    int                 i;

    if(isDedicated)
        return;

    if(layer == DD_SKY) // The whole sky?
    {
        switch(param)
        {
        case DD_COLUMNS:
            Rend_SkyDetail(*((int*) data), skyRows);
            break;

        case DD_ROWS:
            Rend_SkyDetail(skyDetail, *((int*) data));
            break;

        case DD_HEIGHT:
            maxSideAngle = PI / 2 * *((float*) data);
            // Recalculate the vertices.
            Rend_SkyDetail(skyDetail, skyRows);
            break;

        case DD_HORIZON:        // horizon offset angle
            horizonOffset = PI / 2 * *((float*) data);
            // Recalculate the vertices.
            Rend_SkyDetail(skyDetail, skyRows);
            break;

        default:
            // Operate on all layers.
            for(i = 0; i < MAXSKYLAYERS; ++i)
                internalSkyParams(&skyLayers[i], param, data);
        }
    }
    // This is for a specific layer.
    else if(layer >= 0 && layer < MAXSKYLAYERS)
        internalSkyParams(&skyLayers[layer], param, data);
}

D_CMD(SkyDetail)
{
    if(!stricmp(argv[0], "skydetail"))
    {
        Rend_SkyDetail(strtol(argv[1], NULL, 0), skyRows);
    }
    else if(!stricmp(argv[0], "skyrows"))
    {
        Rend_SkyDetail(skyDetail, strtol(argv[1], NULL, 0));
    }
    return true;
}
