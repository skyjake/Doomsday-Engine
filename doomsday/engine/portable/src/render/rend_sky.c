/**\file rend_sky.c
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"

#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"
#include "r_sky.h"

/**
 * @defgroup skySphereRenderFlags  Sky Render Flags
 * @ingroup flags
 * @{
 */
#define SKYHEMI_UPPER       0x1
#define SKYHEMI_LOWER       0x2
#define SKYHEMI_JUST_CAP    0x4 // Just draw the top or bottom cap.
/**@}*/

typedef struct {
    float pos[3];
} skyvertex_t;

typedef struct {
    boolean fadeout, texXFlip;
    Size2Raw texSize;
    float texOffset;
    ColorRawf capColor;
} renderhemispherestate_t;

int skyDetail = 6, skyColumns = 4*6, skyRows = 3;
float skyDistance = 1600;

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
static renderhemispherestate_t rs;

void Rend_SkyRegister(void)
{
    C_VAR_INT2("rend-sky-detail", &skyDetail, 0, 3, 7, updateSphere);
    C_VAR_INT2("rend-sky-rows", &skyRows, 0, 1, 8, updateSphere);
    C_VAR_FLOAT("rend-sky-distance", &skyDistance, CVF_NO_MAX, 1, 0);
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
    float inter;
    int i, c;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Setup basic translation.
    glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);

    for(i = 0, sky = skyModels; i < NUM_SKY_MODELS; ++i, sky++)
    {
        if(!sky->def)
            continue;

        if(!R_SkyLayerActive(sky->def->layer+1))
        {
            // The model has been assigned to a layer, but the layer is
            // not visible.
            continue;
        }

        inter = (sky->maxTimer > 0 ? sky->timer / (float) sky->maxTimer : 0);

        memset(&params, 0, sizeof(params));

        // Calculate the coordinates for the model.
        params.origin[VX] = vOrigin[VX] * -sky->def->coordFactor[VX];
        params.origin[VY] = vOrigin[VZ] * -sky->def->coordFactor[VZ];
        params.origin[VZ] = vOrigin[VY] * -sky->def->coordFactor[VY];
        params.gzt = params.origin[VZ];
        params.distance = 1;

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
static __inline skyvertex_t* skyVertex(int r, int c)
{
    return skyVerts + (r*skyColumns + c%skyColumns);
}

static void renderHemisphereCap(void)
{
    int c;

    // Use the appropriate color.
    glColor3fv(rs.capColor.rgb);

    // Draw the cap.
    glBegin(GL_TRIANGLE_FAN);
    for(c = 0; c < skyColumns; ++c)
    {
        glVertex3fv((const GLfloat*)skyVertex(0, c)->pos);
    }
    glEnd();

    // Are we doing a colored fadeout?
    if(!rs.fadeout) return;

    // We must fill the background for the top row since it'll be
    // partially translucent.
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3fv((const GLfloat*)skyVertex(0, 0)->pos);
    for(c = 0; c < skyColumns; ++c)
    {
        // One step down.
        glVertex3fv((const GLfloat*)skyVertex(1, c)->pos);
        // And one step right.
        glVertex3fv((const GLfloat*)skyVertex(0, c + 1)->pos);
    }
    glVertex3fv((const GLfloat*)skyVertex(1, c)->pos);
    glEnd();
}

static void renderHemisphere(void)
{
#define WRITESKYVERTEX(r_, c_) { \
    svtx = skyVertex(r_, c_); \
    if(rs.texSize.width != 0) \
       glTexCoord2f((c_) / (float) skyColumns, (r_) / (float) skyRows); \
    if(rs.fadeout) \
    { \
        if((r_) == 0) glColor4f(1, 1, 1, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    else \
    { \
        if((r_) == 0) glColor3f(0, 0, 0); \
        else          glColor3f(1, 1, 1); \
    } \
    glVertex3fv((const GLfloat*)svtx->pos); \
}

    const skyvertex_t* svtx;
    int r, c;
    for(r = 0; r < skyRows; ++r)
    {
        glBegin(GL_TRIANGLE_STRIP);
        WRITESKYVERTEX(r, 0);
        WRITESKYVERTEX(r + 1, 0);
        for(c = 1; c <= skyColumns; ++c)
        {
            WRITESKYVERTEX(r, c);
            WRITESKYVERTEX(r + 1, c);
        }
        glEnd();
    }
}

typedef enum {
    HC_NONE = 0,
    HC_TOP,
    HC_BOTTOM
} hemispherecap_t;

static void configureRenderHemisphereStateForLayer(int layer, hemispherecap_t setupCap)
{
    // Default state is no texture and no fadeout.
    rs.texSize.width = rs.texSize.height = 0;
    if(setupCap != HC_NONE)
        rs.fadeout = false;
    rs.texXFlip = true;

    if(renderTextures != 0)
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
            {
                mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
                rs.texXFlip = false;
            }
        }
        assert(mat);

        spec = Materials_VariantSpecificationForContext(MC_SKYSPHERE,
            TSF_NO_COMPRESSION | (R_SkyLayerMasked(layer)? TSF_ZEROMASK : 0),
            0, 0, 0, GL_REPEAT, GL_CLAMP_TO_EDGE, 0, -1, -1, false, true, false, false);
        ms = Materials_Prepare(mat, spec, true);

        rs.texSize.width = Texture_Width(MSU_texture(ms, MTU_PRIMARY));
        rs.texSize.height = Texture_Height(MSU_texture(ms, MTU_PRIMARY));
        if(rs.texSize.width && rs.texSize.height)
        {
            rs.texOffset = R_SkyLayerOffset(layer);
            GL_BindTexture(MST(ms, MTU_PRIMARY));
        }
        else
        {
            // Disable texturing.
            rs.texSize.width = rs.texSize.height = 0;
            GL_SetNoTexture();
        }

        if(setupCap != HC_NONE)
        {
            const averagecolor_analysis_t* avgLineColor = (const averagecolor_analysis_t*)
                    Texture_AnalysisDataPointer(MSU_texture(ms, MTU_PRIMARY),
                        (setupCap == HC_TOP? TA_LINE_TOP_COLOR : TA_LINE_BOTTOM_COLOR));
            const float fadeoutLimit = R_SkyLayerFadeoutLimit(layer);
            if(!avgLineColor)
                Con_Error("configureRenderHemisphereStateForLayer: Texture id:%u has no %s analysis.", Textures_Id(MSU_texture(ms, MTU_PRIMARY)), (setupCap == HC_TOP? "TA_LINE_TOP_COLOR" : "TA_LINE_BOTTOM_COLOR"));

            V3f_Copy(rs.capColor.rgb, avgLineColor->color.rgb);
            // Is the colored fadeout in use?
            rs.fadeout = (rs.capColor.red   >= fadeoutLimit ||
                          rs.capColor.green >= fadeoutLimit ||
                          rs.capColor.blue  >= fadeoutLimit);
        }
    }
    else
    {
        GL_SetNoTexture();
    }

    if(setupCap != HC_NONE && !rs.fadeout)
    {
        // Default color is black.
        V3f_Set(rs.capColor.rgb, 0, 0, 0);
    }
}

/// @param flags  @see skySphereRenderFlags
static void renderSkyHemisphere(int flags)
{
    int firstSkyLayer = R_SkyFirstActiveLayer();
    const boolean yflip = !!(flags & SKYHEMI_LOWER);
    hemispherecap_t cap = !!(flags & SKYHEMI_LOWER)? HC_BOTTOM : HC_TOP;

    // Rebuild the hemisphere model if necessary.
    rebuildHemisphere();

    if(yflip)
    {
        // The lower hemisphere must be flipped.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glScalef(1.0f, -1.0f, 1.0f);
    }

    // First render the cap and the background for fadeouts, if needed.
    configureRenderHemisphereStateForLayer(firstSkyLayer, cap);
    renderHemisphereCap();

    if(!(flags & SKYHEMI_JUST_CAP))
    {
        int i;

        for(i = firstSkyLayer; i <= MAX_SKY_LAYERS; ++i)
        {
            if(!R_SkyLayerActive(i)) continue;
            if(i != firstSkyLayer)
            {
                configureRenderHemisphereStateForLayer(i, HC_NONE);
            }

            if(rs.texSize.width != 0)
            {
                glEnable(GL_TEXTURE_2D);
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();
                glTranslatef(rs.texOffset / rs.texSize.width, 0, 0);
                glScalef(1024.f / rs.texSize.width * (rs.texXFlip? 1 : -1), yflip? -1 : 1, 1);
                if(yflip) glTranslatef(0, -1, 0);
            }

            renderHemisphere();

            if(rs.texSize.width != 0)
            {
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
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

        // Setup a proper matrix.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);
        glScalef(skyDistance, skyDistance, skyDistance);

        // Always draw both hemispheres.
        renderSkyHemisphere(SKYHEMI_LOWER);
        renderSkyHemisphere(SKYHEMI_UPPER);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        // Restore assumed default GL state.
        glEnable(GL_CULL_FACE);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    // How about some 3D models?
    if(skyModelsInited)
    {
        // We don't want anything written in the depth buffer.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        renderSkyModels();

        // Restore assumed default GL state.
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
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
 *
 * The total number of triangles per hemisphere can be calculated thus:
 *
 * Sum: rows * columns * 2 + (hemisphere)
 *      rows * 2 + (fadeout)
 *      rows - 2 (cap)
 */
static void constructSphere(void)
{
    const float maxSideAngle  = (float) PI / 2 * R_SkyHeight();
    const float horizonOffset = (float) PI / 2 * R_SkyHorizonOffset();
    const float scale = 1;
    float realRadius, topAngle, sideAngle;
    int c, r;

    if(skyDetail < 1) skyDetail = 1;
    if(skyRows < 1) skyRows = 1;

    skyColumns = 4 * skyDetail;

    numSkyVerts = skyColumns * (skyRows + 1);
    skyVerts = (skyvertex_t*)realloc(skyVerts, sizeof *skyVerts * numSkyVerts);
    if(!skyVerts)
        Con_Error("constructSphere: Failed (re)allocation of %lu bytes for SkyVertex list.", (unsigned long) sizeof *skyVerts * numSkyVerts);

    // Calculate the vertices.
    for(r = 0; r < skyRows + 1; ++r)
        for(c = 0; c < skyColumns; ++c)
        {
            skyvertex_t* svtx = skyVertex(r, c);

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
