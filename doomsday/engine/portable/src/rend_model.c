/**\file rend_model.c
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
 * @note Light vectors and triangle normals are in an entirely independent,
 *       right-handed coordinate system.
 *
 * There is some more confusion with Y and Z axes as the game uses Z as the
 * vertical axis and the rendering code and model definitions use the Y axis.
 */

#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "net_main.h" // for gametic
#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"

#define DOTPROD(a, b)       (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])
#define QATAN2(y,x)         qatan2(y,x)
#define QASIN(x)            asin(x) // \fixme Precalculate arcsin.

#define MAX_ARRAYS  (2 + MAX_TEX_UNITS)

typedef enum rendcmd_e {
    RC_COMMAND_COORDS,
    RC_OTHER_COORDS,
    RC_BOTH_COORDS
} rendcmd_t;

typedef struct array_s {
    boolean enabled;
    void* data;
} array_t;

int modelLight = 4;
int frameInter = true;
int mirrorHudModels = false;
int modelShinyMultitex = true;
float modelShinyFactor = 1.0f;
int modelTriCount;
float rend_model_lod = 256;

static boolean inited = false;

static array_t arrays[MAX_ARRAYS];

// The global vertex render buffer.
static dgl_vertex_t* modelVertices;
static dgl_vertex_t* modelNormals;
static dgl_color_t* modelColors;
static dgl_texcoord_t* modelTexCoords;

// Global variables for ease of use. (Egads!)
static float modelCenter[3];
static int activeLod;
static char* vertexUsage;

static uint vertexBufferMax; ///< Maximum number of vertices we'll be required to render per submodel.
static uint vertexBufferSize; ///< Current number of vertices supported by the render buffer.
#if _DEBUG
static boolean announcedVertexBufferMaxBreach; ///< @c true if an attempt has been made to expand beyond our capability.
#endif

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: Model Renderer is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

void Rend_ModelRegister(void)
{
    C_VAR_BYTE ("rend-model", &useModels, 0, 0, 1);
    C_VAR_INT  ("rend-model-lights", &modelLight, 0, 0, 10);
    C_VAR_INT  ("rend-model-inter", &frameInter, 0, 0, 1);
    C_VAR_FLOAT("rend-model-aspect", &rModelAspectMod, CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT  ("rend-model-distance", &maxModelDistance, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE ("rend-model-precache", &precacheSkins, 0, 0, 1);
    C_VAR_FLOAT("rend-model-lod", &rend_model_lod, CVF_NO_MAX, 0, 0);
    C_VAR_INT  ("rend-model-mirror-hud", &mirrorHudModels, 0, 0, 1);
    C_VAR_FLOAT("rend-model-spin-speed", &modelSpinSpeed, CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT  ("rend-model-shiny-multitex", &modelShinyMultitex, 0, 0, 1);
    C_VAR_FLOAT("rend-model-shiny-strength", &modelShinyFactor, 0, 0, 10);
}

boolean Rend_ModelExpandVertexBuffers(uint numVertices)
{
    errorIfNotInited("Rend_ModelExpandVertexBuffers");

    if(numVertices <= vertexBufferMax) return true;

    // Sanity check a sane maximum...
    if(numVertices >= RENDER_MAX_MODEL_VERTS)
    {
#if _DEBUG
        if(!announcedVertexBufferMaxBreach)
        {
            Con_Message("Warning: Rend_ModelExpandVertexBuffers: Attempted to expand to %u vertices (max %u), ignoring...\n", numVertices, RENDER_MAX_MODEL_VERTS);
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
static boolean Mod_ExpandVertexBuffer(uint numVertices)
{
    // Mark the vertex buffer if a resize is necessary.
    Rend_ModelExpandVertexBuffers(numVertices);

    // Do we need to resize the buffers?
    if(vertexBufferMax != vertexBufferSize)
    {
        dgl_vertex_t* newVertices;
        dgl_vertex_t* newNormals;
        dgl_color_t* newColors;
        dgl_texcoord_t* newTexCoords;

        /// @todo Align access to this memory along a 4-byte boundary?
        newVertices = calloc(1, sizeof(*newVertices) * vertexBufferMax);
        if(!newVertices) return false;

        newNormals = calloc(1, sizeof(*newNormals) * vertexBufferMax);
        if(!newNormals) return false;

        newColors = calloc(1, sizeof(*newColors) * vertexBufferMax);
        if(!newColors) return false;

        newTexCoords = calloc(1, sizeof(*newTexCoords) * vertexBufferMax);
        if(!newTexCoords) return false;

        // Swap over the buffers.
        if(modelVertices) free(modelVertices);
        modelVertices = newVertices;

        if(modelNormals) free(modelNormals);
        modelNormals = newNormals;

        if(modelColors) free(modelColors);
        modelColors = newColors;

        if(modelTexCoords) free(modelTexCoords);
        modelTexCoords = newTexCoords;

        vertexBufferSize = vertexBufferMax;
    }

    // Is the buffer large enough?
    return vertexBufferSize >= numVertices;
}

static void Mod_InitArrays(void)
{
    if(!GL_state.features.elementArrays) return;
    memset(arrays, 0, sizeof(arrays));
}

static void Mod_EnableArrays(int vertices, int colors, int coords)
{
    int i;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_VERTEX].enabled = true;
        else
            glEnableClientState(GL_VERTEX_ARRAY);
    }

    if(colors)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_COLOR].enabled = true;
        else
            glEnableClientState(GL_COLOR_ARRAY);
    }

    for(i = 0; i < numTexUnits; ++i)
    {
        if(coords & (1 << i))
        {
            if(!GL_state.features.elementArrays)
            {
                arrays[AR_TEXCOORD0 + i].enabled = true;
            }
            else
            {
                glClientActiveTexture(GL_TEXTURE0 + i);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            }
        }
    }

    assert(!Sys_GLCheckError());
}

static void Mod_DisableArrays(int vertices, int colors, int coords)
{
    int i;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_VERTEX].enabled = false;
        else
            glDisableClientState(GL_VERTEX_ARRAY);
    }

    if(colors)
    {
        if(!GL_state.features.elementArrays)
            arrays[AR_COLOR].enabled = false;
        else
            glDisableClientState(GL_COLOR_ARRAY);
    }

    for(i = 0; i < numTexUnits; ++i)
    {
        if(coords & (1 << i))
        {
            if(!GL_state.features.elementArrays)
            {
                arrays[AR_TEXCOORD0 + i].enabled = false;
            }
            else
            {
                glClientActiveTexture(GL_TEXTURE0 + i);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, 0, NULL);
            }
        }
    }

    assert(!Sys_GLCheckError());
}

static __inline void enableTexUnit(byte id)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glActiveTexture(GL_TEXTURE0 + id);
    glEnable(GL_TEXTURE_2D);
}

static __inline void disableTexUnit(byte id)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glActiveTexture(GL_TEXTURE0 + id);
    glDisable(GL_TEXTURE_2D);

    // Implicit disabling of texcoord array.
    if(!GL_state.features.elementArrays)
    {
        Mod_DisableArrays(0, 0, 1 << id);
    }
}

/**
 * The first selected unit is active after this call.
 */
static void Mod_SelectTexUnits(int count)
{
    int i;
    for(i = numTexUnits - 1; i >= count; i--)
    {
        disableTexUnit(i);
    }

    // Enable the selected units.
    for(i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits) continue;
        enableTexUnit(i);
    }
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
static void Mod_Arrays(void* vertices, void* colors, int numCoords,
    void** coords, int lock)
{
    int i;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(vertices)
    {
        if(!GL_state.features.elementArrays)
        {
            arrays[AR_VERTEX].enabled = true;
            arrays[AR_VERTEX].data = vertices;
        }
        else
        {
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, 16, vertices);
        }
    }

    if(colors)
    {
        if(!GL_state.features.elementArrays)
        {
            arrays[AR_COLOR].enabled = true;
            arrays[AR_COLOR].data = colors;
        }
        else
        {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
        }
    }

    for(i = 0; i < numCoords && i < MAX_TEX_UNITS; ++i)
    {
        if(coords[i])
        {
            if(!GL_state.features.elementArrays)
            {
                arrays[AR_TEXCOORD0 + i].enabled = true;
                arrays[AR_TEXCOORD0 + i].data = coords[i];
            }
            else
            {
                glClientActiveTexture(GL_TEXTURE0 + i);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, 0, coords[i]);
            }
        }
    }

    if(GL_state.features.elementArrays && lock > 0)
    {
        // 'lock' is the number of vertices to lock.
        glLockArraysEXT(0, lock);
    }

    assert(!Sys_GLCheckError());
}

static void Mod_UnlockArrays(void)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!GL_state.features.elementArrays) return;

    glUnlockArraysEXT();
    assert(!Sys_GLCheckError());
}

static void Mod_ArrayElement(int index)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(GL_state.features.elementArrays)
    {
        glArrayElement(index);
    }
    else
    {
        int i;
        for(i = 0; i < numTexUnits; ++i)
        {
            if(!arrays[AR_TEXCOORD0 + i].enabled)
                continue;
            glMultiTexCoord2fv(GL_TEXTURE0 + i,
                ((dgl_texcoord_t*)arrays[AR_TEXCOORD0 + i].data)[index].st);
        }

        if(arrays[AR_COLOR].enabled)
            glColor4ubv(((dgl_color_t*) arrays[AR_COLOR].data)[index].rgba);

        if(arrays[AR_VERTEX].enabled)
            glVertex3fv(((dgl_vertex_t*) arrays[AR_VERTEX].data)[index].xyz);
    }
}

static void Mod_DrawElements(dglprimtype_t type, int count, const uint* indices)
{
    GLenum primType = (type == DGL_TRIANGLE_FAN ? GL_TRIANGLE_FAN :
                       type == DGL_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : GL_TRIANGLES);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(GL_state.features.elementArrays)
    {
        glDrawElements(primType, count, GL_UNSIGNED_INT, indices);
    }
    else
    {
        int i;

        glBegin(primType);
        for(i = 0; i < count; ++i)
        {
            Mod_ArrayElement(indices[i]);
        }
        glEnd();
    }

    assert(!Sys_GLCheckError());
}

static __inline float qatan2(float y, float x)
{
    float ang = BANG2RAD(bamsAtan2(y * 512, x * 512));
    if(ang > PI) ang -= 2 * (float) PI;
    return ang;
    // This is slightly faster, I believe...
    //return atan2(y, x);
}

/**
 * Linear interpolation between two values.
 */
static __inline float Mod_Lerp(float start, float end, float pos)
{
    return end * pos + start * (1 - pos);
}

/**
 * Return a pointer to the visible model frame.
 */
static model_frame_t* Mod_GetVisibleFrame(modeldef_t* mf, int subnumber, int mobjid)
{
    model_t* mdl = R_ModelForId(mf->sub[subnumber].model);
    int index = mf->sub[subnumber].frame;

    if(mf->flags & MFF_IDFRAME)
    {
        index += mobjid % mf->sub[subnumber].frameRange;
    }
    if(index >= mdl->info.numFrames)
    {
        Con_Error("Mod_GetVisibleFrame: Frame index out of bounds.\n"
                  "  (Model: %s)\n", mdl->fileName);
    }
    return mdl->frames + index;
}

/**
 * Render a set of GL commands using the given data.
 */
static void Mod_RenderCommands(rendcmd_t mode, void* glCommands, /*uint numVertices,*/
    dgl_vertex_t* vertices, dgl_color_t* colors, dgl_texcoord_t* texCoords)
{
    glcommand_vertex_t* v;
    void* coords[2];
    int count;
    byte* pos;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Disable all vertex arrays.
    Mod_DisableArrays(true, true, DDMAXINT);

    // Load the vertex array.
    switch(mode)
    {
    case RC_OTHER_COORDS:
        coords[0] = texCoords;
        Mod_Arrays(vertices, colors, 1, coords, 0);
        break;

    case RC_BOTH_COORDS:
        coords[0] = NULL;
        coords[1] = texCoords;
        Mod_Arrays(vertices, colors, 2, coords, 0);
        break;

    default:
        Mod_Arrays(vertices, colors, 0, NULL, 0 /* numVertices */ );
        break;
    }

    for(pos = glCommands; *pos;)
    {
        count = LONG( *(int *) pos );
        pos += 4;

        // The type of primitive depends on the sign.
        glBegin(count > 0 ? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);
        if(count < 0)
            count = -count;

        // Increment the total model triangle counter.
        modelTriCount += count - 2;

        while(count--)
        {
            v = (glcommand_vertex_t *) pos;
            pos += sizeof(glcommand_vertex_t);

            if(mode != RC_OTHER_COORDS)
            {
                glTexCoord2f(FLOAT(v->s), FLOAT(v->t));
            }

            Mod_ArrayElement(LONG(v->index));
        }

        // The primitive is complete.
        glEnd();
    }
}

/**
 * Interpolate linearly between two sets of vertices.
 */
static void Mod_LerpVertices(float pos, int count, model_vertex_t* start,
    model_vertex_t* end, dgl_vertex_t* out)
{
    float inv;
    int i;

    if(start == end || pos == 0)
    {
        for(i = 0; i < count; ++i, start++, out++)
        {
            out->xyz[0] = start->xyz[0];
            out->xyz[1] = start->xyz[1];
            out->xyz[2] = start->xyz[2];
        }
        return;
    }

    inv = 1 - pos;

    if(vertexUsage)
    {
        for(i = 0; i < count; ++i, start++, end++, out++)
            if(vertexUsage[i] & (1 << activeLod))
            {
                out->xyz[0] = inv * start->xyz[0] + pos * end->xyz[0];
                out->xyz[1] = inv * start->xyz[1] + pos * end->xyz[1];
                out->xyz[2] = inv * start->xyz[2] + pos * end->xyz[2];
            }
    }
    else
    {
        for(i = 0; i < count; ++i, start++, end++, out++)
        {
            out->xyz[0] = inv * start->xyz[0] + pos * end->xyz[0];
            out->xyz[1] = inv * start->xyz[1] + pos * end->xyz[1];
            out->xyz[2] = inv * start->xyz[2] + pos * end->xyz[2];
        }
    }
}

/**
 * Negate all Z coordinates.
 */
static void Mod_MirrorVertices(int count, dgl_vertex_t* v, int axis)
{
    for(; count-- > 0; v++)
        v->xyz[axis] = -v->xyz[axis];
}

typedef struct {
    float color[3], extra[3], rotateYaw, rotatePitch;
    dgl_vertex_t* normal;
    uint processedLights, maxLights;
    boolean invert;
} lightmodelvertexparams_t;

static boolean lightModelVertex(const vlight_t* vlight, void* context)
{
    float* dest, lightVector[3];
    lightmodelvertexparams_t* params = (lightmodelvertexparams_t*) context;
    float dot;

    // Take a copy of the light vector as we intend to change it.
    lightVector[0] = vlight->vector[0];
    lightVector[1] = vlight->vector[1];
    lightVector[2] = vlight->vector[2];

    // We must transform the light vector to model space.
    M_RotateVector(lightVector, params->rotateYaw, params->rotatePitch);

    // Quick hack: Flip light normal if model inverted.
    if(params->invert)
    {
        lightVector[VX] = -lightVector[VX];
        lightVector[VY] = -lightVector[VY];
    }

    dot = DOTPROD(lightVector, params->normal->xyz);
    dot += vlight->offset; // Shift a bit towards the light.

    if(!vlight->affectedByAmbient)
    {
        // Won't be affected by ambient.
        dest = params->extra;
    }
    else
    {
        dest = params->color;
    }

    // Ability to both light and shade.
    if(dot > 0)
    {
        dot *= vlight->lightSide;
    }
    else
    {
        dot *= vlight->darkSide;
    }

    dot = MINMAX_OF(-1, dot, 1);

    dest[CR] += dot * vlight->color[CR];
    dest[CG] += dot * vlight->color[CG];
    dest[CB] += dot * vlight->color[CB];

    params->processedLights++;
    if(params->maxLights && !(params->processedLights < params->maxLights))
        return false; // Stop iteration.

    return true; // Continue iteration.
}

/**
 * Calculate vertex lighting.
 */
static void Mod_VertexColors(int count, dgl_color_t* out, dgl_vertex_t* normal,
    uint vLightListIdx, uint maxLights, float ambient[4], boolean invert,
    float rotateYaw, float rotatePitch)
{
    lightmodelvertexparams_t params;
    int i, k;

    for(i = 0; i < count; ++i, out++, normal++)
    {
        if(vertexUsage && !(vertexUsage[i] & (1 << activeLod)))
            continue;

        // Begin with total darkness.
        params.color[CR] = params.color[CG] = params.color[CB] = 0;
        params.extra[CR] = params.extra[CG] = params.extra[CB] = 0;
        params.processedLights = 0;
        params.maxLights = maxLights;
        params.normal = normal;
        params.invert = invert;
        params.rotateYaw = rotateYaw;
        params.rotatePitch = rotatePitch;

        // Add light from each source.
        VL_ListIterator(vLightListIdx, &params, lightModelVertex);

        // Check for ambient and convert to ubyte.
        for(k = 0; k < 3; ++k)
        {
            if(params.color[k] < ambient[k])
                params.color[k] = ambient[k];
            params.color[k] += params.extra[k];
            params.color[k] = MINMAX_OF(0, params.color[k], 1);

            // This is the final color.
            out->rgba[k] = (byte) (255 * params.color[k]);
        }

        out->rgba[CA] = (byte) (255 * ambient[CA]);
    }
}

/**
 * Set all the colors in the array to bright white.
 */
static void Mod_FullBrightVertexColors(int count, dgl_color_t* colors, float alpha)
{
    for(; count-- > 0; colors++)
    {
        memset(colors->rgba, 255, 3);
        colors->rgba[3] = (byte) (255 * alpha);
    }
}

/**
 * Set all the colors into the array to the same values.
 */
static void Mod_FixedVertexColors(int count, dgl_color_t* colors, float* color)
{
    byte rgba[4];

    rgba[0] = color[0] * 255;
    rgba[1] = color[1] * 255;
    rgba[2] = color[2] * 255;
    rgba[3] = color[3] * 255;
    for(; count-- > 0; colors++)
        memcpy(colors->rgba, rgba, 4);
}

/**
 * Calculate cylindrically mapped, shiny texture coordinates.
 */
static void Mod_ShinyCoords(int count, dgl_texcoord_t* coords, dgl_vertex_t* normals,
    float normYaw, float normPitch, float shinyAng, float shinyPnt, float reactSpeed)
{
    float u, v, rotatedNormal[3];
    int i;

    for(i = 0; i < count; ++i, coords++, normals++)
    {
        if(vertexUsage && !(vertexUsage[i] & (1 << activeLod)))
            continue;

        rotatedNormal[VX] = normals->xyz[VX];
        rotatedNormal[VY] = normals->xyz[VY];
        rotatedNormal[VZ] = normals->xyz[VZ];

        // Rotate the normal vector so that it approximates the
        // model's orientation compared to the viewer.
        M_RotateVector(rotatedNormal,
                       (shinyPnt + normYaw) * 360 * reactSpeed,
                       (shinyAng + normPitch - .5f) * 180 * reactSpeed);

        u = rotatedNormal[VX] + 1;
        v = rotatedNormal[VZ];

        coords->st[0] = u;
        coords->st[1] = v;
    }
}

static int chooseSelSkin(modeldef_t* mf, int submodel, int selector)
{
    int i = (selector >> DDMOBJ_SELECTOR_SHIFT) &
            mf->def->sub[submodel].selSkinBits[0]; // Selskin mask
    int c = mf->def->sub[submodel].selSkinBits[1]; // Selskin shift

    if(c > 0)
        i >>= c;
    else
        i <<= -c;

    if(i > 7)
        i = 7; // Maximum number of skins for selskin.
    if(i < 0)
        i = 0; // Improbable (impossible?), but doesn't hurt.

    return mf->def->sub[submodel].selSkins[i];
}

static int chooseSkin(modeldef_t* mf, int submodel, int id, int selector, int tmap)
{
    submodeldef_t* smf = &mf->sub[submodel];
    model_t* mdl = R_ModelForId(smf->model);
    int skin = smf->skin;

    // Selskin overrides the skin range.
    if(smf->flags & MFF_SELSKIN)
    {
        skin = chooseSelSkin(mf, submodel, selector);
    }

    // Is there a skin range for this frame?
    // (During model setup skintics and skinrange are set to >0.)
    if(smf->skinRange > 1)
    {
        int offset;

        // What rule to use for determining the skin?
        if(smf->flags & MFF_IDSKIN)
        {
            offset = id;
        }
        else
        {
            offset = SECONDS_TO_TICKS(ddMapTime) / mf->skinTics;
        }

        skin += offset % smf->skinRange;
    }

    // Need translation?
    if(smf->flags & MFF_SKINTRANS)
        skin = tmap;

    if(skin < 0 || skin >= mdl->info.numSkins)
        skin = 0;

    return skin;
}

texturevariantspecification_t* Rend_ModelDiffuseTextureSpec(boolean noCompression)
{
    return GL_TextureVariantSpecificationForContext(TC_MODELSKIN_DIFFUSE,
                                                    (noCompression? TSF_NO_COMPRESSION : 0),
                                                    0, 0, 0, GL_REPEAT, GL_REPEAT, 1, -2, -1,
                                                    true, true, false, false);
}

texturevariantspecification_t* Rend_ModelShinyTextureSpec(void)
{
    return GL_TextureVariantSpecificationForContext(TC_MODELSKIN_REFLECTION,
                                                    TSF_NO_COMPRESSION, 0, 0, 0,
                                                    GL_REPEAT, GL_REPEAT, 1, -2, -1,
                                                    false, false, false, false);
}

/**
 * Render a submodel from the vissprite.
 */
static void Mod_RenderSubModel(uint number, const rendmodelparams_t* params)
{
    modeldef_t* mf = params->mf, *mfNext = params->nextMF;
    submodeldef_t* smf = &mf->sub[number];
    model_t* mdl = R_ModelForId(smf->model);
    model_frame_t* frame = Mod_GetVisibleFrame(mf, number, params->id);
    model_frame_t* nextFrame = NULL;

    int numVerts, useSkin, c;
    float endPos, offset, alpha;
    float delta[3], color[4], ambient[4];
    float shininess, *shinyColor;
    float normYaw, normPitch, shinyAng, shinyPnt;
    float inter = params->inter;
    blendmode_t blending;
    TextureVariant* skinTexture = NULL, *shinyTexture = NULL;
    int zSign = (params->mirror? -1 : 1);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Do not bother with infinitely small models...
    if(mf->scale[VX] == 0 && (int)mf->scale[VY] == 0 && mf->scale[VZ] == 0) return;

    alpha = params->ambientColor[CA];
    // Is the submodel-defined alpha multiplier in effect?
    if(!(params->flags & (DDMF_BRIGHTSHADOW|DDMF_SHADOW|DDMF_ALTSHADOW)))
    {
        alpha *= smf->alpha * reciprocal255;
    }

    // Would this be visible?
    if(alpha <= 0) return;

    // Is the submodel-defined blend mode in effect?
    if(params->flags & DDMF_BRIGHTSHADOW)
    {
        blending = BM_ADD;
    }
    else
    {
        blending = smf->blendMode;
    }

    useSkin = chooseSkin(mf, number, params->id, params->selector, params->tmap);

    // Scale interpos. Intermark becomes zero and endmark becomes one.
    // (Full sub-interpolation!) But only do it for the standard
    // interrange. If a custom one is defined, don't touch interpos.
    if((mf->interRange[0] == 0 && mf->interRange[1] == 1) ||
       (smf->flags & MFF_WORLD_TIME_ANIM))
    {
        endPos = (mf->interNext ? mf->interNext->interMark : 1);
        inter = (params->inter - mf->interMark) / (endPos - mf->interMark);
    }

    // Do we have a sky/particle model here?
    if(params->alwaysInterpolate)
    {
        // Always interpolate, if there's animation.
        // Used with sky and particle models.
        nextFrame = mdl->frames + (smf->frame + 1) % mdl->info.numFrames;
        mfNext = mf;
    }
    else
    {
        // Check for possible interpolation.
        if(frameInter && mfNext && !(smf->flags & MFF_DONT_INTERPOLATE))
        {
            if(mfNext->sub[number].model == smf->model)
            {
                nextFrame = Mod_GetVisibleFrame(mfNext, number, params->id);
            }
        }
    }

    // Clamp interpolation.
    if(inter < 0)
        inter = 0;
    if(inter > 1)
        inter = 1;

    if(!nextFrame)
    {
        // If not interpolating, use the same frame as interpolation target.
        // The lerp routines will recognize this special case.
        nextFrame = frame;
        mfNext = mf;
    }

    // Determine the total number of vertices we have.
    numVerts = mdl->info.numVertices;

    // Ensure our vertex render buffers can accommodate this.
    if(!Mod_ExpandVertexBuffer(numVerts))
    {
        // No can do, we aint got the power!
        return;
    }

    // Setup transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Model space => World space
    glTranslatef(params->origin[VX] + params->srvo[VX] +
                   Mod_Lerp(mf->offset[VX], mfNext->offset[VX], inter),
                 params->origin[VZ] + params->srvo[VZ] +
                   Mod_Lerp(mf->offset[VY], mfNext->offset[VY], inter),
                 params->origin[VY] + params->srvo[VY] + zSign *
                   Mod_Lerp(mf->offset[VZ], mfNext->offset[VZ], inter));

    if(params->extraYawAngle || params->extraPitchAngle)
    {
        // Sky models have an extra rotation.
        glScalef(1, 200 / 240.0f, 1);
        glRotatef(params->extraYawAngle, 1, 0, 0);
        glRotatef(params->extraPitchAngle, 0, 0, 1);
        glScalef(1, 240 / 200.0f, 1);
    }

    // Model rotation.
    glRotatef(params->viewAlign ? params->yawAngleOffset : params->yaw,
              0, 1, 0);
    glRotatef(params->viewAlign ? params->pitchAngleOffset : params->pitch,
              0, 0, 1);

    // Scaling and model space offset.
    glScalef(Mod_Lerp(mf->scale[VX], mfNext->scale[VX], inter),
             Mod_Lerp(mf->scale[VY], mfNext->scale[VY], inter),
             Mod_Lerp(mf->scale[VZ], mfNext->scale[VZ], inter));
    if(params->extraScale)
    {
        // Particle models have an extra scale.
        glScalef(params->extraScale, params->extraScale, params->extraScale);
    }
    glTranslatef(smf->offset[VX], smf->offset[VY], smf->offset[VZ]);

    /**
     * Now we can draw.
     */

    // Determine the suitable LOD.
    if(mdl->info.numLODs > 1 && rend_model_lod != 0)
    {
        float lodFactor;

        lodFactor = rend_model_lod * Window_Width(theWindow) / 640.0f / (fieldOfView / 90.0f);
        if(lodFactor)
            lodFactor = 1 / lodFactor;

        // Determine the LOD we will be using.
        activeLod = (int) (lodFactor * params->distance);
        if(activeLod < 0)
            activeLod = 0;
        if(activeLod >= mdl->info.numLODs)
            activeLod = mdl->info.numLODs - 1;
        vertexUsage = mdl->vertexUsage;
    }
    else
    {
        activeLod = 0;
        vertexUsage = NULL;
    }

    // Interpolate vertices and normals.
    Mod_LerpVertices(inter, numVerts, frame->vertices, nextFrame->vertices,
                     modelVertices);
    Mod_LerpVertices(inter, numVerts, frame->normals, nextFrame->normals,
                     modelNormals);
    if(zSign < 0)
    {
        Mod_MirrorVertices(numVerts, modelVertices, VZ);
        Mod_MirrorVertices(numVerts, modelNormals, VY);
    }

    // Coordinates to the center of the model (game coords).
    modelCenter[VX] = params->origin[VX] + params->srvo[VX] + mf->offset[VX];
    modelCenter[VY] = params->origin[VY] + params->srvo[VY] + mf->offset[VZ];
    modelCenter[VZ] = ((params->origin[VZ] + params->gzt) * 2) + params->srvo[VZ] +
                        mf->offset[VY];

    // Calculate lighting.
    if((smf->flags & MFF_FULLBRIGHT) && !(smf->flags & MFF_DIM))
    {
        // Submodel-specific lighting override.
        ambient[CR] = ambient[CG] = ambient[CB] = ambient[CA] = 1;
        Mod_FullBrightVertexColors(numVerts, modelColors, alpha);
    }
    else if(!params->vLightListIdx)
    {
        // Lit uniformly.
        ambient[CR] = params->ambientColor[CR];
        ambient[CG] = params->ambientColor[CG];
        ambient[CB] = params->ambientColor[CB];
        ambient[CA] = alpha;
        Mod_FixedVertexColors(numVerts, modelColors, ambient);
    }
    else
    {
        // Lit normally.
        ambient[CR] = params->ambientColor[CR];
        ambient[CG] = params->ambientColor[CG];
        ambient[CB] = params->ambientColor[CB];
        ambient[CA] = alpha;

        Mod_VertexColors(numVerts, modelColors, modelNormals,
                         params->vLightListIdx, modelLight + 1, ambient,
                         mf->scale[VY] < 0? true: false,
                         -params->yaw, -params->pitch);
    }

    shininess = MINMAX_OF(0, mf->def->sub[number].shiny * modelShinyFactor, 1);
    // Ensure we've prepared the shiny skin.
    if(shininess > 0)
    {
        Texture* tex = mf->sub[number].shinySkin;
        if(tex)
        {
            shinyTexture = GL_PrepareTextureVariant(tex, Rend_ModelShinyTextureSpec());
        }
        else
        {
            shininess = 0;
        }
    }

    if(shininess > 0)
    {
        // Calculate shiny coordinates.
        shinyColor = mf->def->sub[number].shinyColor;

        // With psprites, add the view angle/pitch.
        offset = params->shineYawOffset;

        // Calculate normalized (0,1) model yaw and pitch.
        normYaw = M_CycleIntoRange(((params->viewAlign ? params->yawAngleOffset
                                                       : params->yaw) + offset) / 360, 1);

        offset = params->shinePitchOffset;

        normPitch = M_CycleIntoRange(((params->viewAlign ? params->pitchAngleOffset
                                                         : params->pitch) + offset) / 360, 1);

        if(params->shinepspriteCoordSpace)
        {
            // This is a hack to accommodate the psprite coordinate space.
            shinyAng = 0;
            shinyPnt = 0.5;
        }
        else
        {
            delta[VX] = modelCenter[VX] - vx;
            delta[VY] = modelCenter[VY] - vz;
            delta[VZ] = modelCenter[VZ] - vy;

            if(params->shineTranslateWithViewerPos)
            {
                delta[VX] += vx;
                delta[VY] += vz;
                delta[VZ] += vy;
            }

            shinyAng = QATAN2(delta[VZ], M_ApproxDistancef(delta[VX], delta[VY])) / PI + 0.5f; // shinyAng is [0,1]

            shinyPnt = QATAN2(delta[VY], delta[VX]) / (2 * PI);
        }

        Mod_ShinyCoords(numVerts, modelTexCoords, modelNormals, normYaw,
                        normPitch, shinyAng, shinyPnt,
                        mf->def->sub[number].shinyReact);

        // Shiny color.
        if(smf->flags & MFF_SHINY_LIT)
        {
            for(c = 0; c < 3; ++c)
                color[c] = ambient[c] * shinyColor[c];
        }
        else
        {
            for(c = 0; c < 3; ++c)
                color[c] = shinyColor[c];
        }
        color[3] = shininess;
    }

    if(renderTextures == 2)
    {
        // For lighting debug, render all surfaces using the gray texture.
        material_t* mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));
        if(mat)
        {
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MODELSKIN, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, -2, -1, true, true, false, false);
            const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

            skinTexture = MST(ms, MTU_PRIMARY);
        }
        else
        {
            skinTexture = 0;
        }
    }
    else
    {
        Texture* tex = mdl->skins[useSkin].texture;

        skinTexture = 0;
        if(tex)
        {
            skinTexture = GL_PrepareTextureVariant(tex, Rend_ModelDiffuseTextureSpec(!mdl->allowTexComp));
        }
    }

    // If we mirror the model, triangles have a different orientation.
    if(zSign < 0)
    {
        glFrontFace(GL_CCW);
    }

    // Twosided models won't use backface culling.
    if(smf->flags & MFF_TWO_SIDED)
        glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);

    // Render using multiple passes?
    if(!modelShinyMultitex || shininess <= 0 || alpha < 1 ||
       blending != BM_NORMAL || !(smf->flags & MFF_SHINY_SPECULAR) ||
       numTexUnits < 2 || !envModAdd)
    {
        // The first pass can be skipped if it won't be visible.
        if(shininess < 1 || smf->flags & MFF_SHINY_SPECULAR)
        {
            Mod_SelectTexUnits(1);
            GL_BlendMode(blending);
            GL_BindTexture(renderTextures? skinTexture : 0);

            Mod_RenderCommands(RC_COMMAND_COORDS,
                               mdl->lods[activeLod].glCommands, /*numVerts,*/
                               modelVertices, modelColors, NULL);
        }

        if(shininess > 0)
        {
            glDepthFunc(GL_LEQUAL);

            // Set blending mode, two choices: reflected and specular.
            if(smf->flags & MFF_SHINY_SPECULAR)
                GL_BlendMode(BM_ADD);
            else
                GL_BlendMode(BM_NORMAL);

            // Shiny color.
            Mod_FixedVertexColors(numVerts, modelColors, color);

            if(numTexUnits > 1 && modelShinyMultitex)
            {
                // We'll use multitexturing to clear out empty spots in
                // the primary texture.
                Mod_SelectTexUnits(2);
                GL_ModulateTexture(11);

                glActiveTexture(GL_TEXTURE1);
                GL_BindTexture(renderTextures ? shinyTexture : 0);

                glActiveTexture(GL_TEXTURE0);
                GL_BindTexture(renderTextures ? skinTexture : 0);

                Mod_RenderCommands(RC_BOTH_COORDS,
                                   mdl->lods[activeLod].glCommands, /*numVerts,*/
                                   modelVertices, modelColors, modelTexCoords);

                Mod_SelectTexUnits(1);
                GL_ModulateTexture(1);
            }
            else
            {
                // Empty spots will get shine, too.
                Mod_SelectTexUnits(1);
                GL_BindTexture(renderTextures ? shinyTexture : 0);
                Mod_RenderCommands(RC_OTHER_COORDS,
                                   mdl->lods[activeLod].glCommands, /*numVerts,*/
                                   modelVertices, modelColors, modelTexCoords);
            }
        }
    }
    else
    {
        // A special case: specular shininess on an opaque object.
        // Multitextured shininess with the normal blending.
        GL_BlendMode(blending);
        Mod_SelectTexUnits(2);

        // Tex1*Color + Tex2RGB*ConstRGB
        GL_ModulateTexture(10);

        glActiveTexture(GL_TEXTURE1);
        GL_BindTexture(renderTextures ? shinyTexture : 0);

        // Multiply by shininess.
        for(c = 0; c < 3; ++c)
            color[c] *= color[3];
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);

        glActiveTexture(GL_TEXTURE0);
        GL_BindTexture(renderTextures ? skinTexture : 0);

        Mod_RenderCommands(RC_BOTH_COORDS, mdl->lods[activeLod].glCommands,
                           /*numVerts,*/ modelVertices, modelColors,
                           modelTexCoords);

        Mod_SelectTexUnits(1);
        GL_ModulateTexture(1);
    }

    // We're done!
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Normally culling is always enabled.
    if(smf->flags & MFF_TWO_SIDED)
        glEnable(GL_CULL_FACE);

    if(zSign < 0)
    {
        glFrontFace(GL_CW);
    }
    glDepthFunc(GL_LESS);

    GL_BlendMode(BM_NORMAL);
}

void Rend_ModelInit(void)
{
    if(inited) return; // Already been here.

    modelVertices = 0;
    modelNormals = 0;
    modelColors = 0;
    modelTexCoords = 0;

    vertexBufferMax = vertexBufferSize = 0;
#if _DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    Mod_InitArrays();

    inited = true;
}

void Rend_ModelShutdown(void)
{
    if(!inited) return;

    if(modelVertices)
    {
        free(modelVertices);
        modelVertices = 0;
    }

    if(modelNormals)
    {
        free(modelNormals);
        modelNormals = 0;
    }

    if(modelColors)
    {
        free(modelColors);
        modelColors = 0;
    }

    if(modelTexCoords)
    {
        free(modelTexCoords);
        modelTexCoords = 0;
    }

    vertexBufferMax = vertexBufferSize = 0;
#if _DEBUG
    announcedVertexBufferMaxBreach = false;
#endif

    inited = false;
}

/**
 * Render all the submodels of a model.
 */
void Rend_RenderModel(const rendmodelparams_t* params)
{
    uint i;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    errorIfNotInited("Rend_RenderModel");

    if(!params || !params->mf) return;

    // Render all the submodels of this model.
    for(i = 0; i < MAX_FRAME_MODELS; ++i)
    {
        if(params->mf->sub[i].model)
        {
            boolean disableZ = (params->mf->flags & MFF_DISABLE_Z_WRITE ||
                                params->mf->sub[i].flags & MFF_DISABLE_Z_WRITE);

            if(disableZ)
                glDepthMask(GL_FALSE);

            Mod_RenderSubModel(i, params);

            if(disableZ)
                glDepthMask(GL_TRUE);
        }
    }

    if(devMobjVLights && params->vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(params->origin[VX], params->origin[VZ], params->origin[VY]);

        VL_ListIterator(params->vLightListIdx, (void*)&params->distance, R_DrawVLightVector);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }
}
