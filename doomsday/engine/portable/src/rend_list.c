/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

/*
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

#define MAX_TEX_UNITS       8   // Only two used, actually.
#define RL_HASH_SIZE        128

// Number of extra bytes to keep allocated in the end of each rendering list.
#define LIST_DATA_PADDING   16

// FIXME: Rlist allocation could be dynamic.
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
#define DCF_SET_BLEND_MODE          0x00000040 // primitive-specific blending
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

// Types of rendering primitives.
typedef enum primtype_e {
    PT_TRIANGLES,               // Used for most stuff.
    PT_FAN,
    PT_DOUBLE_FAN,
} primtype_t;

// Texture coordinate array indices.
enum {
    TCA_MAIN,                   // Main texture.
    TCA_BLEND,                  // Blendtarget texture.
    TCA_DETAIL,                 // Detail texture coordinates.
    TCA_BLEND_DETAIL,           // Blendtarget's detail texture coordinates.
    TCA_LIGHT,                  // Glow texture coordinates.
    //TCA_SHINY,                  // Shiny surface coordinates.
    NUM_TEXCOORD_ARRAYS
};

// TYPES -------------------------------------------------------------------

/**
 * Each primhdr begins a block of polygon data that ends up as one or
 * more triangles on the screen. Note that there are pointers to the
 * rendering list itself here; they will need to be properly restored
 * whenever the list is resized.
 */
typedef struct primhdr_s {
    // RL_AddPoly expects that size is the first thing in the header.
    // Must be an offset since the list is sometimes reallocated.
    uint    size;               // Size of this primitive (zero = n/a).

    // Generic data, common to all polys.
    primtype_t type;
    short   flags;              // RPF_*

    // Primitive-specific blending mode.
    byte    blendMode;          // BM_*

    // Number of vertices in the primitive.
    uint    primSize;

    // Elements in the vertex array for this primitive.
    // The indices are always contiguous: indices[0] is the base, and
    // indices[1...n] > indices[0].
    // All indices in the range indices[0]...indices[n] are used by this
    // primitive (some are shared).
    ushort  numIndices;
    uint   *indices;

    // First index of the other fan in a Double Fan.
    ushort  beginOther;

    // The first dynamic light affecting the surface.
    dynlight_t *light;
} primhdr_t;

// Rendering List 'has' flags.
#define RLF_LIGHTS      0x1     // Primitives are dynamic lights.
#define RLF_BLENDED     0x2     // List contains only texblended prims.

/**
 * The rendering list. When the list is resized, pointers in the primitives
 * need to be restored so that they point to the new list.
 */
typedef struct rendlist_s {
    struct rendlist_s *next;
    int     flags;
    gltexture_t tex, intertex;
    float   interpos;           // 0 = primary, 1 = secondary texture
    size_t  size;               // Number of bytes allocated for the data.
    byte   *data;               // Data for a number of polygons (The List).
    byte   *cursor;             // A pointer to data, for reading/writing.
    primhdr_t *last;            // Pointer to the last primitive (or NULL).
} rendlist_t;

typedef struct listhash_s {
    rendlist_t *first, *last;
} listhash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

vissprite_t *R_NewVisSprite(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int skyhemispheres;
extern int useDynLights, dlBlend, simpleSky;
extern boolean usingFog;
extern float maxLightDist;

extern boolean freezeRLs;
extern int skyflatnum;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     renderTextures = true;
int     renderWireframe = false;
int     useMultiTexLights = true;
int     useMultiTexDetails = true;

// Intensity of angle-based wall lighting.
float   rend_light_wall_angle = 1;

// Rendering parameters for detail textures.
float   detailFactor = .5f;

//float             detailMaxDist = 256;
float   detailScale = 4;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/**
 * The vertex arrays.
 */
static gl_vertex_t *vertices;
static gl_texcoord_t *texCoords[NUM_TEXCOORD_ARRAYS];
static gl_color_t *colors;

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
static byte debugSky = false;

static float blackColor[4] = { 0, 0, 0, 0 };

// CODE --------------------------------------------------------------------

void RL_Register(void)
{
    // TODO: Move cvars here.
    C_VAR_BYTE("rend-dev-sky", &debugSky, 0, 0, 1);
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
static void RL_AddMaskedPoly(rendpoly_t *poly)
{
    vissprite_t *vis = R_NewVisSprite();
    float       brightest[3];
    dynlight_t *dyn;
    int         i, c;
    float       midpoint[3];

    midpoint[VX] =
        (poly->vertices[0].pos[VX] + poly->vertices[1].pos[VX]) / 2;
    midpoint[VY] =
        (poly->vertices[0].pos[VY] + poly->vertices[1].pos[VY]) / 2;
    midpoint[VZ] =
        (poly->vertices[0].pos[VZ] + poly->vertices[1].pos[VZ]) / 2;

    vis->type = VSPR_MASKED_WALL;
    vis->light = NULL;
    vis->center[VX] = midpoint[VX];
    vis->center[VY] = midpoint[VY];
    vis->center[VZ] = midpoint[VZ];
    vis->distance = Rend_PointDist2D(midpoint);
    vis->data.wall.texture = poly->tex.id;
    vis->data.wall.masked = poly->tex.masked; // Store texmask status in flip.
    for(i = 0; i < 4; ++i)
    {
        vis->data.wall.vertices[i].pos[VX] = poly->vertices[i].pos[VX];
        vis->data.wall.vertices[i].pos[VY] = poly->vertices[i].pos[VY];
        vis->data.wall.vertices[i].pos[VZ] = poly->vertices[i].pos[VZ];

        if(poly->flags & RPF_GLOW)
            memset(&vis->data.wall.vertices[i].color, 255, 4 * 4);
        else
        {
            for(c = 0; c < 4; ++c)
            {
                vis->data.wall.vertices[i].color[c] =
                    poly->vertices[i].color.rgba[c];
            }
        }
    }
    vis->data.wall.texc[0][VX] = poly->texoffx / (float) poly->tex.width;
    vis->data.wall.texc[1][VX] =
        vis->data.wall.texc[0][VX] + poly->wall->length / poly->tex.width;
    vis->data.wall.texc[0][VY] = poly->texoffy / (float) poly->tex.height;
    vis->data.wall.texc[1][VY] =
        vis->data.wall.texc[0][VY] + (poly->vertices[2].pos[VZ] -
                                      poly->vertices[0].pos[VZ]) / poly->tex.height;
    vis->data.wall.blendmode = poly->blendmode;

    // FIXME: Semitransparent masked polys arn't lit atm
    if(!(poly->flags & RPF_GLOW) && poly->lights && numTexUnits > 1 &&
       envModAdd && poly->vertices[0].color.rgba[3] == 255)
    {
        // Choose the brightest light.
        memcpy(brightest, poly->lights->color, sizeof(float) * 3);
        vis->data.wall.light = poly->lights;
        for(dyn = poly->lights->next; dyn; dyn = dyn->next)
        {
            if((brightest[0] + brightest[1] + brightest[2]) / 3 <
               (dyn->color[0] + dyn->color[1] + dyn->color[2]) / 3)
            {
                memcpy(brightest, dyn->color, sizeof(float) * 3);
                vis->data.wall.light = dyn;
            }
        }
    }
    else
    {
        vis->data.wall.light = NULL;
    }
}

/**
 * Color distance attenuation, extralight, fixedcolormap.
 * "Torchlight" is white, regardless of the original RGB.
 *
 * @param distanceOverride If positive, this distance will be used for ALL
 *                      of the rendpoly's vertices. Else, the dist will be
 *                      calculated if needed, depending on the properties of
 *                      the rendpoly to be lit, for each vertex seperately.                     
 */
void RL_VertexColors(rendpoly_t *poly, float lightlevel,
                     float distanceOverride, const float *rgb, float alpha)
{
    int         i, num;
    float       light, real, minimum;
    float       dist;
    rendpoly_vertex_t *vtx;
    boolean     usewhite;

    // Check for special case exceptions.
    if(poly->flags & (RPF_SKY_MASK|RPF_LIGHT|RPF_SHADOW))
    {
        return; // Don't need per-vertex lighting.
    }

    if(poly->isWall && rend_light_wall_angle && !useBias)  // A quad?
    {
        // Do a lighting adjustment based on orientation.
        lightlevel += (1.0f / 255.0f) *
            ((poly->vertices[1].pos[VY] -
             poly->vertices[0].pos[VY]) / poly->wall->length * 18 *
            rend_light_wall_angle);
        if(lightlevel < 0)
            lightlevel = 0;
        if(lightlevel > 1)
            lightlevel = 1;
    }

    light = lightlevel;
    num = poly->numvertices;
    for(i = 0, vtx = poly->vertices; i < num; ++i, vtx++)
    {
        usewhite = false;

        if(distanceOverride > 0)
            dist = distanceOverride;
        else
            dist = Rend_PointDist2D(vtx->pos);

        real = light - (dist - 32) / maxLightDist * (1 - light);
        minimum = light * light + (light - .63f) / 2;
        if(real < minimum)
            real = minimum;     // Clamp it.

        // Add extra light.
        real += extralight / 16.0f;

        // Check for torch.
        if(viewplayer->fixedcolormap)
        {
            // Colormap 1 is the brightest. I'm guessing 16 would be
            // the darkest.
            int     ll = 16 - viewplayer->fixedcolormap;
            float   d = (1024 - dist) / 512.0f;
            float   newmin = d * ll / 15.0f;

            if(real < newmin)
            {
                real = newmin;
                usewhite = true;    // FIXME : Do some linear blending.
            }
        }

        // Clamp the final light.
        if(real < 0)
            real = 0;
        if(real > 1)
            real = 1;

        if(usewhite)
        {
            vtx->color.rgba[0] = vtx->color.rgba[1] = vtx->color.rgba[2] =
                (DGLubyte) (0xff * real);
        }
        else
        {
            vtx->color.rgba[0] = (DGLubyte) (255 * rgb[0] * real);
            vtx->color.rgba[1] = (DGLubyte) (255 * rgb[1] * real);
            vtx->color.rgba[2] = (DGLubyte) (255 * rgb[2] * real);
        }

        vtx->color.rgba[3] = (DGLubyte) (255 * alpha);
    }
}

void RL_PreparePlane(subplaneinfo_t *plane, rendpoly_t *poly, float height,
                     subsector_t *subsector, float sectorLight,
                     const float *sectorLightColor, float *surfaceColor)
{
    int         i, num, vid;

    poly->numvertices = subsector->numvertices;

    // Copy the vertices in reverse order for ceilings (flip faces).
    if(plane->type == PLN_CEILING)
        vid = subsector->numvertices - 1;
    else
        vid = 0;

    num = subsector->numvertices;
    for(i = 0; i < num; ++i)
    {
        poly->vertices[i].pos[VX] = subsector->vertices[vid].pos[VX];
        poly->vertices[i].pos[VY] = subsector->vertices[vid].pos[VY];
        poly->vertices[i].pos[VZ] = height;

        (plane->type == PLN_CEILING? vid-- : vid++);
    }

    if(!(poly->flags & RPF_SKY_MASK))
    {
        float       vColor[] = { 0, 0, 0, 0};

        // Calculate the color for each vertex, blended with plane color?
        if(surfaceColor[0] < 1 || surfaceColor[1] < 1 || surfaceColor[2] < 1)
        {
            // Blend sector light+color+surfacecolor
            for(i = 0; i < 3; ++i)
                vColor[i] = surfaceColor[i] * sectorLightColor[i];

            RL_VertexColors(poly, sectorLight, -1, vColor, surfaceColor[3]);
        }
        else
        {
            // Use sector light+color only
            RL_VertexColors(poly, sectorLight, -1, sectorLightColor, surfaceColor[3]);
        }
    }
}

/**
 * The first selected unit is active after this call.
 */
void RL_SelectTexUnits(int count)
{
    int     i;

    // Disable extra units.
    for(i = numTexUnits - 1; i >= count; i--)
        gl.Disable(DGL_TEXTURE0 + i);

    // Enable the selected units.
    for(i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits)
            continue;

        gl.Enable(DGL_TEXTURE0 + i);
        // Enable the texcoord array for this unit.
        gl.EnableArrays(0, 0, 0x1 << i);
    }
}

void RL_SelectTexCoordArray(int unit, int index)
{
    void   *coords[MAX_TEX_UNITS];

    // Does this unit exist?
    if(unit >= numTexUnits)
        return;

    memset(coords, 0, sizeof(coords));
    coords[unit] = texCoords[index];
    gl.Arrays(NULL, NULL, numTexUnits, coords, 0);
}

void RL_Bind(DGLuint texture)
{
    gl.Bind(renderTextures ? texture : 0);
}

void RL_BindTo(int unit, DGLuint texture)
{
    gl.SetInteger(DGL_ACTIVE_TEXTURE, unit);
    gl.Bind(renderTextures ? texture : 0);
}

static void RL_ClearHash(listhash_t *hash)
{
    memset(hash, 0, sizeof(listhash_t) * RL_HASH_SIZE);
}

/**
 * Called only once, from R_Init -> Rend_Init.
 */
void RL_Init(void)
{
    RL_ClearHash(plainHash);
    RL_ClearHash(litHash);
    RL_ClearHash(dynHash);
    RL_ClearHash(shadowHash);
    RL_ClearHash(shinyHash);

    memset(&skyMaskList, 0, sizeof(skyMaskList));
}

static void RL_ClearVertices(void)
{
    numVertices = 0;
}

static void RL_DestroyVertices(void)
{
    int     i;

    numVertices = maxVertices = 0;
    M_Free(vertices);
    vertices = NULL;
    M_Free(colors);
    colors = NULL;
    for(i = 0; i < NUM_TEXCOORD_ARRAYS; ++i)
    {
        M_Free(texCoords[i]);
        texCoords[i] = NULL;
    }
}

/**
 * Allocate vertices from the global vertex array.
 */
static uint RL_AllocateVertices(uint count)
{
    uint    i, base = numVertices;

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

static void RL_DestroyList(rendlist_t *rl)
{
    // All the list data will be destroyed.
    if(rl->data)
        Z_Free(rl->data);
    rl->data = NULL;

#if _DEBUG
    Z_CheckHeap();
#endif

    rl->cursor = NULL;
    //rl->tex.detail = NULL;
    rl->intertex.detail = NULL;
    rl->last = NULL;
    rl->size = 0;
    rl->flags = 0;
}

static void RL_DeleteHash(listhash_t *hash)
{
    int     i;
    rendlist_t *list, *next;

    for(i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(list = hash[i].first; list; list = next)
        {
            next = list->next;
            RL_DestroyList(list);
            Z_Free(list);
        }
    }
    RL_ClearHash(hash);
}

/**
 * All lists will be destroyed.
 */
void RL_DeleteLists(void)
{
    // Delete all lists.
    RL_DeleteHash(plainHash);
    RL_DeleteHash(litHash);
    RL_DeleteHash(dynHash);
    RL_DeleteHash(shadowHash);
    RL_DeleteHash(shinyHash);

    RL_DestroyList(&skyMaskList);

    RL_DestroyVertices();

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
static void RL_RewindList(rendlist_t *rl)
{
    rl->cursor = rl->data;
    rl->last = NULL;
    rl->flags = 0;
    //rl->tex.detail = NULL;
    rl->intertex.detail = NULL;

    // The interpolation target must be explicitly set (in RL_AddPoly).
    memset(&rl->intertex, 0, sizeof(rl->intertex));
    rl->interpos = 0;
}

static void RL_RewindHash(listhash_t *hash)
{
    int     i;
    rendlist_t *list;

    for(i = 0; i < RL_HASH_SIZE; ++i)
    {
        for(list = hash[i].first; list; list = list->next)
            RL_RewindList(list);
    }
}

/**
 * Called before rendering a frame.
 */
void RL_ClearLists(void)
{
    // Clear the vertex array.
    RL_ClearVertices();

    RL_RewindHash(plainHash);
    RL_RewindHash(litHash);
    RL_RewindHash(dynHash);
    RL_RewindHash(shadowHash);
    RL_RewindHash(shinyHash);

    RL_RewindList(&skyMaskList);

    // FIXME: Does this belong here?
    skyhemispheres = 0;
}

static rendlist_t *RL_CreateList(listhash_t *hash)
{
    rendlist_t *list = Z_Calloc(sizeof(rendlist_t), PU_STATIC, 0);

    if(hash->last)
        hash->last->next = list;
    hash->last = list;
    if(!hash->first)
        hash->first = list;
    return list;
}

static rendlist_t *RL_GetListFor(rendpoly_t *poly, boolean useLights)
{
    listhash_t *hash, *table;
    rendlist_t *dest, *convertable = NULL;

    // Check for specialized rendering lists first.
    if(poly->flags & RPF_SKY_MASK)
    {
        return &skyMaskList;
    }

    // Choose the correct hash table.
    if(poly->flags & RPF_SHINY)
    {
        table = shinyHash;
    }
    else if(poly->flags & RPF_SHADOW)
    {
        table = shadowHash;
    }
    else
    {
        table = (useLights ? litHash : plainHash);
    }

    // Find/create a list in the hash.
    hash = &table[poly->tex.id % RL_HASH_SIZE];
    for(dest = hash->first; dest; dest = dest->next)
    {
        if(dest->tex.id == poly->tex.id &&
           dest->tex.detail == poly->tex.detail)
        {
            if(!poly->intertex.id && !dest->intertex.id)
            {
                // This will do great.
                return dest;
            }

            // Is this eligible for conversion to a blended list?
            if(poly->intertex.id && !dest->last && !convertable)
            {
                // If necessary, this empty list will be selected.
                convertable = dest;
            }

            // Possibly an exact match?
            if(poly->intertex.id == dest->intertex.id &&
               poly->interpos == dest->interpos &&
               poly->intertex.detail == dest->intertex.detail)
            {
                return dest;
            }
        }
    }

    // Did we find a convertable list?
    if(convertable)
    {
        // This list is currently empty.
        memcpy(&convertable->intertex, &poly->intertex,
               sizeof(poly->intertex));
        convertable->interpos = poly->interpos;
        return convertable;
    }

    // Create a new list.
    dest = RL_CreateList(hash);

    // Init the info.
    dest->tex.id = poly->tex.id;
    dest->tex.width = poly->tex.width;
    dest->tex.height = poly->tex.height;
    dest->tex.detail = poly->tex.detail;

    if(poly->intertex.id)
    {
        memcpy(&dest->intertex, &poly->intertex, sizeof(poly->intertex));
        dest->interpos = poly->interpos;
    }

    return dest;
}

static rendlist_t *RL_GetLightListFor(DGLuint texture)
{
    listhash_t *hash;
    rendlist_t *dest;

    // Find/create a list in the hash.
    hash = &dynHash[texture % RL_HASH_SIZE];
    for(dest = hash->first; dest; dest = dest->next)
    {
        if(dest->tex.id == texture)
            return dest;
    }

    // There isn't a list for this yet.
    dest = RL_CreateList(hash);
    dest->tex.id = texture;

    return dest;
}

/**
 * Returns a pointer to the start of the allocated data.
 */
static void *RL_AllocateData(rendlist_t *list, int bytes)
{
    size_t  required;
    int     startOffset = list->cursor - list->data;
    primhdr_t *hdr;

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
        byte   *oldData = list->data;
        int     cursorOffset = -1;
        int     lastOffset = -1;

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

static void RL_AllocateIndices(rendlist_t *list, int numIndices)
{
    void   *indices;

    list->last->numIndices = numIndices;
    indices = RL_AllocateData(list, sizeof(*list->last->indices) * numIndices);

    // list->last may change during RL_AllocateData.
    list->last->indices = indices;
}

static void RL_QuadTexCoords(gl_texcoord_t *tc, rendpoly_t *poly, gltexture_t *tex)
{
    float   width, height;

    if(!tex->id)
        return;

    if(poly->flags & RPF_SHADOW)
    {
        // Shadows use the width and height from the polygon itself.
        width = poly->tex.width;
        height = poly->tex.height;

        if(poly->flags & RPF_HORIZONTAL)
        {
            // Special horizontal coordinates for wall shadows.
            tc[3].st[0] = tc[2].st[0] = poly->texoffy / height;
            tc[3].st[1] = tc[0].st[1] = poly->texoffx / width;
            tc[0].st[0] = tc[3].st[0] + (poly->vertices[2].pos[VZ] - poly->vertices[0].pos[VZ]) / height;
            tc[1].st[0] = tc[3].st[0] + (poly->vertices[3].pos[VZ] - poly->vertices[1].pos[VZ]) / height;
            tc[1].st[1] = tc[2].st[1] = tc[3].st[1] + poly->wall->length / width;
            return;
        }
    }
    else
    {
        // Normally the texture's width and height are considered
        // constant inside a rendering list.
        width = tex->width;
        height = tex->height;
    }

    tc[0].st[0] = tc[3].st[0] = poly->texoffx / width;
    tc[0].st[1] = tc[1].st[1] = poly->texoffy / height;
    tc[1].st[0] = tc[2].st[0] = tc[0].st[0] + poly->wall->length / width;
    tc[2].st[1] = tc[0].st[1] + (poly->vertices[2].pos[VZ] - poly->vertices[0].pos[VZ]) / height;
    tc[3].st[1] = tc[0].st[1] + (poly->vertices[3].pos[VZ] - poly->vertices[1].pos[VZ]) / height;
}

static float RL_ShinyVertical(float dy, float dx)
{
    return ( (atan(dy/dx) / (PI/2)) + 1 ) / 2;
}

static void RL_QuadShinyTexCoords(gl_texcoord_t *tc, rendpoly_t *poly)
{
    vec2_t surface, normal, projected, s, reflected;
    vec2_t view;
    float distance, angle, prevAngle = 0;
    unsigned int i;

    // Quad surface vector.
    V2_Set(surface,
           (poly->vertices[1].pos[VX] - poly->vertices[0].pos[VX]) /
           poly->wall->length,
           (poly->vertices[1].pos[VY] - poly->vertices[0].pos[VY]) /
           poly->wall->length);

    V2_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2_Set(view,
               vx - poly->vertices[i].pos[VX],
               vz - poly->vertices[i].pos[VY]);

        distance = V2_Normalize(view);

        V2_Project(projected, view, normal);
        V2_Subtract(s, projected, view);
        V2_Scale(s, 2);
        V2_Sum(reflected, view, s);

        angle = acos(reflected[VY]) / PI;
        if(reflected[VX] < 0)
        {
            angle = 1 - angle;
        }

        if(i == 0)
        {
            prevAngle = angle;
        }
        else
        {
            if(angle > prevAngle)
                angle -= 1;
        }

        // Horizontal coordinates.
        tc[ (i == 0 ? 0 : 1) ].st[0] = tc[ (i == 0 ? 3 : 2) ].st[0] =
            angle + .3f;     /*acos(-dot)/PI*/

        // Vertical coordinates.
        tc[ (i == 0 ? 0 : 1) ].st[1] =
            RL_ShinyVertical(vy - poly->vertices[2+i].pos[VZ], distance);

        tc[ (i == 0 ? 2 : 3) ].st[1] =
            RL_ShinyVertical(vy - poly->vertices[i].pos[VZ], distance);
    }
}

static void RL_QuadDetailTexCoords(gl_texcoord_t *tc, rendpoly_t *poly,
                            gltexture_t *tex)
{
    float   mul = tex->detail->scale * detailScale;

    tc[0].st[0] = tc[3].st[0] = poly->texoffx / tex->detail->width;
    tc[0].st[1] = tc[1].st[1] = poly->texoffy / tex->detail->height;
    tc[1].st[0] = tc[2].st[0] =
        (tc[0].st[0] + poly->wall->length / tex->detail->width) * mul;
    tc[2].st[1] =
        (tc[0].st[1] + (poly->vertices[2].pos[VZ] - poly->vertices[0].pos[VZ]) / tex->detail->height) * mul;
    tc[3].st[1] =
        (tc[0].st[1] + (poly->vertices[3].pos[VZ] - poly->vertices[1].pos[VZ]) / tex->detail->height) * mul;

    tc[0].st[0] *= mul;
    tc[0].st[1] *= mul;
    tc[1].st[1] *= mul;
    tc[3].st[0] *= mul;
}

static void RL_QuadColors(gl_color_t *color, rendpoly_t *poly)
{
    if(poly->flags & RPF_SKY_MASK)
    {
        // Sky mask doesn't need a color.
        return;
    }
    if(poly->flags & RPF_GLOW)
    {
        memset(color, 255, 4 * 4);
        return;
    }
    if(poly->flags & (RPF_SHADOW | RPF_SHINY))
    {
        memcpy(color[0].rgba, poly->vertices[0].color.rgba, 4);
        memcpy(color[1].rgba, poly->vertices[1].color.rgba, 4);
        memcpy(color[2].rgba, poly->vertices[2].color.rgba, 4);
        memcpy(color[3].rgba, poly->vertices[3].color.rgba, 4);
        return;
    }

    // Just copy RGB, set A to 255.
    memcpy(color[0].rgba, poly->vertices[0].color.rgba, 3);
    memcpy(color[1].rgba, poly->vertices[1].color.rgba, 3);
    memcpy(color[2].rgba, poly->vertices[3].color.rgba, 3);
    memcpy(color[3].rgba, poly->vertices[2].color.rgba, 3);

    color[0].rgba[3] = color[1].rgba[3] = color[2].rgba[3] = color[3].rgba[3] =
        255;
}

static void RL_QuadVertices(gl_vertex_t *v, rendpoly_t *poly)
{
    v[0].xyz[0] = v[3].xyz[0] = poly->vertices[0].pos[VX];
    v[0].xyz[1] = poly->vertices[2].pos[VZ];
    v[1].xyz[1] = poly->vertices[3].pos[VZ];
    v[0].xyz[2] = v[3].xyz[2] = poly->vertices[0].pos[VY];
    v[1].xyz[0] = v[2].xyz[0] = poly->vertices[1].pos[VX];
    v[2].xyz[1] = poly->vertices[0].pos[VZ];
    v[3].xyz[1] = poly->vertices[1].pos[VZ];
    v[1].xyz[2] = v[2].xyz[2] = poly->vertices[1].pos[VY];
}

static void RL_QuadLightCoords(gl_texcoord_t *tc, dynlight_t *dyn)
{
    tc[0].st[0] = tc[3].st[0] = dyn->s[0];
    tc[0].st[1] = tc[1].st[1] = dyn->t[0];
    tc[1].st[0] = tc[2].st[0] = dyn->s[1];
    tc[2].st[1] = tc[3].st[1] = dyn->t[1];
}

static void RL_FlatShinyTexCoords(gl_texcoord_t *tc, float xy[2], float height)
{
    vec2_t view, start;
    float distance;
    float offset;

    // View vector.
    V2_Set(view, vx - xy[VX], vz - xy[VY]);

    distance = V2_Normalize(view);
    if(distance < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distance = 10;
    }

    // Offset from the normal view plane.
    V2_Set(start, vx, vz);

    offset = ((start[VY] - xy[VY]) * sin(.4f)/*viewfrontvec[VX]*/ -
              (start[VX] - xy[VX]) * cos(.4f)/*viewfrontvec[VZ]*/);

    tc->st[0] = ((RL_ShinyVertical(offset, distance) - .5f) * 2) + .5f;
    tc->st[1] = RL_ShinyVertical(vy - height, distance);
}

static void RL_FlatDetailTexCoords(gl_texcoord_t *tc, float xy[2], rendpoly_t *poly,
                                   gltexture_t *tex)
{
    tc->st[0] =
        (xy[VX] +
         poly->texoffx) / tex->detail->width * detailScale *
        tex->detail->scale;

    tc->st[1] =
        (-xy[VY] -
         poly->texoffy) / tex->detail->height * detailScale *
        tex->detail->scale;
}

/**
 * Inter = 0 in the bottom. Only 's' is affected.
 */
static void RL_InterpolateTexCoordS(gl_texcoord_t *tc, uint index, uint top,
                                    uint bottom, float inter)
{
    // Start working with the bottom.
    memcpy(&tc[index], &tc[bottom], sizeof(gl_texcoord_t));

    tc[index].st[0] += (tc[top].st[0] - tc[bottom].st[0]) * inter;
}

/**
 * Inter = 0 in the bottom. Only 't' is affected.
 */
static void RL_InterpolateTexCoordT(gl_texcoord_t *tc, uint index, uint top,
                                    uint bottom, float inter)
{
    // Start working with the bottom.
    memcpy(&tc[index], &tc[bottom], sizeof(gl_texcoord_t));

    tc[index].st[1] += (tc[top].st[1] - tc[bottom].st[1]) * inter;
}

static void RL_WriteQuad(rendlist_t *list, rendpoly_t *poly)
{
    uint    base;
    primhdr_t *hdr = NULL;

    // A quad is composed of two triangles.
    // We need four vertices.
    list->last->primSize = 4;
    base = RL_AllocateVertices(list->last->primSize);

    // Setup the indices.
    RL_AllocateIndices(list, 4);
    hdr = list->last;
    hdr->indices[0] = base;
    hdr->indices[1] = base + 1;
    hdr->indices[2] = base + 2;
    hdr->indices[3] = base + 3;

    // Primitive-specific blending mode.
    hdr->blendMode = poly->blendmode;

    // Primary texture coordinates.
    if(poly->flags & RPF_SHINY)
    {
        // Shiny environmental texture coordinates.
        RL_QuadShinyTexCoords(&texCoords[TCA_MAIN][base], poly);

        // Mask texture coordinates.
        if(list->intertex.id)
        {
            RL_QuadTexCoords(&texCoords[TCA_BLEND][base], poly, &poly->tex);
        }
    }
    else
    {
        // Normal primary texture coordinates.
        RL_QuadTexCoords(&texCoords[TCA_MAIN][base], poly, &list->tex);

        // Blend texture coordinates.
        if(list->intertex.id)
        {
            RL_QuadTexCoords(&texCoords[TCA_BLEND][base], poly,
                             &list->intertex);
        }

        // Detail texture coordinates.
        if(list->tex.detail)
        {
            RL_QuadDetailTexCoords(&texCoords[TCA_DETAIL][base], poly,
                                   &list->tex);
        }
        if(list->intertex.detail)
        {
            RL_QuadDetailTexCoords(&texCoords[TCA_BLEND_DETAIL][base], poly,
                                   &list->intertex);
        }
    }

    // Colors.
    RL_QuadColors(&colors[base], poly);

    // Vertices.
    RL_QuadVertices(&vertices[base], poly);

    // Light texture coordinates.
    if(list->last->light && IS_MTEX_LIGHTS)
    {
        RL_QuadLightCoords(&texCoords[TCA_LIGHT][base], list->last->light);
    }
}

static void RL_WriteDivQuad(rendlist_t *list, rendpoly_t *poly)
{
    gl_vertex_t *v;
    uint    i, base;
    uint    sideBase[2];
    int     side, other, index, top, bottom, div, c;
    float   z, height[2], inter;
    primhdr_t *hdr = NULL;

    // Vertex layout (and triangles for one side):
    // [n] = fan base vertex
    //
    //[0]-->---------1
    // |\.......     |
    // 8  \...  .....4
    // |    \ ....   |
    // 7      \   ...5
    // |        \    |
    // 6          \  |
    // |            \|
    // 3---------<--[2]
    //

    height[0] = poly->vertices[2].pos[VZ] - poly->vertices[0].pos[VZ];
    height[1] = poly->vertices[3].pos[VZ] - poly->vertices[1].pos[VZ];

    list->last->type = PT_DOUBLE_FAN;

    // A divquad is composed of two triangle fans.
    list->last->primSize = 4 + poly->wall->divs[0].num + poly->wall->divs[1].num;
    base = RL_AllocateVertices(list->last->primSize);

    // Allocate the indices.
    RL_AllocateIndices(list, 3 + poly->wall->divs[1].num + 
                             3 + poly->wall->divs[0].num);

    hdr = list->last;
    hdr->indices[0] = base;

    // Primitive-specific blending mode.
    hdr->blendMode = poly->blendmode;

    // The first four vertices are the normal quad corner points.
    if(poly->flags & RPF_SHINY)
    {
        // Shiny environmental texture coordinates.
        RL_QuadShinyTexCoords(&texCoords[TCA_MAIN][base], poly);
        if(list->intertex.id)
        {
            RL_QuadTexCoords(&texCoords[TCA_BLEND][base], poly, &poly->tex);
        }
    }
    else
    {
        // Primary texture coordinates.
        RL_QuadTexCoords(&texCoords[TCA_MAIN][base], poly, &list->tex);
        if(list->intertex.id)
        {
            RL_QuadTexCoords(&texCoords[TCA_BLEND][base], poly,
                             &list->intertex);
        }
        if(list->tex.detail)
        {
            RL_QuadDetailTexCoords(&texCoords[TCA_DETAIL][base], poly,
                                   &list->tex);
        }
        if(list->intertex.detail)
        {
            RL_QuadDetailTexCoords(&texCoords[TCA_BLEND_DETAIL][base], poly,
                                   &list->intertex);
        }
    }
    RL_QuadColors(&colors[base], poly);
    RL_QuadVertices(&vertices[base], poly);

    // Texture coordinates for lights (normal quad corners).
    if(hdr->light && IS_MTEX_LIGHTS)
    {
        RL_QuadLightCoords(&texCoords[TCA_LIGHT][base], hdr->light);
    }

    // Index of the indices array.
    index = 0;

    // First vertices of each side (1=right, 0=left).
    sideBase[1] = base + 4;
    sideBase[0] = sideBase[1] + poly->wall->divs[1].num;

    // Set the rest of the indices and init the division vertices.
    for(side = 0; side < 2; ++side) // Left->right is side zero.
    {
        unsigned int num;

        other = !side;

        // The actual top/bottom corner vertex.
        top = base + (!side ? 1 : 0);
        bottom = base + (!side ? 2 : 3);

        // Here begins the other triangle fan.
        if(side)
            hdr->beginOther = index;

        // The fan origin is the same for all the triangles.
        hdr->indices[index++] = base + (!side ? 0 : 2);

        // The first (top/bottom) vertex of the side.
        hdr->indices[index++] = base + (!side ? 1 : 3);

        // The division vertices.
        num = poly->wall->divs[other].num;
        for(i = 0; i < num; ++i)
        {
            // A division vertex of the other side.
            hdr->indices[index++] = div = sideBase[other] + i;

            // Division height of this vertex.
            z = poly->wall->divs[other].pos[i];

            // We'll init this vertex by interpolating.
            inter = (z - poly->vertices[side].pos[VZ]) / height[side];

            if(!(poly->flags & RPF_SKY_MASK))
            {
                if(poly->flags & RPF_HORIZONTAL)
                {
                    // Currently only shadows use this texcoord mode.
                    RL_InterpolateTexCoordS(texCoords[TCA_MAIN], div, top,
                                            bottom, inter);
                }
                else
                {
                    // Primary texture coordinates.
                    RL_InterpolateTexCoordT(texCoords[TCA_MAIN], div, top,
                                            bottom, inter);
                }

                // Detail texture coordinates.
                if(poly->tex.detail)
                {
                    RL_InterpolateTexCoordT(texCoords[TCA_DETAIL], div, top,
                                            bottom, inter);
                }
                if(poly->intertex.detail)
                {
                    RL_InterpolateTexCoordT(texCoords[TCA_BLEND_DETAIL], div,
                                            top, bottom, inter);
                }

                // Light coordinates.
                if(hdr->light && IS_MTEX_LIGHTS)
                {
                    RL_InterpolateTexCoordT(texCoords[TCA_LIGHT], div, top,
                                            bottom, inter);
                }

                // Blend texture coordinates.
                if(list->intertex.id)
                {
                    RL_InterpolateTexCoordT(texCoords[TCA_BLEND], div, top,
                                            bottom, inter);
                }

                // Color.
                //memcpy(&colors[div], &colors[top], sizeof(gl_color_t));
                for(c = 0; c < 4; ++c)
                {
                    colors[div].rgba[c] = colors[bottom].rgba[c] +
                        (colors[top].rgba[c] - colors[bottom].rgba[c]) * inter;
                }
            }

            // Vertex.
            v = &vertices[div];
            v->xyz[0] = vertices[top].xyz[0];
            v->xyz[1] = z;
            v->xyz[2] = vertices[top].xyz[2];
        }

        // The last (bottom/top) vertex of the side.
        hdr->indices[index++] = base + (!side ? 2 : 0);
    }
}

static void RL_WriteFlat(rendlist_t *list, rendpoly_t *poly)
{
    rendpoly_vertex_t *vtx;
    gl_color_t *col;
    gl_texcoord_t *tc;
    gl_vertex_t *v;
    uint    base;
    unsigned int i, num;

    // A flat is composed of N triangles, where N = poly->numvertices - 2.
    list->last->primSize = poly->numvertices;
    base = RL_AllocateVertices(list->last->primSize);

    // Allocate the indices.
    RL_AllocateIndices(list, poly->numvertices);

    // Setup indices in a triangle fan.
    num = poly->numvertices;
    for(i = 0; i < num; ++i)
    {
        list->last->indices[i] = base + i;
    }

    // Primitive-specific blending mode.
    list->last->blendMode = poly->blendmode;

    for(i = 0, vtx = poly->vertices; i < num; ++i, vtx++)
    {
        // Coordinates.
        v = &vertices[base + i];
        v->xyz[0] = vtx->pos[VX];
        v->xyz[1] = vtx->pos[VZ];
        v->xyz[2] = vtx->pos[VY];

        if(poly->flags & RPF_SKY_MASK)
        {
            // Skymask polys don't need any further data.
            continue;
        }

        // Primary texture coordinates.
        if(list->tex.id)
        {
            tc = &texCoords[TCA_MAIN][base + i];
            if(!(poly->flags & RPF_SHINY))
            {
                // Normal texture coordinates.
                tc->st[0] =
                    (vtx->pos[VX] +
                     poly->texoffx) /
                    (poly->flags & RPF_SHADOW ? poly->tex.width :
                     list->tex.width);
                tc->st[1] =
                    (-vtx->pos[VY] -
                     poly->texoffy) /
                    (poly->flags & RPF_SHADOW ? poly->tex.height :
                     list->tex.height);
            }
            else
            {
                // Calculate shiny coordinates.
                RL_FlatShinyTexCoords(tc, vtx->pos, vtx->pos[VZ]);
            }
        }

        // Detail texture coordinates.
        if(list->tex.detail)
        {
            RL_FlatDetailTexCoords(&texCoords[TCA_DETAIL][base + i], vtx->pos,
                                   poly, &list->tex);
        }
        if(list->intertex.detail)
        {
            RL_FlatDetailTexCoords(&texCoords[TCA_BLEND_DETAIL][base + i],
                                   vtx->pos, poly, &list->intertex);
        }

        // Light coordinates.
        if(list->last->light && IS_MTEX_LIGHTS)
        {
            tc = &texCoords[TCA_LIGHT][base + i];
            tc->st[0] =
                (vtx->pos[VX] +
                 list->last->light->s[0]) * list->last->light->s[1];
            tc->st[1] =
                (-vtx->pos[VY] +
                 list->last->light->t[0]) * list->last->light->t[1];
        }

        // Blend texture coordinates.
        if(list->intertex.id)
        {
            tc = &texCoords[TCA_BLEND][base + i];
            tc->st[0] = (vtx->pos[VX] + poly->texoffx) /
                (poly->flags & RPF_SHINY ? poly->tex.width :
                 list->intertex.width);
            tc->st[1] =
                (-vtx->pos[VY] - poly->texoffy) /
                (poly->flags & RPF_SHINY ? poly->tex.height :
                 list->intertex.height);
        }

        // Color.
        col = &colors[base + i];
        if(poly->flags & RPF_GLOW)
        {
            memset(col->rgba, 255, 4);
        }
        else if(poly->flags & (RPF_SHADOW | RPF_SHINY))
        {
            memcpy(col->rgba, vtx->color.rgba, 4);
        }
        else
        {
            memcpy(col->rgba, vtx->color.rgba, 3);
            col->rgba[3] = 255;
        }
    }
}

static void RL_EndWrite(rendlist_t *list)
{
    // The primitive has been written, update the size in the header.
    list->last->size = list->cursor - (byte *) list->last;

    // Write the end marker (which will be overwritten by the next
    // primitive). The idea is that this zero is interpreted as the
    // size of the following primhdr.
    *(int *) list->cursor = 0;
}

static void RL_WriteDynLight(rendlist_t *list, dynlight_t *dyn, primhdr_t *prim,
                             rendpoly_t *poly)
{
    uint        i, c, num, base;
    gl_texcoord_t *tc;
    gl_color_t *col;
    void       *ptr;

    ptr = RL_AllocateData(list, sizeof(primhdr_t));
    list->last = ptr;

    list->last->size = 0;
    list->last->type = prim->type;
    list->last->flags = 0;
    list->last->numIndices = prim->numIndices;
    ptr = RL_AllocateData(list, sizeof(uint) * list->last->numIndices);
    list->last->indices = ptr;
    list->last->beginOther = prim->beginOther;

    // Make copies of the original vertices.
    list->last->primSize = prim->primSize;
    base = RL_AllocateVertices(prim->primSize);
    memcpy(&vertices[base], &vertices[prim->indices[0]],
           prim->primSize * sizeof(gl_vertex_t));

    // Copy the vertex order from the original.
    num = prim->numIndices;
    for(i = 0; i < num; ++i)
    {
        list->last->indices[i] = base + prim->indices[i] - prim->indices[0];
    }

    num = prim->primSize;
    for(i = 0; i < prim->primSize; ++i)
    {
        // Each vertex uses the light's color.
        col = &colors[base + i];
        for(c = 0; c < 3; ++c)
            col->rgba[c] = (byte) (255 * dyn->color[c]);
        col->rgba[3] = 255;
    }

    // Texture coordinates need a bit of calculation.
    tc = &texCoords[TCA_MAIN][base];
    if(poly->type == RP_FLAT)
    {
        num = prim->primSize;
        for(i = 0; i < num; ++i)
        {
            tc[i].st[0] = (vertices[base + i].xyz[0] + dyn->s[0]) * dyn->s[1];
            tc[i].st[1] = (-vertices[base + i].xyz[2] + dyn->t[0]) * dyn->t[1];
        }
    }
    else
    {
        RL_QuadLightCoords(tc, dyn);

        if(poly->type == RP_DIVQUAD)
        {
            int     side, other, top, bottom, sideBase[2];
            float   height[2];

            height[0] = poly->vertices[2].pos[VZ] - poly->vertices[0].pos[VZ];
            height[1] = poly->vertices[3].pos[VZ] - poly->vertices[1].pos[VZ];

            // First vertices of each side (1=right, 0=left).
            sideBase[1] = base + 4;
            sideBase[0] = sideBase[1] + poly->wall->divs[1].num;

            // Set the rest of the indices and init the division vertices.
            for(side = 0; side < 2; ++side) // Left->right is side zero.
            {
                other = !side;

                // The actual top/bottom corner vertex.
                top = base + (!side ? 1 : 0);
                bottom = base + (!side ? 2 : 3);

                // Number of vertices per side: 2 + numdivs
                num = poly->wall->divs[other].num;
                for(i = 1; i <= num; ++i)
                {
                    RL_InterpolateTexCoordT(texCoords[TCA_MAIN],
                                            sideBase[other] + i - 1, top,
                                            bottom,
                                            (poly->wall->divs[other].pos[i - 1] -
                                             poly->vertices[side].pos[VZ]) / height[side]);
                }
            }
        }
    }

    // The dynlight has been written.
    RL_EndWrite(list);
}

/**
 * Adds the given poly onto the correct list.
 */
void RL_AddPoly(rendpoly_t *poly)
{
    rendlist_t *li;
    primhdr_t *hdr;
    dynlight_t *dyn;
    boolean useLights = false;

    if(poly->numvertices < 3)
        return; // huh?

    if(poly->flags & RPF_MASKED)
    {
        // Masked polys (walls) get a special treatment (=> vissprite).
        // This is needed because all masked polys must be sorted (sprites
        // are masked polys). Otherwise there will be artifacts.
        RL_AddMaskedPoly(poly);
        return;
    }

BEGIN_PROF( PROF_RL_ADD_POLY );

    // In debugSky mode we render all polys destined for the skymask as
    // regular world polys (with a few obvious properties).
    if((poly->flags & RPF_SKY_MASK) && debugSky)
    {
        texinfo_t *texinfo;
        poly->tex.id = curtex = GL_PrepareFlat(skyflatnum, &texinfo);
        poly->tex.width = texinfo->width;
        poly->tex.height = texinfo->height;
        poly->tex.detail = 0;
        poly->texoffy = poly->texoffx = 0;
        poly->lights = NULL;
        poly->flags &= ~RPF_SKY_MASK;
        poly->flags |= RPF_GLOW;
    }

    // Are lights allowed?
    if(!(poly->flags & (RPF_SKY_MASK | RPF_SHADOW | RPF_SHINY)))
    {
        // In multiplicative mode, glowing surfaces are fullbright.
        // Rendering lights on them would be pointless.
        if(!IS_MUL || !(poly->flags & RPF_GLOW))
        {
            // Surfaces lit by dynamic lights may need to be rendered
            // differently than non-lit surfaces.
            if(poly->lights)
            {
                uint        i;
                long        avglightlevel = 0;

                // Determine the average light level of this rend poly,
                // if too bright; do not bother with lights.
                for(i = 0; i < poly->numvertices; ++i)
                {
                    avglightlevel += (long) poly->vertices[i].color.rgba[0];
                    avglightlevel += (long) poly->vertices[i].color.rgba[1];
                    avglightlevel += (long) poly->vertices[i].color.rgba[2];
                }
                avglightlevel /= poly->numvertices * 3;

                if(avglightlevel < 250)
                    useLights = true;
            }
        }
    }

BEGIN_PROF( PROF_RL_GET_LIST );

    // Find/create a rendering list for the polygon's texture.
    li = RL_GetListFor(poly, useLights);

END_PROF( PROF_RL_GET_LIST );

    // This becomes the new last primitive.
    li->last = hdr = RL_AllocateData(li, sizeof(primhdr_t));

    hdr->size = 0;
    hdr->type = PT_FAN;
    hdr->indices = NULL;
    hdr->flags = poly->flags;
    hdr->light = (useLights ? poly->lights : NULL);

    switch (poly->type)
    {
    case RP_QUAD:
        RL_WriteQuad(li, poly);
        break;

    case RP_DIVQUAD:
        RL_WriteDivQuad(li, poly);
        break;

    case RP_FLAT:
        RL_WriteFlat(li, poly);
        break;

    default:
        break;
    }

    RL_EndWrite(li);

    // Generate a dynlight primitive for each of the lights affecting
    // the surface. Multitexturing may be used for the first light, so
    // it's skipped.
    if(useLights)
    {
        rendlist_t *dynList, *lastDynList = NULL;
        DGLuint     lastDynTexture = 0;

        for(dyn = (IS_MTEX_LIGHTS ? li->last->light->next : li->last->light);
            dyn; dyn = dyn->next)
        {
            // If the texture is the same as the last, the list will be too.
            if(lastDynTexture == dyn->texture)
                dynList = lastDynList;
            else
            {
                dynList = lastDynList = RL_GetLightListFor(dyn->texture);
                lastDynTexture = dyn->texture;
            }

            RL_WriteDynLight(dynList, dyn, li->last, poly);
        }
    }

END_PROF( PROF_RL_ADD_POLY );
}

void RL_FloatRGB(byte *rgb, float *dest)
{
    unsigned int i;

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
static void RL_DrawPrimitives(int conditions, rendlist_t *list)
{
    primhdr_t  *hdr;
    boolean     skip, bypass = false;

    // Should we just skip all this?
    if(conditions & DCF_SKIP)
        return;

    if(list->intertex.id)
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
            if((conditions & DCF_JUST_ONE_LIGHT) && hdr->light->next)
                skip = true;
            else if((conditions & DCF_MANY_LIGHTS) && !hdr->light->next)
                skip = true;
        }

        if(!skip)
        {
            if(conditions & DCF_SET_LIGHT_ENV)
            {
                // Use the correct texture and color for the light.
                gl.SetInteger(DGL_ACTIVE_TEXTURE,
                              conditions & DCF_SET_LIGHT_ENV0 ? 0 : 1);
                RL_Bind(hdr->light->texture);
                gl.SetFloatv(DGL_ENV_COLOR, hdr->light->color);
                // Make sure the light is not repeated.
                gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
                gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
            }

            if(conditions & DCF_SET_BLEND_MODE)
            {
                // Primitive-specific blending.  Not used in all lists.
                GL_BlendMode(hdr->blendMode);
            }

            // Render a primitive (or two) as a triangle fan.
            if(hdr->type == PT_FAN)
            {
                gl.DrawElements(DGL_TRIANGLE_FAN, hdr->numIndices, hdr->indices);
            }
            else if(hdr->type == PT_DOUBLE_FAN)
            {
                gl.DrawElements(DGL_TRIANGLE_FAN, hdr->beginOther, hdr->indices);

                gl.DrawElements(DGL_TRIANGLE_FAN,
                                hdr->numIndices - hdr->beginOther,
                                hdr->indices + hdr->beginOther);
            }
        }

        hdr = (primhdr_t *) ((byte *) hdr + hdr->size);
    }
}

static void RL_BlendState(rendlist_t *list, int modMode)
{
#ifdef _DEBUG
if(numTexUnits < 2)
    Con_Error("RL_BlendState: Not enough texture units.\n");
#endif

    RL_SelectTexUnits(2);

    RL_BindTo(0, list->tex.id);
    RL_BindTo(1, list->intertex.id);

    gl.SetInteger(DGL_MODULATE_TEXTURE, modMode);
    gl.SetInteger(DGL_ENV_ALPHA, list->interpos * 256);
}

static void RL_DetailFogState(void)
{
    // The fog color alpha is probably meaningless?
    byte    midGray[4];

    midGray[0] = 0x80;
    midGray[1] = 0x80;
    midGray[2] = 0x80;
    midGray[3] = fogColor[3];

    gl.Enable(DGL_FOG);
    gl.Fogv(DGL_FOG_COLOR, midGray);
}

/**
 * Set per-list GL state.
 * Returns the conditions to select primitives.
 */
static int RL_SetupListState(listmode_t mode, rendlist_t *list)
{
    switch (mode)
    {
    case LM_SKYMASK:
        // Render all primitives on the list without discrimination.
        return 0;

    case LM_ALL:                // All surfaces.
        // Should we do blending?
        if(list->intertex.id)
        {
            // Blend between two textures, modulate with primary color.
            RL_BlendState(list, 2);
        }
        else
        {
            // Normal modulation.
            RL_SelectTexUnits(1);
            RL_Bind(list->tex.id);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
        }
        return 0;

    case LM_LIGHT_MOD_TEXTURE:
        // Modulate sector light, dynamic light and regular texture.
        RL_BindTo(1, list->tex.id);
        return DCF_SET_LIGHT_ENV0 | DCF_JUST_ONE_LIGHT | DCF_NO_BLEND;

    case LM_TEXTURE_PLUS_LIGHT:
        RL_BindTo(0, list->tex.id);
        return DCF_SET_LIGHT_ENV1 | DCF_NO_BLEND;

    case LM_FIRST_LIGHT:
        // Draw all primitives with more than one light
        // and all primitives which will have a blended texture.
        return DCF_SET_LIGHT_ENV0 | DCF_MANY_LIGHTS | DCF_BLEND;

    case LM_BLENDED:
        // Only render the blended surfaces.
        if(!list->intertex.id)
            return DCF_SKIP;
        RL_BlendState(list, 2);
        return 0;

    case LM_BLENDED_FIRST_LIGHT:
        // Only blended surfaces.
        if(!list->intertex.id)
            return DCF_SKIP;
        return DCF_SET_LIGHT_ENV0;

    case LM_WITHOUT_TEXTURE:
        // Only render the primitives affected by dynlights.
        return 0;

    case LM_LIGHTS:
        // The light lists only contain dynlight primitives.
        RL_Bind(list->tex.id);
        // Make sure the texture is not repeated.
        gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
        return 0;

    case LM_BLENDED_MOD_TEXTURE:
        // Blending required.
        if(!list->intertex.id)
            break;
    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
        // Texture for surfaces with (many) dynamic lights.
        // Should we do blending?
        if(list->intertex.id)
        {
            // Mode 3 actually just disables the second texture stage,
            // which would modulate with primary color.
            RL_BlendState(list, 3);
            // Render all primitives.
            return 0;
        }
        // No modulation at all.
        RL_SelectTexUnits(1);
        RL_Bind(list->tex.id);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
        return (mode == LM_MOD_TEXTURE_MANY_LIGHTS ? DCF_MANY_LIGHTS : 0);

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        // Blending is not done now.
        if(list->intertex.id)
            break;
        if(list->tex.detail)
        {
            RL_SelectTexUnits(2);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 9); // Tex+Detail, no color.
            RL_BindTo(0, list->tex.id);
            RL_BindTo(1, list->tex.detail->tex);
        }
        else
        {
            RL_SelectTexUnits(1);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
            RL_Bind(list->tex.id);
        }
        return 0;

    case LM_ALL_DETAILS:
        if(!list->tex.detail)
            break;
        RL_Bind(list->tex.detail->tex);
        // Render all surfaces on the list.
        return 0;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        // Only unblended. Details are optional.
        if(list->intertex.id)
            break;
        if(list->tex.detail)
        {
            RL_SelectTexUnits(2);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 8);
            RL_BindTo(0, list->tex.id);
            RL_BindTo(1, list->tex.detail->tex);
        }
        else
        {
            // Normal modulation.
            RL_SelectTexUnits(1);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
            RL_Bind(list->tex.id);
        }
        return 0;

    case LM_BLENDED_DETAILS:
        // We'll only render blended primitives.
        if(!list->intertex.id)
            break;
        if(!list->tex.detail || !list->intertex.detail)
            break;

        RL_BindTo(0, list->tex.detail->tex);
        RL_BindTo(1, list->intertex.detail->tex);
        gl.SetInteger(DGL_ENV_ALPHA, list->interpos * 256);
        return 0;

    case LM_SHADOW:
        // Render all primitives.
        RL_Bind(list->tex.id);
        if(!list->tex.id)
        {
            // Apply a modelview shift.
            gl.MatrixMode(DGL_MODELVIEW);
            gl.PushMatrix();

            // Scale towards the viewpoint to avoid Z-fighting.
            gl.Translatef(vx, vy, vz);
            gl.Scalef(.99f, .99f, .99f);
            gl.Translatef(-vx, -vy, -vz);
        }
        return 0;

    case LM_MASKED_SHINY:
        // The intertex holds the info for the mask texture.
        RL_BindTo(1, list->intertex.id);
        gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
        gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);
        gl.SetInteger(DGL_ENV_ALPHA, 255);
    case LM_ALL_SHINY:
    case LM_SHINY:
        RL_BindTo(mode == LM_MASKED_SHINY ? 0 : 0, list->tex.id);
        // Make sure the texture is not clamped.
        gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
        gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);
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

static void RL_FinishListState(listmode_t mode, rendlist_t *list)
{
    switch (mode)
    {
    default:
        break;

    case LM_SHADOW:
        if(!list->tex.id)
        {
            // Restore original modelview matrix.
            gl.MatrixMode(DGL_MODELVIEW);
            gl.PopMatrix();
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
static void RL_SetupPassState(listmode_t mode)
{
    switch (mode)
    {
    case LM_SKYMASK:
        RL_SelectTexUnits(0);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        // We don't want to write to the color buffer.
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE);
        // No need for fog.
        if(usingFog)
            gl.Disable(DGL_FOG);
        break;

    case LM_BLENDED:
        RL_SelectTexUnits(2);
    case LM_ALL:
        // The first texture unit is used for the main texture.
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        // Fog is allowed during this pass.
        if(usingFog)
            gl.Enable(DGL_FOG);
        // All of the surfaces are opaque.
        gl.Disable(DGL_BLENDING);
        break;

    case LM_LIGHT_MOD_TEXTURE:
    case LM_TEXTURE_PLUS_LIGHT:
        // Modulate sector light, dynamic light and regular texture.
        RL_SelectTexUnits(2);
        if(mode == LM_LIGHT_MOD_TEXTURE)
        {
            RL_SelectTexCoordArray(0, TCA_LIGHT);
            RL_SelectTexCoordArray(1, TCA_MAIN);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 4); // Light * texture.
        }
        else
        {
            RL_SelectTexCoordArray(0, TCA_MAIN);
            RL_SelectTexCoordArray(1, TCA_LIGHT);
            gl.SetInteger(DGL_MODULATE_TEXTURE, 5); // Texture + light.
        }
        gl.Disable(DGL_ALPHA_TEST);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        // Fog is allowed during this pass.
        if(usingFog)
            gl.Enable(DGL_FOG);
        // All of the surfaces are opaque.
        gl.Disable(DGL_BLENDING);
        break;

    case LM_FIRST_LIGHT:
        // One light, no texture.
        RL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_LIGHT);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 6);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        // Fog is allowed during this pass.
        if(usingFog)
            gl.Disable(DGL_FOG);
        // All of the surfaces are opaque.
        gl.Disable(DGL_BLENDING);
        break;

    case LM_BLENDED_FIRST_LIGHT:
        // One additive light, no texture.
        RL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_LIGHT);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 7); // Add light, no color.
        gl.Enable(DGL_ALPHA_TEST);
        gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 1);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        // Fog is allowed during this pass.
        if(usingFog)
            gl.Disable(DGL_FOG);
        // All of the surfaces are opaque.
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE);
        break;

    case LM_WITHOUT_TEXTURE:
        RL_SelectTexUnits(0);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        // Fog must be disabled during this pass.
        gl.Disable(DGL_FOG);
        // All of the surfaces are opaque.
        gl.Disable(DGL_BLENDING);
        break;

    case LM_LIGHTS:
        RL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
        gl.Enable(DGL_ALPHA_TEST);
        gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 1);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);

        if(usingFog)
        {
            gl.Enable(DGL_FOG);
            gl.Fogv(DGL_FOG_COLOR, blackColor);
        }
        else
            gl.Disable(DGL_FOG);

        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
        break;

    case LM_MOD_TEXTURE:
    case LM_MOD_TEXTURE_MANY_LIGHTS:
    case LM_BLENDED_MOD_TEXTURE:
        // The first texture unit is used for the main texture.
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        // All of the surfaces are opaque.
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_ZERO);
        // Fog would mess with the color (this is a multiplicative pass).
        gl.Disable(DGL_FOG);
        break;

    case LM_UNBLENDED_TEXTURE_AND_DETAIL:
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_DETAIL);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Enable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
        // All of the surfaces are opaque.
        gl.Disable(DGL_BLENDING);
        // Fog is allowed.
        if(usingFog)
            gl.Enable(DGL_FOG);
        break;

    case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_DETAIL);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        // All of the surfaces are opaque.
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_ZERO);
        // This is a multiplicative pass.
        gl.Disable(DGL_FOG);
        break;

    case LM_ALL_DETAILS:
        RL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_DETAIL);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        // All of the surfaces are opaque.
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
            RL_DetailFogState();
        break;

    case LM_BLENDED_DETAILS:
        RL_SelectTexUnits(2);
        RL_SelectTexCoordArray(0, TCA_DETAIL);
        RL_SelectTexCoordArray(1, TCA_BLEND_DETAIL);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 3);
        gl.Disable(DGL_ALPHA_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        // All of the surfaces are opaque.
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
        // Use fog to fade the details, if fog is enabled.
        if(usingFog)
            RL_DetailFogState();
        break;

    case LM_SHADOW:
        // A bit like 'negative lights'.
        RL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
        gl.Enable(DGL_ALPHA_TEST);
        gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 1);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        // Set normal fog, if it's enabled.
        if(usingFog)
        {
            gl.Enable(DGL_FOG);
            gl.Fogv(DGL_FOG_COLOR, fogColor);
        }
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        break;

    case LM_SHINY:
        RL_SelectTexUnits(1);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1); // 8 for multitexture
        gl.Disable(DGL_ALPHA_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            gl.Enable(DGL_FOG);
            gl.Fogv(DGL_FOG_COLOR, blackColor);
        }
        else
        {
            gl.Disable(DGL_FOG);
        }
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE); // Purely additive.
        break;

    case LM_MASKED_SHINY:
        RL_SelectTexUnits(2);
        RL_SelectTexCoordArray(0, TCA_MAIN);
        RL_SelectTexCoordArray(1, TCA_BLEND); // the mask
        gl.SetInteger(DGL_MODULATE_TEXTURE, 8); // same as with details
        gl.Disable(DGL_ALPHA_TEST);
        gl.Disable(DGL_DEPTH_WRITE);
        gl.Enable(DGL_DEPTH_TEST);
        gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
        if(usingFog)
        {
            // Fog makes the shininess diminish in the distance.
            gl.Enable(DGL_FOG);
            gl.Fogv(DGL_FOG_COLOR, blackColor);
        }
        else
        {
            gl.Disable(DGL_FOG);
        }
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE); // Purely additive.
        break;

    default:
        break;
    }
}

/**
 * Renders the given lists. They must not be empty.
 */
static void RL_RenderLists(listmode_t mode, rendlist_t **lists, unsigned int num)
{
    unsigned int     i;
    rendlist_t      *list;

    // If the first list is empty, we do nothing. Normally we expect
    // all lists to contain something.
    if(num == 0 || lists[0]->last == NULL)
        return;

    // Setup GL state that's common to all the lists in this mode.
    RL_SetupPassState(mode);

    // Draw each given list.
    for(i = 0; i < num; ++i)
    {
        list = lists[i];

        // Setup GL state for this list, and
        // draw the necessary subset of primitives on the list.
        RL_DrawPrimitives(RL_SetupListState(mode, list), list);

        // Some modes require cleanup.
        RL_FinishListState(mode, list);
    }
}

/**
 * Extracts a selection of lists from the hash.
 */
static uint RL_CollectLists(listhash_t *table, rendlist_t **lists)
{
    rendlist_t *it;
    uint    i, count;

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
Con_Error("RL_CollectLists: Ran out of MAX_RLISTS.\n");
#endif
                    return count;
                }
                lists[count++] = it;
            }
        }
    }
    return count;
}

static void RL_LockVertices(void)
{
    // We're only locking the vertex and color arrays, so disable the
    // texcoord arrays for now. Every pass will enable/disable the texcoords
    // that are needed.
    gl.DisableArrays(0, 0, DGL_ALL_BITS);

    // Actually, don't lock anything. (Massive slowdown?)
    gl.Arrays(vertices, colors, 0, NULL, 0 /*numVertices */ );
}

static void RL_UnlockVertices(void)
{
    // Nothing was locked.
    //gl.UnlockArrays();
}

/**
 * We have several different paths to accommodate both multitextured
 * details and dynamic lights. Details take precedence (they always cover
 * entire primitives, and usually *all* of the surfaces in a scene).
 */
void RL_RenderAllLists(void)
{
    // Pointers to all the rendering lists.
    rendlist_t *lists[MAX_RLISTS];
    uint    count;

BEGIN_PROF( PROF_RL_RENDER_ALL );

    if(!freezeRLs) // only update when lists arn't frozen
        rendSky = !P_IsInVoid(viewplayer);

    // When in the void we don't render a sky.
    // FIXME: We could use a stencil when rendering the sky, using the
    //        already collected skymask polys as a mask.
    if(rendSky && !debugSky)
        // The sky might be visible. Render the needed hemispheres.
        Rend_RenderSky(skyhemispheres);

    RL_LockVertices();

    // Mask the sky in the Z-buffer.
    lists[0] = &skyMaskList;

    // FIXME: As we arn't rendering the sky when in the void we have
    //        have no need to render the skymask.
BEGIN_PROF( PROF_RL_RENDER_SKYMASK );
    if(rendSky)
        RL_RenderLists(LM_SKYMASK, lists, 1);
END_PROF( PROF_RL_RENDER_SKYMASK );

    // Render the real surfaces of the visible world.

    /*
     * Unlit Primitives
     */
    // Collect all normal lists.
BEGIN_PROF( PROF_RL_RENDER_NORMAL );

    count = RL_CollectLists(plainHash, lists);
    if(IS_MTEX_DETAILS)
    {
        // Draw details for unblended surfaces in this pass.
        RL_RenderLists(LM_UNBLENDED_TEXTURE_AND_DETAIL, lists, count);

        // Blended surfaces.
        RL_RenderLists(LM_BLENDED, lists, count);
    }
    else
    {
        // Blending is done during this pass.
        RL_RenderLists(LM_ALL, lists, count);
    }
END_PROF( PROF_RL_RENDER_NORMAL );

    /*
     * Lit Primitives
     */
    // Then the lit primitives.
BEGIN_PROF( PROF_RL_RENDER_LIGHT );

    count = RL_CollectLists(litHash, lists);

    // If multitexturing is available, we'll use it to our advantage
    // when rendering lights.
    if(IS_MTEX_LIGHTS && dlBlend != 2)
    {
        if(IS_MUL)
        {
            // All (unblended) surfaces with exactly one light can be
            // rendered in a single pass.
            RL_RenderLists(LM_LIGHT_MOD_TEXTURE, lists, count);

            // Render surfaces with many lights without a texture, just
            // with the first light.
            RL_RenderLists(LM_FIRST_LIGHT, lists, count);
        }
        else                    // Additive ('foggy') lights.
        {
            RL_RenderLists(LM_TEXTURE_PLUS_LIGHT, lists, count);

            // Render surfaces with blending.
            RL_RenderLists(LM_BLENDED, lists, count);

            // Render the first light for surfaces with blending.
            // (Not optimal but shouldn't matter; texture is changed for
            // each primitive.)
            RL_RenderLists(LM_BLENDED_FIRST_LIGHT, lists, count);
        }
    }
    else                        // Multitexturing is not available for lights.
    {
        if(IS_MUL)
        {
            // Render all lit surfaces without a texture.
            RL_RenderLists(LM_WITHOUT_TEXTURE, lists, count);
        }
        else
        {
            if(IS_MTEX_DETAILS) // Draw detail textures using multitexturing.
            {
                // Unblended surfaces with a detail.
                RL_RenderLists(LM_UNBLENDED_TEXTURE_AND_DETAIL, lists, count);

                // Blended surfaces without details.
                RL_RenderLists(LM_BLENDED, lists, count);

                // Details for blended surfaces.
                RL_RenderLists(LM_BLENDED_DETAILS, lists, count);
            }
            else
            {
                RL_RenderLists(LM_ALL, lists, count);
            }
        }
    }

    /*
     * Dynamic Lights
     */
    // Draw all dynamic lights (always additive).
    count = RL_CollectLists(dynHash, lists);
    if(dlBlend != 2)
        RL_RenderLists(LM_LIGHTS, lists, count);

END_PROF( PROF_RL_RENDER_LIGHT );

    /*
     * Texture Modulation Pass
     */
    if(IS_MUL)
    {
        // Finish the lit surfaces that didn't yet get a texture.
        count = RL_CollectLists(litHash, lists);
        if(IS_MTEX_DETAILS)
        {
            RL_RenderLists(LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL, lists, count);
            RL_RenderLists(LM_BLENDED_MOD_TEXTURE, lists, count);
            RL_RenderLists(LM_BLENDED_DETAILS, lists, count);
        }
        else
        {
            if(IS_MTEX_LIGHTS && dlBlend != 2)
            {
                RL_RenderLists(LM_MOD_TEXTURE_MANY_LIGHTS, lists, count);
            }
            else
            {
                RL_RenderLists(LM_MOD_TEXTURE, lists, count);
            }
        }
    }

    /*
     * Detail Modulation Pass
     */
    // If multitexturing is not available for details, we need to apply
    // them as an extra pass over all the detailed surfaces.
    if(r_detail)
    {
        // Render detail textures for all surfaces that need them.
        count = RL_CollectLists(plainHash, lists);
        if(IS_MTEX_DETAILS)
        {
            // Blended detail textures.
            RL_RenderLists(LM_BLENDED_DETAILS, lists, count);
        }
        else
        {
            RL_RenderLists(LM_ALL_DETAILS, lists, count);

            count = RL_CollectLists(litHash, lists);
            RL_RenderLists(LM_ALL_DETAILS, lists, count);
        }
    }

    /*
     * Shiny Surfaces Pass
     */
    // Draw the shiny environment surfaces.
    //
    // If we have two texture units, the shiny masks will be
    // enabled.  Otherwise the masks are ignored.  The shine is
    // basically specular environmental additive light, multiplied
    // by the mask so that black texels in the mask produce areas
    // without shine.
    //
    // Walls with holes (so called 'masked textures') cannot be
    // shiny.
BEGIN_PROF( PROF_RL_RENDER_SHINY );

    count = RL_CollectLists(shinyHash, lists);
    if(numTexUnits > 1)
    {
        // Render masked shiny surfaces in a separate pass.
        RL_RenderLists(LM_SHINY, lists, count);
        RL_RenderLists(LM_MASKED_SHINY, lists, count);
    }
    else
    {
        RL_RenderLists(LM_ALL_SHINY, lists, count);
    }
END_PROF( PROF_RL_RENDER_SHINY );

    /*
     * Shadow Pass: Objects and FakeRadio
     */
    {
        int     oldr = renderTextures;

        renderTextures = true;
BEGIN_PROF( PROF_RL_RENDER_SHADOW );

        count = RL_CollectLists(shadowHash, lists);

        RL_RenderLists(LM_SHADOW, lists, count);

END_PROF( PROF_RL_RENDER_SHADOW );

        renderTextures = oldr;
    }

    RL_UnlockVertices();

    // Return to the normal GL state.
    RL_SelectTexUnits(1);
    gl.DisableArrays(true, true, DGL_ALL_BITS);
    gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
    gl.Enable(DGL_DEPTH_WRITE);
    gl.Enable(DGL_DEPTH_TEST);
    gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
    gl.Enable(DGL_ALPHA_TEST);
    gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 0);
    gl.Enable(DGL_BLENDING);
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    if(usingFog)
    {
        gl.Enable(DGL_FOG);
        gl.Fogv(DGL_FOG_COLOR, fogColor);
    }
    else
    {
        gl.Disable(DGL_FOG);
    }

    // Draw masked walls, sprites and models.
BEGIN_PROF( PROF_RL_RENDER_MASKED );

    Rend_DrawMasked();

    // Draw particles.
    PG_Render();

END_PROF( PROF_RL_RENDER_MASKED );
END_PROF( PROF_RL_RENDER_ALL );
}
