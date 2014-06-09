/** @file rend_model.cpp  3D Model Rendering.
 *
 * @note Light vectors and triangle normals are in an entirely independent,
 *       right-handed coordinate system.
 *
 * There is some more confusion with Y and Z axes as the game uses Z as the
 * vertical axis and the rendering code and model definitions use the Y axis.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "clientapp.h"
#include "dd_def.h"
#include "dd_main.h" // App_WorldSystem()

#include "render/rend_model.h"
#include "render/rend_main.h"
#include "render/vlight.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include <doomsday/console/var.h>
#include <de/Log>
#include <de/ArrayValue>
#include <de/binangle.h>
#include <de/memory.h>
#include <de/concurrency.h>
#include <cstdlib>
#include <cmath>
#include <cstring>

using namespace de;

#define QATAN2(y,x)         qatan2(y,x)
#define QASIN(x)            asin(x) // @todo Precalculate arcsin.

static inline float qatan2(float y, float x)
{
    float ang = BANG2RAD(bamsAtan2(y * 512, x * 512));
    if(ang > PI) ang -= 2 * (float) PI;
    return ang;
}

enum rendcmd_t
{
    RC_COMMAND_COORDS,
    RC_OTHER_COORDS,
    RC_BOTH_COORDS
};

byte useModels         = true;
int modelLight         = 4;
int frameInter         = true;
float modelAspectMod   = 1 / 1.2f; //.833334f;
int mirrorHudModels;
int modelShinyMultitex = true;
float modelShinyFactor = 1.0f;
float modelSpinSpeed   = 1;
int maxModelDistance   = 1500;
float rend_model_lod   = 256;
byte precacheSkins     = true;

static bool inited;

struct array_t
{
    bool enabled;
    void *data;
};
#define MAX_ARRAYS (2 + MAX_TEX_UNITS)
static array_t arrays[MAX_ARRAYS];

// The global vertex render buffer.
static Vector3f *modelPosCoords;
static Vector3f *modelNormCoords;
static Vector4ub *modelColorCoords;
static Vector2f *modelTexCoords;

// Global variables for ease of use. (Egads!)
static Vector3f modelCenter;
static ModelDetailLevel *activeLod;

static uint vertexBufferMax; ///< Maximum number of vertices we'll be required to render per submodel.
static uint vertexBufferSize; ///< Current number of vertices supported by the render buffer.
#ifdef DENG_DEBUG
static bool announcedVertexBufferMaxBreach; ///< @c true if an attempt has been made to expand beyond our capability.
#endif

/*static void modelAspectModChanged()
{
    /// @todo Reload and resize all models.
}*/

void Rend_ModelRegister()
{
    C_VAR_BYTE ("rend-model",                &useModels,            0, 0, 1);
    C_VAR_INT  ("rend-model-lights",         &modelLight,           0, 0, 10);
    C_VAR_INT  ("rend-model-inter",          &frameInter,           0, 0, 1);
    C_VAR_FLOAT("rend-model-aspect",         &modelAspectMod,       CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT  ("rend-model-distance",       &maxModelDistance,     CVF_NO_MAX, 0, 0);
    C_VAR_BYTE ("rend-model-precache",       &precacheSkins,        0, 0, 1);
    C_VAR_FLOAT("rend-model-lod",            &rend_model_lod,       CVF_NO_MAX, 0, 0);
    C_VAR_INT  ("rend-model-mirror-hud",     &mirrorHudModels,      0, 0, 1);
    C_VAR_FLOAT("rend-model-spin-speed",     &modelSpinSpeed,       CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT  ("rend-model-shiny-multitex", &modelShinyMultitex,   0, 0, 1);
    C_VAR_FLOAT("rend-model-shiny-strength", &modelShinyFactor,     0, 0, 10);
}

void Rend_ModelInit()
{
    if(inited) return; // Already been here.

    modelPosCoords   = 0;
    modelNormCoords  = 0;
    modelColorCoords = 0;
    modelTexCoords   = 0;

    vertexBufferMax = vertexBufferSize = 0;
#ifdef DENG_DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    inited = true;
}

void Rend_ModelShutdown()
{
    if(!inited) return;

    M_Free(modelPosCoords); modelPosCoords = 0;
    M_Free(modelNormCoords); modelNormCoords = 0;
    M_Free(modelColorCoords); modelColorCoords = 0;
    M_Free(modelTexCoords); modelTexCoords = 0;

    vertexBufferMax = vertexBufferSize = 0;
#ifdef DENG_DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    inited = false;
}

bool Rend_ModelExpandVertexBuffers(uint numVertices)
{
    DENG2_ASSERT(inited);

    LOG_AS("Rend_ModelExpandVertexBuffers");

    if(numVertices <= vertexBufferMax) return true;

    // Sanity check a sane maximum...
    if(numVertices >= RENDER_MAX_MODEL_VERTS)
    {
#ifdef DENG_DEBUG
        if(!announcedVertexBufferMaxBreach)
        {
            LOGDEV_GL_WARNING("Attempted to expand to %u vertices (max %u)")
                << numVertices << RENDER_MAX_MODEL_VERTS;
            announcedVertexBufferMaxBreach = true;
        }
#endif
        return false;
    }

    // Defer resizing of the render buffer until draw time as it may be repeatedly expanded.
    vertexBufferMax = numVertices;
    return true;
}

/// @return  @c true= Vertex buffer is large enough to handle @a numVertices.
static bool resizeVertexBuffer(uint numVertices)
{
    // Mark the vertex buffer if a resize is necessary.
    Rend_ModelExpandVertexBuffers(numVertices);

    // Do we need to resize the buffers?
    if(vertexBufferMax != vertexBufferSize)
    {
        /// @todo Align access to this memory along a 4-byte boundary?
        modelPosCoords   =  (Vector3f *) M_Realloc(modelPosCoords,   sizeof(*modelPosCoords)   * vertexBufferMax);
        modelNormCoords  =  (Vector3f *) M_Realloc(modelNormCoords,  sizeof(*modelNormCoords)  * vertexBufferMax);
        modelColorCoords = (Vector4ub *) M_Realloc(modelColorCoords, sizeof(*modelColorCoords) * vertexBufferMax);
        modelTexCoords   =  (Vector2f *) M_Realloc(modelTexCoords,   sizeof(*modelTexCoords)   * vertexBufferMax);

        vertexBufferSize = vertexBufferMax;
    }

    // Is the buffer large enough?
    return vertexBufferSize >= numVertices;
}

static void disableArrays(int vertices, int colors, int coords)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        arrays[AR_VERTEX].enabled = false;
    }

    if(colors)
    {
        arrays[AR_COLOR].enabled = false;
    }

    for(int i = 0; i < numTexUnits; ++i)
    {
        if(coords & (1 << i))
        {
            arrays[AR_TEXCOORD0 + i].enabled = false;
        }
    }

    DENG_ASSERT(!Sys_GLCheckError());
}

static inline void enableTexUnit(byte id)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glActiveTexture(GL_TEXTURE0 + id);
    glEnable(GL_TEXTURE_2D);
}

static inline void disableTexUnit(byte id)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glActiveTexture(GL_TEXTURE0 + id);
    glDisable(GL_TEXTURE_2D);

    // Implicit disabling of texcoord array.
    disableArrays(0, 0, 1 << id);
}

/**
 * The first selected unit is active after this call.
 */
static void selectTexUnits(int count)
{
    for(int i = numTexUnits - 1; i >= count; i--)
    {
        disableTexUnit(i);
    }

    // Enable the selected units.
    for(int i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits) continue;
        enableTexUnit(i);
    }
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
static void configureArrays(void *vertices, void *colors, int numCoords = 0,
    void **coords = 0, int lock = 0)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        arrays[AR_VERTEX].enabled = true;
        arrays[AR_VERTEX].data = vertices;
    }

    if(colors)
    {
        arrays[AR_COLOR].enabled = true;
        arrays[AR_COLOR].data = colors;
    }

    for(int i = 0; i < numCoords && i < MAX_TEX_UNITS; ++i)
    {
        if(coords[i])
        {
            arrays[AR_TEXCOORD0 + i].enabled = true;
            arrays[AR_TEXCOORD0 + i].data = coords[i];
        }
    }

    DENG_ASSERT(!Sys_GLCheckError());
}

static void drawArrayElement(int index)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    for(int i = 0; i < numTexUnits; ++i)
    {
        if(!arrays[AR_TEXCOORD0 + i].enabled) continue;

        Vector2f const &texCoord = ((Vector2f const *)arrays[AR_TEXCOORD0 + i].data)[index];
        glMultiTexCoord2f(GL_TEXTURE0 + i, texCoord.x, texCoord.y);
    }

    if(arrays[AR_COLOR].enabled)
    {
        Vector4ub const &colorCoord = ((Vector4ub const *) arrays[AR_COLOR].data)[index];
        glColor4ub(colorCoord.x, colorCoord.y, colorCoord.z, colorCoord.w);
    }

    if(arrays[AR_VERTEX].enabled)
    {
        Vector3f const &posCoord = ((Vector3f const *) arrays[AR_VERTEX].data)[index];
        glVertex3f(posCoord.x, posCoord.y, posCoord.z);
    }
}

/**
 * Return a pointer to the visible model frame.
 */
static ModelFrame &visibleModelFrame(ModelDef &modef, int subnumber, int mobjId)
{
    if(subnumber >= int(modef.subCount()))
    {
        throw Error("Rend_DrawModel.visibleFrame",
                    QString("Model has %1 submodels, but submodel #%2 was requested")
                        .arg(modef.subCount()).arg(subnumber));
    }
    SubmodelDef const &sub = modef.subModelDef(subnumber);

    int curFrame = sub.frame;
    if(modef.flags & MFF_IDFRAME)
    {
        curFrame += mobjId % sub.frameRange;
    }

    return App_ResourceSystem().model(sub.modelId).frame(curFrame);
}

/**
 * Render a set of 3D model primitives using the given data.
 */
static void drawPrimitives(rendcmd_t mode, Model::Primitives const &primitives,
    Vector3f *posCoords, Vector4ub *colorCoords, Vector2f *texCoords = 0)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Disable all vertex arrays.
    disableArrays(true, true, DDMAXINT);

    // Load the vertex array.
    void *coords[2];
    switch(mode)
    {
    case RC_OTHER_COORDS:
        coords[0] = texCoords;
        configureArrays(posCoords, colorCoords, 1, coords);
        break;

    case RC_BOTH_COORDS:
        coords[0] = NULL;
        coords[1] = texCoords;
        configureArrays(posCoords, colorCoords, 2, coords);
        break;

    default:
        configureArrays(posCoords, colorCoords);
        break;
    }

    foreach(Model::Primitive const &prim, primitives)
    {
        // The type of primitive depends on the sign.
        glBegin(prim.triFan? GL_TRIANGLE_FAN : GL_TRIANGLE_STRIP);

        foreach(Model::Primitive::Element const &elem, prim.elements)
        {
            if(mode != RC_OTHER_COORDS)
            {
                glTexCoord2f(elem.texCoord.x, elem.texCoord.y);
            }

            drawArrayElement(elem.index);
        }

        // The primitive is complete.
        glEnd();
    }
}

/**
 * Interpolate linearly between two sets of vertices.
 */
static void Mod_LerpVertices(float inter, int count, ModelFrame const &from,
    ModelFrame const &to, Vector3f *posOut, Vector3f *normOut)
{
    DENG2_ASSERT(&from.model == &to.model); // sanity check.
    DENG2_ASSERT(!activeLod || &activeLod->model == &from.model); // sanity check.
    DENG2_ASSERT(from.vertices.count() == to.vertices.count()); // sanity check.

    ModelFrame::VertexBuf::const_iterator startIt = from.vertices.begin();
    ModelFrame::VertexBuf::const_iterator endIt   = to.vertices.begin();

    if(&from == &to || de::fequal(inter, 0))
    {
        for(int i = 0; i < count; ++i, startIt++, posOut++, normOut++)
        {
            if(!activeLod || activeLod->hasVertex(i))
            {
                *posOut  = startIt->pos;
                *normOut = startIt->norm;
            }
        }
    }
    else
    {
        for(int i = 0; i < count; ++i, startIt++, endIt++, posOut++, normOut++)
        {
            if(!activeLod || activeLod->hasVertex(i))
            {
                *posOut  = de::lerp(startIt->pos,  endIt->pos,  inter);
                *normOut = de::lerp(startIt->norm, endIt->norm, inter);
            }
        }
    }
}

static void Mod_MirrorCoords(int count, Vector3f *coords, int axis)
{
    for(; count-- > 0; coords++)
    {
        (*coords)[axis] = -(*coords)[axis];
    }
}

struct lightmodelvertexworker_params_t
{
    Vector3f color, extra;
    float rotateYaw, rotatePitch;
    Vector3f normal;
    uint numProcessed, max;
    bool invert;
};

static void lightModelVertex(VectorLight const &vlight, lightmodelvertexworker_params_t &parms)
{
    // We must transform the light vector to model space.
    float vlightDirection[3] = { vlight.direction.x, vlight.direction.y, vlight.direction.z };
    M_RotateVector(vlightDirection, parms.rotateYaw, parms.rotatePitch);

    // Quick hack: Flip light normal if model inverted.
    if(parms.invert)
    {
        vlightDirection[VX] = -vlightDirection[VX];
        vlightDirection[VY] = -vlightDirection[VY];
    }

    float strength = Vector3f(vlightDirection).dot(parms.normal)
                   + vlight.offset; // Shift a bit towards the light.

    // Ability to both light and shade.
    if(strength > 0)
    {
        strength *= vlight.lightSide;
    }
    else
    {
        strength *= vlight.darkSide;
    }

    Vector3f &dest = vlight.affectedByAmbient? parms.color : parms.extra;
    dest += vlight.color * de::clamp(-1.f, strength, 1.f);
}

static int lightModelVertexWorker(VectorLight const *vlight, void *context)
{
    lightmodelvertexworker_params_t &parms = *static_cast<lightmodelvertexworker_params_t *>(context);

    lightModelVertex(*vlight, parms);
    parms.numProcessed += 1;

    // Time to stop?
    return parms.max && parms.numProcessed == parms.max;
}

/**
 * Calculate vertex lighting.
 * @todo construct a rotation matrix once and use it for all vertices.
 */
static void Mod_VertexColors(Vector4ub *out, int count, Vector3f const *normCoords,
    uint vLightListIdx, uint maxLights, Vector4f const &ambient, bool invert,
    float rotateYaw, float rotatePitch)
{
    Vector4f const saturated(1, 1, 1, 1);
    lightmodelvertexworker_params_t parms;

    for(int i = 0; i < count; ++i, out++, normCoords++)
    {
        if(activeLod && !activeLod->hasVertex(i))
            continue;

        // Begin with total darkness.
        parms.color        = Vector3f();
        parms.extra        = Vector3f();
        parms.normal       = *normCoords;
        parms.invert       = invert;
        parms.rotateYaw    = rotateYaw;
        parms.rotatePitch  = rotatePitch;
        parms.max          = maxLights;
        parms.numProcessed = 0;

        // Add light from each source.
        VL_ListIterator(vLightListIdx, lightModelVertexWorker, &parms);

        // Check for ambient and convert to ubyte.
        Vector4f color(parms.color.max(ambient) + parms.extra, ambient[3]);

        *out = (color.min(saturated) * 255).toVector4ub();
    }
}

/**
 * Set all the colors in the array to bright white.
 */
static void Mod_FullBrightVertexColors(int count, Vector4ub *colorCoords, float alpha)
{
    DENG2_ASSERT(colorCoords != 0);
    for(; count-- > 0; colorCoords++)
    {
        *colorCoords = Vector4ub(255, 255, 255, 255 * alpha);
    }
}

/**
 * Set all the colors into the array to the same values.
 */
static void Mod_FixedVertexColors(int count, Vector4ub *colorCoords, Vector4ub const &color)
{
    DENG2_ASSERT(colorCoords != 0);
    for(; count-- > 0; colorCoords++)
    {
        *colorCoords = color;
    }
}

/**
 * Calculate cylindrically mapped, shiny texture coordinates.
 */
static void Mod_ShinyCoords(Vector2f *out, int count, Vector3f const *normCoords,
    float normYaw, float normPitch, float shinyAng, float shinyPnt, float reactSpeed)
{
    for(int i = 0; i < count; ++i, out++, normCoords++)
    {
        if(activeLod && !activeLod->hasVertex(i))
            continue;

        float rotatedNormal[3] = { normCoords->x, normCoords->y, normCoords->z };

        // Rotate the normal vector so that it approximates the
        // model's orientation compared to the viewer.
        M_RotateVector(rotatedNormal,
                       (shinyPnt + normYaw) * 360 * reactSpeed,
                       (shinyAng + normPitch - .5f) * 180 * reactSpeed);

        *out = Vector2f(rotatedNormal[0] + 1, rotatedNormal[2]);
    }
}

static int chooseSelSkin(ModelDef &mf, int submodel, int selector)
{
    if(mf.def.hasSub(submodel))
    {
        Record &subDef = mf.def.sub(submodel);

        int i = (selector >> DDMOBJ_SELECTOR_SHIFT) & subDef.geti("selSkinMask");
        int c = subDef.geti("selSkinShift");

        if(c > 0) i >>= c;
        else      i <<= -c;

        if(i > 7) i = 7; // Maximum number of skins for selskin.
        if(i < 0) i = 0; // Improbable (impossible?), but doesn't hurt.

        return subDef.geta("selSkins")[i].asInt();
    }
    return 0;
}

static int chooseSkin(ModelDef &mf, int submodel, int id, int selector, int tmap)
{
    if(submodel >= int(mf.subCount()))
    {
        return 0;
    }

    SubmodelDef &smf = mf.subModelDef(submodel);
    Model &mdl = App_ResourceSystem().model(smf.modelId);
    int skin = smf.skin;

    // Selskin overrides the skin range.
    if(smf.testFlag(MFF_SELSKIN))
    {
        skin = chooseSelSkin(mf, submodel, selector);
    }

    // Is there a skin range for this frame?
    // (During model setup skintics and skinrange are set to >0.)
    if(smf.skinRange > 1)
    {
        // What rule to use for determining the skin?
        int offset;
        if(smf.testFlag(MFF_IDSKIN))
        {
            offset = id;
        }
        else
        {
            offset = SECONDS_TO_TICKS(App_WorldSystem().time()) / mf.skinTics;
        }

        skin += offset % smf.skinRange;
    }

    // Need translation?
    if(smf.testFlag(MFF_SKINTRANS))
    {
        skin = tmap;
    }

    if(skin < 0 || skin >= mdl.skinCount())
    {
        skin = 0;
    }

    return skin;
}

static void drawSubmodel(uint number, vismodel_t const &vmodel)
{
    int const zSign = (vmodel.mirror? -1 : 1);
    ModelDef *mf = vmodel.mf, *mfNext = vmodel.nextMF;
    SubmodelDef const &smf = mf->subModelDef(number);

    Model &mdl = App_ResourceSystem().model(smf.modelId);

    // Do not bother with infinitely small models...
    if(mf->scale == Vector3f(0, 0, 0))
        return;

    float alpha = vmodel.ambientColor.w;

    // Is the submodel-defined alpha multiplier in effect?
    // With df_brightshadow2, the alpha multiplier will be applied anyway.
    if(smf.testFlag(MFF_BRIGHTSHADOW2) ||
       !(vmodel.flags & (DDMF_BRIGHTSHADOW|DDMF_SHADOW|DDMF_ALTSHADOW)))
    {
        alpha *= smf.alpha / 255.f;
    }

    // Would this be visible?
    if(alpha <= 0) return;

    blendmode_t blending = smf.blendMode;
    // Is the submodel-defined blend mode in effect?
    if(vmodel.flags & DDMF_BRIGHTSHADOW)
    {
        blending = BM_ADD;
    }

    int useSkin = chooseSkin(*mf, number, vmodel.id, vmodel.selector, vmodel.tmap);

    // Scale interpos. Intermark becomes zero and endmark becomes one.
    // (Full sub-interpolation!) But only do it for the standard
    // interrange. If a custom one is defined, don't touch interpos.
    float endPos = 0;
    float inter = vmodel.inter;
    if((mf->interRange[0] == 0 && mf->interRange[1] == 1) || smf.testFlag(MFF_WORLD_TIME_ANIM))
    {
        endPos = (mf->interNext ? mf->interNext->interMark : 1);
        inter = (vmodel.inter - mf->interMark) / (endPos - mf->interMark);
    }

    ModelFrame *frame = &visibleModelFrame(*mf, number, vmodel.id);
    ModelFrame *nextFrame = 0;
    // Do we have a sky/particle model here?
    if(vmodel.alwaysInterpolate)
    {
        // Always interpolate, if there's animation.
        // Used with sky and particle models.
        nextFrame = &mdl.frame((smf.frame + 1) % mdl.frameCount());
        mfNext = mf;
    }
    else
    {
        // Check for possible interpolation.
        if(frameInter && mfNext && !smf.testFlag(MFF_DONT_INTERPOLATE))
        {
            if(mfNext->hasSub(number) && mfNext->subModelId(number) == smf.modelId)
            {
                nextFrame = &visibleModelFrame(*mfNext, number, vmodel.id);
            }
        }
    }

    // Clamp interpolation.
    inter = de::clamp(0.f, inter, 1.f);

    if(!nextFrame)
    {
        // If not interpolating, use the same frame as interpolation target.
        // The lerp routines will recognize this special case.
        nextFrame = frame;
        mfNext = mf;
    }

    // Determine the total number of vertices we have.
    int numVerts = mdl.vertexCount();

    // Ensure our vertex render buffers can accommodate this.
    if(!resizeVertexBuffer(numVerts))
    {
        // No can do, we aint got the power!
        return;
    }

    // Setup transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Model space => World space
    glTranslatef(vmodel.origin().x + vmodel.srvo[VX] +
                   de::lerp(mf->offset.x, mfNext->offset.x, inter),
                 vmodel.origin().z + vmodel.srvo[VZ] +
                   de::lerp(mf->offset.y, mfNext->offset.y, inter),
                 vmodel.origin().y + vmodel.srvo[VY] + zSign *
                   de::lerp(mf->offset.z, mfNext->offset.z, inter));

    if(vmodel.extraYawAngle || vmodel.extraPitchAngle)
    {
        // Sky models have an extra rotation.
        glScalef(1, 200 / 240.0f, 1);
        glRotatef(vmodel.extraYawAngle, 1, 0, 0);
        glRotatef(vmodel.extraPitchAngle, 0, 0, 1);
        glScalef(1, 240 / 200.0f, 1);
    }

    // Model rotation.
    glRotatef(vmodel.viewAlign ? vmodel.yawAngleOffset : vmodel.yaw,
              0, 1, 0);
    glRotatef(vmodel.viewAlign ? vmodel.pitchAngleOffset : vmodel.pitch,
              0, 0, 1);

    // Scaling and model space offset.
    glScalef(de::lerp(mf->scale.x, mfNext->scale.x, inter),
             de::lerp(mf->scale.y, mfNext->scale.y, inter),
             de::lerp(mf->scale.z, mfNext->scale.z, inter));
    if(vmodel.extraScale)
    {
        // Particle models have an extra scale.
        glScalef(vmodel.extraScale, vmodel.extraScale, vmodel.extraScale);
    }
    glTranslatef(smf.offset.x, smf.offset.y, smf.offset.z);

    // Determine the suitable LOD.
    if(mdl.lodCount() > 1 && rend_model_lod != 0)
    {
        float lodFactor = rend_model_lod * DENG_GAMEVIEW_WIDTH / 640.0f / (Rend_FieldOfView() / 90.0f);
        if(!de::fequal(lodFactor, 0))
        {
            lodFactor = 1 / lodFactor;
        }

        // Determine the LOD we will be using.
        activeLod = &mdl.lod(de::clamp<int>(0, lodFactor * vmodel.distance(), mdl.lodCount() - 1));
    }
    else
    {
        activeLod = 0;
    }

    // Interpolate vertices and normals.
    Mod_LerpVertices(inter, numVerts, *frame, *nextFrame,
                     modelPosCoords, modelNormCoords);

    if(zSign < 0)
    {
        Mod_MirrorCoords(numVerts, modelPosCoords, 2);
        Mod_MirrorCoords(numVerts, modelNormCoords, 1);
    }

    // Coordinates to the center of the model (game coords).
    modelCenter = Vector3f(vmodel.origin().x, vmodel.origin().y, (vmodel.origin().z + vmodel.gzt) * 2)
            + Vector3d(vmodel.srvo) + Vector3f(mf->offset.x, mf->offset.z, mf->offset.y);

    // Calculate lighting.
    Vector4f ambient;
    if(smf.testFlag(MFF_FULLBRIGHT) && !smf.testFlag(MFF_DIM))
    {
        // Submodel-specific lighting override.
        ambient = Vector4f(1, 1, 1, 1);
        Mod_FullBrightVertexColors(numVerts, modelColorCoords, alpha);
    }
    else if(!vmodel.vLightListIdx)
    {
        // Lit uniformly.
        ambient = Vector4f(vmodel.ambientColor.xyz(), alpha);
        Mod_FixedVertexColors(numVerts, modelColorCoords,
                              (ambient * 255).toVector4ub());
    }
    else
    {
        // Lit normally.
        ambient = Vector4f(vmodel.ambientColor.xyz(), alpha);

        Mod_VertexColors(modelColorCoords, numVerts,
                         modelNormCoords, vmodel.vLightListIdx, modelLight + 1,
                         ambient, (mf->scale[VY] < 0), -vmodel.yaw, -vmodel.pitch);
    }

    TextureVariant *shinyTexture = 0;
    float shininess = 0;
    if(mf->def.hasSub(number))
    {
        shininess = float(de::clamp(0.0, mf->def.sub(number).getd("shiny") * modelShinyFactor, 1.0));
        // Ensure we've prepared the shiny skin.
        if(shininess > 0)
        {
            if(Texture *tex = mf->subModelDef(number).shinySkin)
            {
                shinyTexture = tex->prepareVariant(Rend_ModelShinyTextureSpec());
            }
            else
            {
                shininess = 0;
            }
        }
    }

    Vector4f color;
    if(shininess > 0)
    {
        // Calculate shiny coordinates.
        Vector3f shinyColor = mf->def.sub(number).get("shinyColor");

        // With psprites, add the view angle/pitch.
        float offset = vmodel.shineYawOffset;

        // Calculate normalized (0,1) model yaw and pitch.
        float normYaw = M_CycleIntoRange(((vmodel.viewAlign ? vmodel.yawAngleOffset
                                                           : vmodel.yaw) + offset) / 360, 1);

        offset = vmodel.shinePitchOffset;

        float normPitch = M_CycleIntoRange(((vmodel.viewAlign ? vmodel.pitchAngleOffset
                                                             : vmodel.pitch) + offset) / 360, 1);

        float shinyAng = 0;
        float shinyPnt = 0;
        if(vmodel.shinepspriteCoordSpace)
        {
            // This is a hack to accommodate the psprite coordinate space.
            shinyPnt = 0.5;
        }
        else
        {
            Vector3f delta = modelCenter;

            if(!vmodel.shineTranslateWithViewerPos)
            {
                delta -= vOrigin.xzy();
            }

            shinyAng = QATAN2(delta.z, M_ApproxDistancef(delta.x, delta.y)) / PI + 0.5f; // shinyAng is [0,1]

            shinyPnt = QATAN2(delta.y, delta.x) / (2 * PI);
        }

        Mod_ShinyCoords(modelTexCoords, numVerts,
                        modelNormCoords, normYaw, normPitch, shinyAng, shinyPnt,
                        mf->def.sub(number).getf("shinyReact"));

        // Shiny color.
        if(smf.testFlag(MFF_SHINY_LIT))
        {
            color = Vector4f(ambient * shinyColor, shininess);
        }
        else
        {
            color = Vector4f(shinyColor, shininess);
        }
    }

    TextureVariant *skinTexture = 0;
    if(renderTextures == 2)
    {
        // For lighting debug, render all surfaces using the gray texture.
        MaterialVariantSpec const &spec = ClientApp::resourceSystem()
                .materialSpec(ModelSkinContext, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                              1, -2, -1, true, true, false, false);

        MaterialSnapshot const &ms = ClientApp::resourceSystem()
                .material(de::Uri("System", Path("gray"))).prepare(spec);

        skinTexture = &ms.texture(MTU_PRIMARY);
    }
    else
    {
        skinTexture = 0;
        if(Texture *tex = mdl.skin(useSkin).texture)
        {
            skinTexture = tex->prepareVariant(Rend_ModelDiffuseTextureSpec(mdl.flags().testFlag(Model::NoTextureCompression)));
        }
    }

    // If we mirror the model, triangles have a different orientation.
    if(zSign < 0)
    {
        glFrontFace(GL_CCW);
    }

    // Twosided models won't use backface culling.
    if(smf.testFlag(MFF_TWO_SIDED))
    {
        glDisable(GL_CULL_FACE);
    }
    glEnable(GL_TEXTURE_2D);

    Model::Primitives const &primitives =
        activeLod? activeLod->primitives : mdl.primitives();

    // Render using multiple passes?
    if(!modelShinyMultitex || shininess <= 0 || alpha < 1 ||
       blending != BM_NORMAL || !smf.testFlag(MFF_SHINY_SPECULAR) ||
       numTexUnits < 2 || !envModAdd)
    {
        // The first pass can be skipped if it won't be visible.
        if(shininess < 1 || smf.testFlag(MFF_SHINY_SPECULAR))
        {
            selectTexUnits(1);
            GL_BlendMode(blending);
            GL_BindTexture(renderTextures? skinTexture : 0);

            drawPrimitives(RC_COMMAND_COORDS, primitives,
                           modelPosCoords, modelColorCoords);
        }

        if(shininess > 0)
        {
            glDepthFunc(GL_LEQUAL);

            // Set blending mode, two choices: reflected and specular.
            if(smf.testFlag(MFF_SHINY_SPECULAR))
                GL_BlendMode(BM_ADD);
            else
                GL_BlendMode(BM_NORMAL);

            // Shiny color.
            Mod_FixedVertexColors(numVerts, modelColorCoords,
                                  (color * 255).toVector4ub());

            if(numTexUnits > 1 && modelShinyMultitex)
            {
                // We'll use multitexturing to clear out empty spots in
                // the primary texture.
                selectTexUnits(2);
                GL_ModulateTexture(11);

                glActiveTexture(GL_TEXTURE1);
                GL_BindTexture(renderTextures? shinyTexture : 0);

                glActiveTexture(GL_TEXTURE0);
                GL_BindTexture(renderTextures? skinTexture : 0);

                drawPrimitives(RC_BOTH_COORDS, primitives,
                               modelPosCoords, modelColorCoords, modelTexCoords);

                selectTexUnits(1);
                GL_ModulateTexture(1);
            }
            else
            {
                // Empty spots will get shine, too.
                selectTexUnits(1);
                GL_BindTexture(renderTextures? shinyTexture : 0);

                drawPrimitives(RC_OTHER_COORDS, primitives,
                               modelPosCoords, modelColorCoords, modelTexCoords);
            }
        }
    }
    else
    {
        // A special case: specular shininess on an opaque object.
        // Multitextured shininess with the normal blending.
        GL_BlendMode(blending);
        selectTexUnits(2);

        // Tex1*Color + Tex2RGB*ConstRGB
        GL_ModulateTexture(10);

        glActiveTexture(GL_TEXTURE1);
        GL_BindTexture(renderTextures? shinyTexture : 0);

        // Multiply by shininess.
        float colorv1[] = { color.x * color.w, color.y * color.w, color.z * color.w, color.w };
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colorv1);

        glActiveTexture(GL_TEXTURE0);
        GL_BindTexture(renderTextures? skinTexture : 0);

        drawPrimitives(RC_BOTH_COORDS, primitives,
                       modelPosCoords, modelColorCoords, modelTexCoords);

        selectTexUnits(1);
        GL_ModulateTexture(1);
    }

    // We're done!
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Normally culling is always enabled.
    if(smf.testFlag(MFF_TWO_SIDED))
    {
        glEnable(GL_CULL_FACE);
    }

    if(zSign < 0)
    {
        glFrontFace(GL_CW);
    }
    glDepthFunc(GL_LESS);

    GL_BlendMode(BM_NORMAL);
}

static int drawLightVectorWorker(VectorLight const *vlight, void *context)
{
    coord_t distFromViewer = *static_cast<coord_t *>(context);
    if(distFromViewer < 1600 - 8)
    {
        Rend_DrawVectorLight(vlight, 1 - distFromViewer / 1600);
    }
    return false; // Continue iteration.
}

void vismodel_t::setup(Vector3d const &origin, coord_t distToEye,
    Vector3d const &visOffset, float gzt, float yaw, float yawAngleOffset,
    float pitch, float pitchAngleOffset, ModelDef *mf, ModelDef *nextMF, float inter,
    Vector4f const &ambientColor, uint vLightListIdx,
    int id, int selector, BspLeaf * /*bspLeafAtOrigin*/, int mobjDDFlags, int tmap,
    bool viewAlign, bool /*fullBright*/, bool alwaysInterpolate)
{
    vismodel_t &p = *this;

    p.mf                = mf;
    p.nextMF            = nextMF;
    p.inter             = inter;
    p.alwaysInterpolate = alwaysInterpolate;
    p.id                = id;
    p.selector          = selector;
    p.flags             = mobjDDFlags;
    p.tmap              = tmap;
    p._origin[VX]       = origin.x;
    p._origin[VY]       = origin.y;
    p._origin[VZ]       = origin.z;
    p.srvo[VX]          = visOffset.x;
    p.srvo[VY]          = visOffset.y;
    p.srvo[VZ]          = visOffset.z;
    p.gzt               = gzt;
    p._distance         = distToEye;
    p.yaw               = yaw;
    p.extraYawAngle     = 0;
    p.yawAngleOffset    = yawAngleOffset;
    p.pitch             = pitch;
    p.extraPitchAngle   = 0;
    p.pitchAngleOffset  = pitchAngleOffset;
    p.extraScale        = 0;
    p.viewAlign         = viewAlign;
    p.mirror            = false;
    p.shineYawOffset    = 0;
    p.shinePitchOffset  = 0;

    p.shineTranslateWithViewerPos = p.shinepspriteCoordSpace = false;

    p.ambientColor      = ambientColor.x;
    p.vLightListIdx     = vLightListIdx;
}

void vismodel_t::draw()
{
    vismodel_t &p = *this;

    DENG2_ASSERT(inited);
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!p.mf) return;

    // Render all the submodels of this model.
    for(uint i = 0; i < p.mf->subCount(); ++i)
    {
        if(p.mf->subModelId(i))
        {
            bool disableZ = (p.mf->flags & MFF_DISABLE_Z_WRITE ||
                             p.mf->testSubFlag(i, MFF_DISABLE_Z_WRITE));

            if(disableZ)
            {
                glDepthMask(GL_FALSE);
            }

            drawSubmodel(i, p);

            if(disableZ)
            {
                glDepthMask(GL_TRUE);
            }
        }
    }

    if(devMobjVLights && p.vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(p.origin().x, p.origin().z, p.origin().y);

        coord_t distFromViewer = de::abs(p.distance());
        VL_ListIterator(p.vLightListIdx, drawLightVectorWorker, &distFromViewer);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }
}

TextureVariantSpec const &Rend_ModelDiffuseTextureSpec(bool noCompression)
{
    return ClientApp::resourceSystem().textureSpec(TC_MODELSKIN_DIFFUSE,
        (noCompression? TSF_NO_COMPRESSION : 0), 0, 0, 0, GL_REPEAT, GL_REPEAT,
        1, -2, -1, true, true, false, false);
}

TextureVariantSpec const &Rend_ModelShinyTextureSpec()
{
    return ClientApp::resourceSystem().textureSpec(TC_MODELSKIN_REFLECTION,
        TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, -2, -1, false,
        false, false, false);
}
