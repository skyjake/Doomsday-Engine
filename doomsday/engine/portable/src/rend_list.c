/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
#include "de_dgl.h"
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
#define DCF_SET_MATRIX_TEXTURE0     0x00000080
#define DCF_SET_MATRIX_TEXTURE1     0x00000100
#define DCF_SET_MATRIX_TEXTURE      (DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1)
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
    TCA_LIGHT, // Glow texture coordinates.
    NUM_TEXCOORD_ARRAYS
};

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

    glprimtype_t    type;
    blendmode_t     blendMode; // BM_* Primitive-specific blending mode.
    uint            primSize; // Number of vertices in the primitive.

    // Elements in the vertex array for this primitive.
    // The indices are always contiguous: indices[0] is the base, and
    // indices[1...n] > indices[0].
    // All indices in the range indices[0]...indices[n] are used by this
    // primitive (some are shared).
    ushort          numIndices;
    uint*           indices;

    // The number of lights affecting the primitive.
    ushort          numLights;

    // Texture matrix manipulations (not used in all lists).
    float           texOffset[2], texScale[2];

    // Some primitives are modulated with an additional texture and color
    // using multitexturing (if available), depending on the list state.
    // Example: first light affecting the primitive.
    DGLuint         modTex;
    float           modColor[3];
} primhdr_t;

// Rendering List 'has' flags.
#define RLF_LIGHTS          0x1 // Primitives are dynamic lights.
#define RLF_BLENDED         0x2 // List contains only texblended prims.

/**
 * The rendering list. When the list is resized, pointers in the primitives
 * need to be restored so that they point to the new list.
 */
typedef struct rendlist_s {
    struct rendlist_s* next;
    int             flags;
    rtexmapunit_t   texmapunits[NUM_TEXMAP_UNITS];
    float           interPos; // 0 = primary, 1 = secondary texture
    size_t          size; // Number of bytes allocated for the data.
    byte*           data; // Data for a number of polygons (The List).
    byte*           cursor; // A pointer to data, for reading/writing.
    primhdr_t*      last; // Pointer to the last primitive (or NULL).
} rendlist_t;

typedef struct listhash_s {
    rendlist_t*     first, *last;
} listhash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

vissprite_t* R_NewVisSprite(void);
boolean RLIT_DynGetFirst(const dynlight_t* dyn, void* data);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int skyhemispheres;
extern int devSkyMode;
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
static gl_vertex_t* vertices;
static gl_texcoord_t* texCoords[NUM_TEXCOORD_ARRAYS];
static gl_color_t* colors;

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

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void RL_AddMaskedPoly(const rvertex_t* rvertices,
                      const rcolor_t* rcolors, float wallLength,
                      float texWidth, float texHeight,
                      const float texOffset[2], blendmode_t blendMode,
                      uint lightListIdx, boolean glow, boolean masked,
                      const rladdpoly_params_t* params)
{
    vissprite_t*        vis = R_NewVisSprite();
    int                 i, c;
    float               midpoint[3];

    midpoint[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    midpoint[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    midpoint[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;

    vis->type = VSPR_MASKED_WALL;
    vis->lumIdx = 0;
    vis->center[VX] = midpoint[VX];
    vis->center[VY] = midpoint[VY];
    vis->center[VZ] = midpoint[VZ];
    vis->isDecoration = false;
    vis->distance = Rend_PointDist2D(midpoint);
    vis->data.wall.tex = params->texmapunits[TMU_PRIMARY].tex;
    vis->data.wall.magMode = params->texmapunits[TMU_PRIMARY].magMode;
    vis->data.wall.masked = masked;
    for(i = 0; i < 4; ++i)
    {
        vis->data.wall.vertices[i].pos[VX] = rvertices[i].pos[VX];
        vis->data.wall.vertices[i].pos[VY] = rvertices[i].pos[VY];
        vis->data.wall.vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(c = 0; c < 4; ++c)
        {
            vis->data.wall.vertices[i].color[c] =
                MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }
    vis->data.wall.texCoord[0][VX] = texOffset[VX] / texWidth;
    vis->data.wall.texCoord[1][VX] =
        vis->data.wall.texCoord[0][VX] + wallLength / texWidth;
    vis->data.wall.texCoord[0][VY] = texOffset[VY] / texHeight;
    vis->data.wall.texCoord[1][VY] =
        vis->data.wall.texCoord[0][VY] +
            (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / texHeight;
    vis->data.wall.blendMode = blendMode;

    //// \fixme Semitransparent masked polys arn't lit atm
    if(!glow && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        dynlight_t*         dyn = NULL;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        DL_ListIterator(lightListIdx, &dyn, RLIT_DynGetFirst);

        vis->data.wall.modTex = dyn->texture;
        vis->data.wall.modTexCoord[0][0] = dyn->s[0];
        vis->data.wall.modTexCoord[0][1] = dyn->s[1];
        vis->data.wall.modTexCoord[1][0] = dyn->t[0];
        vis->data.wall.modTexCoord[1][1] = dyn->t[1];
        for(c = 0; c < 3; ++c)
            vis->data.wall.modColor[c] = dyn->color[c];
    }
    else
    {
        vis->data.wall.modTex = 0;
    }
}

void RL_SelectTexCoordArray(int unit, int index)
{
    void*               coords[MAX_TEX_UNITS];

    // Does this unit exist?
    if(unit >= numTexUnits)
        return;

    memset(coords, 0, sizeof(coords));
    coords[unit] = texCoords[index];
    DGL_Arrays(NULL, NULL, numTexUnits, coords, 0);
}

static void rlBind(const rtexmapunit_t* tmu)
{
    GL_BindTexture(renderTextures ? tmu->tex : 0, tmu->magMode);
}

static void rlBindTo(int unit, const rtexmapunit_t* tmu)
{
    DGL_SetInteger(DGL_ACTIVE_TEXTURE, unit);
    GL_BindTexture(renderTextures ? tmu->tex : 0, tmu->magMode);
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

        vertices = M_Realloc(vertices, sizeof(gl_vertex_t) * maxVertices);
        colors = M_Realloc(colors, sizeof(gl_color_t) * maxVertices);
        for(i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
        {
            texCoords[i] =
                M_Realloc(texCoords[i], sizeof(gl_texcoord_t) * maxVertices);
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
    TMU(rl, TMU_INTER_DETAIL)->tex = 0;
    rl->last = NULL;
    rl->size = 0;
    rl->flags = 0;
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
    rl->flags = 0;

    // The interpolation target must be explicitly set (in RL_AddPoly).
    TMU(rl, TMU_INTER)->tex = 0;
    TMU(rl, TMU_INTER_DETAIL)->tex = 0;
    rl->interPos = 0;
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

static __inline void copyTMU(rtexmapunit_t* dest, const rtexmapunit_t* src)
{
    memcpy(dest, src, sizeof(rtexmapunit_t));
}

static __inline boolean compareTMU(const rtexmapunit_t* a,
                                   const rtexmapunit_t* b)
{
    if(a->tex == b->tex && a->magMode == b->magMode)
        return true;

    return false;
}

static rendlist_t* getListFor(const rladdpoly_params_t* params,
                              boolean useLights)
{
    listhash_t*         hash, *table;
    rendlist_t*         dest, *convertable = NULL;

    // Check for specialized rendering lists first.
    if(params->type == RPT_SKY_MASK)
    {
        return &skyMaskList;
    }

    // Choose the correct hash table.
    switch(params->type)
    {
    case RPT_SHINY:     table = shinyHash;  break;
    case RPT_SHADOW:    table = shadowHash; break;
    case RPT_LIGHT:     table = dynHash;    break;
    default:
        table = (useLights ? litHash : plainHash);
        break;
    }

    // Find/create a list in the hash.
    hash = &table[TMU(params, TMU_PRIMARY)->tex % RL_HASH_SIZE];
    for(dest = hash->first; dest; dest = dest->next)
    {
        if((params->type == RPT_SHINY &&
            compareTMU(TMU(dest, TMU_PRIMARY), TMU(params, TMU_PRIMARY))) ||
           (params->type != RPT_SHINY &&
            compareTMU(TMU(dest, TMU_PRIMARY), TMU(params, TMU_PRIMARY)) &&
            compareTMU(TMU(dest, TMU_PRIMARY_DETAIL), TMU(params, TMU_PRIMARY_DETAIL))))
        {
            if(!TMU(dest, TMU_INTER)->tex && !TMU(params, TMU_INTER)->tex)
            {
                // This will do great.
                return dest;
            }

            // Is this eligible for conversion to a blended list?
            if(!dest->last && TMU(params, TMU_INTER)->tex && !convertable)
            {
                // If necessary, this empty list will be selected.
                convertable = dest;
            }

            // Possibly an exact match?
            if((params->type == RPT_SHINY &&
                compareTMU(TMU(dest, TMU_INTER), TMU(params, TMU_INTER))) ||
               (params->type != RPT_SHINY &&
                compareTMU(TMU(dest, TMU_INTER), TMU(params, TMU_INTER)) &&
                compareTMU(TMU(dest, TMU_INTER_DETAIL), TMU(params, TMU_INTER_DETAIL)) &&
                dest->interPos == params->interPos))
            {
                return dest;
            }
        }
    }

    // Did we find a convertable list?
    if(convertable)
    {   // This list is currently empty.
        copyTMU(TMU(convertable, TMU_INTER), TMU(params, TMU_INTER));
        copyTMU(TMU(convertable, TMU_INTER_DETAIL), TMU(params, TMU_INTER_DETAIL));

        convertable->interPos = params->interPos;
        return convertable;
    }

    // Create a new list.
    dest = createList(hash);

    // Init the info.
    copyTMU(TMU(dest, TMU_PRIMARY), TMU(params, TMU_PRIMARY));

    if(params->type != RPT_SHINY)
        copyTMU(TMU(dest, TMU_PRIMARY_DETAIL), TMU(params, TMU_PRIMARY_DETAIL));

    if(TMU(params, TMU_INTER)->tex)
    {
        copyTMU(TMU(dest, TMU_INTER), TMU(params, TMU_INTER));
        copyTMU(TMU(dest, TMU_INTER_DETAIL), TMU(params, TMU_INTER_DETAIL));

        if(params->type != RPT_SHINY)
            dest->interPos = params->interPos;
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
            boolean isDone;

            hdr = (primhdr_t *) list->data;
            isDone = false;
            while(!isDone)
            {
                if(hdr->indices != NULL)
                {
                    hdr->indices =
                        (uint *) (list->data +
                                  ((byte *) hdr->indices - oldData));
                }

                // Check here in the end; primitive composition may be
                // in progress.
                if(hdr->size != 0)
                    hdr = (primhdr_t *) ((byte *) hdr + hdr->size);
                else
                    isDone = true;
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
                           const rtexcoord_t* rtexcoords,
                           const rtexcoord_t* rtexcoords2,
                           const rtexcoord_t* rtexcoords5,
                           const rcolor_t* rcolors, uint numVertices,
                           rendpolytype_t type)
{
    uint                i;

    for(i = 0; i < numVertices; ++i)
    {
        // Vertex.
        {
        const rvertex_t*    rvtx = &rvertices[i];
        gl_vertex_t*        vtx = &vertices[base + i];

        vtx->xyz[0] = rvtx->pos[VX];
        vtx->xyz[1] = rvtx->pos[VZ];
        vtx->xyz[2] = rvtx->pos[VY];
        }

        if(type == RPT_SKY_MASK)
            continue; // Sky masked polys need nothing more.

        // Normal primary texture coordinates.
        if(TMU(list, TMU_PRIMARY)->tex)
        {
            const rtexcoord_t*  rtc = &rtexcoords[i];
            gl_texcoord_t*      tc = &texCoords[TCA_MAIN][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // Blend texture coordinates.
        if(TMU(list, TMU_INTER)->tex)
        {
            const rtexcoord_t*  rtc = &rtexcoords2[i];
            gl_texcoord_t*      tc = &texCoords[TCA_BLEND][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // First light texture coordinates.
        if(list->last->numLights > 0 && IS_MTEX_LIGHTS)
        {
            const rtexcoord_t*  rtc = &rtexcoords5[i];
            gl_texcoord_t*      tc = &texCoords[TCA_LIGHT][base + i];

            tc->st[0] = rtc->st[0];
            tc->st[1] = rtc->st[1];
        }

        // Color.
        {
        const rcolor_t*     rcolor = &rcolors[i];
        gl_color_t*         color = &colors[base + i];

        color->rgba[CR] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CR], 1));
        color->rgba[CG] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CG], 1));
        color->rgba[CB] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CB], 1));
        color->rgba[CA] = (DGLubyte) (255 * MINMAX_OF(0, rcolor->rgba[CA], 1));
        }
    }
}

/**
 * Adds the given poly onto the correct list.
 */
void RL_AddPoly(primtype_t type, const rvertex_t* rvertices,
                const rtexcoord_t* rtexcoords, const rtexcoord_t* rtexcoords2,
                const rtexcoord_t* rtexcoords5, const rcolor_t* rcolors,
                uint numVertices, blendmode_t blendMode,
                uint numLights, const rladdpoly_params_t* params)
{
    uint                i, base, primSize, numIndices;
    rendlist_t*         li;
    primhdr_t*          hdr;

    if(numVertices < 3)
        return; // huh?

    if(type < PT_FIRST || type >= NUM_PRIM_TYPES)
        Con_Error("RL_AddPoly: Unknown primtype %i.", type);

BEGIN_PROF( PROF_RL_ADD_POLY );

BEGIN_PROF( PROF_RL_GET_LIST );

    // Find/create a rendering list for the polygon's texture.
    li = getListFor(params, params->type != RPT_LIGHT? numLights : false);

END_PROF( PROF_RL_GET_LIST );

    primSize = numVertices;
    numIndices = numVertices;
    base = allocateVertices(primSize);

    hdr = allocateData(li, sizeof(primhdr_t));
    assert(hdr);
    li->last = hdr; // This becomes the new last primitive.

    hdr->primSize = primSize;
    // Primitive-specific blending mode.
    hdr->blendMode = (params->type != RPT_LIGHT? blendMode : BM_NORMAL);
    hdr->size = 0;
    hdr->indices = NULL;
    hdr->numIndices = 0;
    hdr->numLights = numLights;
    hdr->modTex = params->modTex;
    hdr->modColor[CR] = params->modColor[CR];
    hdr->modColor[CG] = params->modColor[CG];
    hdr->modColor[CB] = params->modColor[CB];
    if(TMU(params, TMU_PRIMARY_DETAIL)->tex)
    {
        hdr->texScale[0] = TMU(params, TMU_PRIMARY_DETAIL)->scale[0];
        hdr->texScale[1] = TMU(params, TMU_PRIMARY_DETAIL)->scale[1];
        hdr->texOffset[0] = TMU(params, TMU_PRIMARY_DETAIL)->offset[1];
        hdr->texOffset[1] = TMU(params, TMU_PRIMARY_DETAIL)->offset[1];
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
        (type == PT_TRIANGLE_STRIP? DGL_TRIANGLE_STRIP : DGL_TRIANGLE_FAN);

    writePrimitive(li, base, rvertices, rtexcoords, rtexcoords2,
                   rtexcoords5, rcolors, numVertices, params->type);
    endWrite(li);

END_PROF( PROF_RL_ADD_POLY );
}

void RL_FloatRGB(byte* rgb, float* dest)
{
    unsigned int            i;

    for(i = 0; i < 3; ++i)
    {
        dest[i] = ((float)rgb[i]) * reciprocal255;
    }
    dest[3] = 1;
}

/**
 * Draws the privitives that match the conditions. If no condition bits
 * are given, all primitives are considered eligible.
 */
static void drawPrimitives(int conditions, rendlist_t* list)
{
    primhdr_t*              hdr;
    boolean                 skip, bypass = false;

    // Should we just skip all this?
    if(conditions & DCF_SKIP)
        return;

    if(TMU(list, TMU_INTER)->tex)
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
            if((conditions & DCF_JUST_ONE_LIGHT) && hdr->numLights > 1)
                skip = true;
            else if((conditions & DCF_MANY_LIGHTS) && hdr->numLights == 1)
                skip = true;
        }

        if(!skip)
        {
            if(conditions & DCF_SET_LIGHT_ENV)
            {   // Use the correct texture and color for the light.
                DGL_SetInteger(DGL_ACTIVE_TEXTURE,
                               (conditions & DCF_SET_LIGHT_ENV0)? 0 : 1);
                GL_BindTexture(renderTextures ? hdr->modTex : 0, DGL_LINEAR);
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, hdr->modColor);
                // Make sure the light is not repeated.
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
            }

            if(conditions & DCF_SET_MATRIX_TEXTURE)
            {   // Primitive-specific texture translation & scale.
                if(conditions & DCF_SET_MATRIX_TEXTURE0)
                {
                    DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);
                    DGL_MatrixMode(DGL_TEXTURE);
                    DGL_LoadIdentity();
                    DGL_Translatef(hdr->texOffset[0], hdr->texOffset[1], 1);
                    DGL_Scalef(hdr->texScale[0], hdr->texScale[1], 1);
                }
                if(conditions & DCF_SET_MATRIX_TEXTURE1)
                {
                    DGL_SetInteger(DGL_ACTIVE_TEXTURE, 1);
                    DGL_MatrixMode(DGL_TEXTURE);
                    DGL_LoadIdentity();
                    DGL_Translatef(hdr->texOffset[0], hdr->texOffset[1], 1);
                    DGL_Scalef(hdr->texScale[0], hdr->texScale[1], 1);
                }
            }

            if(conditions & DCF_SET_BLEND_MODE)
            {   // Primitive-specific blending. Not used in all lists.
                GL_BlendMode(hdr->blendMode);
            }

            // Render the primitive.
            DGL_DrawElements(hdr->type, hdr->numIndices, hdr->indices);

            // Restore the texture matrix if changed.
            if(conditions & DCF_SET_MATRIX_TEXTURE)
            {
                if(conditions & DCF_SET_MATRIX_TEXTURE0)
                {
                    DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);
                    DGL_MatrixMode(DGL_TEXTURE);
                    DGL_LoadIdentity();
                }
                if(conditions & DCF_SET_MATRIX_TEXTURE1)
                {
                    DGL_SetInteger(DGL_ACTIVE_TEXTURE, 1);
                    DGL_MatrixMode(DGL_TEXTURE);
                    DGL_LoadIdentity();
                }
            }
        }

        hdr = (primhdr_t *) ((byte *) hdr + hdr->size);
    }
}

static void setEnvAlpha(float alpha)
{
    float           color[4];

    color[0] = color[1] = color[2] = 0;
    color[3] = MINMAX_OF(0, alpha, 1);

    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
}

static void setBlendState(rendlist_t* list, int modMode)
{
#ifdef _DEBUG
if(numTexUnits < 2)
    Con_Error("setBlendState: Not enough texture units.\n");
#endif

    GL_SelectTexUnits(2);

    rlBindTo(0, TMU(list, TMU_PRIMARY));
    rlBindTo(1, TMU(list, TMU_INTER));

    DGL_SetInteger(DGL_MODULATE_TEXTURE, modMode);
    setEnvAlpha(list->interPos);
}

static void setFogStateForDetails(void)
{
    // The fog color alpha is probably meaningless?
    float                   midGray[4];

    midGray[0] = .5f;
    midGray[1] = .5f;
    midGray[2] = .5f;
    midGray[3] = fogColor[3];

    DGL_Enable(DGL_FOG);
    glFogfv(GL_FOG_COLOR, midGray);
}

/**
 * Set per-list GL state.
 *
 * @return                  The conditions to select primitives.
 */
static int setupListState(listmode_t mode, rendlist_t* list)
{
    switch (mode)
    {
    case LM_SKYMASK:
        // Render all primitives on the list without discrimination.
        return 0;

    case LM_ALL: // All surfaces.
        // Should we do blending?
        if(TMU(list, TMU_INTER)->tex)
        {
            // Blend between two textures, modulate with primary color.
            setBlendState(list, 2);
        }
        else
        {
            // Normal modulation.
            GL_SelectTexUnits(1);
            rlBind(TMU(list, TMU_PRIMARY));
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
        }
        return 0;

    case LM_LIGHT_MOD_TEXTURE:
        // Modulate sector light, dynamic light and regular texture.
        rlBindTo(1, TMU(list, TMU_PRIMARY));
        return DCF_SET_LIGHT_ENV0 | DCF_JUST_ONE_LIGHT | DCF_NO_BLEND;

    case LM_TEXTURE_PLUS_LIGHT:
        rlBindTo(0, TMU(list, TMU_PRIMARY));
        return DCF_SET_LIGHT_ENV1 | DCF_NO_BLEND;

    case LM_FIRST_LIGHT:
        // Draw all primitives with more than one light
        // and all primitives which will have a blended texture.
        return DCF_SET_LIGHT_ENV0 | DCF_MANY_LIGHTS | DCF_BLEND;

    case LM_BLENDED:
        // Only render the blended surfaces.
        if(!TMU(list, TMU_INTER)->tex)
            return DCF_SKIP;
        setBlendState(list, 2);
        return 0;

    case LM_BLENDED_FIRST_LIGHT:
        // Only blended surfaces.
        if(!TMU(list, TMU_INTER)->tex)
            return DCF_SKIP;
        return DCF_SET_LIGHT_ENV0;

    case LM_WITHOUT_TEXTURE:
        // Only render the primitives affected by dynlights.
        return 0;

    case LM_LIGHTS:
        // The light lists only contain dynlight primitives.
        rlBind(TMU(list, TMU_PRIMARY));
        // Make sure the texture is not repeated.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return 0;

    case LM_BLENDED_MOD_TEXTURE:
        // Blending required.
        if(!TMU(list, TMU_INTER)->tex)
            break;
    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
        // Texture for surfaces with (many) dynamic lights.
        // Should we do blending?
        if(TMU(list, TMU_INTER)->tex)
        {
            // Mode 3 actually just disables the second texture stage,
            // which would modulate with primary color.
            setBlendState(list, 3);
            // Render all primitives.
            return 0;
        }
        // No modulation at all.
        GL_SelectTexUnits(1);
        rlBind(TMU(list, TMU_PRIMARY));
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 0);
        return (mode == LM_MOD_TEXTURE_MANY_LIGHTS ? DCF_MANY_LIGHTS : 0);

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        // Blending is not done now.
        if(TMU(list, TMU_INTER)->tex)
            break;
        if(TMU(list, TMU_PRIMARY_DETAIL)->tex)
        {
            GL_SelectTexUnits(2);
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 9); // Tex+Detail, no color.
            rlBindTo(0, TMU(list, TMU_PRIMARY));
            rlBindTo(1, TMU(list, TMU_PRIMARY_DETAIL));
            return DCF_SET_MATRIX_TEXTURE1;
        }
        else
        {
            GL_SelectTexUnits(1);
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 0);
            rlBind(TMU(list, TMU_PRIMARY));
            return 0;
        }
        break;

    case LM_ALL_DETAILS:
        if(TMU(list, TMU_PRIMARY_DETAIL)->tex)
        {
            rlBind(TMU(list, TMU_PRIMARY_DETAIL));
            // Render all surfaces on the list.
            return DCF_SET_MATRIX_TEXTURE0;
        }
        break;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        // Only unblended. Details are optional.
        if(TMU(list, TMU_INTER)->tex)
            break;
        if(TMU(list, TMU_PRIMARY_DETAIL)->tex)
        {
            GL_SelectTexUnits(2);
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 8);
            rlBindTo(0, TMU(list, TMU_PRIMARY));
            rlBindTo(1, TMU(list, TMU_PRIMARY_DETAIL));
            return DCF_SET_MATRIX_TEXTURE1;
        }
        else
        {
            // Normal modulation.
            GL_SelectTexUnits(1);
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
            rlBind(TMU(list, TMU_PRIMARY));
            return 0;
        }
        break;

    case LM_BLENDED_DETAILS:
        // We'll only render blended primitives.
        if(!TMU(list, TMU_INTER)->tex)
            break;
        if(!TMU(list, TMU_PRIMARY_DETAIL)->tex ||
           !TMU(list, TMU_INTER_DETAIL)->tex)
            break;

        rlBindTo(0, TMU(list, TMU_PRIMARY_DETAIL));
        rlBindTo(1, TMU(list, TMU_INTER_DETAIL));
        setEnvAlpha(list->interPos);
        return DCF_SET_MATRIX_TEXTURE0 | DCF_SET_MATRIX_TEXTURE1;

    case LM_SHADOW:
        // Render all primitives.
        rlBind(TMU(list, TMU_PRIMARY));
        if(!TMU(list, TMU_PRIMARY)->tex)
        {
            // Apply a modelview shift.
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();

            // Scale towards the viewpoint to avoid Z-fighting.
            DGL_Translatef(vx, vy, vz);
            DGL_Scalef(.99f, .99f, .99f);
            DGL_Translatef(-vx, -vy, -vz);
        }
        return 0;

    case LM_MASKED_SHINY:
        // The intertex holds the info for the mask texture.
        rlBindTo(1, TMU(list, TMU_INTER));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        setEnvAlpha(1.0f);
    case LM_ALL_SHINY:
    case LM_SHINY:
        rlBindTo(0, TMU(list, TMU_PRIMARY));
        // Make sure the texture is not clamped.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // Render all primitives.
        if(mode == LM_ALL_SHINY)
            return DCF_SET_BLEND_MODE;
        else
            return DCF_SET_BLEND_MODE |
                (mode == LM_MASKED_SHINY ? DCF_BLEND : DCF_NO_BLEND);

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
        if(!TMU(list, TMU_PRIMARY)->tex)
        {
            // Restore original modelview matrix.
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
        break;

    case LM_SHINY:
    case LM_ALL_SHINY:
    case LM_MASKED_SHINY:
        GL_BlendMode(BM_NORMAL);
        break;
    }
}

/**
 * Setup GL state for an entire rendering pass (compassing multiple lists).
 */
static void setupPassState(listmode_t mode)
{
    switch(mode)
    {
    case LM_SKYMASK:
        GL_SelectTexUnits(0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // We don't want to write to the color buffer.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE);
        // No need for fog.
        if(usingFog)
            DGL_Disable(DGL_FOG);
        break;

    case LM_BLENDED:
        GL_SelectTexUnits(2);
    case LM_ALL:
        // The first texture unit is used for the main texture.
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
            DGL_Enable(DGL_FOG);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_LIGHT_MOD_TEXTURE:
    case LM_TEXTURE_PLUS_LIGHT:
        // Modulate sector light, dynamic light and regular texture.
        GL_SelectTexUnits(2);
        if(mode == LM_LIGHT_MOD_TEXTURE)
        {
            RL_SelectTexCoordArray(0, TCA_LIGHT);
            RL_SelectTexCoordArray(1, TCA_MAIN);
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 4); // Light * texture.
        }
        else
        {
            RL_SelectTexCoordArray(0, TCA_MAIN);
            RL_SelectTexCoordArray(1, TCA_LIGHT);
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 5); // Texture + light.
        }
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
            DGL_Enable(DGL_FOG);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_FIRST_LIGHT:
        // One light, no texture.
        GL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_LIGHT);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 6);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog is allowed during this pass.
        if(usingFog)
            DGL_Disable(DGL_FOG);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_BLENDED_FIRST_LIGHT:
        // One additive light, no texture.
        GL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_LIGHT);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 7); // Add light, no color.
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        // Fog is allowed during this pass.
        if(usingFog)
            DGL_Disable(DGL_FOG);
        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        break;

    case LM_WITHOUT_TEXTURE:
        GL_SelectTexUnits(0);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Fog must be disabled during this pass.
        DGL_Disable(DGL_FOG);
        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        break;

    case LM_LIGHTS:
        GL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            DGL_Enable(DGL_FOG);
            glFogfv(GL_FOG_COLOR, blackColor);
        }
        else
            DGL_Disable(DGL_FOG);

        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD);
        break;

    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
    case LM_BLENDED_MOD_TEXTURE:
        // The first texture unit is used for the main texture.
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        // Fog would mess with the color (this is a multiplicative pass).
        DGL_Disable(DGL_FOG);
        break;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_MAIN);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // All of the surfaces are opaque.
        glDisable(GL_BLEND);
        // Fog is allowed.
        if(usingFog)
            DGL_Enable(DGL_FOG);
        break;

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_MAIN);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        // This is a multiplicative pass.
        DGL_Disable(DGL_FOG);
        break;

    case LM_ALL_DETAILS:
        GL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 0);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
            setFogStateForDetails();
        break;

    case LM_BLENDED_DETAILS:
        GL_SelectTexUnits(2);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 3);
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // All of the surfaces are opaque.
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
            setFogStateForDetails();
        break;

    case LM_SHADOW:
        // A bit like 'negative lights'.
        GL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 1 / 255.0f);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // Set normal fog, if it's enabled.
        if(usingFog)
        {
            DGL_Enable(DGL_FOG);
            glFogfv(GL_FOG_COLOR, fogColor);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_NORMAL);
        break;

    case LM_SHINY:
        GL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1); // 8 for multitexture
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            DGL_Enable(DGL_FOG);
            glFogfv(GL_FOG_COLOR, blackColor);
        }
        else
        {
            DGL_Disable(DGL_FOG);
        }
        glEnable(GL_BLEND);
        GL_BlendMode(BM_ADD); // Purely additive.
        break;

    case LM_MASKED_SHINY:
        GL_SelectTexUnits(2);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND); // the mask
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 8); // same as with details
        glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            DGL_Enable(DGL_FOG);
            glFogfv(GL_FOG_COLOR, blackColor);
        }
        else
        {
            DGL_Disable(DGL_FOG);
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

    // If the first list is empty, we do nothing. Normally we expect
    // all lists to contain something.
    if(num == 0 || lists[0]->last == NULL)
        return;

    // Setup GL state that's common to all the lists in this mode.
    setupPassState(mode);

    // Draw each given list.
    for(i = 0; i < num; ++i)
    {
        list = lists[i];

        // Setup GL state for this list, and
        // draw the necessary subset of primitives on the list.
        drawPrimitives(setupListState(mode, list), list);

        // Some modes require cleanup.
        finishListState(mode, list);
    }
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

static void lockVertices(void)
{
    // We're only locking the vertex and color arrays, so disable the
    // texcoord arrays for now. Every pass will enable/disable the texcoords
    // that are needed.
    DGL_DisableArrays(0, 0, DGL_ALL_BITS);

    // Actually, don't lock anything. (Massive slowdown?)
    DGL_Arrays(vertices, colors, 0, NULL, 0 /*numVertices */ );
}

static void unlockVertices(void)
{
    // Nothing was locked.
    //DGL_UnlockArrays();
}

/**
 * We have several different paths to accommodate both multitextured
 * details and dynamic lights. Details take precedence (they always cover
 * entire primitives, and usually *all* of the surfaces in a scene).
 */
void RL_RenderAllLists(void)
{
    uint                count;
    // Pointers to all the rendering lists.
    rendlist_t*         lists[MAX_RLISTS];

BEGIN_PROF( PROF_RL_RENDER_ALL );

    if(!freezeRLs) // only update when lists arn't frozen
        rendSky = !P_IsInVoid(viewPlayer);

    // When in the void we don't render a sky.
    // \fixme We could use a stencil when rendering the sky, using the
    // already collected skymask polys as a mask.
    if(rendSky && !devSkyMode)
        // The sky might be visible. Render the needed hemispheres.
        Rend_RenderSky(skyhemispheres);

    lockVertices();

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
    int                 oldr = renderTextures;

    renderTextures = true;
BEGIN_PROF( PROF_RL_RENDER_SHADOW );

    count = collectLists(shadowHash, lists);

    renderLists(LM_SHADOW, lists, count);

END_PROF( PROF_RL_RENDER_SHADOW );

    renderTextures = oldr;
    }

    unlockVertices();

    // Return to the normal GL state.
    GL_SelectTexUnits(1);
    DGL_DisableArrays(true, true, DGL_ALL_BITS);
    DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);
    glEnable(GL_BLEND);
    GL_BlendMode(BM_NORMAL);
    if(usingFog)
    {
        DGL_Enable(DGL_FOG);
        glFogfv(GL_FOG_COLOR, fogColor);
    }
    else
    {
        DGL_Disable(DGL_FOG);
    }

    // Draw masked walls, sprites and models.
BEGIN_PROF( PROF_RL_RENDER_MASKED );

    Rend_DrawMasked();

    // Draw particles.
    PG_Render();

END_PROF( PROF_RL_RENDER_MASKED );
END_PROF( PROF_RL_RENDER_ALL );
}
