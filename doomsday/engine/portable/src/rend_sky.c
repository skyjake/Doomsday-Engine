/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

// MACROS ------------------------------------------------------------------

#define MAXSKYLAYERS        2

#define SKYVTX_IDX(c, r)    ( (r)*skyColumns + (c)%skyColumns )

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
int     firstLayer, activeLayers;

skyvertex_t *skyVerts = NULL;   // Vertices for the upper hemisphere.
int     numSkyVerts = 0;

int     skyDetail = 6, simpleSky;
int     skyColumns, skyRows = 3;
int     skyhemispheres;
float   skyDist = 1600;
int     r_fullsky = false;

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
    C_VAR_INT("rend-sky-full", &r_fullsky, 0, 0, 1);
    C_VAR_INT("rend-sky-simple", &simpleSky, 0, 0, 2);

    // Ccmds
    C_CMD_FLAGS("skydetail", "i", SkyDetail, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("skyrows", "i", SkyDetail, CMDF_NO_DEDICATED);
}

void Rend_RenderSkyModels(void)
{
    int     i, k;
    float   inter;
    skymodel_t *sky;
    modelparams_t params;
    float   pos[3];

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    // Setup basic translation.
    gl.Translatef(vx, vy, vz);

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
        pos[0] = vx * -sky->def->coord_factor[0];
        pos[1] = vy * -sky->def->coord_factor[1];
        pos[2] = vz * -sky->def->coord_factor[2];

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
        params.lightLevel = -1;    // Fullbright.
        for(k = 0; k < 3; ++k)
        {
            params.rgb[k] = sky->def->color[k];
        }
        params.uniformColor = true;
        params.alpha = sky->def->color[3];
        params.shineTranslateWithViewerPos = true;

        Rend_RenderModel(&params);
    }

    // We don't want that anything interferes with what was drawn.
    gl.Clear(DGL_DEPTH_BUFFER_BIT);

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

/*
 * Calculate the vertex and texture coordinates.
 */
static void SkyVertex(int r, int c)
{
    // The direction must be clockwise.
    skyvertex_t *svtx = skyVerts + SKYVTX_IDX(c, r);

    // And the texture coordinates.
    if(!yflip)                  // Flipped Y is for the lower hemisphere.
    {
        gl.TexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns +
                      skyTexOff / skyTexWidth, r / (float) skyRows);
    }
    else
    {
        gl.TexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns +
                      skyTexOff / skyTexWidth, (skyRows - r) / (float) skyRows);
    }
    // Also the color.
    if(currentFO->use)
    {
        if(r == 0)
            gl.Color4f(1, 1, 1, 0);
        else
            gl.Color3f(1, 1, 1);
    }
    else
    {
        if(r == 0)
            gl.Color3f(0, 0, 0);
        else
            gl.Color3f(1, 1, 1);
    }

    // And finally the vertex itself.
    gl.Vertex3f(svtx->pos[VX], svtx->pos[VY] * (yflip ? -1 : 1), svtx->pos[VZ]);
}

static void CapSideVertex(int r, int c)
{   // Look up the precalculated vertex.
    skyvertex_t *svtx = skyVerts + SKYVTX_IDX(c, r);

    gl.Vertex3f(svtx->pos[VX], svtx->pos[VY] * (yflip ? -1 : 1), svtx->pos[VZ]);
}

/**
 * Hemi is Upper or Lower. Zero is not acceptable.
 * The current texture is used. SKYHEMI_NO_TOPCAP can be used.
 */
void Rend_SkyRenderer(int hemi)
{
    int     r, c;

    if(hemi & SKYHEMI_LOWER)
        yflip = true;
    else
        yflip = false;

    // The top row (row 0) is the one that's faded out.
    // There must be at least 4 columns. The preferable number
    // is 4n, where n is 1, 2, 3... There should be at least
    // two rows because the first one is always faded.
    if(hemi & SKYHEMI_JUST_CAP)
    {
        gl.Disable(DGL_TEXTURING);
        // Use the appropriate color.
        if(currentFO->use)
            gl.Color3fv(currentFO->rgb);
        else
            gl.Color3f(0, 0, 0);

        // Draw the cap.
        gl.Begin(DGL_TRIANGLE_FAN);
        for(c = 0; c < skyColumns; ++c)
            CapSideVertex(0, c);
        gl.End();

        // If we are doing a colored fadeout...
        if(hemi & SKYHEMI_FADEOUT_BG)
        {
            // We must fill the background for the top row since it'll
            // be partially translucent.
            gl.Begin(DGL_TRIANGLE_STRIP);
            CapSideVertex(0, 0);
            for(c = 0; c < skyColumns; ++c)
            {
                CapSideVertex(1, c);    // One step down.
                CapSideVertex(0, c + 1);    // And one step right.
            }
            CapSideVertex(1, c);
            gl.End();
        }
        gl.Enable(DGL_TEXTURING);
        return;
    }

    // The total number of triangles per hemisphere can be calculated
    // as follows: rows * columns * 2 + 2 (for the top cap).
    for(r = 0; r < skyRows; ++r)
    {
        if(simpleSky)
        {
            gl.Begin(DGL_QUADS);
            for(c = 0; c < skyColumns; ++c)
            {
                SkyVertex(r, c);
                SkyVertex(r + 1, c);
                SkyVertex(r + 1, c + 1);
                SkyVertex(r, c + 1);
            }
            gl.End();
        }
        else
        {
            gl.Begin(DGL_TRIANGLE_STRIP);
            SkyVertex(r, 0);
            SkyVertex(r + 1, 0);
            for(c = 1; c <= skyColumns; ++c)
            {
                SkyVertex(r, c);
                SkyVertex(r + 1, c);
            }
            gl.End();
        }
    }
}

static void SetupFadeout(skylayer_t *slayer)
{
    int     i;

    GL_GetSkyTopColor(slayer->texture, slayer->fadeout.rgb);

    // Determine if it should be used.
    for(slayer->fadeout.use = false, i = 0; i < 3; ++i)
        if(slayer->fadeout.rgb[i] > slayer->fadeout.limit)
        {
            // Colored fadeout is needed.
            slayer->fadeout.use = true;
            break;
        }
}

void Rend_RenderSkyHemisphere(int whichHemi)
{
    int     i, resetup;
    skylayer_t *slayer;

    // The current fadeout is the first layer's fadeout.
    currentFO = &skyLayers[firstLayer].fadeout;

    // First render the cap and the background for fadeouts, if needed.
    // The color for both is the current fadeout color.
    Rend_SkyRenderer(whichHemi | SKYHEMI_JUST_CAP |
                     (currentFO->use ? SKYHEMI_FADEOUT_BG : 0));

    for(i = firstLayer, slayer = skyLayers + firstLayer; i < MAXSKYLAYERS;
        ++i, slayer++)
    {
        if(slayer->flags & SLF_ENABLED)
        {
            texinfo_t  *texinfo;
            resetup = false;

            if(slayer->texture == -1)
                Con_Error("Rend_RenderSkyHemisphere: Sky layer "
                          "without a texture!\n");

            // See if we have to re-setup the fadeout.
            // This happens if the texture is for some reason deleted
            // (forced texture reload, for example).
            if(!GL_GetTextureName(slayer->texture))
                resetup = true;

            // The texture is actually loaded when an update is done.
            gl.Bind(renderTextures ?
                    GL_PrepareSky(slayer->texture,
                                  slayer->flags & SLF_MASKED ? true : false, &texinfo) : 0);

            if(resetup)
                SetupFadeout(slayer);

            if(renderTextures)
            {
                skyTexWidth = texinfo->width;
                skyTexHeight = texinfo->height;
            }
            else
                skyTexWidth = skyTexHeight = 64;

            skyTexOff = slayer->offset;
            Rend_SkyRenderer(whichHemi);
        }
    }
}

void Rend_RenderSky(int hemis)
{
    // IS there a sky to be rendered?
    if(!hemis || firstLayer == -1)
        return;

    // If sky models have been inited, they will be used.
    if(!skyModelsInited || alwaysDrawSphere)
    {
        // Always render the full sky?
        if(r_fullsky)
            hemis = SKYHEMI_UPPER | SKYHEMI_LOWER;

        // We don't want anything written in the depth buffer, not yet.
        gl.Disable(DGL_DEPTH_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        // Disable culling, all triangles face the viewer.
        gl.Disable(DGL_CULL_FACE);
        gl.DisableArrays(true, true, DGL_ALL_BITS);

        // Setup a proper matrix.
        gl.MatrixMode(DGL_MODELVIEW);
        gl.PushMatrix();
        gl.Translatef(vx, vy, vz);
        gl.Scalef(skyDist, skyDist, skyDist);

        // Draw the possibly visible hemispheres.
        if(hemis & SKYHEMI_LOWER)
            Rend_RenderSkyHemisphere(SKYHEMI_LOWER);
        if(hemis & SKYHEMI_UPPER)
            Rend_RenderSkyHemisphere(SKYHEMI_UPPER);

        gl.MatrixMode(DGL_MODELVIEW);
        gl.PopMatrix();

        // Enable the disabled things.
        gl.Enable(DGL_CULL_FACE);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
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
void Rend_InitSky()
{
    int     i;

    firstLayer = 0;
    //maxSideAngle = (float) PI/3;

    Rend_SkyDetail(skyDetail, skyRows);

    // Initialize the layers.
    for(i = 0; i < MAXSKYLAYERS; ++i)
    {
        skyLayers[i].texture = -1;  // No texture.
        skyLayers[i].fadeout.limit = .3f;
    }
}

void Rend_ShutdownSky()
{
    free(skyVerts);
    skyVerts = NULL;
    numSkyVerts = 0;
}

void Rend_SkyDetail(int quarterDivs, int rows)
{
    float       topAngle, sideAngle, realRadius, scale = 1 /*32 */ ;
    int         c, r;
    skyvertex_t *svtx;

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
    skyVerts = realloc(skyVerts, sizeof(*skyVerts) * numSkyVerts);

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

static void updateLayerStats()
{
    int         i = 0;

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

static void internalSkyParams(skylayer_t *slayer, int parm, float value)
{
    switch(parm)
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
        if(value == DD_YES)
        {
            // Invalidate the loaded texture, if necessary.
            if(!(slayer->flags & SLF_MASKED))
                GL_DeleteTexture(slayer->texture);
            slayer->flags |= SLF_MASKED;
        }
        else
        {
            // Invalidate the loaded texture, if necessary.
            if(slayer->flags & SLF_MASKED)
                GL_DeleteTexture(slayer->texture);
            slayer->flags &= ~SLF_MASKED;
        }
        break;

    case DD_TEXTURE:
        slayer->texture = (int) value;
        GL_PrepareSky(slayer->texture,
                      slayer->flags & SLF_MASKED ? true : false, NULL);
        SetupFadeout(slayer);
        break;

    case DD_OFFSET:
        slayer->offset = value;
        break;

    case DD_COLOR_LIMIT:
        slayer->fadeout.limit = value;
        SetupFadeout(slayer);
        break;

    default:
        Con_Error("R_SkyParams: Bad parameter (%d).\n", parm);
    }
}

void Rend_SkyParams(int layer, int parm, float value)
{
    int         i;

    if(isDedicated)
        return;

    if(layer == DD_SKY)         // The whole sky?
    {
        switch (parm)
        {
        case DD_COLUMNS:
            Rend_SkyDetail((int) value, skyRows);
            break;

        case DD_ROWS:
            Rend_SkyDetail(skyDetail, (int) value);
            break;

        case DD_HEIGHT:
            maxSideAngle = PI / 2 * value;
            // Recalculate the vertices.
            Rend_SkyDetail(skyDetail, skyRows);
            break;

        case DD_HORIZON:        // horizon offset angle
            horizonOffset = PI / 2 * value;
            // Recalculate the vertices.
            Rend_SkyDetail(skyDetail, skyRows);
            break;

        default:
            // Operate on all layers.
            for(i = 0; i < MAXSKYLAYERS; i++)
                internalSkyParams(skyLayers + i, parm, value);
        }
    }
    // This is for a specific layer.
    else if(layer >= 0 && layer < MAXSKYLAYERS)
        internalSkyParams(skyLayers + layer, parm, value);
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
