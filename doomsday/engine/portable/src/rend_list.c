/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * rend_list.c: Doomsday Rendering Lists v3.2
 *
 * 3.2 -- Shiny walls and floors
 * 3.1 -- Support for multiple shadow textures
 * 3.0 -- Multitexturing
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "def_main.h"
#include "m_profiler.h"

// MACROS ------------------------------------------------------------------

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

// \fixme Rlist allocation could be dynamic.
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

// TYPES -------------------------------------------------------------------

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
    TCA_LIGHT, // Dynlight texture coordinates.
    NUM_TEXCOORD_ARRAYS
};

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
    uint*           indices;

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

// \note: Slighty different representation than that passed to RL_AddPoly.
typedef struct rendlist_texmapunit_s {
    DGLuint         tex;
    int             magMode;
    float           blend; // Blend amount.
    blendmode_t     blendMode; // Currently used only with shiny pass.
} rendlist_texmapunit_t;

/**
 * The rendering list. When the list is resized, pointers in the primitives
 * need to be restored so that they point to the new list.
 */
typedef struct rendlist_s {
    struct rendlist_s* next;
    rendlist_texmapunit_t texmapunits[NUM_TEXMAP_UNITS];
    size_t          size; // Number of bytes allocated for the data.
    byte*           data; // Data for a number of polygons (The List).
    byte*           cursor; // A pointer to data, for reading/writing.
    primhdr_t*      last; // Pointer to the last primitive (or NULL).
} rendlist_t;

typedef struct listhash_s {
    rendlist_t*     first, *last;
} listhash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int skyhemispheres;
extern int devRendSkyMode;
extern byte devRendSkyAlways;
extern int useDynLights, dlBlend, simpleSky;
extern boolean usingFog;

extern byte freezeRLs;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int renderTextures = true;
int renderWireframe = false;
int useMultiTexLights = true;
int useMultiTexDetails = true;

// Rendering parameters for detail textures.
float detailFactor = .5f;
float detailScale = 4;

float torchColor[3] = {1, 1, 1};
int torchAdditive = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/**
 * The vertex arrays.
 */
static dgl_vertex_t* vertices;
static dgl_texcoord_t* texCoords[NUM_TEXCOORD_ARRAYS];
static dgl_color_t* colors;

static uint numVertices, maxVertices;

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

static boolean rendSky;

static float blackColor[4] = { 0, 0, 0, 0 };

// CODE --------------------------------------------------------------------

void RL_Register(void)
{
    // \todo Move cvars here.
}

static void rlBind(DGLuint tex, int magMode)
{
    if(!renderTextures)
        tex = 0;

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magMode);
    if(GL_state.useAnisotropic)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        GL_GetTexAnisoMul(texAniso));
#ifdef _DEBUG
{
GLenum  error;
if((error = glGetError()) != GL_NO_ERROR)
    Con_Error("OpenGL error: %i\n", error);
}
#endif
}

static void rlBind2(const rendlist_texmapunit_t* tmu)
{
    if(!tmu->tex)
        return;

    rlBind(tmu->tex, tmu->magMode);
}

static void rlBindTo(int unit, const rendlist_texmapunit_t* tmu)
{
    if(!tmu->tex)
        return;

    GL_ActiveTexture(GL_TEXTURE0 + (byte) unit);
    rlBind(tmu->tex, tmu->magMode);
}

static void clearHash(listhash_t* hash)
{
    memset(hash, 0, sizeof(listhash_t) * RL_HASH_SIZE);
}

/**
 * Called only once, from R_Init -> Rend_Init.
 */
void RL_Init(void)
{
    clearHash(plainHash);
    clearHash(litHash);
    clearHash(dynHash);
    clearHash(shadowHash);
    clearHash(shinyHash);

    memset(&skyMaskList, 0, sizeof(skyMaskList));
}

boolean RL_IsMTexLights(void)
{
    return IS_MTEX_LIGHTS;
}

boolean RL_IsMTexDetails(void)
{
    return IS_MTEX_DETAILS;
}

/**
 * The first selected unit is active after this call.
 */
static void selectTexUnits(int count)
{
    int                 i;

    // Disable extra units.
    for(i = numTexUnits - 1; i >= count; i--)
    {
        GL_ActiveTexture(GL_TEXTURE0 + (byte) i);
        glDisable(GL_TEXTURE_2D);
    }

    // Enable the selected units.
    for(i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits)
            continue;

        GL_ActiveTexture(GL_TEXTURE0 + (byte) i);
        glEnable(GL_TEXTURE_2D);
    }
}

static void clearVertices(void)
{
    numVertices = 0;
}

static void destroyVertices(void)
{
    uint                i;

    numVertices = maxVertices = 0;

    if(vertices)
        M_Free(vertices);
    vertices = NULL;

    if(colors)
        M_Free(colors);
    colors = NULL;

    for(i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
    {
        if(texCoords[i])
            M_Free(texCoords[i]);
        texCoords[i] = NULL;
    }
}

/**
 * Allocate vertices from the global vertex array.
 */
static uint allocateVertices(uint count)
{
    uint                i, base = numVertices;

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

        vertices = M_Realloc(vertices, sizeof(dgl_vertex_t) * maxVertices);
        colors = M_Realloc(colors, sizeof(dgl_color_t) * maxVertices);
        for(i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
        {
            texCoords[i] =
                M_Realloc(texCoords[i], sizeof(dgl_texcoord_t) * maxVertices);
        }
    }
    return base;
}

static void destroyList(rendlist_t* rl)
{
    // All the list data will be destroyed.
    if(rl->data)
        Z_Free(rl->data);
    rl->data = NULL;

#if _DEBUG
    Z_CheckHeap();
#endif

    rl->cursor = NULL;
    TU(rl, TU_INTER_DETAIL)->tex = 0;
    rl->last = NULL;
    rl->size = 0;
}

static void deleteHash(listhash_t* hash)
{
    uint                i;
    rendlist_t*         list, *next;

    for(i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(list = hash[i].first; list; list = next)
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
void RL_DeleteLists(void)
{
    // Delete all lists.
    deleteHash(plainHash);
    deleteHash(litHash);
    deleteHash(dynHash);
    deleteHash(shadowHash);
    deleteHash(shinyHash);

    destroyList(&skyMaskList);

    destroyVertices();

#ifdef _DEBUG
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
static void rewindList(rendlist_t* rl)
{
    rl->cursor = rl->data;
    rl->last = NULL;

    // The interpolation target must be explicitly set (in RL_AddPoly).
    TU(rl, TU_INTER)->tex = 0;
    TU(rl, TU_INTER)->blend = 0;
    TU(rl, TU_INTER_DETAIL)->tex = 0;
    TU(rl, TU_INTER_DETAIL)->blend = 0;
}

static void rewindHash(listhash_t* hash)
{
    uint                i;
    rendlist_t*         list;

    for(i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(list = hash[i].first; list; list = list->next)
            rewindList(list);
    }
}

/**
 * Called before rendering a frame.
 */
void RL_ClearLists(void)
{
    rewindHash(plainHash);
    rewindHash(litHash);
    rewindHash(dynHash);
    rewindHash(shadowHash);
    rewindHash(shinyHash);

    rewindList(&skyMaskList);

    // Clear the vertex array.
    clearVertices();

    // \fixme Does this belong here?
    skyhemispheres = 0;
}

static rendlist_t* createList(listhash_t* hash)
{
    rendlist_t*         list = Z_Calloc(sizeof(rendlist_t), PU_STATIC, 0);

    if(hash->last)
        hash->last->next = list;
    hash->last = list;
    if(!hash->first)
        hash->first = list;
    return list;
}

static __inline void copyTU(rendlist_texmapunit_t* lTU,
                            const rtexmapunit_t* rTU)
{
    lTU->tex = rTU->tex;
    lTU->magMode = rTU->magMode;
    lTU->blendMode = rTU->blendMode;
    lTU->blend = MINMAX_OF(0, rTU->blend, 1);
}

static __inline boolean compareTU(const rendlist_texmapunit_t* lTU,
                                  const rtexmapunit_t* rTU)
{
    if(lTU->tex == rTU->tex && lTU->magMode == rTU->magMode &&
       lTU->blend == rTU->blend)
        return true;

    return false;
}

static rendlist_t* getListFor(rendpolytype_t polyType,
                              const rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                              boolean useLights)
{
    listhash_t*         hash, *table;
    rendlist_t*         dest, *convertable = NULL;

    // Check for specialized rendering lists first.
    if(polyType == RPT_SKY_MASK)
    {
        return &skyMaskList;
    }

    // Choose the correct hash table.
    switch(polyType)
    {
    case RPT_SHINY:     table = shinyHash;  break;
    case RPT_SHADOW:    table = shadowHash; break;
    case RPT_LIGHT:     table = dynHash;    break;
    default:
        table = (useLights ? litHash : plainHash);
        break;
    }

    // Find/create a list in the hash.
    hash = &table[rTU[TU_PRIMARY].tex % RL_HASH_SIZE];
    for(dest = hash->first; dest; dest = dest->next)
    {
        if((polyType == RPT_SHINY &&
            compareTU(TU(dest, TU_PRIMARY), &rTU[TU_PRIMARY])) ||
           (polyType != RPT_SHINY &&
            compareTU(TU(dest, TU_PRIMARY), &rTU[TU_PRIMARY]) &&
            compareTU(TU(dest, TU_PRIMARY_DETAIL), &rTU[TU_PRIMARY_DETAIL])))
        {
            if(!TU(dest, TU_INTER)->tex && !rTU[TU_INTER].tex)
            {
                // This will do great.
                return dest;
            }

            // Is this eligible for conversion to a blended list?
            if(!dest->last && !convertable && rTU[TU_INTER].tex)
            {
                // If necessary, this empty list will be selected.
                convertable = dest;
            }

            // Possibly an exact match?
            if((polyType == RPT_SHINY &&
                compareTU(TU(dest, TU_INTER), &rTU[TU_INTER])) ||
               (polyType != RPT_SHINY &&
                compareTU(TU(dest, TU_INTER), &rTU[TU_INTER]) &&
                compareTU(TU(dest, TU_INTER_DETAIL), &rTU[TU_INTER_DETAIL])))
            {
                return dest;
            }
        }
    }

    // Did we find a convertable list?
    if(convertable)
    {   // This list is currently empty.
        if(polyType == RPT_SHINY)
        {
            copyTU(TU(convertable, TU_INTER), &rTU[TU_INTER]);
        }
        else
        {
            copyTU(TU(convertable, TU_INTER), &rTU[TU_INTER]);
            copyTU(TU(convertable, TU_INTER_DETAIL), &rTU[TU_INTER_DETAIL]);
        }

        return convertable;
    }

    // Create a new list.
    dest = createList(hash);

    // Init the info.
    if(polyType == RPT_SHINY)
    {
        copyTU(TU(dest, TU_PRIMARY), &rTU[TU_PRIMARY]);
        if(rTU[TU_INTER].tex)
            copyTU(TU(dest, TU_INTER), &rTU[TU_INTER]);
    }
    else
    {
        copyTU(TU(dest, TU_PRIMARY), &rTU[TU_PRIMARY]);
        copyTU(TU(dest, TU_PRIMARY_DETAIL), &rTU[TU_PRIMARY_DETAIL]);

        if(rTU[TU_INTER].tex)
        {
            copyTU(TU(dest, TU_INTER), &rTU[TU_INTER]);
            copyTU(TU(dest, TU_INTER_DETAIL), &rTU[TU_INTER_DETAIL]);
        }
    }

    return dest;
}

/**
 * @return              Pointer to the start of the allocated data.
 */
static void* allocateData(rendlist_t* list, int bytes)
{
    size_t              required;
    int                 startOffset = list->cursor - list->data;
    primhdr_t*          hdr;

    if(bytes <= 0)
        return NULL;

    // We require the extra bytes because we want that the end of the
    // list data is always safe for writing-in-advance. This is needed
    // when the 'end of data' marker is written in RL_AddPoly.
    required = startOffset + bytes + LIST_DATA_PADDING;

    // First check that the data buffer of the list is large enough.
    if(required > list->size)
    {
        // Offsets must be preserved.
        byte*               oldData = list->data;
        int                 cursorOffset = -1;
        int                 lastOffset = -1;

        if(list->cursor)
            cursorOffset = list->cursor - oldData;
        if(list->last)
            lastOffset = (byte *) list->last - oldData;

        // Allocate more memory for the data buffer.
        if(list->size == 0)
            list->size = 1024;
        while(list->size < required)
            list->size *= 2;

        list->data = Z_Realloc(list->data, list->size, PU_STATIC);

        // Restore main pointers.
        list->cursor =
            (cursorOffset >= 0 ? list->data + cursorOffset : list->data);
        list->last =
            (lastOffset >= 0 ? (primhdr_t *) (list->data + lastOffset) : NULL);

        // Restore in-list pointers.
        if(oldData)
        {
            hdr = (primhdr_t *) list->data;
            while(hdr <= list->last)
            {
                if(hdr->indices != NULL)
                {
                    hdr->indices =
                        (uint *) (list->data +
                                  ((byte *) hdr->indices - oldData));
                }

                // Check here in the end; primitive composition may be
                // in progress.
                if(hdr->size == 0)
                    break;
                hdr = (primhdr_t*) ((byte*) hdr + hdr->size);
            }
        }
    }

    // Advance the cursor.
    list->cursor += bytes;

    return list->data + startOffset;
}

static void allocateIndices(rendlist_t* list, uint numIndices)
{
    void*               indices;

    list->last->numIndices = numIndices;
    indices = allocateData(list, sizeof(uint) * numIndices);

    // list->last may change during allocateData.
    list->last->indices = indices;
}

static void endWrite(rendlist_t* list)
{
    // The primitive has been written, update the size in the header.
    list->last->size = list->cursor - (byte *) list->last;

    // Write the end marker (which will be overwritten by the next
    // primitive). The idea is that this zero is interpreted as the
    // size of the following primhdr.
    *(int *) list->cursor = 0;
}

static void writePrimitive(const rendlist_t* list, uint base,
                           const rvertex_t* rvertices,
                           const rtexcoord_t* coords,
                           const rtexcoord_t* coords1,
                           const rtexcoord_t* coords2,
                           const rcolor_t* rcolors, uint numVertices,
                           rendpolytype_t type)
{
    uint                i;

    for(i = 0; i < numVertices; ++i)
    {
        // Vertex.
        {
        const rvertex_t*    rvtx = &rvertices[i];
        dgl_vertex_t*        vtx = &vertices[base + i];

        vtx->xyz[0] = rvtx->pos[VX];
        vtx->xyz[1] = rvtx->pos[VZ];
        vtx->xyz[2] = rvtx->pos[VY];
        }

        if(type == RPT_SKY_MASK)
            continue; // Sky masked polys need nothing more.

        // Primary texture coordinates.
        if(TU(list, TU_PRIMARY)->tex)
        {
            const rtexcoord_t*  rtc = &coords[i];
            dgl_texcoord_t*      tc = &texCoords[TCA_MAIN][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // Secondary texture coordinates.
        if(TU(list, TU_INTER)->tex)
        {
            const rtexcoord_t*  rtc = &coords1[i];
            dgl_texcoord_t*      tc = &texCoords[TCA_BLEND][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // First light texture coordinates.
        if((list->last->flags & PF_IS_LIT) && IS_MTEX_LIGHTS)
        {
            const rtexcoord_t*  rtc = &coords2[i];
            dgl_texcoord_t*      tc = &texCoords[TCA_LIGHT][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // Color.
        {
        const rcolor_t*     rcolor = &rcolors[i];
        dgl_color_t*         color = &colors[base + i];

        color->rgba[CR] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CR], 1));
        color->rgba[CG] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CG], 1));
        color->rgba[CB] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CB], 1));
        color->rgba[CA] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CA], 1));
        }
    }
}

/**
 * Adds one or more polys the render lists depending on configuration.
 */
static void addPoly(primtype_t type, rendpolytype_t polyType,
                    const rvertex_t* rvertices,
                    const rtexcoord_t* rtexcoords,
                    const rtexcoord_t* rtexcoords1,
                    const rtexcoord_t* rtexcoords2, const rcolor_t* rcolors,
                    uint numVertices, blendmode_t blendMode,
                    uint numLights,
                    DGLuint modTex, float modColor[3],
                    const rtexmapunit_t rTU[NUM_TEXMAP_UNITS])
{
    uint                i, base, primSize, numIndices;
    rendlist_t*         li;
    primhdr_t*          hdr;
    boolean             useLights =
        (polyType != RPT_LIGHT && numLights > 0);

BEGIN_PROF( PROF_RL_ADD_POLY );

BEGIN_PROF( PROF_RL_GET_LIST );

    // Find/create a rendering list for the polygon's texture.
    li = getListFor(polyType, rTU, useLights);

END_PROF( PROF_RL_GET_LIST );

    primSize = numVertices;
    numIndices = numVertices;
    base = allocateVertices(primSize);

    hdr = allocateData(li, sizeof(primhdr_t));
    assert(hdr);
    li->last = hdr; // This becomes the new last primitive.

    // Primitive-specific blending mode.
    hdr->blendMode = blendMode;
    hdr->size = 0;
    hdr->indices = NULL;
    hdr->numIndices = 0;
    hdr->flags = 0;
    if(numLights > 1)
        hdr->flags |= PF_MANY_LIGHTS;
    else if(numLights == 1)
        hdr->flags |= PF_ONE_LIGHT;
    hdr->modTex = modTex;
    hdr->modColor[CR] = modColor? modColor[CR] : 0;
    hdr->modColor[CG] = modColor? modColor[CG] : 0;
    hdr->modColor[CB] = modColor? modColor[CB] : 0;
    hdr->modColor[CA] = 0;

    if(polyType == RPT_SHINY && rTU[TU_INTER].tex)
    {
        hdr->ptexScale[0] = rTU[TU_INTER].scale[0];
        hdr->ptexScale[1] = rTU[TU_INTER].scale[1];
        hdr->ptexOffset[0] = rTU[TU_INTER].offset[0];
        hdr->ptexOffset[1] = rTU[TU_INTER].offset[1];
    }
    else if(rTU[TU_PRIMARY].tex)
    {
        hdr->ptexScale[0] = rTU[TU_PRIMARY].scale[0];
        hdr->ptexScale[1] = rTU[TU_PRIMARY].scale[1];
        hdr->ptexOffset[0] = rTU[TU_PRIMARY].offset[0];
        hdr->ptexOffset[1] = rTU[TU_PRIMARY].offset[1];
    }

    if(rTU[TU_PRIMARY_DETAIL].tex)
    {
        hdr->texScale[0] = rTU[TU_PRIMARY_DETAIL].scale[0];
        hdr->texScale[1] = rTU[TU_PRIMARY_DETAIL].scale[1];
        hdr->texOffset[0] = rTU[TU_PRIMARY_DETAIL].offset[0];
        hdr->texOffset[1] = rTU[TU_PRIMARY_DETAIL].offset[1];
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

    writePrimitive(li, base, rvertices, rtexcoords, rtexcoords1,
                   rtexcoords2, rcolors, numVertices, polyType);
    endWrite(li);

END_PROF( PROF_RL_ADD_POLY );
}

void RL_AddPoly(primtype_t type, rendpolytype_t polyType,
                const rvertex_t* rvertices,
                const rtexcoord_t* rtexcoords, const rtexcoord_t* rtexcoords1,
                const rtexcoord_t* rtexcoords2,
                const rcolor_t* rcolors,
                uint numVertices, uint numLights,
                DGLuint modTex, float modColor[3],
                const rtexmapunit_t rTU[NUM_TEXMAP_UNITS])
{
    if(numVertices < 3)
        return; // huh?

    if(type < PT_FIRST || type >= NUM_PRIM_TYPES)
        Con_Error("RL_AddPoly: Unknown primtype %i.", type);

    addPoly(type, polyType, rvertices, rtexcoords, rtexcoords1, rtexcoords2,
            rcolors, numVertices, rTU[TU_PRIMARY].blendMode, numLights, modTex, modColor, rTU);
}

/**
 * Draws the privitives that match the conditions. If no condition bits
 * are given, all primitives are considered eligible.
 */
static void drawPrimitives(int conditions, uint coords[MAX_TEX_UNITS],
                           const rendlist_t* list)
{
    primhdr_t*              hdr;
    boolean                 skip, bypass = false;

    // Should we just skip all this?
    if(conditions & DCF_SKIP)
        return;

    if(TU(list, TU_INTER)->tex)
    {
        // Is blending allowed?
        if(conditions & DCF_NO_BLEND)
            return;

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
    hdr = (primhdr_t *) list->data;
    skip = false;
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
        {   // Render the primitive.
            int                 i;

            if(conditions & DCF_SET_LIGHT_ENV)
            {   // Use the correct texture and color for the light.
                GL_ActiveTexture((conditions & DCF_SET_LIGHT_ENV0)? GL_TEXTURE0 : GL_TEXTURE1);
                rlBind(hdr->modTex, GL_LINEAR);
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, hdr->modColor);
                // Make sure the light is not repeated.
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
            }

            if(conditions & DCF_SET_MATRIX_DTEXTURE)
            {   // Primitive-specific texture translation & scale.
                if(conditions & DCF_SET_MATRIX_DTEXTURE0)
                {
                    GL_ActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->texOffset[0], hdr->texOffset[1], 1);
                    glScalef(hdr->texScale[0], hdr->texScale[1], 1);
                }
                if(conditions & DCF_SET_MATRIX_DTEXTURE1)
                {   // Primitive-specific texture translation & scale.
                    GL_ActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->texOffset[0], hdr->texOffset[1], 1);
                    glScalef(hdr->texScale[0], hdr->texScale[1], 1);
                }
            }

            if(conditions & DCF_SET_MATRIX_TEXTURE)
            {   // Primitive-specific texture translation & scale.
                if(conditions & DCF_SET_MATRIX_TEXTURE0)
                {
                    GL_ActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->ptexOffset[0], hdr->ptexOffset[1], 1);
                    glScalef(hdr->ptexScale[0], hdr->ptexScale[1], 1);
                }
                if(conditions & DCF_SET_MATRIX_TEXTURE1)
                {
                    GL_ActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPushMatrix();
                    glLoadIdentity();
                    glTranslatef(hdr->ptexOffset[0], hdr->ptexOffset[1], 1);
                    glScalef(hdr->ptexScale[0], hdr->ptexScale[1], 1);
                }
            }

            if(conditions & DCF_SET_BLEND_MODE)
            {   // Primitive-specific blending. Not used in all lists.
                GL_BlendMode(hdr->blendMode);
            }

            glBegin(hdr->type);
            for(i = 0; i < hdr->numIndices; ++i)
            {
                int                 j;
                uint                index = hdr->indices[i];

                for(j = 0; j < GL_state.maxTexUnits && j < MAX_TEX_UNITS; ++j)
                {
                    if(coords[j])
                    {
                        glMultiTexCoord2fvARB(GL_TEXTURE0 + j,
                                              texCoords[coords[j] - 1][index].st);
                    }
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
                    GL_ActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
                if(conditions & DCF_SET_MATRIX_TEXTURE1)
                {
                    GL_ActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
            }
            if(conditions & DCF_SET_MATRIX_DTEXTURE)
            {
                if(conditions & DCF_SET_MATRIX_DTEXTURE0)
                {
                    GL_ActiveTexture(GL_TEXTURE0);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
                if(conditions & DCF_SET_MATRIX_DTEXTURE1)
                {
                    GL_ActiveTexture(GL_TEXTURE1);
                    glMatrixMode(GL_TEXTURE);
                    glPopMatrix();
                }
            }

#ifdef _DEBUG
Sys_CheckGLError();
#endif
        }

        hdr = (primhdr_t *) ((byte *) hdr + hdr->size);
    }
}

/**
 * Set per-list GL state.
 *
 * @return                  The conditions to select primitives.
 */
static int setupListState(listmode_t mode, rendlist_t* list)
{
    switch(mode)
    {
    case LM_SKYMASK:
        // Render all primitives on the list without discrimination.
        return DCF_NO_COLOR;

    case LM_ALL: // All surfaces.
        // Should we do blending?
        if(TU(list, TU_INTER)->tex)
        {
            float           color[4];

            // Blend between two textures, modulate with primary color.
#ifdef _DEBUG
if(numTexUnits < 2)
    Con_Error("setupListState: Not enough texture units.\n");
#endif
            selectTexUnits(2);

            rlBindTo(0, TU(list, TU_PRIMARY));
            rlBindTo(1, TU(list, TU_INTER));
            GL_ModulateTexture(2);

            color[0] = color[1] = color[2] = 0;
            color[3] = TU(list, TU_INTER)->blend;
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        }
        else
        {
            // Normal modulation.
            selectTexUnits(1);
            rlBind2(TU(list, TU_PRIMARY));
            GL_ModulateTexture(1);
        }
        return DCF_SET_MATRIX_TEXTURE0 | (TU(list, TU_INTER)->tex? DCF_SET_MATRIX_TEXTURE1 : 0);

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

    case LM_BLENDED:
        {
        float           color[4];
        // Only render the blended surfaces.
        if(!TU(list, TU_INTER)->tex)
            return DCF_SKIP;
#ifdef _DEBUG
if(numTexUnits < 2)
    Con_Error("setupListState: Not enough texture units.\n");
#endif

        selectTexUnits(2);

        rlBindTo(0, TU(list, TU_PRIMARY));
        rlBindTo(1, TU(list, TU_INTER));

        GL_ModulateTexture(2);

        color[0] = color[1] = color[2] = 0; color[3] = TU(list, TU_INTER)->blend;
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1;
        }
    case LM_BLENDED_FIRST_LIGHT:
        // Only blended surfaces.
        if(!TU(list, TU_INTER)->tex)
            return DCF_SKIP;
        return DCF_SET_MATRIX_TEXTURE1 | DCF_SET_LIGHT_ENV0;

    case LM_WITHOUT_TEXTURE:
        // Only render the primitives affected by dynlights.
        return 0;

    case LM_LIGHTS:
        // The light lists only contain dynlight primitives.
        rlBind2(TU(list, TU_PRIMARY));
        // Make sure the texture is not repeated.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return 0;

    case LM_BLENDED_MOD_TEXTURE:
        // Blending required.
        if(!TU(list, TU_INTER)->tex)
            break;
    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
        // Texture for surfaces with (many) dynamic lights.
        // Should we do blending?
        if(TU(list, TU_INTER)->tex)
        {
            float           color[4];

            // Mode 3 actually just disables the second texture stage,
            // which would modulate with primary color.
#ifdef _DEBUG
if(numTexUnits < 2)
    Con_Error("setupListState: Not enough texture units.\n");
#endif
            selectTexUnits(2);

            rlBindTo(0, TU(list, TU_PRIMARY));
            rlBindTo(1, TU(list, TU_INTER));

            GL_ModulateTexture(3);

            color[0] = color[1] = color[2] = 0; color[3] = TU(list, TU_INTER)->blend;
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            // Render all primitives.
            return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1;
        }
        // No modulation at all.
        selectTexUnits(1);
        rlBind2(TU(list, TU_PRIMARY));
        GL_ModulateTexture(0);
        return DCF_SET_MATRIX_TEXTURE0 | (mode == LM_MOD_TEXTURE_MANY_LIGHTS ? DCF_MANY_LIGHTS : 0);

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        // Blending is not done now.
        if(TU(list, TU_INTER)->tex)
            break;
        if(TU(list, TU_PRIMARY_DETAIL)->tex)
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
            rlBind2(TU(list, TU_PRIMARY));
            return DCF_SET_MATRIX_TEXTURE0;
        }
        break;

    case LM_ALL_DETAILS:
        if(TU(list, TU_PRIMARY_DETAIL)->tex)
        {
            rlBind2(TU(list, TU_PRIMARY_DETAIL));
            // Render all surfaces on the list.
            return DCF_SET_MATRIX_DTEXTURE0;
        }
        break;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        // Only unblended. Details are optional.
        if(TU(list, TU_INTER)->tex)
            break;
        if(TU(list, TU_PRIMARY_DETAIL)->tex)
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
            rlBind2(TU(list, TU_PRIMARY));
            return DCF_SET_MATRIX_TEXTURE0;
        }
        break;

    case LM_BLENDED_DETAILS:
        {
        float           color[4];

        // We'll only render blended primitives.
        if(!TU(list, TU_INTER)->tex)
            break;
        if(!TU(list, TU_PRIMARY_DETAIL)->tex ||
           !TU(list, TU_INTER_DETAIL)->tex)
            break;

        rlBindTo(0, TU(list, TU_PRIMARY_DETAIL));
        rlBindTo(1, TU(list, TU_INTER_DETAIL));

        color[0] = color[1] = color[2] = 0; color[3] = TU(list, TU_INTER_DETAIL)->blend;
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        return DCF_SET_MATRIX_DTEXTURE0 | DCF_SET_MATRIX_DTEXTURE1;
        }
    case LM_SHADOW:
        // Render all primitives.
        if(TU(list, TU_PRIMARY)->tex)
            rlBind2(TU(list, TU_PRIMARY));
        else
            rlBind(0, GL_LINEAR);

        if(!TU(list, TU_PRIMARY)->tex)
        {
            // Apply a modelview shift.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            // Scale towards the viewpoint to avoid Z-fighting.
            glTranslatef(vx, vy, vz);
            glScalef(.99f, .99f, .99f);
            glTranslatef(-vx, -vy, -vz);
        }
        return 0;

    case LM_MASKED_SHINY:
        if(TU(list, TU_INTER)->tex)
        {
            selectTexUnits(2);
            // The intertex holds the info for the mask texture.
            rlBindTo(1, TU(list, TU_INTER));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            {float           color[4];
            color[0] = color[1] = color[2] = 0; color[3] = 1.0f;
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color); }
        }
    case LM_ALL_SHINY:
    case LM_SHINY:
        rlBindTo(0, TU(list, TU_PRIMARY));
        if(!TU(list, TU_INTER)->tex)
            selectTexUnits(1);

        // Make sure the texture is not clamped.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

static void finishListState(listmode_t mode, rendlist_t* list)
{
    switch(mode)
    {
    default:
        break;

    case LM_SHADOW:
        if(!TU(list, TU_PRIMARY)->tex)
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
    memset(coords, 0, sizeof(*coords) * MAX_TEX_UNITS);

    switch(mode)
    {
    case LM_SKYMASK:
        selectTexUnits(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // We don't want to write to the color buffer.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE);
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
            float               midGray[4];

            glEnable(GL_FOG);
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
            float               midGray[4];

            glEnable(GL_FOG);
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
static void renderLists(listmode_t mode, rendlist_t** lists, uint num)
{
    uint                i;
    rendlist_t*         list;
    uint                coords[MAX_TEX_UNITS];

    // If the first list is empty, we do nothing. Normally we expect
    // all lists to contain something.
    if(num == 0 || lists[0]->last == NULL)
        return;

    // Setup GL state that's common to all the lists in this mode.
    setupPassState(mode, coords);

    // Draw each given list.
    for(i = 0; i < num; ++i)
    {
        list = lists[i];

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
static uint collectLists(listhash_t* table, rendlist_t** lists)
{
    uint                i, count;
    rendlist_t*         it;

    // Collect a list of rendering lists.
    count = 0;
    for(i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(it = table[i].first; it; it = it->next)
        {
            // Only non-empty lists are collected.
            if(it->last != NULL)
            {
                if(count == MAX_RLISTS)
                {
#ifdef _DEBUG
Con_Error("collectLists: Ran out of MAX_RLISTS.\n");
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
void RL_RenderAllLists(void)
{
    uint count;
    // Pointers to all the rendering lists.
    rendlist_t* lists[MAX_RLISTS];

BEGIN_PROF( PROF_RL_RENDER_ALL );

    if(!freezeRLs) // Only update when lists are not frozen.
    {
        if(devRendSkyAlways)
        {
            rendSky = true;
            skyhemispheres |= SKYHEMI_UPPER | SKYHEMI_LOWER;
        }
        else
            rendSky = !P_IsInVoid(viewPlayer);
    }

    // When in the void we don't render a sky.
    // \fixme We could use a stencil when rendering the sky, using the
    // already collected skymask polys as a mask.
    if(rendSky && !devRendSkyMode)
        // The sky might be visible. Render the needed hemispheres.
        Rend_RenderSky(skyhemispheres);

    // Mask the sky in the Z-buffer.
    lists[0] = &skyMaskList;

    // \fixme As we arn't rendering the sky when in the void we have
    // have no need to render the skymask.
BEGIN_PROF( PROF_RL_RENDER_SKYMASK );
    if(rendSky)
        renderLists(LM_SKYMASK, lists, 1);
END_PROF( PROF_RL_RENDER_SKYMASK );

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
    if(IS_MTEX_LIGHTS && dlBlend != 2)
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
    if(dlBlend != 2)
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
            if(IS_MTEX_LIGHTS && dlBlend != 2)
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
     * by the mask so that black texels in the mask produce areas
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
    {
    int oldr = renderTextures;

    renderTextures = true;
BEGIN_PROF( PROF_RL_RENDER_SHADOW );

    count = collectLists(shadowHash, lists);

    renderLists(LM_SHADOW, lists, count);

END_PROF( PROF_RL_RENDER_SHADOW );

    renderTextures = oldr;
    }

    // Return to the normal GL state.
    selectTexUnits(1);
    GL_ModulateTexture(1);
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
}
