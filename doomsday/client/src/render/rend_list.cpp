/** @file rend_list.cpp Rendering Lists v3.3
 *
 * 3.3 -- Texture unit write state and revised primitive write interface.
 * 3.2 -- Shiny walls and floors
 * 3.1 -- Support for multiple shadow textures
 * 3.0 -- Multitexturing
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <cstdlib>
#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"
//#include "de_misc.h"

#include <de/memory.h>
#include <de/memoryzone.h>

#include "def_main.h"
#include "Texture"
#include "m_profiler.h"

#include "render/rend_list.h"

using namespace de;

BEGIN_PROF_TIMERS()
  PROF_RL_ADD_POLY,
  PROF_RL_GET_LIST,
  PROF_RL_RENDER_ALL,
  PROF_RL_RENDER_NORMAL,
  PROF_RL_RENDER_LIGHT,
  PROF_RL_RENDER_MASKED,
  PROF_RL_RENDER_SHINY,
  PROF_RL_RENDER_SHADOW,
  PROF_RL_RENDER_SKYMASK
END_PROF_TIMERS()

#define RL_HASH_SIZE        128

// Number of extra bytes to keep allocated in the end of each rendering list.
#define LIST_DATA_PADDING   16

// @todo Rlist allocation could be dynamic.
#define MAX_RLISTS          1024

#define MTEX_DETAILS_ENABLED (r_detail && useMultiTexDetails && \
                              defs.count.details.num > 0)
#define IS_MTEX_DETAILS     (MTEX_DETAILS_ENABLED && numTexUnits > 1)
#define IS_MTEX_LIGHTS      (!IS_MTEX_DETAILS && !usingFog && useMultiTexLights \
                             && numTexUnits > 1 && envModAdd)

// Drawing condition flags.
#define DCF_NO_BLEND                0x00000001
#define DCF_BLEND                   0x00000002
#define DCF_SET_LIGHT_ENV0          0x00000004
#define DCF_SET_LIGHT_ENV1          0x00000008
#define DCF_SET_LIGHT_ENV           (DCF_SET_LIGHT_ENV0 | DCF_SET_LIGHT_ENV1)
#define DCF_JUST_ONE_LIGHT          0x00000010
#define DCF_MANY_LIGHTS             0x00000020
#define DCF_SET_BLEND_MODE          0x00000040 // Primitive-specific blending.
#define DCF_SET_MATRIX_DTEXTURE0    0x00000080
#define DCF_SET_MATRIX_DTEXTURE1    0x00000100
#define DCF_SET_MATRIX_DTEXTURE     (DCF_SET_MATRIX_DTEXTURE0 | DCF_SET_MATRIX_DTEXTURE1)
#define DCF_SET_MATRIX_TEXTURE0     0x00000200
#define DCF_SET_MATRIX_TEXTURE1     0x00000400
#define DCF_SET_MATRIX_TEXTURE      (DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1)
#define DCF_NO_COLOR                0x00000800
#define DCF_SKIP                    0x80000000

// List Modes.
typedef enum listmode_e {
    LM_SKYMASK,
    LM_ALL,
    LM_LIGHT_MOD_TEXTURE,
    LM_FIRST_LIGHT,
    LM_TEXTURE_PLUS_LIGHT,
    LM_UNBLENDED_TEXTURE_AND_DETAIL,
    LM_BLENDED,
    LM_BLENDED_FIRST_LIGHT,
    LM_NO_LIGHTS,
    LM_WITHOUT_TEXTURE,
    LM_LIGHTS,
    LM_MOD_TEXTURE,
    LM_MOD_TEXTURE_MANY_LIGHTS,
    LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL,
    LM_BLENDED_MOD_TEXTURE,
    LM_ALL_DETAILS,
    LM_BLENDED_DETAILS,
    LM_SHADOW,
    LM_SHINY,
    LM_MASKED_SHINY,
    LM_ALL_SHINY
} listmode_t;

// Texture coordinate array indices.
enum {
    TCA_MAIN, // Main texture.
    TCA_BLEND, // Blendtarget texture.
    TCA_LIGHT, // Dynlight texture.
    NUM_TEXCOORD_ARRAYS
};

// Texture unit indices. These map to real GL texture units.
typedef enum {
    TU_PRIMARY = 0,
    TU_PRIMARY_DETAIL,
    TU_INTER,
    TU_INTER_DETAIL,
    NUM_TEXTURE_UNITS
} texunitid_t;

// Primitive types.
typedef enum {
    PT_NORMAL = 0,
    PT_SKY_MASK, // A sky mask polygon.
    PT_LIGHT, // A dynamic light.
    PT_SHADOW, // An object shadow or fakeradio edge shadow.
    PT_SHINY
} rendpolytype_t;

/**
 * Primitive flags:
 */
#define PF_ONE_LIGHT        0x01
#define PF_MANY_LIGHTS      0x02
#define PF_IS_LIT           (PF_ONE_LIGHT | PF_MANY_LIGHTS)

/**
 * Each primhdr begins a block of polygon data that ends up as one or
 * more triangles on the screen. Note that there are pointers to the
 * rendering list itself here; they will need to be properly restored
 * whenever the list is resized.
 */
typedef struct primhdr_s {
    // RL_AddPoly expects that size is the first thing in the header.
    // Must be an offset since the list is sometimes reallocated.
    uint            size; // Size of this primitive (zero = n/a).

    ushort          type; // OpenGL primitive type e.g., GL_TRIANGLE_STRIP.
    blendmode_t     blendMode; // BM_* Primitive-specific blending mode.

    // Elements in the vertex array for this primitive.
    // The indices are always contiguous: indices[0] is the base, and
    // indices[1...n] > indices[0].
    // All indices in the range indices[0]...indices[n] are used by this
    // primitive (some are shared).
    ushort          numIndices;
    uint           *indices;

    byte            flags; // PF_* primitive flags.

    float           ptexOffset[2], ptexScale[2];

    // Detail texture matrix manipulations.
    float           texOffset[2], texScale[2];

    // Some primitives are modulated with an additional texture and color
    // using multitexturing (if available), depending on the list state.
    // Example: first light affecting the primitive.
    DGLuint         modTex;
    float           modColor[4];
} primhdr_t;

// Helper macro for accessing rendlist texmap units.
#define TU(x, n)            (&((x)->texmapunits[(n)]))

/// @note Slighty different representation than that passed to RL_AddPoly.
typedef struct rendlist_texmapunit_s {
    rtexmapunit_texture_t texture;
    float opacity; // Blend amount.
    blendmode_t blendMode; // Currently used only with shiny pass.

    inline bool hasTexture() const { return texture.hasTexture(); }
} rendlist_texmapunit_t;

/**
 * The rendering list. When the list is resized, pointers in the primitives
 * need to be restored so that they point to the new list.
 */
typedef struct rendlist_s {
    struct rendlist_s *next;
    rendlist_texmapunit_t texmapunits[NUM_TEXTURE_UNITS];
    size_t          size; // Number of bytes allocated for the data.
    byte           *data; // Data for a number of polygons (The List).
    byte           *cursor; // A pointer to data, for reading/writing.
    primhdr_t      *last; // Pointer to the last primitive (or NULL).
} rendlist_t;

typedef struct listhash_s {
    rendlist_t     *first, *last;
} listhash_t;

extern boolean usingFog;

int renderTextures = true;
int renderWireframe = false;
int useMultiTexLights = true;
int useMultiTexDetails = true;

// Rendering paramaters for dynamic lights.
int dynlightBlend = 0;

float torchColor[3] = {1, 1, 1};
int torchAdditive = true;

static boolean initedOk = false;

// Logical texture unit state. Used with RL_LoadDefaultRtus and RL_CopyRtu
static rtexmapunit_t rtuDefault;
static rtexmapunit_t rtuState[NUM_TEXMAP_UNITS];
static rtexmapunit_t const *rtuMap[NUM_TEXMAP_UNITS];

// GL texture unit state used during write. Global for performance reasons.
static rtexmapunit_t const *texunits[NUM_TEXTURE_UNITS];

/**
 * The vertex arrays.
 */
static dgl_vertex_t *vertices;
static dgl_texcoord_t *texCoords[NUM_TEXCOORD_ARRAYS];
static dgl_color_t *colors;

static uint numVertices, maxVertices;
static boolean rDrawSky;

/**
 * The rendering lists.
 */
// Surfaces without lights.
static listhash_t plainHash[RL_HASH_SIZE];

// Surfaces with lights.
static listhash_t litHash[RL_HASH_SIZE];

// Additional light primitives.
static listhash_t dynHash[RL_HASH_SIZE];

// Shiny surfaces.
static listhash_t shinyHash[RL_HASH_SIZE];

static listhash_t shadowHash[RL_HASH_SIZE];
static rendlist_t skyMaskList;

static float blackColor[4] = { 0, 0, 0, 0 };

void RL_Register()
{
    /// @todo Move cvars here.
    C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);
    C_VAR_INT("rend-light-blend", &dynlightBlend, 0, 0, 2);
}

static inline ushort unitHashForTexture(rtexmapunit_texture_t const *tu)
{
    DGLuint glName;
    if(tu->flags & TUF_TEXTURE_IS_MANAGED)
    {
        glName = tu->variant? reinterpret_cast<de::Texture::Variant *>(tu->variant)->glName() : 0;
    }
    else
    {
        glName = tu->gl.name;
    }
    return glName % RL_HASH_SIZE;
}

static boolean compareUnitTexture(rtexmapunit_texture_t const *ltu,
    rtexmapunit_texture_t const *rtu)
{
    if(ltu->hasTexture() != rtu->hasTexture()) return false;

    if((ltu->flags & TUF_TEXTURE_IS_MANAGED) != (rtu->flags & TUF_TEXTURE_IS_MANAGED)) return false;
    if(ltu->flags & TUF_TEXTURE_IS_MANAGED)
    {
        if(ltu->variant != rtu->variant) return false;
    }
    else
    {
        if(ltu->gl.name    != rtu->gl.name)    return false;
        if(ltu->gl.magMode != rtu->gl.magMode) return false;
        if(ltu->gl.wrapS   != rtu->gl.wrapS)   return false;
        if(ltu->gl.wrapT   != rtu->gl.wrapT)   return false;
    }
    return true;
}

static inline boolean compareUnit(rendlist_texmapunit_t const *ltu, rtexmapunit_t const *rtu)
{
    if(!compareUnitTexture(&ltu->texture, &rtu->texture)) return false;
    if(ltu->opacity != rtu->opacity) return false;
    return true;
}

static inline void copyUnit(rendlist_texmapunit_t *ltu, rtexmapunit_t const *rtu)
{
    std::memcpy(&ltu->texture, &rtu->texture, sizeof(ltu->texture));
    ltu->blendMode = rtu->blendMode;
    ltu->opacity = MINMAX_OF(0, rtu->opacity, 1);
}

static void rlBind(rendlist_texmapunit_t const *tmu)
{
    DENG2_ASSERT(tmu);
    if(!tmu->hasTexture()) return;

    if(!renderTextures)
    {
        GL_SetNoTexture();
        return;
    }

    if(tmu->texture.flags & TUF_TEXTURE_IS_MANAGED)
    {
        GL_BindTexture(tmu->texture.variant);
    }
    else
    {
        GL_BindTextureUnmanaged(tmu->texture.gl.name, tmu->texture.gl.wrapS,
                                tmu->texture.gl.wrapT, tmu->texture.gl.magMode);
    }
}

static void rlBindTo(int unit, rendlist_texmapunit_t const *tmu)
{
    DENG2_ASSERT(tmu);
    if(!tmu->hasTexture()) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();
    glActiveTexture(GL_TEXTURE0 + byte(unit));

    rlBind(tmu);
}

static void clearHash(listhash_t *hash)
{
    DENG_ASSERT(hash);
    std::memset(hash, 0, sizeof(listhash_t) * RL_HASH_SIZE);
}

void RL_Init()
{
    if(initedOk) return; // Already been here.

    clearHash(plainHash);
    clearHash(litHash);
    clearHash(dynHash);
    clearHash(shadowHash);
    clearHash(shinyHash);

    std::memset(&skyMaskList, 0, sizeof(skyMaskList));
    Rtu_Init(&rtuDefault);
    RL_LoadDefaultRtus();
    initedOk = true;
}

void RL_Shutdown()
{
    // Stub.
    /// @todo Rework list memory management so we explicitly free it, rather
    /// than depend on it being free'd by the Zone when it is shutdown.
    initedOk = false;
}

boolean RL_IsMTexLights()
{
    return IS_MTEX_LIGHTS;
}

boolean RL_IsMTexDetails()
{
    return IS_MTEX_DETAILS;
}

static void clearVertices()
{
    numVertices = 0;
}

static void destroyVertices()
{
    numVertices = maxVertices = 0;

    if(vertices)
    {
        M_Free(vertices); vertices = 0;
    }

    if(colors)
    {
        M_Free(colors); colors = 0;
    }

    for(uint i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
    {
        if(texCoords[i])
        {
            M_Free(texCoords[i]); texCoords[i] = 0;
        }
    }
}

/**
 * Allocate vertices from the global vertex array.
 */
static uint allocateVertices(uint count)
{
    uint base = numVertices;

    // Do we need to allocate more memory?
    numVertices += count;
    while(numVertices > maxVertices)
    {
        if(maxVertices == 0)
        {
            maxVertices = 16;
        }
        else
        {
            maxVertices *= 2;
        }

        vertices = (dgl_vertex_t *) M_Realloc(vertices, sizeof(dgl_vertex_t) * maxVertices);
        colors   = (dgl_color_t *) M_Realloc(colors,   sizeof(dgl_color_t)  * maxVertices);
        for(uint i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
        {
            texCoords[i] = (dgl_texcoord_t *) M_Realloc(texCoords[i], sizeof(dgl_texcoord_t) * maxVertices);
        }
    }
    return base;
}

static void destroyList(rendlist_t *rl)
{
    DENG_ASSERT(rl);

    // All the list data will be destroyed.
    if(rl->data)
    {
        Z_Free(rl->data); rl->data = 0;
    }

#ifdef DENG_DEBUG
    Z_CheckHeap();
#endif

    rl->cursor = NULL;
    TU(rl, TU_INTER_DETAIL)->texture.gl.name = 0;
    TU(rl, TU_INTER_DETAIL)->texture.flags   = 0;
    rl->last = 0;
    rl->size = 0;
}

static void deleteHash(listhash_t* hash)
{
    rendlist_t *next;

    for(uint i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(rendlist_t *list = hash[i].first; list; list = next)
        {
            next = list->next;
            destroyList(list);
            Z_Free(list);
        }
    }
    clearHash(hash);
}

/**
 * All lists will be destroyed.
 */
void RL_DeleteLists()
{
    // Delete all lists.
    deleteHash(plainHash);
    deleteHash(litHash);
    deleteHash(dynHash);
    deleteHash(shadowHash);
    deleteHash(shinyHash);

    destroyList(&skyMaskList);

    destroyVertices();

#ifdef DENG_DEBUG
    Z_CheckHeap();
#endif

PRINT_PROF( PROF_RL_ADD_POLY );
PRINT_PROF( PROF_RL_GET_LIST );
PRINT_PROF( PROF_RL_RENDER_ALL );
PRINT_PROF( PROF_RL_RENDER_NORMAL );
PRINT_PROF( PROF_RL_RENDER_LIGHT );
PRINT_PROF( PROF_RL_RENDER_MASKED );
PRINT_PROF( PROF_RL_RENDER_SHADOW );
PRINT_PROF( PROF_RL_RENDER_SHINY );
PRINT_PROF( PROF_RL_RENDER_SKYMASK );
}

/**
 * Set the R/W cursor to the beginning.
 */
static void rewindList(rendlist_t *rl)
{
    DENG_ASSERT(rl);

    rl->cursor = rl->data;
    rl->last = NULL;

    // The interpolation target must be explicitly set (in RL_AddPoly).
    TU(rl, TU_INTER)->texture.gl.name = 0;
    TU(rl, TU_INTER)->texture.flags   = 0;
    TU(rl, TU_INTER)->opacity         = 0;

    TU(rl, TU_INTER_DETAIL)->texture.gl.name = 0;
    TU(rl, TU_INTER_DETAIL)->texture.flags   = 0;
    TU(rl, TU_INTER_DETAIL)->opacity         = 0;
}

static void rewindHash(listhash_t *hash)
{
    DENG_ASSERT(hash);
    for(uint i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(rendlist_t *list = hash[i].first; list; list = list->next)
        {
            rewindList(list);
        }
    }
}

/**
 * Called before rendering a frame.
 */
void RL_ClearLists()
{
    rewindHash(plainHash);
    rewindHash(litHash);
    rewindHash(dynHash);
    rewindHash(shadowHash);
    rewindHash(shinyHash);

    rewindList(&skyMaskList);

    // Clear the vertex array.
    clearVertices();

    // @todo Does this belong here?
    rDrawSky = false;
}

static rendlist_t *createList(listhash_t *hash)
{
    rendlist_t* list = (rendlist_t *) Z_Calloc(sizeof(rendlist_t), PU_APPSTATIC, 0);

    if(hash->last)
        hash->last->next = list;
    hash->last = list;
    if(!hash->first)
        hash->first = list;
    return list;
}

static rendlist_t *getListFor(rendpolytype_t polyType, boolean isLit)
{
    listhash_t *list, *table;
    rendlist_t *dest, *convertable = 0;

    // Check for specialized rendering lists first.
    if(polyType == PT_SKY_MASK)
    {
        return &skyMaskList;
    }

    // Choose the correct hash table.
    switch(polyType)
    {
    case PT_SHINY:     table = shinyHash;  break;
    case PT_SHADOW:    table = shadowHash; break;
    case PT_LIGHT:     table = dynHash;    break;
    default:
        table = (isLit ? litHash : plainHash);
        break;
    }

    // Find/create a list in the hash.
    list = &table[unitHashForTexture(&texunits[TU_PRIMARY]->texture)];
    for(dest = list->first; dest; dest = dest->next)
    {
        if((polyType == PT_SHINY &&
            compareUnit(TU(dest, TU_PRIMARY), texunits[TU_PRIMARY])) ||
           (polyType != PT_SHINY &&
            compareUnit(TU(dest, TU_PRIMARY), texunits[TU_PRIMARY]) &&
            compareUnit(TU(dest, TU_PRIMARY_DETAIL), texunits[TU_PRIMARY_DETAIL])))
        {
            if(!TU(dest, TU_INTER)->hasTexture() &&
               !texunits[TU_INTER]->hasTexture())
            {
                // This will do great.
                return dest;
            }

            // Is this eligible for conversion to a blended list?
            if(!dest->last && !convertable && texunits[TU_INTER]->hasTexture())
            {
                // If necessary, this empty list will be selected.
                convertable = dest;
            }

            // Possibly an exact match?
            if((polyType == PT_SHINY &&
                compareUnit(TU(dest, TU_INTER), texunits[TU_INTER])) ||
               (polyType != PT_SHINY &&
                compareUnit(TU(dest, TU_INTER), texunits[TU_INTER]) &&
                compareUnit(TU(dest, TU_INTER_DETAIL), texunits[TU_INTER_DETAIL])))
            {
                return dest;
            }
        }
    }

    // Did we find a convertable list?
    if(convertable)
    {
        // This list is currently empty.
        if(polyType == PT_SHINY)
        {
            copyUnit(TU(convertable, TU_INTER), texunits[TU_INTER]);
        }
        else
        {
            copyUnit(TU(convertable, TU_INTER), texunits[TU_INTER]);
            copyUnit(TU(convertable, TU_INTER_DETAIL), texunits[TU_INTER_DETAIL]);
        }

        return convertable;
    }

    // Create a new list.
    dest = createList(list);

    // Init the info.
    if(polyType == PT_SHINY)
    {
        copyUnit(TU(dest, TU_PRIMARY), texunits[TU_PRIMARY]);
        if(texunits[TU_INTER]->hasTexture())
            copyUnit(TU(dest, TU_INTER), texunits[TU_INTER]);
    }
    else
    {
        copyUnit(TU(dest, TU_PRIMARY), texunits[TU_PRIMARY]);
        copyUnit(TU(dest, TU_PRIMARY_DETAIL), texunits[TU_PRIMARY_DETAIL]);

        if(texunits[TU_INTER]->hasTexture())
        {
            copyUnit(TU(dest, TU_INTER), texunits[TU_INTER]);
            copyUnit(TU(dest, TU_INTER_DETAIL), texunits[TU_INTER_DETAIL]);
        }
    }

    return dest;
}

/**
 * @return  Start of the allocated data.
 */
static void* allocateData(rendlist_t *list, int bytes)
{
    if(bytes <= 0)
        return 0;

    // We require the extra bytes because we want that the end of the
    // list data is always safe for writing-in-advance. This is needed
    // when the 'end of data' marker is written in RL_AddPoly.
    int startOffset = list->cursor - list->data;
    size_t required = startOffset + bytes + LIST_DATA_PADDING;

    // First check that the data buffer of the list is large enough.
    if(required > list->size)
    {
        // Offsets must be preserved.
        byte *oldData = list->data;
        int cursorOffset = -1;
        int lastOffset = -1;

        if(list->cursor)
            cursorOffset = list->cursor - oldData;
        if(list->last)
            lastOffset = (byte *) list->last - oldData;

        // Allocate more memory for the data buffer.
        if(list->size == 0)
            list->size = 1024;
        while(list->size < required)
            list->size *= 2;

        list->data = (byte *) Z_Realloc(list->data, list->size, PU_APPSTATIC);

        // Restore main pointers.
        list->cursor = (cursorOffset >= 0 ? list->data + cursorOffset : list->data);
        list->last   = (lastOffset >= 0 ? (primhdr_t *) (list->data + lastOffset) : NULL);

        // Restore in-list pointers.
        if(oldData)
        {
            primhdr_t *hdr = (primhdr_t *) list->data;
            while(hdr <= list->last)
            {
                if(hdr->indices != NULL)
                {
                    hdr->indices = (uint *) (list->data + ((byte *) hdr->indices - oldData));
                }

                // Check here in the end; primitive composition may be
                // in progress.
                if(hdr->size == 0) break;

                hdr = (primhdr_t *) ((byte *) hdr + hdr->size);
            }
        }
    }

    // Advance the cursor.
    list->cursor += bytes;

    return list->data + startOffset;
}

static void allocateIndices(rendlist_t *list, uint numIndices)
{
    void *indices;

    list->last->numIndices = numIndices;
    indices = allocateData(list, sizeof(uint) * numIndices);

    // list->last may change during allocateData.
    list->last->indices = (uint *) indices;
}

static void endWrite(rendlist_t *list)
{
    // The primitive has been written, update the size in the header.
    list->last->size = list->cursor - (byte *) list->last;

    // Write the end marker (which will be overwritten by the next
    // primitive). The idea is that this zero is interpreted as the
    // size of the following primhdr.
    *(int *) list->cursor = 0;
}

static void writePrimitive(rendlist_t const *list, uint base,
    rvertex_t const *rvertices, rtexcoord_t const *coords,
    rtexcoord_t const *coords1, rtexcoord_t const *coords2,
    ColorRawf const *rcolors, uint numElements, rendpolytype_t type)
{
    for(uint i = 0; i < numElements; ++i)
    {
        // Vertex.
        rvertex_t const *rvtx = &rvertices[i];
        dgl_vertex_t *vtx = &vertices[base + i];

        vtx->xyz[0] = rvtx->pos[VX];
        vtx->xyz[1] = rvtx->pos[VZ];
        vtx->xyz[2] = rvtx->pos[VY];

        // Sky masked polys need nothing more.
        if(type == PT_SKY_MASK) continue;

        // Primary texture coordinates.
        if(TU(list, TU_PRIMARY)->hasTexture())
        {
            rtexcoord_t const *rtc = &coords[i];
            dgl_texcoord_t *tc = &texCoords[TCA_MAIN][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // Secondary texture coordinates.
        if(TU(list, TU_INTER)->hasTexture())
        {
            rtexcoord_t const *rtc = &coords1[i];
            dgl_texcoord_t *tc = &texCoords[TCA_BLEND][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // First light texture coordinates.
        if((list->last->flags & PF_IS_LIT) && IS_MTEX_LIGHTS)
        {
            rtexcoord_t const *rtc = &coords2[i];
            dgl_texcoord_t *tc = &texCoords[TCA_LIGHT][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // Color.
        ColorRawf const *rcolor = &rcolors[i];
        dgl_color_t *color = &colors[base + i];

        if(rcolors)
        {
            color->rgba[CR] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CR], 1));
            color->rgba[CG] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CG], 1));
            color->rgba[CB] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CB], 1));
            color->rgba[CA] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CA], 1));
        }
        else
        {
            color->rgba[CR] = color->rgba[CG] = color->rgba[CB] = color->rgba[CA] = 255;
        }
    }
}

/**
 * Adds one or more polys the render lists depending on configuration.
 * @pre Caller knows what they are doing. Arguments are not validity checked.
 */
static void writePoly2(primtype_t type, rendpolytype_t polyType, int flags,
    uint numElements, rvertex_t const *vertices, ColorRawf const *colors,
    rtexcoord_t const *primaryCoords, rtexcoord_t const *interCoords,
    DGLuint modTex, ColorRawf const *modColor, rtexcoord_t const *modCoords)
{
    boolean const isLit = (polyType != PT_LIGHT && (modTex || !!(flags & RPF_HAS_DYNLIGHTS)));
    uint i, base, primSize, numIndices;
    rendlist_t *li;
    primhdr_t *hdr;

    if(polyType == PT_SKY_MASK)
        rDrawSky = true;

BEGIN_PROF( PROF_RL_ADD_POLY );

BEGIN_PROF( PROF_RL_GET_LIST );

    // Find/create a rendering list for the polygon's texture.
    li = getListFor(polyType, isLit);

END_PROF( PROF_RL_GET_LIST );

    primSize = numElements;
    numIndices = numElements;
    base = allocateVertices(primSize);

    hdr = (primhdr_t *) allocateData(li, sizeof(primhdr_t));
    DENG_ASSERT(hdr);
    li->last = hdr; // This becomes the new last primitive.

    // Primitive-specific blending mode.
    hdr->blendMode = texunits[TU_PRIMARY]->blendMode;
    hdr->size = 0;
    hdr->indices = NULL;
    hdr->numIndices = 0;
    hdr->flags = 0;
    if(isLit)
    {
        if(modTex && !(flags & RPF_HAS_DYNLIGHTS))
            hdr->flags |= PF_ONE_LIGHT; // Using modulation.
        else
            hdr->flags |= PF_MANY_LIGHTS;
    }
    hdr->modTex = modTex;
    hdr->modColor[CR] = modColor? modColor->red   : 0;
    hdr->modColor[CG] = modColor? modColor->green : 0;
    hdr->modColor[CB] = modColor? modColor->blue  : 0;
    hdr->modColor[CA] = 0;

    if(polyType == PT_SHINY && texunits[TU_INTER]->hasTexture())
    {
        hdr->ptexScale[0] = texunits[TU_INTER]->scale[0];
        hdr->ptexScale[1] = texunits[TU_INTER]->scale[1];
        hdr->ptexOffset[0] = texunits[TU_INTER]->offset[0] * texunits[TU_INTER]->scale[0];
        hdr->ptexOffset[1] = texunits[TU_INTER]->offset[1] * texunits[TU_INTER]->scale[1];
    }
    else if(texunits[TU_PRIMARY]->hasTexture())
    {
        hdr->ptexScale[0] = texunits[TU_PRIMARY]->scale[0];
        hdr->ptexScale[1] = texunits[TU_PRIMARY]->scale[1];
        hdr->ptexOffset[0] = texunits[TU_PRIMARY]->offset[0] * texunits[TU_PRIMARY]->scale[0];
        hdr->ptexOffset[1] = texunits[TU_PRIMARY]->offset[1] * texunits[TU_PRIMARY]->scale[1];
    }

    if(texunits[TU_PRIMARY_DETAIL]->hasTexture())
    {
        hdr->texScale[0] = texunits[TU_PRIMARY_DETAIL]->scale[0];
        hdr->texScale[1] = texunits[TU_PRIMARY_DETAIL]->scale[1];
        hdr->texOffset[0] = texunits[TU_PRIMARY_DETAIL]->offset[0] * texunits[TU_PRIMARY_DETAIL]->scale[0];
        hdr->texOffset[1] = texunits[TU_PRIMARY_DETAIL]->offset[1] * texunits[TU_PRIMARY_DETAIL]->scale[1];
    }
    else
    {
        hdr->texScale[0] = hdr->texScale[1] = 1.f;
        hdr->texOffset[0] = hdr->texOffset[1] = 1.f;
    }

    // Setup the indices.
    allocateIndices(li, numIndices);
    for(i = 0; i < numIndices; ++i)
        li->last->indices[i] = base + i;
    li->last->type =
        (type == PT_TRIANGLE_STRIP? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);

    writePrimitive(li, base, vertices, primaryCoords, interCoords,
                   modCoords, colors, numElements, polyType);
    endWrite(li);

END_PROF( PROF_RL_ADD_POLY );
}

/**
 * Rationalizes write arguments prior to flushing the write.
 * Implemented for the purposes of cleaner/more readable code in modules which
 * interface with this.
 */
static void writePoly(primtype_t type, rendpolytype_t polyType, int flags,
    uint numElements, rvertex_t const *vertices, ColorRawf const *colors,
    rtexcoord_t const *primaryCoords, rtexcoord_t const *interCoords,
    DGLuint modTex, ColorRawf const *modColor, rtexcoord_t const *modCoords)
{
    if(numElements < 3) return; // huh?

    /// @todo Logical disconnect: modulation VS dynlight multitexture state.
    if(modTex && !RL_IsMTexLights())
        Con_Error("RL_AddPoly: Attempt to write modulated primitive with multitexture disabled.");

    if(flags & RPF_SKYMASK)
    {
        flags      &= ~(RPF_LIGHT|RPF_SHADOW|RPF_HAS_DYNLIGHTS);
        colors      = NULL;
        primaryCoords = NULL;
        interCoords = NULL;
        modTex      = 0;
        modColor    = NULL;
        modCoords   = NULL;
    }
    else if(flags & RPF_LIGHT)
    {
        flags      &= ~(RPF_SHADOW|RPF_HAS_DYNLIGHTS);
        interCoords = NULL;
        modTex      = 0;
        modColor    = NULL;
        modCoords   = NULL;
    }
    else if(flags & RPF_SHADOW)
    {
        flags      &= ~RPF_HAS_DYNLIGHTS;
        interCoords = NULL;
        modTex      = 0;
        modColor    = NULL;
        modCoords   = NULL;
    }

    // Flush the write.
    writePoly2(type, polyType, flags, numElements, vertices,
               colors, primaryCoords, interCoords, modTex, modColor, modCoords);
}

static inline boolean validRTUIndex(uint idx)
{
    return idx < NUM_TEXMAP_UNITS;
}

static inline void errorIfNotValidRTUIndex(uint idx, char const *callerName)
{
    if(validRTUIndex(idx)) return;
    Con_Error("%s: Invalid texture unit index %u.", callerName, idx);
    exit(1); // Unreachable.
}

static inline boolean isWriteStateRTU(rtexmapunit_t const *ptr)
{
    // Note that the default texture unit is not considered as being
    // part of the write state.
    return ptr >= &rtuState[0] && ptr <= &rtuState[NUM_TEXMAP_UNITS];
}

/**
 * If the identified @a idx texture unit of the primitive writer has been
 * mapped to an external address, insert a copy of it into our internal
 * write state.
 *
 * To be called before customizing a texture unit begins to ensure we
 * are modifying data we have ownership of!
 */
static void copyMappedRtuToState(uint idx)
{
    DENG_ASSERT(validRTUIndex(idx));
    if(isWriteStateRTU(rtuMap[idx])) return;
    RL_CopyRtu(idx, rtuMap[idx]);
}

void RL_LoadDefaultRtus()
{
    for(int i = 0; i < NUM_TEXMAP_UNITS; ++i)
    {
        rtuMap[i] = &rtuDefault;
    }
}

void RL_MapRtu(uint idx, rtexmapunit_t const *rtu)
{
    errorIfNotValidRTUIndex(idx, "RL_MapRtu");
    rtuMap[idx] = (rtu? rtu : &rtuDefault);
}

void RL_CopyRtu(uint idx, rtexmapunit_t const *rtu)
{
    errorIfNotValidRTUIndex(idx, "RL_CopyRtu");
    if(!rtu)
    {
        // Restore defaults.
        rtuMap[idx] = &rtuDefault;
        return;
    }

    // Some debug error checking here wouldn't go amiss!
    std::memcpy(rtuState + idx, rtu, sizeof rtuState[0]);
    // Map this unit to that owned by the write state.
    rtuMap[idx] = rtuState + idx;
}

void RL_Rtu_SetScale(uint idx, Vector2f const &st)
{
    errorIfNotValidRTUIndex(idx, "RL_Rtu_SetScale");
    copyMappedRtuToState(idx);
    Rtu_SetScale(rtuState + idx, st.x, st.y);
}

void RL_Rtu_Scale(uint idx, float scalar)
{
    errorIfNotValidRTUIndex(idx, "RL_Rtu_Scale");
    copyMappedRtuToState(idx);
    Rtu_Scale(rtuState + idx, scalar);
}

void RL_Rtu_ScaleST(uint idx, Vector2f const &st)
{
    errorIfNotValidRTUIndex(idx, "RL_Rtu_ScaleST");
    copyMappedRtuToState(idx);
    Rtu_ScaleST(rtuState + idx, st);
}

void RL_Rtu_SetOffset(uint idx, Vector2f const &xy)
{
    errorIfNotValidRTUIndex(idx, "RL_Rtu_SetOffset");
    copyMappedRtuToState(idx);
    Rtu_SetOffset(rtuState + idx, xy);
}

void RL_Rtu_TranslateOffset(uint idx, Vector2f const &xy)
{
    errorIfNotValidRTUIndex(idx, "RL_Rtu_TranslateOffset");
    copyMappedRtuToState(idx);
    Rtu_TranslateOffset(rtuState + idx, xy.x, xy.y);
}

void RL_Rtu_SetTextureUnmanaged(uint idx, DGLuint glName, int wrapS, int wrapT)
{
    errorIfNotValidRTUIndex(idx, "RL_Rtu_SetTextureUnmanaged");
    copyMappedRtuToState(idx);
    rtuState[idx].texture.gl.name = glName;
    rtuState[idx].texture.gl.wrapS = wrapS;
    rtuState[idx].texture.gl.wrapT = wrapT;
    rtuState[idx].texture.flags &= ~TUF_TEXTURE_IS_MANAGED;
}

/**
 * Choose a specialised polytype from the specified primitive configuration.
 * @param flags  @ref rendpolyFlags
 */
static inline rendpolytype_t choosePolyType(int flags)
{
    return ((flags & RPF_SKYMASK)? PT_SKY_MASK :
            (flags & RPF_LIGHT)  ? PT_LIGHT :
            (flags & RPF_SHADOW) ? PT_SHADOW : PT_NORMAL);
}

// Prepare the final texture unit map for writing "normal" polygons, filling
// any gaps using a default configured texture unit.
static void prepareTextureUnitMap()
{
    // Map logical texture units to "real" ones known to the GL renderer.
    texunits[TU_PRIMARY]        = rtuMap[RTU_PRIMARY];
    texunits[TU_PRIMARY_DETAIL] = rtuMap[RTU_PRIMARY_DETAIL];
    texunits[TU_INTER]          = rtuMap[RTU_INTER];
    texunits[TU_INTER_DETAIL]   = rtuMap[RTU_INTER_DETAIL];
}

// Prepare the final texture unit map for writing "shiny" polygons, filling
// any gaps using a default configured texture unit.
static void prepareTextureUnitMapForShinyPoly()
{
    // Map logical texture units to "real" ones known to the GL renderer.
    texunits[TU_PRIMARY]        = rtuMap[RTU_REFLECTION];
    texunits[TU_PRIMARY_DETAIL] = &rtuDefault;
    texunits[TU_INTER]          = rtuMap[RTU_REFLECTION_MASK];
    texunits[TU_INTER_DETAIL]   = &rtuDefault;
}

void RL_AddPolyWithCoordsModulationReflection(primtype_t primType, int flags,
    uint numElements, rvertex_t const *vertices, ColorRawf const *colors,
    rtexcoord_t const *primaryCoords, rtexcoord_t const *interCoords,
    DGLuint modTex, ColorRawf const *modColor, rtexcoord_t const *modCoords,
    ColorRawf const *reflectionColors, rtexcoord_t const *reflectionCoords,
    rtexcoord_t const *reflectionMaskCoords)
{
    prepareTextureUnitMap();
    writePoly(primType, choosePolyType(flags), flags, numElements,
              vertices, colors, primaryCoords, interCoords, modTex, modColor, modCoords);

    // We are currently limited to two texture units, therefore shiny effects
    // must be drawn in a separate pass using a new primitive.
    if(!rtuMap[RTU_REFLECTION]->hasTexture()) return;

    prepareTextureUnitMapForShinyPoly();
    writePoly(primType, PT_SHINY, flags & ~RPF_HAS_DYNLIGHTS, numElements,
              vertices, reflectionColors, reflectionCoords, reflectionMaskCoords, 0, NULL, NULL);
}

void RL_AddPolyWithCoordsModulation(primtype_t primType, int flags, uint numElements,
    rvertex_t const *vertices, ColorRawf const *colors, rtexcoord_t const *primaryCoords,
    rtexcoord_t const *interCoords,
    DGLuint modTex, ColorRawf const *modColor, rtexcoord_t const *modCoords)
{
    prepareTextureUnitMap();
    writePoly(primType, choosePolyType(flags), flags, numElements,
              vertices, colors, primaryCoords, interCoords, modTex, modColor, modCoords);
}

void RL_AddPolyWithCoords(primtype_t primType, int flags, uint numElements,
    rvertex_t const *vertices, ColorRawf const *colors,
    rtexcoord_t const *primaryCoords, rtexcoord_t const *interCoords)
{
    prepareTextureUnitMap();
    writePoly(primType, choosePolyType(flags), flags, numElements,
              vertices, colors, primaryCoords, interCoords, 0, NULL, NULL);
}

void RL_AddPolyWithModulation(primtype_t primType, int flags, uint numElements,
    rvertex_t const *vertices, ColorRawf const *colors,
    DGLuint modTex, ColorRawf const *modColor, rtexcoord_t const *modCoords)
{
    prepareTextureUnitMap();
    writePoly(primType, choosePolyType(flags), flags, numElements,
              vertices, colors, NULL, NULL, modTex, modColor, modCoords);
}

void RL_AddPoly(primtype_t primType, int flags, uint numElements, rvertex_t const *vertices,
    ColorRawf const *colors)
{
    prepareTextureUnitMap();
    writePoly(primType, choosePolyType(flags), flags, numElements,
              vertices, colors, NULL, NULL, 0, NULL, NULL);
}

/**
 * Draws the privitives that match the conditions. If no condition bits
 * are given, all primitives are considered eligible.
 */
static void drawPrimitives(int conditions, uint coords[MAX_TEX_UNITS],
    rendlist_t const *list)
{
    // Should we just skip all this?
    if(conditions & DCF_SKIP) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    boolean bypass = false;
    if(TU(list, TU_INTER)->hasTexture())
    {
        // Is blending allowed?
        if(conditions & DCF_NO_BLEND) return;

        // Should all blended primitives be included?
        if(conditions & DCF_BLEND)
        {
            // The other conditions will be bypassed.
            bypass = true;
        }
    }

    // Check conditions dependant on primitive-specific values once before
    // entering the loop. If none of the conditions are true for this list
    // then we can bypass the skip tests completely during iteration.
    if(!bypass)
    {
        if(!(conditions & DCF_JUST_ONE_LIGHT) &&
           !(conditions & DCF_MANY_LIGHTS))
            bypass = true;
    }

    // Compile our list of indices.
    primhdr_t *hdr = (primhdr_t *) list->data;
    boolean skip = false;
    while(hdr->size != 0)
    {
        // Check for skip conditions.
        if(!bypass)
        {
            skip = false;
            if((conditions & DCF_JUST_ONE_LIGHT) &&
               (hdr->flags & PF_MANY_LIGHTS))
                skip = true;
            else if((conditions & DCF_MANY_LIGHTS) &&
                    (hdr->flags & PF_ONE_LIGHT))
                skip = true;
        }

        if(!skip)
        {
            // Render the primitive.
            if(conditions & DCF_SET_LIGHT_ENV)
            {
                // Use the correct texture and color for the light.
                glActiveTexture((conditions & DCF_SET_LIGHT_ENV0)? GL_TEXTURE0 : GL_TEXTURE1);
                GL_BindTextureUnmanaged(!renderTextures? 0 : hdr->modTex,
                                        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, hdr->modColor);
            }

            if(conditions & DCF_SET_MATRIX_DTEXTURE)
            {
                // Primitive-specific texture translation & scale.
                if(conditions & DCF_SET_MATRIX_DTEXTURE0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->texOffset[0], hdr->texOffset[1], 1);
                    glScalef(hdr->texScale[0], hdr->texScale[1], 1);
                }
                if(conditions & DCF_SET_MATRIX_DTEXTURE1)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->texOffset[0], hdr->texOffset[1], 1);
                    glScalef(hdr->texScale[0], hdr->texScale[1], 1);
                }
            }

            if(conditions & DCF_SET_MATRIX_TEXTURE)
            {
                // Primitive-specific texture translation & scale.
                if(conditions & DCF_SET_MATRIX_TEXTURE0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->ptexOffset[0], hdr->ptexOffset[1], 1);
                    glScalef(hdr->ptexScale[0], hdr->ptexScale[1], 1);
                }
                if(conditions & DCF_SET_MATRIX_TEXTURE1)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->ptexOffset[0], hdr->ptexOffset[1], 1);
                    glScalef(hdr->ptexScale[0], hdr->ptexScale[1], 1);
                }
            }

            if(conditions & DCF_SET_BLEND_MODE)
            {
                // Primitive-specific blending. Not used in all lists.
                GL_BlendMode(hdr->blendMode);
            }

            glBegin(hdr->type);
            for(short i = 0; i < hdr->numIndices; ++i)
            {
                uint const index = hdr->indices[i];
                for(short j = 0; j < numTexUnits; ++j)
                {
                    if(coords[j])
                        glMultiTexCoord2fv(GL_TEXTURE0 + j, texCoords[coords[j] - 1][index].st);
                }
                if(!(conditions & DCF_NO_COLOR))
                    glColor4ubv(colors[index].rgba);
                glVertex3fv(vertices[index].xyz);
            }
            glEnd();

            // Restore the texture matrix if changed.
            if(conditions & DCF_SET_MATRIX_TEXTURE)
            {
                if(conditions & DCF_SET_MATRIX_TEXTURE0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
                if(conditions & DCF_SET_MATRIX_TEXTURE1)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
            }
            if(conditions & DCF_SET_MATRIX_DTEXTURE)
            {
                if(conditions & DCF_SET_MATRIX_DTEXTURE0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
                if(conditions & DCF_SET_MATRIX_DTEXTURE1)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
            }

            DENG_ASSERT(!Sys_GLCheckError());
        }

        hdr = (primhdr_t *) ((byte *) hdr + hdr->size);
    }
}

/**
 * The first selected unit is active after this call.
 */
static void selectTexUnits(int count)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    for(int i = numTexUnits - 1; i >= count; i--)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glDisable(GL_TEXTURE_2D);
    }

    // Enable the selected units.
    for(int i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits) continue;

        glActiveTexture(GL_TEXTURE0 + i);
        glEnable(GL_TEXTURE_2D);
    }
}

/**
 * Set per-list GL state.
 *
 * @return  The conditions to select primitives.
 */
static int setupListState(listmode_t mode, rendlist_t *list)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(mode)
    {
    case LM_SKYMASK:
        // Render all primitives on the list without discrimination.
        return DCF_NO_COLOR;

    case LM_ALL: // All surfaces.
        // Should we do blending?
        if(TU(list, TU_INTER)->hasTexture())
        {
            // Blend between two textures, modulate with primary color.
#ifdef DENG_DEBUG
            if(numTexUnits < 2)
                Con_Error("setupListState: Not enough texture units.\n");
#endif
            selectTexUnits(2);

            rlBindTo(0, TU(list, TU_PRIMARY));
            rlBindTo(1, TU(list, TU_INTER));
            GL_ModulateTexture(2);

            float color[4];
            color[0] = color[1] = color[2] = 0;
            color[3] = TU(list, TU_INTER)->opacity;
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        }
        else if(!TU(list, TU_PRIMARY)->hasTexture())
        {
            // Opaque texture-less surface.
            return 0;
        }
        else
        {
            // Normal modulation.
            selectTexUnits(1);
            rlBind(TU(list, TU_PRIMARY));
            GL_ModulateTexture(1);
        }
        return DCF_SET_MATRIX_TEXTURE0 | (TU(list, TU_INTER)->hasTexture()? DCF_SET_MATRIX_TEXTURE1 : 0);

    case LM_LIGHT_MOD_TEXTURE:
        // Modulate sector light, dynamic light and regular texture.
        rlBindTo(1, TU(list, TU_PRIMARY));
        return DCF_SET_MATRIX_TEXTURE1 | DCF_SET_LIGHT_ENV0 | DCF_JUST_ONE_LIGHT | DCF_NO_BLEND;

    case LM_TEXTURE_PLUS_LIGHT:
        rlBindTo(0, TU(list, TU_PRIMARY));
        return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_LIGHT_ENV1 | DCF_NO_BLEND;

    case LM_FIRST_LIGHT:
        // Draw all primitives with more than one light
        // and all primitives which will have a blended texture.
        return DCF_SET_LIGHT_ENV0 | DCF_MANY_LIGHTS | DCF_BLEND;

    case LM_BLENDED: {
        // Only render the blended surfaces.
        if(!TU(list, TU_INTER)->hasTexture())
            return DCF_SKIP;

#ifdef DENG_DEBUG
        if(numTexUnits < 2)
            Con_Error("setupListState: Not enough texture units.\n");
#endif

        selectTexUnits(2);

        rlBindTo(0, TU(list, TU_PRIMARY));
        rlBindTo(1, TU(list, TU_INTER));

        GL_ModulateTexture(2);

        float color[4];
        color[0] = color[1] = color[2] = 0; color[3] = TU(list, TU_INTER)->opacity;
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1; }

    case LM_BLENDED_FIRST_LIGHT:
        // Only blended surfaces.
        if(!TU(list, TU_INTER)->hasTexture())
            return DCF_SKIP;
        return DCF_SET_MATRIX_TEXTURE1 | DCF_SET_LIGHT_ENV0;

    case LM_WITHOUT_TEXTURE:
        // Only render the primitives affected by dynlights.
        return 0;

    case LM_LIGHTS:
        // The light lists only contain dynlight primitives.
        rlBind(TU(list, TU_PRIMARY));
        return 0;

    case LM_BLENDED_MOD_TEXTURE:
        // Blending required.
        if(!TU(list, TU_INTER)->hasTexture())
            break;
    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
        // Texture for surfaces with (many) dynamic lights.
        // Should we do blending?
        if(TU(list, TU_INTER)->hasTexture())
        {
            // Mode 3 actually just disables the second texture stage,
            // which would modulate with primary color.
#ifdef DENG_DEBUG
            if(numTexUnits < 2)
                Con_Error("setupListState: Not enough texture units.\n");
#endif
            selectTexUnits(2);

            rlBindTo(0, TU(list, TU_PRIMARY));
            rlBindTo(1, TU(list, TU_INTER));

            GL_ModulateTexture(3);

            float color[4];
            color[0] = color[1] = color[2] = 0; color[3] = TU(list, TU_INTER)->opacity;
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            // Render all primitives.
            return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1;
        }
        // No modulation at all.
        selectTexUnits(1);
        rlBind(TU(list, TU_PRIMARY));
        GL_ModulateTexture(0);
        return DCF_SET_MATRIX_TEXTURE0 | (mode == LM_MOD_TEXTURE_MANY_LIGHTS ? DCF_MANY_LIGHTS : 0);

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        // Blending is not done now.
        if(TU(list, TU_INTER)->hasTexture())
            break;

        if(TU(list, TU_PRIMARY_DETAIL)->hasTexture())
        {
            selectTexUnits(2);
            GL_ModulateTexture(9); // Tex+Detail, no color.
            rlBindTo(0, TU(list, TU_PRIMARY));
            rlBindTo(1, TU(list, TU_PRIMARY_DETAIL));
            return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_DTEXTURE1;
        }
        else
        {
            selectTexUnits(1);
            GL_ModulateTexture(0);
            rlBind(TU(list, TU_PRIMARY));
            return DCF_SET_MATRIX_TEXTURE0;
        }
        break;

    case LM_ALL_DETAILS:
        if(TU(list, TU_PRIMARY_DETAIL)->hasTexture())
        {
            rlBind(TU(list, TU_PRIMARY_DETAIL));
            // Render all surfaces on the list.
            return DCF_SET_MATRIX_DTEXTURE0;
        }
        break;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        // Only unblended. Details are optional.
        if(TU(list, TU_INTER)->hasTexture())
            break;

        if(TU(list, TU_PRIMARY_DETAIL)->hasTexture())
        {
            selectTexUnits(2);
            GL_ModulateTexture(8);
            rlBindTo(0, TU(list, TU_PRIMARY));
            rlBindTo(1, TU(list, TU_PRIMARY_DETAIL));
            return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_DTEXTURE1;
        }
        else
        {
            // Normal modulation.
            selectTexUnits(1);
            GL_ModulateTexture(1);
            rlBind(TU(list, TU_PRIMARY));
            return DCF_SET_MATRIX_TEXTURE0;
        }
        break;

    case LM_BLENDED_DETAILS: {
        // We'll only render blended primitives.
        if(!TU(list, TU_INTER)->hasTexture())
            break;

        if(!TU(list, TU_PRIMARY_DETAIL)->hasTexture() ||
           !TU(list, TU_INTER_DETAIL)->hasTexture())
            break;

        rlBindTo(0, TU(list, TU_PRIMARY_DETAIL));
        rlBindTo(1, TU(list, TU_INTER_DETAIL));

        float color[4];
        color[0] = color[1] = color[2] = 0; color[3] = TU(list, TU_INTER_DETAIL)->opacity;
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        return DCF_SET_MATRIX_DTEXTURE0 | DCF_SET_MATRIX_DTEXTURE1; }

    case LM_SHADOW:
        // Render all primitives.
        if(TU(list, TU_PRIMARY)->hasTexture())
            rlBind(TU(list, TU_PRIMARY));
        else
            GL_BindTextureUnmanaged(0);

        if(!TU(list, TU_PRIMARY)->hasTexture())
        {
            // Apply a modelview shift.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            // Scale towards the viewpoint to avoid Z-fighting.
            glTranslatef(vOrigin[VX], vOrigin[VY], vOrigin[VZ]);
            glScalef(.99f, .99f, .99f);
            glTranslatef(-vOrigin[VX], -vOrigin[VY], -vOrigin[VZ]);
        }
        return 0;

    case LM_MASKED_SHINY:
        if(TU(list, TU_INTER)->hasTexture())
        {
            selectTexUnits(2);
            // The intertex holds the info for the mask texture.
            rlBindTo(1, TU(list, TU_INTER));
            float color[4];
            color[0] = color[1] = color[2] = 0; color[3] = 1.0f;
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        }

        // Intentional fall-through.

    case LM_ALL_SHINY:
    case LM_SHINY:
        rlBindTo(0, TU(list, TU_PRIMARY));
        if(!TU(list, TU_INTER)->hasTexture())
            selectTexUnits(1);

        // Render all primitives.
        if(mode == LM_ALL_SHINY)
            return DCF_SET_BLEND_MODE;
        if(mode == LM_MASKED_SHINY)
            return DCF_SET_BLEND_MODE | DCF_SET_MATRIX_TEXTURE1;
        return DCF_SET_BLEND_MODE | DCF_NO_BLEND;

    default:
        break;
    }

    // Unknown mode, let's not draw anything.
    return DCF_SKIP;
}

static void finishListState(listmode_t mode, rendlist_t *list)
{
    switch(mode)
    {
    default:
        break;

    case LM_SHADOW:
        if(!TU(list, TU_PRIMARY)->hasTexture())
        {
            // Restore original modelview matrix.
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
        break;

    case LM_SHINY:
    case LM_ALL_SHINY:
    case LM_MASKED_SHINY:
        GL_BlendMode(BM_NORMAL);
        break;
    }
}

static void finishPassState(listmode_t mode)
{
    switch(mode)
    {
    default:
        break;

    case LM_ALL:
    case LM_SHADOW:
    case LM_BLENDED:
    case LM_LIGHT_MOD_TEXTURE:
    case LM_TEXTURE_PLUS_LIGHT:
    case LM_LIGHTS:
    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
    case LM_ALL_DETAILS:
    case LM_BLENDED_DETAILS:
    case LM_SHINY:
    case LM_MASKED_SHINY:
    case LM_ALL_SHINY:
        if(usingFog)
            glDisable(GL_FOG);
        break;
    }
}

/**
 * Setup GL state for an entire rendering pass (compassing multiple lists).
 */
static void setupPassState(listmode_t mode, uint coords[MAX_TEX_UNITS])
{
    std::memset(coords, 0, sizeof(*coords) * MAX_TEX_UNITS);

    switch(mode)
    {
    case LM_SKYMASK:
        selectTexUnits(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        break;

    case LM_BLENDED:
        selectTexUnits(2);
    case LM_ALL:
        // The first texture unit is used for the main texture.
        coords[0] = TCA_MAIN + 1;
        coords[1] = TCA_BLEND + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
            glEnable(GL_FOG);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_LIGHT_MOD_TEXTURE:
    case LM_TEXTURE_PLUS_LIGHT:
        // Modulate sector light, dynamic light and regular texture.
        selectTexUnits(2);
        if(mode == LM_LIGHT_MOD_TEXTURE)
        {
            coords[0] = TCA_LIGHT + 1;
            coords[1] = TCA_MAIN + 1;
            GL_ModulateTexture(4); // Light * texture.
        }
        else
        {
            coords[0] = TCA_MAIN + 1;
            coords[1] = TCA_LIGHT + 1;
            GL_ModulateTexture(5); // Texture + light.
        }
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
            glEnable(GL_FOG);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_FIRST_LIGHT:
        // One light, no texture.
        selectTexUnits(1);
        coords[0] = TCA_LIGHT + 1;
        GL_ModulateTexture(6);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_BLENDED_FIRST_LIGHT:
        // One additive light, no texture.
        selectTexUnits(1);
        coords[0] = TCA_LIGHT + 1;
        GL_ModulateTexture(7); // Add light, no color.
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        break;

    case LM_WITHOUT_TEXTURE:
        selectTexUnits(0);
        GL_ModulateTexture(1);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_LIGHTS:
        selectTexUnits(1);
        coords[0] = TCA_MAIN + 1;
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, blackColor);
        }

        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD);
        break;

    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
    case LM_BLENDED_MOD_TEXTURE:
        // The first texture unit is used for the main texture.
        coords[0] = TCA_MAIN + 1;
        coords[1] = TCA_BLEND + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        coords[0] = TCA_MAIN + 1;
        coords[1] = TCA_MAIN + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        // Fog is allowed.
        if(usingFog)
            glEnable(GL_FOG);
        break;

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        coords[0] = TCA_MAIN + 1;
        coords[1] = TCA_MAIN + 1;
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;

    case LM_ALL_DETAILS:
        selectTexUnits(1);
        coords[0] = TCA_MAIN + 1;
        GL_ModulateTexture(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            float midGray[4];
            midGray[0] = midGray[1] = midGray[2] = .5f;
            midGray[3] = fogColor[3]; // The alpha is probably meaningless?
            glFogfv(GL_FOG_COLOR, midGray);
        }
        break;

    case LM_BLENDED_DETAILS:
        selectTexUnits(2);
        coords[0] = TCA_MAIN + 1;
        coords[1] = TCA_BLEND + 1;
        GL_ModulateTexture(3);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            float midGray[4];
            midGray[0] = midGray[1] = midGray[2] = .5f;
            midGray[3] = fogColor[3]; // The alpha is probably meaningless?
            glFogfv(GL_FOG_COLOR, midGray);
        }
        break;

    case LM_SHADOW:
        // A bit like 'negative lights'.
        selectTexUnits(1);
        coords[0] = TCA_MAIN + 1;
        GL_ModulateTexture(1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // Set normal fog, if it's enabled.
        if(usingFog)
        {
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, fogColor);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_NORMAL);
        break;

    case LM_SHINY:
        selectTexUnits(1);
        coords[0] = TCA_MAIN + 1;
        GL_ModulateTexture(1); // 8 for multitexture
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {   // Fog makes the shininess diminish in the distance.
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, blackColor);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    case LM_MASKED_SHINY:
        selectTexUnits(2);
        coords[0] = TCA_MAIN + 1;
        coords[1] = TCA_BLEND + 1; // the mask
        GL_ModulateTexture(8); // same as with details
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            glEnable(GL_FOG);
            glFogfv(GL_FOG_COLOR, blackColor);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    default:
        break;
    }
}

/**
 * Renders the given lists. They must not be empty.
 */
static void renderLists(listmode_t mode, rendlist_t **lists, uint num)
{
    // If the first list is empty, we do nothing. Normally we expect
    // all lists to contain something.
    if(!num || !lists[0]->last) return;

    // Setup GL state that's common to all the lists in this mode.
    uint coords[MAX_TEX_UNITS];
    setupPassState(mode, coords);

    // Draw each given list.
    for(uint i = 0; i < num; ++i)
    {
        rendlist_t *list = lists[i];

        // Setup GL state for this list, and draw the necessary subset of
        // primitives on the list.
        drawPrimitives(setupListState(mode, list), coords, list);

        // Some modes require cleanup.
        finishListState(mode, list);
    }

    finishPassState(mode);
}

/**
 * Extracts a selection of lists from the hash.
 */
static uint collectLists(listhash_t *table, rendlist_t **lists)
{
    // Collect a list of rendering lists.
    uint count = 0;
    for(uint i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(rendlist_t *it = table[i].first; it; it = it->next)
        {
            // Only non-empty lists are collected.
            if(it->last != NULL)
            {
                if(count == MAX_RLISTS)
                {
#ifdef DENG_DEBUG
                    Con_Error("collectLists: Exhausted MAX_RLISTS.\n");
#endif
                    return count;
                }
                lists[count++] = it;
            }
        }
    }
    return count;
}

/**
 * We have several different paths to accommodate both multitextured
 * details and dynamic lights. Details take precedence (they always cover
 * entire primitives, and usually *all* of the surfaces in a scene).
 */
void RL_RenderAllLists()
{
    // Pointers to all the rendering lists.
    rendlist_t *lists[MAX_RLISTS];
    uint count;

    DENG_ASSERT(!Sys_GLCheckError());
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

BEGIN_PROF( PROF_RL_RENDER_ALL );

    if(!freezeRLs) // Only update when lists are not frozen.
    {
        if(devRendSkyAlways)
            rDrawSky = true;
    }

    lists[0] = &skyMaskList;

    // Is the sky visible?
    if(rDrawSky &&!devRendSkyMode)
    {
BEGIN_PROF( PROF_RL_RENDER_SKYMASK );
        // We do not want to update color and/or depth.
        glDisable(GL_DEPTH_TEST);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        // Mask out stencil buffer, setting the drawn areas to 1.
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

        if(!devRendSkyAlways)
        {
            renderLists(LM_SKYMASK, lists, 1);
        }
        else
        {
            glClearStencil(1);
            glClear(GL_STENCIL_BUFFER_BIT);
        }

        // Re-enable update of color and depth.
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);

END_PROF( PROF_RL_RENDER_SKYMASK );

        // Now, only render where the stencil is set to 1.
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xffffffff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        Sky_Render();

        if(!devRendSkyAlways)
        {
            glClearStencil(0);
        }

        // Return GL state to normal.
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
    }

    // Render the real surfaces of the visible world.

    /**
     * Unlit Primitives (all normal lists).
     */
BEGIN_PROF( PROF_RL_RENDER_NORMAL );

    count = collectLists(plainHash, lists);
    if(IS_MTEX_DETAILS)
    {
        // Draw details for unblended surfaces in this pass.
        renderLists(LM_UNBLENDED_TEXTURE_AND_DETAIL, lists, count);

        // Blended surfaces.
        renderLists(LM_BLENDED, lists, count);
    }
    else
    {
        // Blending is done during this pass.
        renderLists(LM_ALL, lists, count);
    }
END_PROF( PROF_RL_RENDER_NORMAL );

    /**
     * Lit Primitives
     */
BEGIN_PROF( PROF_RL_RENDER_LIGHT );

    count = collectLists(litHash, lists);

    // If multitexturing is available, we'll use it to our advantage
    // when rendering lights.
    if(IS_MTEX_LIGHTS && dynlightBlend != 2)
    {
        if(IS_MUL)
        {
            // All (unblended) surfaces with exactly one light can be
            // rendered in a single pass.
            renderLists(LM_LIGHT_MOD_TEXTURE, lists, count);

            // Render surfaces with many lights without a texture, just
            // with the first light.
            renderLists(LM_FIRST_LIGHT, lists, count);
        }
        else                    // Additive ('foggy') lights.
        {
            renderLists(LM_TEXTURE_PLUS_LIGHT, lists, count);

            // Render surfaces with blending.
            renderLists(LM_BLENDED, lists, count);

            // Render the first light for surfaces with blending.
            // (Not optimal but shouldn't matter; texture is changed for
            // each primitive.)
            renderLists(LM_BLENDED_FIRST_LIGHT, lists, count);
        }
    }
    else // Multitexturing is not available for lights.
    {
        if(IS_MUL)
        {
            // Render all lit surfaces without a texture.
            renderLists(LM_WITHOUT_TEXTURE, lists, count);
        }
        else
        {
            if(IS_MTEX_DETAILS) // Draw detail textures using multitexturing.
            {
                // Unblended surfaces with a detail.
                renderLists(LM_UNBLENDED_TEXTURE_AND_DETAIL, lists, count);

                // Blended surfaces without details.
                renderLists(LM_BLENDED, lists, count);

                // Details for blended surfaces.
                renderLists(LM_BLENDED_DETAILS, lists, count);
            }
            else
            {
                renderLists(LM_ALL, lists, count);
            }
        }
    }

    /**
     * Dynamic Lights
     *
     * Draw all dynamic lights (always additive).
     */
    count = collectLists(dynHash, lists);
    if(dynlightBlend != 2)
        renderLists(LM_LIGHTS, lists, count);

END_PROF( PROF_RL_RENDER_LIGHT );

    /**
     * Texture Modulation Pass
     */
    if(IS_MUL)
    {
        // Finish the lit surfaces that didn't yet get a texture.
        count = collectLists(litHash, lists);
        if(IS_MTEX_DETAILS)
        {
            renderLists(LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL, lists, count);
            renderLists(LM_BLENDED_MOD_TEXTURE, lists, count);
            renderLists(LM_BLENDED_DETAILS, lists, count);
        }
        else
        {
            if(IS_MTEX_LIGHTS && dynlightBlend != 2)
            {
                renderLists(LM_MOD_TEXTURE_MANY_LIGHTS, lists, count);
            }
            else
            {
                renderLists(LM_MOD_TEXTURE, lists, count);
            }
        }
    }

    /**
     * Detail Modulation Pass
     *
     * If multitexturing is not available for details, we need to apply
     * them as an extra pass over all the detailed surfaces.
     */
    if(r_detail)
    {
        // Render detail textures for all surfaces that need them.
        count = collectLists(plainHash, lists);
        if(IS_MTEX_DETAILS)
        {
            // Blended detail textures.
            renderLists(LM_BLENDED_DETAILS, lists, count);
        }
        else
        {
            renderLists(LM_ALL_DETAILS, lists, count);

            count = collectLists(litHash, lists);
            renderLists(LM_ALL_DETAILS, lists, count);
        }
    }

    /**
     * Shiny Surfaces Pass
     *
     * Draw the shiny environment surfaces.
     *
     * If we have two texture units, the shiny masks will be
     * enabled.  Otherwise the masks are ignored.  The shine is
     * basically specular environmental additive light, multiplied
     * by the mask so that black texels from the mask produce areas
     * without shine.
     *
     * Walls with holes (so called 'masked textures') cannot be
     * shiny.
     */
BEGIN_PROF( PROF_RL_RENDER_SHINY );

    count = collectLists(shinyHash, lists);
    if(numTexUnits > 1)
    {
        // Render masked shiny surfaces in a separate pass.
        renderLists(LM_SHINY, lists, count);
        renderLists(LM_MASKED_SHINY, lists, count);
    }
    else
    {
        renderLists(LM_ALL_SHINY, lists, count);
    }
END_PROF( PROF_RL_RENDER_SHINY );

    /**
     * Shadow Pass: Objects and FakeRadio
     */
    int oldr = renderTextures;

    renderTextures = true;
BEGIN_PROF( PROF_RL_RENDER_SHADOW );

    count = collectLists(shadowHash, lists);

    renderLists(LM_SHADOW, lists, count);

END_PROF( PROF_RL_RENDER_SHADOW );

    renderTextures = oldr;

    // Return to the normal GL state.
    selectTexUnits(1);
    GL_ModulateTexture(1);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);
    glEnable(GL_BLEND);
    GL_BlendMode(BM_NORMAL);
    if(usingFog)
    {
        glEnable(GL_FOG);
        glFogfv(GL_FOG_COLOR, fogColor);
    }

    // Draw masked walls, sprites and models.
BEGIN_PROF( PROF_RL_RENDER_MASKED );

    Rend_DrawMasked();

    // Draw particles.
    Rend_RenderParticles();

    if(usingFog)
        glDisable(GL_FOG);

END_PROF( PROF_RL_RENDER_MASKED );
END_PROF( PROF_RL_RENDER_ALL );

    DENG_ASSERT(!Sys_GLCheckError());
}
