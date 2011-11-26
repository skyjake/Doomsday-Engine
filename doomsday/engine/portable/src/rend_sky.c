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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"

#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"
#include "r_sky.h"

/**
 * @defgroup skySphereRenderFlags  Sky Render Flags
 * @{
 */
#define SKYHEMI_UPPER       0x1
#define SKYHEMI_LOWER       0x2
#define SKYHEMI_JUST_CAP    0x4 // Just draw the top or bottom cap.
/**@}*/

typedef struct {
    float pos[3];
} skyvertex_t;

#define SKYVTX_IDX(c, r)    ( (r)*skyColumns + (c)%skyColumns )

int skyDetail = 6, skyColumns = 4*6, skyRows = 3;
float skyDist = 1600;
byte skySimple = false;

static void constructSphere(void);
static void destroySphere(void);
// CVar callback function which marks the sphere as needing to be rebuilt.
static void updateSphere(void);

static void rebuildHemisphere(void);

// @c true iff this module has been initialized.
static boolean initedOk = false;

// Hemisphere geometry used with the sky sphere.
static skyvertex_t* skyVerts; // Crest is up.
static int numSkyVerts;
static boolean needRebuildHemisphere = true;

// Sphere render state paramaters. Global for performance reasons.
static float skyTexOffset;
static int skyTexWidth, skyTexHeight;
static rcolor_t skyCapColor;
static boolean skyFadeout;

void Rend_SkyRegister(void)
{
    C_VAR_INT2("rend-sky-detail", &skyDetail, 0, 3, 7, updateSphere);
    C_VAR_INT2("rend-sky-rows", &skyRows, 0, 1, 8, updateSphere);
    C_VAR_FLOAT("rend-sky-distance", &skyDist, CVF_NO_MAX, 1, 0);
    C_VAR_BYTE("rend-sky-simple", &skySimple, 0, 0, 1);
}

void Rend_SkyInit(void)
{
    if(novideo || isDedicated || initedOk) return;
    initedOk = true;
}

void Rend_SkyShutdown(void)
{
    if(novideo || isDedicated || !initedOk) return;
    destroySphere();
    initedOk = false;
}

static void renderSkyModels(void)
{
    rendmodelparams_t params;
    skymodel_t* sky;
    float pos[3];
    float inter;

    int i, c;
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Setup basic translation.
    glTranslatef(vx, vy, vz);

    for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; ++i, sky++)
    {
        if(!sky->def)
            continue;

        if(!R_SkyLayerActive(sky->def->layer))
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

// Look up the precalculated vertex.
static __inline const skyvertex_t* hemisphereVertex(int r, int c)
{
    return skyVerts + SKYVTX_IDX(c, r);
}

static void renderHemisphereCap(void)
{
    int c;

    // Use the appropriate color.
    glColor3fv(skyCapColor.rgb);

    // Draw the cap.
    glBegin(GL_TRIANGLE_FAN);
    for(c = 0; c < skyColumns; ++c)
    {
        glVertex3fv((const GLfloat*)hemisphereVertex(0, c)->pos);
    }
    glEnd();

    // Are we doing a colored fadeout?
    if(!skyFadeout) return;

    // We must fill the background for the top row since it'll be
    // partially translucent.
    if(!skySimple)
    {
        glBegin(GL_TRIANGLE_STRIP);
        glVertex3fv((const GLfloat*)hemisphereVertex(0, 0)->pos);
        for(c = 0; c < skyColumns; ++c)
        {
            // One step down.
            glVertex3fv((const GLfloat*)hemisphereVertex(1, c)->pos);
            // And one step right.
            glVertex3fv((const GLfloat*)hemisphereVertex(0, c + 1)->pos);
        }
        glVertex3fv((const GLfloat*)hemisphereVertex(1, c)->pos);
        glEnd();
    }
    else
    {
        glBegin(GL_QUADS);
        for(c = 0; c < skyColumns; ++c)
        {
            glVertex3fv((const GLfloat*)hemisphereVertex(0, c)->pos);
            glVertex3fv((const GLfloat*)hemisphereVertex(1, c)->pos);
            glVertex3fv((const GLfloat*)hemisphereVertex(1, c + 1)->pos);
            glVertex3fv((const GLfloat*)hemisphereVertex(0, c + 1)->pos);
        }
        glEnd();
    }
}

static void skyVertex(int r, int c)
{
    const skyvertex_t* svtx = hemisphereVertex(r, c);

    // And the texture coordinates.
    glTexCoord2f((1024 / skyTexWidth) * c / (float) skyColumns, r / (float) skyRows);

    // Also the color.
    if(skyFadeout)
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

    glVertex3fv((const GLfloat*)svtx->pos);
}

static void renderHemisphere(void)
{
    // The total number of triangles per hemisphere can be calculated
    // as follows: rows * columns * 2 + 2 (for the top cap).
    glEnable(GL_TEXTURE_2D);
    if(!skySimple)
    {
        int r, c;
        for(r = 0; r < skyRows; ++r)
        {
            glBegin(GL_TRIANGLE_STRIP);
            skyVertex(r, 0);
            skyVertex(r + 1, 0);
            for(c = 1; c <= skyColumns; ++c)
            {
                skyVertex(r, c);
                skyVertex(r + 1, c);
            }
            glEnd();
        }
    }
    else
    {
        int r, c;
        for(r = 0; r < skyRows; ++r)
        {
            glBegin(GL_QUADS);
            for(c = 0; c < skyColumns; ++c)
            {
                skyVertex(r, c);
                skyVertex(r + 1, c);
                skyVertex(r + 1, c + 1);
                skyVertex(r, c + 1);
            }
            glEnd();
        }
    }
    glDisable(GL_TEXTURE_2D);
}

static void configureRenderHemisphereStateForLayer(int layer, boolean setupCap)
{
    int magMode;
    DGLuint tex;

    if(renderTextures == 0)
    {
        tex = 0;
        magMode = GL_LINEAR;
        skyTexWidth  = skyTexHeight = 1;
        if(setupCap)
            skyFadeout = false;
    }
    else
    {
        const materialvariantspecification_t* spec;
        const materialsnapshot_t* ms;
        material_t* mat;

        if(renderTextures == 2)
        {
            mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));
        }
        else
        {
            mat = R_SkyLayerMaterial(layer);
            if(!mat)
                mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
        }
        assert(mat);

        spec = Materials_VariantSpecificationForContext(MC_SKYSPHERE,
            TSF_NO_COMPRESSION | (R_SkyLayerMasked(layer)? TSF_ZEROMASK : 0),
            0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE, 1, -2, -1, false, true, false, false);
        ms = Materials_Prepare(mat, spec, true);

        tex     = MSU_gltexture(ms, MTU_PRIMARY);
        magMode = MSU(ms, MTU_PRIMARY).magMode;
        Texture_Dimensions(MSU_texture(ms, MTU_PRIMARY), &skyTexWidth, &skyTexHeight);

        if(setupCap)
        {
            float fadeoutLimit = R_SkyLayerFadeoutLimit(layer);

            skyCapColor.red   = ms->topColor[CR];
            skyCapColor.green = ms->topColor[CG];
            skyCapColor.blue  = ms->topColor[CB];

            // Is the colored fadeout in use?
            skyFadeout = (skyCapColor.red   >= fadeoutLimit ||
                          skyCapColor.green >= fadeoutLimit ||
                          skyCapColor.blue  >= fadeoutLimit);
        }
    }

    skyTexOffset = R_SkyLayerOffset(layer);
    if(setupCap && !skyFadeout)
    {
        // Default color is black.
        skyCapColor.red = skyCapColor.green = skyCapColor.blue = 0;
    }
    if(skyTexWidth == 0 || skyTexHeight == 0)
    {
        // Disable texturing.
        tex = 0;
    }

    // Rebuild the hemisphere model if necessary.
    rebuildHemisphere();

    GL_BindTexture(tex, magMode);
}

/// @param flags  @see skySphereRenderFlags
static void renderSkyHemisphere(int flags)
{
    int firstSkyLayer = R_SkyFirstActiveLayer();
    const boolean yflip = !!(flags & SKYHEMI_LOWER);

    if(yflip)
    {
        // The lower hemisphere must be flipped.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glScalef(1.0f, -1.0f, 1.0f);
    }

    // First render the cap and the background for fadeouts, if needed.
    configureRenderHemisphereStateForLayer(firstSkyLayer, true/*setup cap*/);
    renderHemisphereCap();

    if(!(flags & SKYHEMI_JUST_CAP))
    {
        // Now render the textured layers.
        boolean popTextureMatrix;
        int i;

        for(i = firstSkyLayer; i <= MAX_SKY_LAYERS; ++i)
        {
            if(!R_SkyLayerActive(i)) continue;
            if(i != firstSkyLayer)
            {
                configureRenderHemisphereStateForLayer(i, false/*do not setup cap*/);
            }

            popTextureMatrix = false;
            if(yflip || skyTexOffset != 0)
            {
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();
                if(yflip) glScalef(1.0f, -1.0f, 1.0f);
                glTranslatef(skyTexOffset / skyTexWidth, yflip? -1.0f : 0.0f, 0.0f);
                popTextureMatrix = true;
            }

            renderHemisphere();

            if(popTextureMatrix)
            {
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
            }
        }
    }

    if(yflip)
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}

void Rend_RenderSky(void)
{
    if(novideo || isDedicated || !initedOk) return;

    // Is there a sky to be rendered?
    if(!R_SkyFirstActiveLayer()) return;

    // If sky models have been inited, they will be used.
    if(!skyModelsInited || alwaysDrawSphere)
    {
        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        // Disable culling, all triangles face the viewer.
        glDisable(GL_CULL_FACE);
        GL_DisableArrays(true, true, DDMAXINT);

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vx, vy, vz);
        glScalef(skyDist, skyDist, skyDist);

        // Always draw both hemispheres.
        renderSkyHemisphere(SKYHEMI_LOWER);
        renderSkyHemisphere(SKYHEMI_UPPER);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Enable the disabled things.
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    // How about some 3D models?
    if(skyModelsInited)
    {
        renderSkyModels();
    }
}

static void destroySphere(void)
{
    if(skyVerts)
    {
        free(skyVerts);
        skyVerts = NULL;
    }
    numSkyVerts = 0;
}

/**
 * The top row (row 0) is the one that's faded out.
 * There must be at least 4 columns. The preferable number is 4n, where
 * n is 1, 2, 3... There should be at least two rows because the first
 * one is always faded.
 */
static void constructSphere(void)
{
    const float maxSideAngle  = (float) PI / 2 * R_SkyHeight();
    const float horizonOffset = (float) PI / 2 * R_SkyHorizonOffset();
    const float scale = 1;
    float realRadius, topAngle, sideAngle;
    skyvertex_t* svtx;
    int c, r;

    if(skyDetail < 1) skyDetail = 1;
    if(skyRows < 1) skyRows = 1;

    skyColumns = 4 * skyDetail;

    numSkyVerts = skyColumns * (skyRows + 1);
    skyVerts = (skyvertex_t*)realloc(skyVerts, sizeof *skyVerts * numSkyVerts);
    if(!skyVerts)
        Con_Error(__FILE__":constructSphere: Failed (re)allocation of %lu bytes for sphere verts.", (unsigned long) sizeof *skyVerts * numSkyVerts);

    // Calculate the vertices.
    for(r = 0; r < skyRows + 1; ++r)
        for(c = 0; c < skyColumns; ++c)
        {
            svtx = skyVerts + SKYVTX_IDX(c, r);
            topAngle = ((c / (float) skyColumns) *2) * PI;
            sideAngle = horizonOffset + maxSideAngle * (skyRows - r) / (float) skyRows;
            realRadius = scale * cos(sideAngle);
            svtx->pos[VX] = realRadius * cos(topAngle);
            svtx->pos[VY] = scale * sin(sideAngle); // The height.
            svtx->pos[VZ] = realRadius * sin(topAngle);
        }
}

static void rebuildHemisphere(void)
{
    static boolean firstBuild = true;
    static float oldHorizonOffset;
    static float oldHeight;

    // Rebuild our hemisphere model if any paramaters have changed.
    if(firstBuild || R_SkyHorizonOffset() != oldHorizonOffset)
    {
        oldHorizonOffset = R_SkyHorizonOffset();
        needRebuildHemisphere = true;
    }
    if(firstBuild || R_SkyHeight() != oldHeight)
    {
        oldHeight = R_SkyHeight();
        needRebuildHemisphere = true;
    }
    firstBuild = false;

    if(!needRebuildHemisphere) return;

    // We have work to do...
    constructSphere();
    needRebuildHemisphere = false;
}

/// \note A CVar callback.
static void updateSphere(void)
{
    // Defer this task until render time, when we can be sure we are in correct thread.
    needRebuildHemisphere = true;
}
