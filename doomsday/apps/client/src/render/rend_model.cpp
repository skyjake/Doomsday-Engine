/** @file rend_model.cpp  3D Model Rendering (frame models).
 *
 * @note Light vectors and triangle normals are in an entirely independent,
 *       right-handed coordinate system.
 *
 * There is some more confusion with Y and Z axes as the game uses Z as the
 * vertical axis and the rendering code and model definitions use the Y axis.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "dd_main.h" // App_World()

#include "world/p_players.h"
#include "world/clientmobjthinkerdata.h"
#include "render/rend_model.h"
#include "render/rend_main.h"
#include "render/rendersystem.h"
#include "render/vissprite.h"
#include "render/vectorlightdata.h"
#include "render/modelrenderer.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "resource/materialvariantspec.h"
#include "resource/clienttexture.h"
#include "resource/clientmaterial.h"

#include <doomsday/console/var.h>
#include <doomsday/world/materials.h>
#include <de/log.h>
#include <de/arrayvalue.h>
#include <de/glinfo.h>
#include <de/legacy/binangle.h>
#include <de/legacy/memory.h>
#include <de/legacy/concurrency.h>
#include <cstdlib>
#include <cmath>
#include <cstring>

using namespace de;

#define QATAN2(y,x)         qatan2(y,x)
#define QASIN(x)            asin(x) // @todo Precalculate arcsin.

static inline float qatan2(float y, float x)
{
    float ang = BANG2RAD(bamsAtan2(y * 512, x * 512));
    if (ang > PI) ang -= 2 * (float) PI;
    return ang;
}

enum rendcmd_t {
    RC_COMMAND_COORDS,
    RC_OTHER_COORDS,
    RC_BOTH_COORDS,
};

byte  useModels      = true;
int   modelLight     = 4;
int   frameInter     = true;
float modelAspectMod = 1 / 1.2f; //.833334f;
int   mirrorHudModels;
//int modelShinyMultitex = true;
float modelShinyFactor = 1.0f;
float modelSpinSpeed   = 1;
int   maxModelDistance = 1500;
float rend_model_lod   = 256;
byte  precacheSkins    = true;

static bool inited;

struct array_t
{
    bool enabled;
    void *data;
};
#define MAX_ARRAYS (2 + MAX_TEX_UNITS)
static array_t arrays[MAX_ARRAYS];

// The global vertex render buffer.
static Vec3f *modelPosCoords;
static Vec3f *modelNormCoords;
static Vec4ub *modelColorCoords;
static Vec2f *modelTexCoords;

// Global variables for ease of use. (Egads!)
static Vec3f modelCenter;
static FrameModelLOD *activeLod;

static uint vertexBufferMax; ///< Maximum number of vertices we'll be required to render per submodel.
static uint vertexBufferSize; ///< Current number of vertices supported by the render buffer.
#ifdef DE_DEBUG
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
    C_VAR_FLOAT("rend-model-shiny-strength", &modelShinyFactor,     0, 0, 10);
    C_VAR_FLOAT("rend-model-fov",            &weaponFixedFOV,       0, 0, 180);
}

void Rend_ModelInit()
{
    if (inited) return; // Already been here.

    modelPosCoords   = 0;
    modelNormCoords  = 0;
    modelColorCoords = 0;
    modelTexCoords   = 0;

    vertexBufferMax = vertexBufferSize = 0;
#ifdef DE_DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    inited = true;
}

void Rend_ModelShutdown()
{
    if (!inited) return;

    M_Free(modelPosCoords); modelPosCoords = 0;
    M_Free(modelNormCoords); modelNormCoords = 0;
    M_Free(modelColorCoords); modelColorCoords = 0;
    M_Free(modelTexCoords); modelTexCoords = 0;

    vertexBufferMax = vertexBufferSize = 0;
#ifdef DE_DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    inited = false;
}

bool Rend_ModelExpandVertexBuffers(uint numVertices)
{
    DE_ASSERT(inited);

    LOG_AS("Rend_ModelExpandVertexBuffers");

    if (numVertices <= vertexBufferMax) return true;

    // Sanity check a sane maximum...
    if (numVertices >= RENDER_MAX_MODEL_VERTS)
    {
#ifdef DE_DEBUG
        if (!announcedVertexBufferMaxBreach)
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
    if (vertexBufferMax != vertexBufferSize)
    {
        /// @todo Align access to this memory along a 4-byte boundary?
        modelPosCoords   =  (Vec3f *) M_Realloc(modelPosCoords,   sizeof(*modelPosCoords)   * vertexBufferMax);
        modelNormCoords  =  (Vec3f *) M_Realloc(modelNormCoords,  sizeof(*modelNormCoords)  * vertexBufferMax);
        modelColorCoords = (Vec4ub *) M_Realloc(modelColorCoords, sizeof(*modelColorCoords) * vertexBufferMax);
        modelTexCoords   =  (Vec2f *) M_Realloc(modelTexCoords,   sizeof(*modelTexCoords)   * vertexBufferMax);

        vertexBufferSize = vertexBufferMax;
    }

    // Is the buffer large enough?
    return vertexBufferSize >= numVertices;
}

static void disableArrays(int vertices, int colors, int coords)
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    if (vertices)
    {
        arrays[AR_VERTEX].enabled = false;
    }

    if (colors)
    {
        arrays[AR_COLOR].enabled = false;
    }

    for (int i = 0; i < MAX_TEX_UNITS; ++i)
    {
        if (coords & (1 << i))
        {
            arrays[AR_TEXCOORD0 + i].enabled = false;
        }
    }

    DE_ASSERT(!Sys_GLCheckError());
}

static inline void enableTexUnit(int id)
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Enable(DGL_TEXTURE0 + id);
}

static inline void disableTexUnit(int id)
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Disable(DGL_TEXTURE0 + id);

    // Implicit disabling of texcoord array.
    disableArrays(0, 0, 1 << id);
}

/**
 * The first selected unit is active after this call.
 */
static void selectTexUnits(int count)
{
    if (count < MAX_TEX_UNITS)
    {
        for (int i = MAX_TEX_UNITS - 1; i >= count; i--)
        {
            disableTexUnit(i);
        }
    }

    // Enable the selected units.
    for (int i = count - 1; i >= 0; i--)
    {
        if (i >= MAX_TEX_UNITS) continue;
        enableTexUnit(i);
    }
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
static void configureArrays(void *vertices, void *colors, int numCoords = 0,
                            void **coords = 0)
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    if (vertices)
    {
        arrays[AR_VERTEX].enabled = true;
        arrays[AR_VERTEX].data = vertices;
    }

    if (colors)
    {
        arrays[AR_COLOR].enabled = true;
        arrays[AR_COLOR].data = colors;
    }

    for (int i = 0; i < numCoords && i < MAX_TEX_UNITS; ++i)
    {
        if (coords[i])
        {
            arrays[AR_TEXCOORD0 + i].enabled = true;
            arrays[AR_TEXCOORD0 + i].data = coords[i];
        }
    }

    DE_ASSERT(!Sys_GLCheckError());
}

static void drawArrayElement(int index)
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    for (int i = 0; i < MAX_TEX_UNITS; ++i)
    {
        if (arrays[AR_TEXCOORD0 + i].enabled)
        {
            const Vec2f &texCoord = reinterpret_cast<const Vec2f *>(arrays[AR_TEXCOORD0 + i].data)[index];
            DGL_TexCoord2fv(byte(i), texCoord.constPtr());
        }
    }

    if (arrays[AR_COLOR].enabled)
    {
        const Vec4ub &colorCoord = reinterpret_cast<const Vec4ub *>(arrays[AR_COLOR].data)[index];
        DGL_Color4ubv(colorCoord.constPtr());
    }

    if (arrays[AR_VERTEX].enabled)
    {
        const Vec3f &posCoord = reinterpret_cast<const Vec3f *>(arrays[AR_VERTEX].data)[index];
        DGL_Vertex3fv(posCoord.constPtr());
    }
}

/**
 * Return a pointer to the visible model frame.
 */
static FrameModelFrame &visibleModelFrame(FrameModelDef &modef, int subnumber, int mobjId)
{
    if (subnumber >= int(modef.subCount()))
    {
        throw Error("Rend_DrawModel.visibleFrame",
                    stringf("Model has %i submodels, but submodel #%i was requested",
                            modef.subCount(),
                            subnumber));
    }
    const SubmodelDef &sub = modef.subModelDef(subnumber);

    int curFrame = sub.frame;
    if (modef.flags & MFF_IDFRAME)
    {
        curFrame += mobjId % sub.frameRange;
    }

    return App_Resources().model(sub.modelId).frame(curFrame);
}

/**
 * Render a set of 3D model primitives using the given data.
 */
static void drawPrimitives(rendcmd_t mode,
                           const FrameModel::Primitives &primitives,
                           Vec3f *posCoords,
                           Vec4ub *colorCoords,
                           Vec2f *texCoords = 0)
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // Disable all vertex arrays.
    disableArrays(true, true, DDMAXINT);

    // Load the vertex array.
    void *coords[2];
    switch (mode)
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

    const FrameModel::Primitive::Element *firstElem = nullptr;
    bool joining = false;

    auto submitElement = [mode, &firstElem] (const FrameModel::Primitive::Element &elem)
    {
        if (!firstElem)
        {
            firstElem = &elem;
        }
        if (mode != RC_OTHER_COORDS)
        {
            DGL_TexCoord2fv(0, elem.texCoord.constPtr());
        }
        drawArrayElement(elem.index);
    };

    int lastLength = 0;

    // Combine all triangle strips and fans into one big strip. Fans are converted
    // to strips. When joining strips, winding is retained so that each sub-strip
    // begins with the same winding.
    
    DGL_Begin(DGL_TRIANGLE_STRIP);
    for (const FrameModel::Primitive &prim : primitives)
    {        
        joining = false;
        if (lastLength > 0)
        {
            // Disconnect strip.
            DGL_Vertex3fv(nullptr);
            if (lastLength & 1) DGL_Vertex3fv(nullptr); // Retain the winding.
            joining = true;
        }
        firstElem = nullptr;

        if (!prim.triFan)
        {
            lastLength = prim.elements.size();
            for (const FrameModel::Primitive::Element &elem : prim.elements)
            {
                submitElement(elem);
                if (joining)
                {
                    DGL_Vertex3fv(nullptr);
                    joining = false;
                }
            }
        }
        else
        {
            lastLength = 2; // just make it even, so it doesn't affect winding (see above)
            for (duint i = 1; i < prim.elements.size(); ++i)
            {
                submitElement(prim.elements.at(0));
                if (joining)
                {
                    DGL_Vertex3fv(nullptr);
                    joining = false;
                }
                submitElement(prim.elements.at(i));
            }
        }
    }
    DGL_End();
}

/**
 * Interpolate linearly between two sets of vertices.
 */
static void Mod_LerpVertices(float inter, int count, const FrameModelFrame &from,
    const FrameModelFrame &to, Vec3f *posOut, Vec3f *normOut)
{
    DE_ASSERT(&from.model == &to.model); // sanity check.
    DE_ASSERT(!activeLod || &activeLod->model == &from.model); // sanity check.
    DE_ASSERT(from.vertices.count() == to.vertices.count()); // sanity check.

    FrameModelFrame::VertexBuf::const_iterator startIt = from.vertices.begin();
    FrameModelFrame::VertexBuf::const_iterator endIt   = to.vertices.begin();

    if (&from == &to || de::fequal(inter, 0))
    {
        for (int i = 0; i < count; ++i, startIt++, posOut++, normOut++)
        {
            if (!activeLod || activeLod->hasVertex(i))
            {
                *posOut  = startIt->pos;
                *normOut = startIt->norm;
            }
        }
    }
    else
    {
        for (int i = 0; i < count; ++i, startIt++, endIt++, posOut++, normOut++)
        {
            if (!activeLod || activeLod->hasVertex(i))
            {
                *posOut  = de::lerp(startIt->pos,  endIt->pos,  inter);
                *normOut = de::lerp(startIt->norm, endIt->norm, inter);
            }
        }
    }
}

static void Mod_MirrorCoords(dint count, Vec3f *coords, dint axis)
{
    DE_ASSERT(coords);
    for (; count-- > 0; coords++)
    {
        (*coords)[axis] = -(*coords)[axis];
    }
}

/**
 * Rotate a VectorLight direction vector from world space to model space.
 *
 * @param vlight  Light to process.
 * @param yaw     Yaw rotation angle.
 * @param pitch   Pitch rotation angle.
 * @param invert  @c true= flip light normal (for use with inverted models).
 *
 * @todo Construct a rotation matrix once and use it for all lights.
 */
static Vec3f rotateLightVector(const VectorLightData &vlight, dfloat yaw, dfloat pitch,
    bool invert = false)
{
    dfloat rotated[3]; vlight.direction.decompose(rotated);
    M_RotateVector(rotated, yaw, pitch);

    // Quick hack: Flip light normal if model inverted.
    if (invert)
    {
        rotated[0] = -rotated[0];
        rotated[1] = -rotated[1];
    }

    return Vec3f(rotated);
}

/**
 * Calculate vertex lighting.
 */
static void Mod_VertexColors(Vec4ub *out, dint count, const Vec3f *normCoords,
    duint lightListIdx, duint maxLights, const Vec4f &ambient, bool invert,
    dfloat rotateYaw, dfloat rotatePitch)
{
    Vec4f const saturated(1, 1, 1, 1);

    for (dint i = 0; i < count; ++i, out++, normCoords++)
    {
        if (activeLod && !activeLod->hasVertex(i))
            continue;

        const Vec3f &normal = *normCoords;

        // Accumulate contributions from all affecting lights.
        dint numProcessed = 0;
        Vec3f accum[2];  // Begin with total darkness [color, extra].
        ClientApp::render().forAllVectorLights(lightListIdx, [&maxLights, &invert, &rotateYaw
                                                      , &rotatePitch, &normal
                                                      , &accum, &numProcessed] (const VectorLightData &vlight)
        {
            numProcessed += 1;

            // We must transform the light vector to model space.
            Vec3f const lightDirection
                    = rotateLightVector(vlight, rotateYaw, rotatePitch, invert);

            dfloat strength = lightDirection.dot(normal)
                            + vlight.offset;  // Shift a bit towards the light.

            // Ability to both light and shade.
            if (strength > 0) strength *= vlight.lightSide;
            else             strength *= vlight.darkSide;

            accum[vlight.affectedByAmbient? 0 : 1]
                += vlight.color * de::clamp(-1.f, strength, 1.f);

            // Time to stop?
            return (maxLights && duint(numProcessed) == maxLights);
        });

        // Check for ambient and convert to ubyte.
        Vec4f color(accum[0].max(ambient) + accum[1], ambient[3]);

        *out = (color.min(saturated) * 255).toVec4ub();
    }
}

/**
 * Set all the colors in the array to bright white.
 */
static void Mod_FullBrightVertexColors(dint count, Vec4ub *colorCoords, dfloat alpha)
{
    DE_ASSERT(colorCoords);
    for (; count-- > 0; colorCoords++)
    {
        *colorCoords = Vec4ub(255, 255, 255, 255 * alpha);
    }
}

/**
 * Set all the colors into the array to the same values.
 */
static void Mod_FixedVertexColors(dint count, Vec4ub *colorCoords, const Vec4ub &color)
{
    DE_ASSERT(colorCoords);
    for (; count-- > 0; colorCoords++)
    {
        *colorCoords = color;
    }
}

/**
 * Calculate cylindrically mapped, shiny texture coordinates.
 */
static void Mod_ShinyCoords(Vec2f *out, int count, const Vec3f *normCoords,
    float normYaw, float normPitch, float shinyAng, float shinyPnt, float reactSpeed)
{
    for (int i = 0; i < count; ++i, out++, normCoords++)
    {
        if (activeLod && !activeLod->hasVertex(i))
            continue;

        float rotatedNormal[3] = { normCoords->x, normCoords->y, normCoords->z };

        // Rotate the normal vector so that it approximates the
        // model's orientation compared to the viewer.
        M_RotateVector(rotatedNormal,
                       (shinyPnt + normYaw) * 360 * reactSpeed,
                       (shinyAng + normPitch - .5f) * 180 * reactSpeed);

        *out = Vec2f(rotatedNormal[0] + 1, rotatedNormal[2]);
    }
}

static int chooseSelSkin(FrameModelDef &mf, int submodel, int selector)
{
    if (mf.def.hasSub(submodel))
    {
        Record &subDef = mf.def.sub(submodel);

        int i = (selector >> DDMOBJ_SELECTOR_SHIFT) & subDef.geti("selSkinMask");
        int c = subDef.geti("selSkinShift");

        if (c > 0) i >>= c;
        else       i <<= -c;

        if (i > 7) i = 7; // Maximum number of skins for selskin.
        if (i < 0) i = 0; // Improbable (impossible?), but doesn't hurt.

        return subDef.geta("selSkins")[i].asInt();
    }
    return 0;
}

static int chooseSkin(FrameModelDef &mf, int submodel, int id, int selector, int tmap)
{
    if (submodel >= int(mf.subCount()))
    {
        return 0;
    }

    SubmodelDef &smf = mf.subModelDef(submodel);
    FrameModel &mdl = App_Resources().model(smf.modelId);
    int skin = smf.skin;

    // Selskin overrides the skin range.
    if (smf.testFlag(MFF_SELSKIN))
    {
        skin = chooseSelSkin(mf, submodel, selector);
    }

    // Is there a skin range for this frame?
    // (During model setup skintics and skinrange are set to >0.)
    if (smf.skinRange > 1)
    {
        // What rule to use for determining the skin?
        int offset;
        if (smf.testFlag(MFF_IDSKIN))
        {
            offset = id;
        }
        else
        {
            offset = SECONDS_TO_TICKS(App_World().time()) / mf.skinTics;
        }

        skin += offset % smf.skinRange;
    }

    // Need translation?
    if (smf.testFlag(MFF_SKINTRANS))
    {
        skin = tmap;
    }

    if (skin < 0 || skin >= mdl.skinCount())
    {
        skin = 0;
    }

    return skin;
}

static inline const MaterialVariantSpec &modelSkinMaterialSpec()
{
    return ClientApp::resources().materialSpec(ModelSkinContext, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                                 1, -2, -1, true, true, false, false);
}

static void drawSubmodel(uint number, const vissprite_t &spr)
{
    const drawmodelparams_t &parm = *VS_MODEL(&spr);
    const int zSign = (spr.pose.mirrored? -1 : 1);
    FrameModelDef *mf = parm.mf, *mfNext = parm.nextMF;
    const SubmodelDef &smf = mf->subModelDef(number);

    FrameModel &mdl = App_Resources().model(smf.modelId);

    // Do not bother with infinitely small models...
    if (mf->scale == Vec3f(0.0f))
        return;

    float alpha = spr.light.ambientColor[CA];

    // Is the submodel-defined alpha multiplier in effect?
    // With df_brightshadow2, the alpha multiplier will be applied anyway.
    if (smf.testFlag(MFF_BRIGHTSHADOW2) ||
       !(parm.flags & (DDMF_BRIGHTSHADOW|DDMF_SHADOW|DDMF_ALTSHADOW)))
    {
        alpha *= smf.alpha / 255.f;
    }

    // Would this be visible?
    if (alpha <= 0) return;

    blendmode_t blending = smf.blendMode;
    // Is the submodel-defined blend mode in effect?
    if (parm.flags & DDMF_BRIGHTSHADOW)
    {
        blending = BM_ADD;
    }

    int useSkin = chooseSkin(*mf, number, parm.id, parm.selector, parm.tmap);

    // Scale interpos. Intermark becomes zero and endmark becomes one.
    // (Full sub-interpolation!) But only do it for the standard
    // interrange. If a custom one is defined, don't touch interpos.
    float endPos = 0;
    float inter = parm.inter;
    if ((mf->interRange[0] == 0 && mf->interRange[1] == 1) || smf.testFlag(MFF_WORLD_TIME_ANIM))
    {
        endPos = (mf->interNext ? mf->interNext->interMark : 1);
        inter = (parm.inter - mf->interMark) / (endPos - mf->interMark);
    }

    FrameModelFrame *frame = &visibleModelFrame(*mf, number, parm.id);
    FrameModelFrame *nextFrame = 0;
    // Do we have a sky/particle model here?
    if (parm.alwaysInterpolate)
    {
        // Always interpolate, if there's animation.
        // Used with sky and particle models.
        nextFrame = &mdl.frame((smf.frame + 1) % mdl.frameCount());
        mfNext = mf;
    }
    else
    {
        // Check for possible interpolation.
        if (frameInter && mfNext && !smf.testFlag(MFF_DONT_INTERPOLATE))
        {
            if (mfNext->hasSub(number) && mfNext->subModelId(number) == smf.modelId)
            {
                nextFrame = &visibleModelFrame(*mfNext, number, parm.id);
            }
        }
    }

    // Clamp interpolation.
    inter = de::clamp(0.f, inter, 1.f);

    if (!nextFrame)
    {
        // If not interpolating, use the same frame as interpolation target.
        // The lerp routines will recognize this special case.
        nextFrame = frame;
        mfNext = mf;
    }

    // Determine the total number of vertices we have.
    int numVerts = mdl.vertexCount();

    // Ensure our vertex render buffers can accommodate this.
    if (!resizeVertexBuffer(numVerts))
    {
        // No can do, we aint got the power!
        return;
    }

    // Setup transformation.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    // Model space => World space
    DGL_Translatef(spr.pose.origin[VX] + spr.pose.srvo[VX] +
                   de::lerp(mf->offset.x, mfNext->offset.x, inter),
                   spr.pose.origin[VZ] + spr.pose.srvo[VZ] +
                   de::lerp(mf->offset.y, mfNext->offset.y, inter),
                   spr.pose.origin[VY] + spr.pose.srvo[VY] + zSign *
                   de::lerp(mf->offset.z, mfNext->offset.z, inter));

    if (spr.pose.extraYawAngle || spr.pose.extraPitchAngle)
    {
        // Sky models have an extra rotation.
        DGL_Scalef(1, 200 / 240.0f, 1);
        DGL_Rotatef(spr.pose.extraYawAngle, 1, 0, 0);
        DGL_Rotatef(spr.pose.extraPitchAngle, 0, 0, 1);
        DGL_Scalef(1, 240 / 200.0f, 1);
    }

    // Model rotation.
    DGL_Rotatef(spr.pose.viewAligned? spr.pose.yawAngleOffset   : spr.pose.yaw,   0, 1, 0);
    DGL_Rotatef(spr.pose.viewAligned? spr.pose.pitchAngleOffset : spr.pose.pitch, 0, 0, 1);

    // Scaling and model space offset.
    DGL_Scalef(de::lerp(mf->scale.x, mfNext->scale.x, inter),
             de::lerp(mf->scale.y, mfNext->scale.y, inter),
             de::lerp(mf->scale.z, mfNext->scale.z, inter));
    if (spr.pose.extraScale)
    {
        // Particle models have an extra scale.
        DGL_Scalef(spr.pose.extraScale, spr.pose.extraScale, spr.pose.extraScale);
    }
    DGL_Translatef(smf.offset.x, smf.offset.y, smf.offset.z);

    // Determine the suitable LOD.
    if (mdl.lodCount() > 1 && rend_model_lod != 0)
    {
        float lodFactor = rend_model_lod * DE_GAMEVIEW_WIDTH / 640.0f / (Rend_FieldOfView() / 90.0f);
        if (!de::fequal(lodFactor, 0))
        {
            lodFactor = 1 / lodFactor;
        }

        // Determine the LOD we will be using.
        activeLod = &mdl.lod(de::clamp<int>(0, lodFactor * spr.pose.distance, mdl.lodCount() - 1));
    }
    else
    {
        activeLod = 0;
    }

    // Interpolate vertices and normals.
    Mod_LerpVertices(inter, numVerts, *frame, *nextFrame,
                     modelPosCoords, modelNormCoords);

    if (zSign < 0)
    {
        Mod_MirrorCoords(numVerts, modelPosCoords, 2);
        Mod_MirrorCoords(numVerts, modelNormCoords, 1);
    }

    // Coordinates to the center of the model (game coords).
    modelCenter = Vec3f(spr.pose.origin[VX], spr.pose.origin[VY], spr.pose.midZ())
            + Vec3d(spr.pose.srvo) + Vec3f(mf->offset.x, mf->offset.z, mf->offset.y);

    // Calculate lighting.
    Vec4f ambient;
    if (smf.testFlag(MFF_FULLBRIGHT) && !smf.testFlag(MFF_DIM))
    {
        // Submodel-specific lighting override.
        ambient = Vec4f(1);
        Mod_FullBrightVertexColors(numVerts, modelColorCoords, alpha);
    }
    else if (!spr.light.vLightListIdx)
    {
        // Lit uniformly.
        ambient = Vec4f(spr.light.ambientColor, alpha);
        Mod_FixedVertexColors(numVerts, modelColorCoords,
                              (ambient * 255).toVec4ub());
    }
    else
    {
        // Lit normally.
        ambient = Vec4f(spr.light.ambientColor, alpha);

        Mod_VertexColors(modelColorCoords, numVerts,
                         modelNormCoords, spr.light.vLightListIdx, modelLight + 1,
                         ambient, (mf->scale[VY] < 0), -spr.pose.yaw, -spr.pose.pitch);
    }

    TextureVariant *shinyTexture = 0;
    float shininess = 0;
    if (mf->def.hasSub(number))
    {
        shininess = float(de::clamp(0.0, mf->def.sub(number).getd("shiny") * modelShinyFactor, 1.0));
        // Ensure we've prepared the shiny skin.
        if (shininess > 0)
        {
            if (ClientTexture *tex = static_cast<ClientTexture *>(mf->subModelDef(number).shinySkin))
            {
                shinyTexture = tex->prepareVariant(Rend_ModelShinyTextureSpec());
            }
            else
            {
                shininess = 0;
            }
        }
    }

    Vec4f color;
    if (shininess > 0)
    {
        // Calculate shiny coordinates.
        Vec3f shinyColor = mf->def.sub(number).get("shinyColor");

        // With psprites, add the view angle/pitch.
        float offset = parm.shineYawOffset;

        // Calculate normalized (0,1) model yaw and pitch.
        float normYaw = M_CycleIntoRange(((spr.pose.viewAligned? spr.pose.yawAngleOffset
                                                               : spr.pose.yaw) + offset) / 360, 1);

        offset = parm.shinePitchOffset;

        float normPitch = M_CycleIntoRange(((spr.pose.viewAligned? spr.pose.pitchAngleOffset
                                                                 : spr.pose.pitch) + offset) / 360, 1);

        float shinyAng = 0;
        float shinyPnt = 0;
        if (parm.shinepspriteCoordSpace)
        {
            // This is a hack to accommodate the psprite coordinate space.
            shinyPnt = 0.5;
        }
        else
        {
            Vec3f delta = modelCenter;

            if (!parm.shineTranslateWithViewerPos)
            {
                delta -= Rend_EyeOrigin().xzy();
            }

            shinyAng = QATAN2(delta.z, M_ApproxDistancef(delta.x, delta.y)) / PI + 0.5f; // shinyAng is [0,1]

            shinyPnt = QATAN2(delta.y, delta.x) / (2 * PI);
        }

        Mod_ShinyCoords(modelTexCoords, numVerts,
                        modelNormCoords, normYaw, normPitch, shinyAng, shinyPnt,
                        mf->def.sub(number).getf("shinyReact"));

        // Shiny color.
        if (smf.testFlag(MFF_SHINY_LIT))
        {
            color = Vec4f(ambient * shinyColor, shininess);
        }
        else
        {
            color = Vec4f(shinyColor, shininess);
        }
    }

    TextureVariant *skinTexture = 0;
    if (renderTextures == 2)
    {
        // For lighting debug, render all surfaces using the gray texture.
        MaterialAnimator &matAnimator = ClientMaterial::find(res::Uri("System", Path("gray")))
                .getAnimator(modelSkinMaterialSpec());

        // Ensure we've up to date info about the material.
        matAnimator.prepare();

        skinTexture = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    }
    else
    {
        skinTexture = 0;
        if (ClientTexture *tex = static_cast<ClientTexture *>(mdl.skin(useSkin).texture))
        {
            skinTexture = tex->prepareVariant(Rend_ModelDiffuseTextureSpec(mdl.flags().testFlag(FrameModel::NoTextureCompression)));
        }
    }

    // If we mirror the model, triangles have a different orientation.
    if (zSign < 0)
    {
        DGL_Flush();
        glFrontFace(GL_CCW);
    }

    // Twosided models won't use backface culling.
    if (smf.testFlag(MFF_TWO_SIDED))
    {
        DGL_CullFace(DGL_NONE);
    }
    DGL_Enable(DGL_TEXTURE_2D);

    const FrameModel::Primitives &primitives =
        activeLod? activeLod->primitives : mdl.primitives();

    // Render using multiple passes?
    if (shininess <= 0 || alpha < 1 ||
        blending != BM_NORMAL || !smf.testFlag(MFF_SHINY_SPECULAR))
    {
        // The first pass can be skipped if it won't be visible.
        if (shininess < 1 || smf.testFlag(MFF_SHINY_SPECULAR))
        {
            selectTexUnits(1);
            GL_BlendMode(blending);
            GL_BindTexture(renderTextures? skinTexture : 0);

            drawPrimitives(RC_COMMAND_COORDS, primitives,
                           modelPosCoords, modelColorCoords);
        }

        if (shininess > 0)
        {
            DGL_DepthFunc(DGL_LEQUAL);

            // Set blending mode, two choices: reflected and specular.
            if (smf.testFlag(MFF_SHINY_SPECULAR))
                GL_BlendMode(BM_ADD);
            else
                GL_BlendMode(BM_NORMAL);

            // Shiny color.
            Mod_FixedVertexColors(numVerts, modelColorCoords,
                                  (color * 255).toVec4ub());

            // We'll use multitexturing to clear out empty spots in
            // the primary texture.
            selectTexUnits(2);
            DGL_ModulateTexture(11);

            DGL_SetInteger(DGL_ACTIVE_TEXTURE, 1);
            GL_BindTexture(renderTextures? shinyTexture : 0);

            DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);
            GL_BindTexture(renderTextures? skinTexture : 0);

            drawPrimitives(RC_BOTH_COORDS, primitives,
                           modelPosCoords, modelColorCoords, modelTexCoords);

            selectTexUnits(1);
            DGL_ModulateTexture(1);
        }
    }
    else
    {
        // A special case: specular shininess on an opaque object.
        // Multitextured shininess with the normal blending.
        GL_BlendMode(blending);
        selectTexUnits(2);

        // Tex1*Color + Tex2RGB*ConstRGB
        DGL_ModulateTexture(10);

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, 1);
        GL_BindTexture(renderTextures? shinyTexture : 0);

        // Multiply by shininess.
        DGL_SetModulationColor({color.x * color.w, color.y * color.w, color.z * color.w, color.w});

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);
        GL_BindTexture(renderTextures? skinTexture : 0);

        drawPrimitives(RC_BOTH_COORDS, primitives,
                       modelPosCoords, modelColorCoords, modelTexCoords);

        selectTexUnits(1);
        DGL_ModulateTexture(1);
    }
    
    DGL_Flush();

    // We're done!
    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    // Normally culling is always enabled.
    if (smf.testFlag(MFF_TWO_SIDED))
    {
        DGL_CullFace(DGL_BACK);
    }

    if (zSign < 0)
    {
        DGL_Flush();
        glFrontFace(GL_CW);
    }
    DGL_DepthFunc(DGL_LESS);

    GL_BlendMode(BM_NORMAL);
}

void Rend_DrawModel(const vissprite_t &spr)
{
    const drawmodelparams_t &parm = *VS_MODEL(&spr);

    DE_ASSERT(inited);
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    if (!parm.mf) return;

    DE_ASSERT(parm.mf->select == (parm.selector & DDMOBJ_SELECTOR_MASK))

    // Render all the submodels of this model.
    for (uint i = 0; i < parm.mf->subCount(); ++i)
    {
        if (parm.mf->subModelId(i))
        {
            bool disableZ = (parm.mf->flags & MFF_DISABLE_Z_WRITE ||
                             parm.mf->testSubFlag(i, MFF_DISABLE_Z_WRITE));

            if (disableZ)
            {
                DGL_Disable(DGL_DEPTH_WRITE);
            }

            drawSubmodel(i, spr);

            if (disableZ)
            {
                DGL_Enable(DGL_DEPTH_WRITE);
            }
        }
    }

    if (devMobjVLights && spr.light.vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        DGL_PushState();
        DGL_Disable(DGL_DEPTH_TEST);
        DGL_CullFace(DGL_NONE);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        DGL_Translatef(spr.pose.origin[0], spr.pose.origin[2], spr.pose.origin[1]);

        const coord_t distFromViewer = de::abs(spr.pose.distance);
        ClientApp::render().forAllVectorLights(spr.light.vLightListIdx, [&distFromViewer] (const VectorLightData &vlight)
        {
            if (distFromViewer < 1600 - 8)
            {
                Rend_DrawVectorLight(vlight, 1 - distFromViewer / 1600);
            }
            return LoopContinue;
        });

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();

        DGL_PopState();
    }
}

const TextureVariantSpec &Rend_ModelDiffuseTextureSpec(bool noCompression)
{
    return ClientApp::resources().textureSpec(TC_MODELSKIN_DIFFUSE,
        (noCompression? TSF_NO_COMPRESSION : 0), 0, 0, 0, GL_REPEAT, GL_REPEAT,
        1, -2, -1, true, true, false, false);
}

const TextureVariantSpec &Rend_ModelShinyTextureSpec()
{
    return ClientApp::resources().textureSpec(TC_MODELSKIN_REFLECTION,
        TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, -2, -1, false,
        false, false, false);
}
