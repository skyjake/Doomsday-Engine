/**\file r_data.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Data Structures and Constants for Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

#include "colorpalette.h"
#include "m_stack.h"
#include "texture.h"
#include "materialvariant.h"

// MACROS ------------------------------------------------------------------

#define RAWTEX_HASH_SIZE    128
#define RAWTEX_HASH(x)      (rawtexhash + (((unsigned) x) & (RAWTEX_HASH_SIZE - 1)))

#define COLORPALETTENAME_MAXLEN     (32)

// TYPES -------------------------------------------------------------------

typedef char patchname_t[9];

typedef struct rawtexhash_s {
    rawtex_t*     first;
} rawtexhash_t;

typedef enum {
    RPT_VERT,
    RPT_COLOR,
    RPT_TEXCOORD
} rpolydatatype_t;

typedef struct {
    boolean         inUse;
    rpolydatatype_t type;
    uint            num;
    void*           data;
} rendpolydata_t;

/**
 * Color palette name binding. Mainly intended as a public API convenience.
 * Internally we are only interested in the associated palette indices.
 */
typedef struct {
    char name[COLORPALETTENAME_MAXLEN+1];
    uint idx;
} colorpalettebind_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean mapSetup; // We are currently setting up a map.

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte precacheSkins = true;
byte precacheMapMaterials = true;
byte precacheSprites = true;

byte* translationTables = NULL;

int gameDataFormat; // Use a game-specifc data format where applicable.
byte rendInfoRPolys = 0;

colorpaletteid_t defaultColorPalette = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// LUT which translates flat "original indices" to their associated Texture* (if any).
// Index with origIndex - flatOrigIndexBase
static int flatOrigIndexBase = 0;
static uint flatOrigIndexMapSize = 0;
static texture_t** flatOrigIndexMap = NULL;

// LUT which translates patchcomposite "original indices" to their associated Texture* (if any).
// Index with origIndex - patchCompositeOrigIndexBase
static int patchCompositeOrigIndexBase = 0;
static uint patchCompositeOrigIndexMapSize = 0;
static texture_t** patchCompositeOrigIndexMap = NULL;

// LUT which translates patchid_t to patchtex_t*. Index with patchid_t-1
static uint patchTextureIdMapSize = 0;
static patchtex_t** patchTextureIdMap = NULL;

static rawtexhash_t rawtexhash[RAWTEX_HASH_SIZE];

static unsigned int numrendpolys = 0;
static unsigned int maxrendpolys = 0;
static rendpolydata_t** rendPolys;

static boolean initedColorPalettes = false;

static int numColorPalettes;
static colorpalette_t** colorPalettes;
static int numColorPaletteBinds;
static colorpalettebind_t* colorPaletteBinds;

// CODE --------------------------------------------------------------------

static colorpaletteid_t colorPaletteNumForName(const char* name)
{
    assert(initedColorPalettes);
    // Linear search (sufficiently fast enough given the probably small set
    // and infrequency of searches).
    { int i;
    for(i = 0; i < numColorPaletteBinds; ++i)
    {
        colorpalettebind_t* pal = &colorPaletteBinds[i];
        if(!strnicmp(pal->name, name, COLORPALETTENAME_MAXLEN))
            return i + 1; // Already registered. 1-based index.
    }}
    return 0; // Not found.
}

static int createColorPalette(const int compOrder[3], const uint8_t compSize[3],
    const uint8_t* data, ushort num)
{
    assert(initedColorPalettes && compOrder && compSize && data);
    {
    colorpalette_t* pal = ColorPalette_NewWithColorTable(compOrder, compSize, data, num);

    colorPalettes = (colorpalette_t**) realloc(colorPalettes, (numColorPalettes + 1) * sizeof(*colorPalettes));
    if(NULL == colorPalettes)
        Con_Error("createColorPalette: Failed on (re)allocation of %lu bytes for "
            "color palette list.", (unsigned long) ((numColorPalettes + 1) * sizeof(*colorPalettes)));
    colorPalettes[numColorPalettes] = pal;

    return ++numColorPalettes; // 1-based index.
    }
}

static void deleteColorPalettes(size_t n, const int* palettes)
{
    assert(initedColorPalettes);

    if(!(n > 0) || !palettes)
        return;

    { size_t i;
    for(i = 0; i < n; ++i)
    {
        if(palettes[i] > 0 && palettes[i] - 1 < numColorPalettes)
        {
            int idx = palettes[i]-1;
            ColorPalette_Delete(colorPalettes[idx]);
            memmove(&colorPalettes[idx], &colorPalettes[idx+1], sizeof(colorpalette_t*));
            numColorPalettes -= 1;
        }
    }}

    if(numColorPalettes)
    {
        colorPalettes = (colorpalette_t**) realloc(colorPalettes, numColorPalettes * sizeof(colorpalette_t*));
        if(NULL == colorPalettes)
            Con_Error("deleteColorPalettes: Failed on (re)allocation of %lu bytes for "
                "color palette list.", (unsigned long) (numColorPalettes * sizeof(*colorPalettes)));
    }
    else
    {
        free(colorPalettes); colorPalettes = NULL;
    }
}

void R_InitColorPalettes(void)
{
    if(initedColorPalettes)
    {   // Re-init.
        R_DestroyColorPalettes();
        return;
    }

    colorPalettes = NULL;
    numColorPalettes = 0;

    colorPaletteBinds = NULL;
    numColorPaletteBinds = 0;
    defaultColorPalette = 0;

    initedColorPalettes = true;
}

void R_DestroyColorPalettes(void)
{
    if(!initedColorPalettes) return;

    if(0 != numColorPalettes)
    {
        int i;
        for(i = 0; i < numColorPalettes; ++i)
            ColorPalette_Delete(colorPalettes[i]);
        free(colorPalettes); colorPalettes = NULL;
        numColorPalettes = 0;
    }

    if(colorPaletteBinds)
    {
        free(colorPaletteBinds); colorPaletteBinds = NULL;
        numColorPaletteBinds = 0;
    }

    defaultColorPalette = 0;
}

int R_ColorPaletteCount(void)
{
    assert(initedColorPalettes);
    return numColorPalettes;
}

colorpalette_t* R_ToColorPalette(colorpaletteid_t id)
{
    assert(initedColorPalettes);
    if(id == 0 || id - 1 >= (unsigned)numColorPaletteBinds)
        id = defaultColorPalette;
    if(id != 0 && numColorPaletteBinds > 0)
        return colorPalettes[colorPaletteBinds[id-1].idx-1];
    return NULL;
}

colorpalette_t* R_GetColorPaletteByIndex(int paletteIdx)
{
    assert(initedColorPalettes);
    if(paletteIdx > 0 && numColorPalettes >= paletteIdx)
    {
        return colorPalettes[paletteIdx-1];
    }
    Con_Error("R_GetColorPaletteByIndex: Failed to locate palette for index #%i", paletteIdx);
    exit(1); // Unreachable.
}

boolean R_SetDefaultColorPalette(colorpaletteid_t id)
{
    assert(initedColorPalettes);
    if(id - 1 < (unsigned)numColorPaletteBinds)
    {
        defaultColorPalette = id;
        return true;
    }
    VERBOSE( Con_Message("R_SetDefaultColorPalette: Invalid id %u.\n", id) );
    return false;
}

colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name,
    const uint8_t* colorData, int colorCount)
{
    static const char* compNames[] = { "red", "green", "blue" };
    colorpaletteid_t id;
    colorpalettebind_t* bind;
    const char* c, *end;
    int i, pos, compOrder[3];
    uint8_t compSize[3];

    if(!initedColorPalettes)
        Con_Error("R_CreateColorPalette: Color palettes not yet initialized.");

    if(!name || !name[0])
        Con_Error("R_CreateColorPalette: Color palettes require a name.");

    if(strlen(name) > COLORPALETTENAME_MAXLEN)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", color palette "
            "name can be at most %i characters long.", name, COLORPALETTENAME_MAXLEN);

    if(!fmt || !fmt[0])
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", missing "
            "format string.", name);

    if(NULL == colorData || colorCount <= 0)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", cannot create "
            "a zero-sized palette.", name);

    // All arguments supplied. Parse the format string.
    memset(compOrder, -1, sizeof(compOrder));
    pos = 0;
    end = fmt + (strlen(fmt) - 1);
    c = fmt;
    do
    {
        int comp = -1;

        if(*c == 'R' || *c == 'r')
            comp = CR;
        else if(*c == 'G' || *c == 'g')
            comp = CG;
        else if(*c == 'B' || *c == 'b')
            comp = CB;

        if(comp != -1)
        {
            // Have we encountered this component yet?
            if(compOrder[comp] == -1)
            {   // No.
                const char* start;
                size_t numDigits;

                compOrder[comp] = pos++;

                // Read the number of bits.
                start = ++c;
                while((*c >= '0' && *c <= '9') && ++c < end);

                numDigits = c - start;
                if(numDigits != 0 && numDigits <= 2)
                {
                    char buf[3];

                    memset(buf, 0, sizeof(buf));
                    memcpy(buf, start, numDigits);

                    compSize[comp] = atoi(buf);

                    if(pos == 3)
                        break; // We're done.

                    // Unread the last character.
                    c--;
                    continue;
                }
            }
        }

        Con_Error("R_CreateColorPalette: Failed creating \"%s\", invalid character '%c' "
            "in format string at position %u.\n", name, *c, (unsigned int) (c - fmt));
    } while(++c <= end);

    if(pos != 3)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", incomplete "
            "format specification.\n", name);

    // Check validity of bits per component.
    for(i = 0; i < 3; ++i)
        if(compSize[i] == 0 || compSize[i] > COLORPALETTE_MAX_COMPONENT_BITS)
        {
            Con_Error("R_CreateColorPalette: Failed creating \"%s\", unsupported bit "
                "depth %i for %s component.\n", name, compSize[i], compNames[compOrder[i]]);
        }

    if(0 != (id = colorPaletteNumForName(name)))
    {   // Replacing an existing palette.
        colorpalette_t* palette;

        bind = &colorPaletteBinds[id-1];
        palette = R_GetColorPaletteByIndex(bind->idx);
        ColorPalette_ReplaceColorTable(palette, compOrder, compSize, colorData, colorCount);
        GL_ReleaseGLTexturesByColorPalette(id);
    }
    else
    {   // A new palette.
        colorPaletteBinds = (colorpalettebind_t*) realloc(colorPaletteBinds,
                (numColorPaletteBinds + 1) * sizeof(colorpalettebind_t));
        if(NULL == colorPaletteBinds)
            Con_Error("R_CreateColorPalette: Failed on (re)allocation of %lu bytes for "
                "color palette bind list.", (unsigned long) ((numColorPaletteBinds + 1) * sizeof(colorpalettebind_t)));

        bind = &colorPaletteBinds[numColorPaletteBinds];
        memset(bind, 0, sizeof(*bind));

        strncpy(bind->name, name, COLORPALETTENAME_MAXLEN);

        id = (colorpaletteid_t) ++numColorPaletteBinds; // 1-based index.
        if(1 == numColorPaletteBinds)
            defaultColorPalette = (colorpaletteid_t) numColorPaletteBinds;
    }

    // Generate the color palette.
    bind->idx = createColorPalette(compOrder, compSize, colorData, colorCount);

    return id; // 1-based index.
}

colorpaletteid_t R_GetColorPaletteNumForName(const char* name)
{
    if(!initedColorPalettes)
        Con_Error("R_GetColorPaletteNumForName: Color palettes not yet initialized.");

    if(name && name[0] && strlen(name) <= COLORPALETTENAME_MAXLEN)
        return colorPaletteNumForName(name);
    return 0;
}

const char* R_GetColorPaletteNameForNum(colorpaletteid_t id)
{
    if(!initedColorPalettes)
        Con_Error("R_GetColorPaletteNameForNum: Color palettes not yet initialized.");

    if(id != 0 && id - 1 < (unsigned)numColorPaletteBinds)
        return colorPaletteBinds[id-1].name;
    return NULL;
}

void R_GetColorPaletteRGBubv(colorpaletteid_t paletteId, int colorIdx, uint8_t rgb[3],
    boolean applyTexGamma)
{
    if(!initedColorPalettes)
        Con_Error("R_GetColorPaletteRGBubv: Color palettes not yet initialized.");
    if(NULL == rgb)
        Con_Error("R_GetColorPaletteRGBubv: Invalid arguments (rgb==NULL).");

    if(colorIdx < 0)
    {
        rgb[CR] = rgb[CG] = rgb[CB] = 0;
        return;
    }

    { colorpalette_t* palette = R_ToColorPalette(paletteId);
    if(NULL != palette)
    {
        ColorPalette_Color(palette, colorIdx, rgb);
        if(applyTexGamma)
        {
            rgb[CR] = gammaTable[rgb[CR]];
            rgb[CG] = gammaTable[rgb[CG]];
            rgb[CB] = gammaTable[rgb[CB]];
        }
        return;
    }}

    Con_Message("Warning:R_GetColorPaletteRGBubv: Failed to locate ColorPalette for id %i.\n", paletteId);
}

void R_GetColorPaletteRGBf(colorpaletteid_t paletteId, int colorIdx, float rgb[3],
    boolean applyTexGamma)
{
    if(!initedColorPalettes)
        Con_Error("R_GetColorPaletteRGBf: Color palettes not yet initialized.");
    if(NULL == rgb)
        Con_Error("R_GetColorPaletteRGBf: Invalid arguments (rgb==NULL).");

    if(colorIdx < 0)
    {
        rgb[CR] = rgb[CG] = rgb[CB] = 0;
        return;
    }

    { colorpalette_t* palette = R_ToColorPalette(paletteId);
    if(NULL != palette)
    {
        uint8_t ubv[3];
        ColorPalette_Color(palette, colorIdx, ubv);
        if(applyTexGamma)
        {
            ubv[CR] = gammaTable[ubv[CR]];
            ubv[CG] = gammaTable[ubv[CG]];
            ubv[CB] = gammaTable[ubv[CB]];
        }
        rgb[CR] = ubv[CR] * reciprocal255;
        rgb[CG] = ubv[CG] * reciprocal255;
        rgb[CB] = ubv[CB] * reciprocal255;
        return;
    }}

    Con_Message("Warning:R_GetColorPaletteRGBf: Failed to locate ColorPalette for id %i.\n", paletteId);
}

void R_InfoRendVerticesPool(void)
{
    uint                i;

    if(!rendInfoRPolys)
        return;

    Con_Printf("RP Count: %-4i\n", numrendpolys);

    for(i = 0; i < numrendpolys; ++i)
    {
        rendpolydata_t*     rp = rendPolys[i];

        Con_Printf("RP: %-4u %c (vtxs=%u t=%c)\n", i,
                   (rp->inUse? 'Y':'N'), rp->num,
                   (rp->type == RPT_VERT? 'v' :
                    rp->type == RPT_COLOR?'c' : 't'));
    }
}

/**
 * Called at the start of each map.
 */
void R_InitRendVerticesPool(void)
{
    rvertex_t*          rvertices;
    rcolor_t*           rcolors;
    rtexcoord_t*        rtexcoords;

    numrendpolys = maxrendpolys = 0;
    rendPolys = NULL;

    rvertices = R_AllocRendVertices(24);
    rcolors = R_AllocRendColors(24);
    rtexcoords = R_AllocRendTexCoords(24);

    // Mark unused.
    R_FreeRendVertices(rvertices);
    R_FreeRendColors(rcolors);
    R_FreeRendTexCoords(rtexcoords);
}

/**
 * Retrieves a batch of rvertex_t.
 * Possibly allocates new if necessary.
 *
 * @param num           The number of verts required.
 *
 * @return              Ptr to array of rvertex_t
 */
rvertex_t* R_AllocRendVertices(uint num)
{
    unsigned int        idx;
    boolean             found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse || rendPolys[idx]->type != RPT_VERT)
            continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (rvertex_t*) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint                i, newCount;
            rendpolydata_t*     newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys =
                Z_Realloc(rendPolys, sizeof(rendpolydata_t*) * maxrendpolys,
                          PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData =
                Z_Malloc(sizeof(rendpolydata_t) * newCount, PU_MAP, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                ptr->data = NULL;
                ptr->type = RPT_VERT;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type = RPT_VERT;
    rendPolys[idx]->num = num;
    rendPolys[idx]->data =
        Z_Malloc(sizeof(rvertex_t) * num, PU_MAP, 0);

    return (rvertex_t*) rendPolys[idx]->data;
}

/**
 * Retrieves a batch of rcolor_t.
 * Possibly allocates new if necessary.
 *
 * @param num           The number of verts required.
 *
 * @return              Ptr to array of rcolor_t
 */
rcolor_t* R_AllocRendColors(uint num)
{
    unsigned int        idx;
    boolean             found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse || rendPolys[idx]->type != RPT_COLOR)
            continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (rcolor_t*) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint                i, newCount;
            rendpolydata_t*     newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys =
                Z_Realloc(rendPolys, sizeof(rendpolydata_t*) * maxrendpolys,
                          PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData =
                Z_Malloc(sizeof(rendpolydata_t) * newCount, PU_MAP, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                ptr->data = NULL;
                ptr->type = RPT_COLOR;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type = RPT_COLOR;
    rendPolys[idx]->num = num;
    rendPolys[idx]->data =
        Z_Malloc(sizeof(rcolor_t) * num, PU_MAP, 0);

    return (rcolor_t*) rendPolys[idx]->data;
}

/**
 * Retrieves a batch of rtexcoord_t.
 * Possibly allocates new if necessary.
 *
 * @param num           The number required.
 *
 * @return              Ptr to array of rtexcoord_t
 */
rtexcoord_t* R_AllocRendTexCoords(uint num)
{
    unsigned int        idx;
    boolean             found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse || rendPolys[idx]->type != RPT_TEXCOORD)
            continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (rtexcoord_t*) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint                i, newCount;
            rendpolydata_t*     newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys =
                Z_Realloc(rendPolys, sizeof(rendpolydata_t*) * maxrendpolys,
                          PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData =
                Z_Malloc(sizeof(rendpolydata_t) * newCount, PU_MAP, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                ptr->data = NULL;
                ptr->type = RPT_TEXCOORD;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type = RPT_TEXCOORD;
    rendPolys[idx]->num = num;
    rendPolys[idx]->data =
        Z_Malloc(sizeof(rtexcoord_t) * num, PU_MAP, 0);

    return (rtexcoord_t*) rendPolys[idx]->data;
}

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param vertices      Ptr to array of rvertex_t to mark unused.
 */
void R_FreeRendVertices(rvertex_t* rvertices)
{
    uint                i;

    if(!rvertices)
        return;

    for(i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rvertices)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }
#if _DEBUG
    Con_Message("R_FreeRendPoly: Dangling poly ptr!\n");
#endif
}

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param vertices      Ptr to array of rcolor_t to mark unused.
 */
void R_FreeRendColors(rcolor_t* rcolors)
{
    uint                i;

    if(!rcolors)
        return;

    for(i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rcolors)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }
#if _DEBUG
    Con_Message("R_FreeRendPoly: Dangling poly ptr!\n");
#endif
}

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param rtexcoords    Ptr to array of rtexcoord_t to mark unused.
 */
void R_FreeRendTexCoords(rtexcoord_t* rtexcoords)
{
    uint                i;

    if(!rtexcoords)
        return;

    for(i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rtexcoords)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }
#if _DEBUG
    Con_Message("R_FreeRendPoly: Dangling poly ptr!\n");
#endif
}

void R_DivVerts(rvertex_t* dst, const rvertex_t* src, const walldiv_t* divs)
{
#define COPYVERT(d, s)  (d)->pos[VX] = (s)->pos[VX]; \
    (d)->pos[VY] = (s)->pos[VY]; \
    (d)->pos[VZ] = (s)->pos[VZ];

    uint                i;
    uint                numL = 3 + divs[0].num;
    uint                numR = 3 + divs[1].num;

    // Right fan:
    COPYVERT(&dst[numL + 0], &src[0])
    COPYVERT(&dst[numL + 1], &src[3]);
    COPYVERT(&dst[numL + numR - 1], &src[2]);

    for(i = 0; i < divs[1].num; ++i)
    {
        dst[numL + 2 + i].pos[VX] = src[2].pos[VX];
        dst[numL + 2 + i].pos[VY] = src[2].pos[VY];
        dst[numL + 2 + i].pos[VZ] = divs[1].pos[i];
    }

    // Left fan:
    COPYVERT(&dst[0], &src[3]);
    COPYVERT(&dst[1], &src[0]);
    COPYVERT(&dst[numL - 1], &src[1]);

    for(i = 0; i < divs[0].num; ++i)
    {
        dst[2 + i].pos[VX] = src[0].pos[VX];
        dst[2 + i].pos[VY] = src[0].pos[VY];
        dst[2 + i].pos[VZ] = divs[0].pos[i];
    }

#undef COPYVERT
}

void R_DivTexCoords(rtexcoord_t* dst, const rtexcoord_t* src,
                    const walldiv_t* divs, float bL, float tL, float bR,
                    float tR)
{
#define COPYTEXCOORD(d, s)    (d)->st[0] = (s)->st[0]; \
    (d)->st[1] = (s)->st[1];

    uint                i;
    uint                numL = 3 + divs[0].num;
    uint                numR = 3 + divs[1].num;
    float               height;

    // Right fan:
    COPYTEXCOORD(&dst[numL + 0], &src[0]);
    COPYTEXCOORD(&dst[numL + 1], &src[3]);
    COPYTEXCOORD(&dst[numL + numR-1], &src[2]);

    height = tR - bR;
    for(i = 0; i < divs[1].num; ++i)
    {
        float               inter = (divs[1].pos[i] - bR) / height;

        dst[numL + 2 + i].st[0] = src[3].st[0];
        dst[numL + 2 + i].st[1] = src[2].st[1] +
            (src[3].st[1] - src[2].st[1]) * inter;
    }

    // Left fan:
    COPYTEXCOORD(&dst[0], &src[3]);
    COPYTEXCOORD(&dst[1], &src[0]);
    COPYTEXCOORD(&dst[numL - 1], &src[1]);

    height = tL - bL;
    for(i = 0; i < divs[0].num; ++i)
    {
        float               inter = (divs[0].pos[i] - bL) / height;

        dst[2 + i].st[0] = src[0].st[0];
        dst[2 + i].st[1] = src[0].st[1] +
            (src[1].st[1] - src[0].st[1]) * inter;
    }

#undef COPYTEXCOORD
}

void R_DivVertColors(rcolor_t* dst, const rcolor_t* src,
                     const walldiv_t* divs, float bL, float tL, float bR,
                     float tR)
{
#define COPYVCOLOR(d, s)    (d)->rgba[CR] = (s)->rgba[CR]; \
    (d)->rgba[CG] = (s)->rgba[CG]; \
    (d)->rgba[CB] = (s)->rgba[CB]; \
    (d)->rgba[CA] = (s)->rgba[CA];

    uint                i;
    uint                numL = 3 + divs[0].num;
    uint                numR = 3 + divs[1].num;
    float               height;

    // Right fan:
    COPYVCOLOR(&dst[numL + 0], &src[0]);
    COPYVCOLOR(&dst[numL + 1], &src[3]);
    COPYVCOLOR(&dst[numL + numR-1], &src[2]);

    height = tR - bR;
    for(i = 0; i < divs[1].num; ++i)
    {
        uint                c;
        float               inter = (divs[1].pos[i] - bR) / height;

        for(c = 0; c < 4; ++c)
        {
            dst[numL + 2 + i].rgba[c] = src[2].rgba[c] +
                (src[3].rgba[c] - src[2].rgba[c]) * inter;
        }
    }

    // Left fan:
    COPYVCOLOR(&dst[0], &src[3]);
    COPYVCOLOR(&dst[1], &src[0]);
    COPYVCOLOR(&dst[numL - 1], &src[1]);

    height = tL - bL;
    for(i = 0; i < divs[0].num; ++i)
    {
        uint                c;
        float               inter = (divs[0].pos[i] - bL) / height;

        for(c = 0; c < 4; ++c)
        {
            dst[2 + i].rgba[c] = src[0].rgba[c] +
                (src[1].rgba[c] - src[0].rgba[c]) * inter;
        }
    }

#undef COPYVCOLOR
}

int R_GetTextureOriginalIndex(texture_t* tex)
{
    assert(tex);
    switch(Textures_Namespace(tex))
    {
    case TN_FLATS: {
        flat_t* flat = (flat_t*)Texture_UserData(tex);
        assert(flat);
        return flat->origIndex;
      }
    case TN_TEXTURES: {
        patchcompositetex_t* pcTex = (patchcompositetex_t*)Texture_UserData(tex);
        assert(pcTex);
        return pcTex->origIndex;
      }
    default:
        Con_Error("R_GetTextureOriginalIndex: Textures in namespace '%s' do not have an 'original index'.", Str_Text(Textures_NamespaceName(Textures_Namespace(tex))));
        exit(1); // Unreachable.
    }
}

texture_t* R_TextureForOriginalIndex(int index, texturenamespaceid_t texNamespace)
{
    if(VALID_TEXTURENAMESPACEID(texNamespace))
    {
        switch(texNamespace)
        {
        case TN_FLATS:
            if(index >= flatOrigIndexBase && (unsigned)(index - flatOrigIndexBase) <= flatOrigIndexMapSize)
            {
                return flatOrigIndexMap[index - flatOrigIndexBase];
            }
            break;
        case TN_TEXTURES:
            if(index >= patchCompositeOrigIndexBase &&
               (unsigned)(index - patchCompositeOrigIndexBase) <= patchCompositeOrigIndexMapSize)
            {
                return patchCompositeOrigIndexMap[index - patchCompositeOrigIndexBase];
            }
            break;
        default:
            Con_Message("Warning, textures in namespace '%s' do not have an 'original index', returning null-object.\n", Str_Text(Textures_NamespaceName(texNamespace)));
            break;
        }
    }
    return NULL; // Not found.
}

/// \note Part of the Doomsday public API.
int R_OriginalIndexForTexture2(const Uri* uri, boolean quiet)
{
    texture_t* tex = Textures_TextureForUri2(uri, quiet);
    if(tex)
    {
        switch(Textures_Namespace(tex))
        {
        case TN_FLATS:
        case TN_TEXTURES:
            return R_GetTextureOriginalIndex(tex);
        default:
            if(!quiet)
            {
                ddstring_t* path = Uri_ToString(uri);
                Con_Message("Warning texture \"%s\" does not have an 'original index', returning 0.\n", Str_Text(path));
                Str_Delete(path);
            }
            return -1;
        }
    }
    if(!quiet)
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, unknown texture: %s\n", Str_Text(path));
        Str_Delete(path);
    }
    return -1;
}

/// \note Part of the Doomsday public API.
int R_OriginalIndexForTexture(const Uri* uri)
{
    return R_OriginalIndexForTexture2(uri, false);
}

void R_InitSystemTextures(void)
{
    static const struct texdef_s {
        const char* texPath;
        const char* filePath;
    } defs[] = {
        { "unknown", GRAPHICS_RESOURCE_NAMESPACE_NAME":unknown" },
        { "missing", GRAPHICS_RESOURCE_NAMESPACE_NAME":missing" },
        { "bbox",    GRAPHICS_RESOURCE_NAMESPACE_NAME":bbox" },
        { "gray",    GRAPHICS_RESOURCE_NAMESPACE_NAME":gray" },
        { NULL, NULL }
    };
    Uri* uri = Uri_New();
    uint i;

    VERBOSE( Con_Message("Initializing System textures...\n") )

    Uri_SetScheme(uri, TN_SYSTEM_NAME);
    for(i = 0; defs[i].texPath; ++i)
    {
        texture_t* tex;
        systex_t* sysTex;

        // Have we already encountered this name?
        Uri_SetPath(uri, defs[i].texPath);
        tex = Textures_TextureForUri2(uri, true/*quiet please*/);
        if(!tex)
        {
            // A new systex.
            sysTex = (systex_t*)malloc(sizeof *sysTex);
            if(!sysTex)
                Con_Error("R_InitSystemTextures: Failed on allocation of %lu bytes for new SysTex.", (unsigned long) sizeof *sysTex);
            sysTex->external = NULL;

            tex = Textures_Create(uri, TXF_CUSTOM, (void*)sysTex);
            if(!tex)
            {
                ddstring_t* path = Uri_ToString(uri);
                Con_Message("Warning, failed creating Texture for new SysTex %s.\n", Str_Text(path));
                Str_Delete(path);
                free(sysTex);
            }
        }
        else
        {
            sysTex = (systex_t*)Texture_UserData(tex);

            /// \fixme Update any Materials (and thus Surfaces) which reference this.
        }

        // (Re)configure this SysTex.
        if(!sysTex->external)
            sysTex->external = Uri_New();
        Uri_SetUri2(sysTex->external, defs[i].filePath);
    }
    Uri_Delete(uri);
}

static patchtex_t* getPatchTex(patchid_t id)
{
    if(id == 0 || id > patchTextureIdMapSize) return NULL;
    return patchTextureIdMap[id-1];
}

static patchid_t getPatchId(patchtex_t* ptex)
{
    uint i;
    assert(ptex);

    for(i = 0; i < patchTextureIdMapSize; ++i)
    {
        if(patchTextureIdMap[i] == ptex)
            return i+1; // 1-based index.
    }
    Con_Error("getPatchId: Failed to locate id for ptex [%p].", (void*)ptex);
    exit(1); // Unreachable.
}

static patchtex_t* findPatchTextureByName(const char* name)
{
    texture_t* tex;
    Uri* uri;
    assert(name && name[0]);

    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_PATCHES_NAME);
    tex = Textures_TextureForUri2(uri, true/*quiet please*/);
    Uri_Delete(uri);
    if(!tex) return NULL;
    return (patchtex_t*)Texture_UserData(tex);
}

patchtex_t* R_PatchTextureByIndex(patchid_t id)
{
    patchtex_t* patchTex = getPatchTex(id);
    if(!patchTex)
        Con_Error("R_PatchTextureByIndex: Unknown patch %i.", id);
    return patchTex;
}

patchid_t R_RegisterPatch(const char* name)
{
    const doompatch_header_t* patch;
    abstractfile_t* fsObject;
    int lumpIdx, flags = 0;
    lumpnum_t lumpNum;
    texture_t* tex;
    patchtex_t* p;
    Uri* uri;

    if(!name || !name[0]) return 0;

    // Already defined as a patch?
    p = findPatchTextureByName(name);
    if(p) return getPatchId(p);

    lumpNum = F_CheckLumpNumForName2(name, true);
    if(lumpNum < 0)
    {
        Con_Message("Warning:R_RegisterPatch: Failed to locate lump for patch '%s'.\n", name);
        return 0;
    }
    
    fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    /// \fixme What about min lumpNum size?
    patch = (const doompatch_header_t*) F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);

    // An entirely new patch.
    p = (patchtex_t*)malloc(sizeof *p);
    if(!p)
        Con_Error("R_RegisterPatch: Failed on allocation of %lu bytes for new PatchTex.", (unsigned long) sizeof *p);

    p->lumpNum = lumpNum;
    /**
     * \fixme: Cannot be sure this is in Patch format until a load attempt
     * is made. We should not read this info here!
     */
    p->offX = -SHORT(patch->leftOffset);
    p->offY = -SHORT(patch->topOffset);

    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    p->flags = 0;
    if(monochrome)               p->flags |= PF_MONOCHROME;
    if(upscaleAndSharpenPatches) p->flags |= PF_UPSCALE_AND_SHARPEN;

    // Register a texture for this.
    if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;

    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_PATCHES_NAME);
    tex = Textures_CreateWithDimensions(uri, flags, SHORT(patch->width), SHORT(patch->height), (void*)p);
    F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);
    Uri_Delete(uri);

    if(!tex)
    {
        Con_Message("Warning, failed creating Texture for patch lump %s.\n", name);
        free(p);
        return 0;
    }
    p->texId = Textures_Id(tex);

    // Add it to the patch id map.
    patchTextureIdMap = (patchtex_t**)realloc(patchTextureIdMap, sizeof *patchTextureIdMap * ++patchTextureIdMapSize);
    if(!patchTextureIdMap)
        Con_Error("R_RegisterPatch: Failed on (re)allocation of %lu bytes enlarging PatchTex id map.", (unsigned long) sizeof *patchTextureIdMap * patchTextureIdMapSize);
    patchTextureIdMap[patchTextureIdMapSize-1] = p;

    return patchTextureIdMapSize; // 1-based index.
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info)
{
    const patchtex_t* patch;
    if(!info)
        Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");

    memset(info, 0, sizeof(*info));
    patch = getPatchTex(id);
    if(patch)
    {
        texture_t* tex = Textures_ToTexture(patch->texId);
        assert(tex);

        info->id = id;
        info->width = Texture_Width(tex);
        info->height = Texture_Height(tex);
        info->isCustom = Texture_IsCustom(tex);
        info->offset = patch->offX;
        info->topOffset = patch->offY;
        /// \kludge:
        info->extraOffset[0] = info->extraOffset[1] = (patch->flags & PF_UPSCALE_AND_SHARPEN)? -1 : 0;
        return true;
    }
    if(id != 0)
    {
        VERBOSE( Con_Message("Warning:R_GetPatchInfo Invalid Patch id #%u.\n", (uint)id) )
    }
    return false;
}

/// \note Part of the Doomsday public API.
Uri* R_ComposePatchUri(patchid_t id)
{
    const patchtex_t* patch = getPatchTex(id);
    if(patch && patch->texId) return Textures_ComposeUri(Textures_ToTexture(patch->texId));
    if(id != 0)
    {
        VERBOSE( Con_Message("Warning:R_ComposePatchUri: Invalid Patch id #%u.\n", (uint)id) )
    }
    return NULL;
}

patchid_t R_PrecachePatch(const char* name, patchinfo_t* info)
{
    patchid_t patchId;

    if(info)
    {
        memset(info, 0, sizeof(patchinfo_t));
    }

    if(!name || !name[0])
    {
        Con_Message("Warning:R_PrecachePatch: Invalid 'name' argument, ignoring.\n");
        return 0;
    }

    patchId = R_RegisterPatch(name);
    if(patchId)
    {
        GL_PreparePatch(getPatchTex(patchId));
        if(info)
        {
            R_GetPatchInfo(patchId, info);
        }
    }
    return patchId;
}

rawtex_t** R_CollectRawTexs(int* count)
{
    rawtex_t* r, **list;
    int i, num;

    // First count the number of patchtexs.
    for(num = 0, i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(r = rawtexhash[i].first; r; r = r->next)
            num++;
    }

    // Tell this to the caller.
    if(count) *count = num;

    // Allocate the array, plus one for the terminator.
    list = Z_Malloc(sizeof(r) * (num + 1), PU_APPSTATIC, NULL);

    // Collect the pointers.
    for(num = 0, i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(r = rawtexhash[i].first; r; r = r->next)
            list[num++] = r;
    }

    // Terminate.
    list[num] = NULL;

    return list;
}

rawtex_t* R_FindRawTex(lumpnum_t lumpNum)
{
    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_FindRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, F_LumpCount());
#endif
        return NULL;
    }

    { rawtex_t* i;
    for(i = RAWTEX_HASH(lumpNum)->first; i; i = i->next)
    {
        if(i->lumpNum == lumpNum)
            return i;
    }}
    return 0;
}

rawtex_t* R_GetRawTex(lumpnum_t lumpNum)
{
    rawtexhash_t* hash = 0;
    rawtex_t* r;

    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_GetRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, F_LumpCount());
#endif
        return NULL;
    }

    // Check if this lumpNum has already been loaded as a rawtex.
    r = R_FindRawTex(lumpNum);
    if(NULL != r) return r;

    // Hmm, this is an entirely new rawtex.
    r = Z_Calloc(sizeof(*r), PU_REFRESHRAW, 0);
    Str_Init(&r->name);
    Str_Set(&r->name, F_LumpName(lumpNum));
    r->lumpNum = lumpNum;

    // Link to the hash.
    hash = RAWTEX_HASH(lumpNum);
    r->next = hash->first;
    hash->first = r;

    return r;
}

void R_InitRawTexs(void)
{
    memset(rawtexhash, 0, sizeof(rawtexhash));
}

void R_UpdateRawTexs(void)
{
    int i;
    rawtex_t* rawTex;
    for(i = 0; i < RAWTEX_HASH_SIZE; ++i)
    for(rawTex = rawtexhash[i].first; NULL != rawTex; rawTex = rawTex->next)
    {
        Str_Free(&rawTex->name);
    }

    Z_FreeTags(PU_REFRESHRAW, PU_REFRESHRAW);
    R_InitRawTexs();
}

static patchname_t* loadPatchNames(lumpnum_t lumpNum, int* num)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    size_t lumpSize = F_LumpLength(lumpNum);
    const uint8_t* lump = F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);
    patchname_t* names, *name;
    int i, numNames;

    if(lumpSize < 4)
    {
        Con_Message("Warning:loadPatchNames: Lump '%s'(#%i) is not valid PNAMES data.\n", F_LumpName(lumpNum), lumpNum);
        if(num) *num = 0;
        return NULL;
    }

    numNames = LONG(*((const int*) lump));
    if(numNames <= 0)
    {
        if(num) *num = 0;
        return NULL;
    }

    if((unsigned)numNames > (lumpSize - 4) / 8)
    {
        // Lump is truncated.
        Con_Message("Warning:loadPatchNames: Lump '%s'(#%i) truncated (%lu bytes, expected %lu).\n",
            F_LumpName(lumpNum), lumpNum, (unsigned long) lumpSize, (unsigned long) (numNames * 8 + 4));
        numNames = (int) ((lumpSize - 4) / 8);
    }

    names = (patchname_t*) calloc(1, sizeof *names * numNames);
    if(!names)
        Con_Error("loadPatchNames: Failed on allocation of %lu bytes for patch name list.", (unsigned long) sizeof *names * numNames);

    name = names;
    for(i = 0; i < numNames; ++i)
    {
        /// \fixme Some filtering of invalid characters wouldn't go amiss...
        strncpy(*name, (const char*) (lump + 4 + i * 8), 8);
        name++;
    }

    F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);

    if(num) *num = numNames;

    return names;
}

/**
 * Read DOOM and Strife format texture definitions from the specified lump.
 */
static patchcompositetex_t** readDoomTextureDefLump(lumpnum_t lumpNum,
    patchname_t* patchNames, int numPatchNames, int* origIndexBase, boolean firstNull, int* numDefs)
{
#pragma pack(1)
typedef struct {
    int16_t         originX;
    int16_t         originY;
    int16_t         patch;
    int16_t         stepDir;
    int16_t         colorMap;
} mappatch_t;

typedef struct {
    byte            name[8];
    int16_t         unused;
    byte            scale[2]; // [x, y] Used by ZDoom, div 8.
    int16_t         width;
    int16_t         height;
    //void          **columnDirectory;  // OBSOLETE
    int32_t         columnDirectoryPadding;
    int16_t         patchCount;
    mappatch_t      patches[1];
} maptexture_t;

// strifeformat texture definition variants
typedef struct {
    int16_t         originX;
    int16_t         originY;
    int16_t         patch;
} strifemappatch_t;

typedef struct {
    byte            name[8];
    int16_t         unused;
    byte            scale[2]; // [x, y] Used by ZDoom, div 8.
    int16_t         width;
    int16_t         height;
    int16_t         patchCount;
    strifemappatch_t patches[1];
} strifemaptexture_t;
#pragma pack()

typedef struct {
    lumpnum_t lumpNum;
    struct {
        char processed:1; /// @c true if this patch has already been searched for.
    } flags;
} patchinfo_t;

    patchcompositetex_t** texDefs = NULL;
    size_t lumpSize, offset, n, numValidPatchRefs;
    int numTexDefs, numValidTexDefs;
    abstractfile_t* fsObject;
    int* directory, *maptex1;
    short* texDefNumPatches;
    patchinfo_t* patchInfo;
    byte* validTexDefs;
    int i, lumpIdx;
    assert(patchNames && origIndexBase);

    patchInfo = (patchinfo_t*)calloc(1, sizeof *patchInfo * numPatchNames);
    if(!patchInfo)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for patch info.", (unsigned long) sizeof *patchInfo * numPatchNames);

    lumpSize = F_LumpLength(lumpNum);
    maptex1 = (int*) malloc(lumpSize);
    if(!maptex1)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for temporary copy of archived DOOM texture definitions.", (unsigned long) lumpSize);

    fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)maptex1, 0, lumpSize);

    numTexDefs = LONG(*maptex1);

    VERBOSE( Con_Message("  Processing lump \"%s\"...\n", F_LumpName(lumpNum)) )

    validTexDefs = (byte*) calloc(1, sizeof *validTexDefs * numTexDefs);
    if(!validTexDefs)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for valid texture record list.", (unsigned long) sizeof *validTexDefs * numTexDefs);

    texDefNumPatches = (short*) calloc(1, sizeof *texDefNumPatches * numTexDefs);
    if(!texDefNumPatches)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for texture patch count record list.", (unsigned long) sizeof *texDefNumPatches * numTexDefs);

    /**
     * Pass #1
     * Count total number of texture and patch defs we'll need and check
     * for missing patches and any other irregularities.
     */
    numValidTexDefs = 0;
    numValidPatchRefs = 0;
    directory = maptex1 + 1;
    for(i = 0; i < numTexDefs; ++i, directory++)
    {
        offset = LONG(*directory);
        if(offset > lumpSize)
        {
            Con_Message("Warning, bad offset %lu for definition %i in lump \"%s\".\n", (unsigned long) offset, i, F_LumpName(lumpNum));
            continue;
        }

        if(gameDataFormat == 0)
        {   // DOOM format.
            maptexture_t* mtexture = (maptexture_t*) ((byte*) maptex1 + offset);
            short j, n, patchCount = SHORT(mtexture->patchCount);

            n = 0;
            if(patchCount > 0)
            {
                mappatch_t* mpatch = &mtexture->patches[0];

                for(j = 0; j < patchCount; ++j, mpatch++)
                {
                    short patchNum = SHORT(mpatch->patch);
                    patchinfo_t* pinfo;

                    if(patchNum < 0 || patchNum >= numPatchNames)
                    {
                        Con_Message("Warning, invalid Patch %i in texture definition '%s'.\n", (int) patchNum, mtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = F_CheckLumpNumForName2(*(patchNames + patchNum), true);
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning, failed to locate Patch '%s'.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning, missing Patch %i in texture definition '%s'.\n", (int) j, mtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning, invalid patchcount %i in texture definition '%s'.\n", (int) patchCount, mtexture->name);
            }

            texDefNumPatches[i] = n;
            numValidPatchRefs += n;
        }
        else if(gameDataFormat == 3)
        {   // Strife format.
            strifemaptexture_t* smtexture = (strifemaptexture_t *) ((byte *) maptex1 + offset);
            short j, n, patchCount = SHORT(smtexture->patchCount);

            n = 0;
            if(patchCount > 0)
            {
                strifemappatch_t* smpatch = &smtexture->patches[0];
                for(j = 0; j < patchCount; ++j, smpatch++)
                {
                    short patchNum = SHORT(smpatch->patch);
                    patchinfo_t* pinfo;

                    if(patchNum < 0 || patchNum >= numPatchNames)
                    {
                        Con_Message("Warning, invalid Patch %i in texture definition '%s'.\n", (int) patchNum, smtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = F_CheckLumpNumForName2(*(patchNames + patchNum), true);
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning, failed to locate Patch '%s'.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning, missing patch %i in texture definition '%s'.\n", (int) j, smtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning, invalid patchcount %i in texture definition '%s'.\n", (int) patchCount, smtexture->name);
            }

            texDefNumPatches[i] = n;
            numValidPatchRefs += n;
        }

        if(texDefNumPatches[i] > 0)
        {
            // This is a valid texture definition.
            validTexDefs[i] = true;
            numValidTexDefs++;
        }
    }

    if(numValidTexDefs > 0 && numValidPatchRefs > 0)
    {
        /**
         * Pass #2
         * There is at least one valid texture def in this lump so convert
         * to the internal format.
         */

        // Build the texturedef index.
        texDefs = (patchcompositetex_t**)malloc(sizeof *texDefs * numValidTexDefs);
        directory = maptex1 + 1;
        n = 0;
        for(i = 0; i < numTexDefs; ++i, directory++)
        {
            patchcompositetex_t* texDef;
            short j;

            if(!validTexDefs[i]) continue;

            offset = LONG(*directory);

            // Read and create the texture def.
            if(gameDataFormat == 0)
            {   // DOOM format.
                texpatch_t* patch;
                mappatch_t* mpatch;
                maptexture_t* mtexture = (maptexture_t*) ((byte*) maptex1 + offset);

                texDef = (patchcompositetex_t*) malloc(sizeof *texDef);
                if(!texDef)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

                texDef->patchCount = texDefNumPatches[i];
                texDef->flags = 0;
                texDef->origIndex = (*origIndexBase) + i;
                Str_Init(&texDef->name);
                Str_PartAppend(&texDef->name, (const char*) mtexture->name, 0, 8);
                texDef->width = SHORT(mtexture->width);
                texDef->height = SHORT(mtexture->height);

                texDef->patches = (texpatch_t*)malloc(sizeof *texDef->patches * texDef->patchCount);
                if(!texDef->patches)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new TexPatch list for texture definition '%s'.", (unsigned long) sizeof *texDef->patches * texDef->patchCount, Str_Text(&texDef->name));

                mpatch = &mtexture->patches[0];
                patch = texDef->patches;
                for(j = 0; j < SHORT(mtexture->patchCount); ++j, mpatch++)
                {
                    short patchNum = SHORT(mpatch->patch);

                    if(patchNum < 0 || patchNum >= numPatchNames ||
                       patchInfo[patchNum].lumpNum == -1)
                        continue;

                    patch->offX = SHORT(mpatch->originX);
                    patch->offY = SHORT(mpatch->originY);
                    patch->lumpNum = patchInfo[patchNum].lumpNum;
                    patch++;
                }
            }
            else if(gameDataFormat == 3)
            {   // Strife format.
                texpatch_t* patch;
                strifemappatch_t* smpatch;
                strifemaptexture_t* smtexture = (strifemaptexture_t*) ((byte*) maptex1 + offset);

                texDef = (patchcompositetex_t*) malloc(sizeof *texDef);
                if(!texDef)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

                texDef->patchCount = texDefNumPatches[i];
                texDef->flags = 0;
                texDef->origIndex = (*origIndexBase) + i;
                Str_Init(&texDef->name);
                Str_PartAppend(&texDef->name, (const char*) smtexture->name, 0, 8);
                texDef->width = SHORT(smtexture->width);
                texDef->height = SHORT(smtexture->height);

                texDef->patches = (texpatch_t*)malloc(sizeof *texDef->patches * texDef->patchCount);
                if(!texDef->patches)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new TexPatch list for texture definition '%s'.", (unsigned long) sizeof *texDef->patches * texDef->patchCount, Str_Text(&texDef->name));

                smpatch = &smtexture->patches[0];
                patch = texDef->patches;
                for(j = 0; j < SHORT(smtexture->patchCount); ++j, smpatch++)
                {
                    short patchNum = SHORT(smpatch->patch);

                    if(patchNum < 0 || patchNum >= numPatchNames ||
                       patchInfo[patchNum].lumpNum == -1)
                        continue;

                    patch->offX = SHORT(smpatch->originX);
                    patch->offY = SHORT(smpatch->originY);
                    patch->lumpNum = patchInfo[patchNum].lumpNum;
                    patch++;
                }
            }
            else
            {
                Con_Error("R_ReadTextureDefs: Invalid gameDataFormat=%i.", gameDataFormat);
                exit(1); // Unreachable.
            }

            /**
             * Vanilla DOOM's implementation of the texture collection has a flaw which
             * results in the first texture being used dually as a "NULL" texture.
             */
            if(firstNull && i == 0)
                texDef->flags |= TXDF_NODRAW;

            /// Is this a custom texture?
            j = 0;
            while(j < texDef->patchCount && !(texDef->flags & TXDF_CUSTOM))
            {
                if(F_LumpIsCustom(texDef->patches[j].lumpNum))
                    texDef->flags |= TXDF_CUSTOM;
                else
                    j++;
            }

            // Add it to the list.
            texDefs[n++] = texDef;
        }
    }

    *origIndexBase += numTexDefs;

    VERBOSE2( Con_Message("  Loaded %i of %i definitions.\n", numValidTexDefs, numTexDefs) )

    // Free all temporary storage.
    free(validTexDefs);
    free(texDefNumPatches);
    free(maptex1);
    free(patchInfo);

    if(numDefs) *numDefs = numValidTexDefs;

    return texDefs;
}

static patchcompositetex_t** loadPatchCompositeDefs(int* numDefs)
{
    int patchCompositeTexturesCount = 0;
    patchcompositetex_t** patchCompositeTextures = NULL;

    patchcompositetex_t** list = 0, **listCustom = 0, ***eList = 0;
    lumpnum_t pnames, firstTexLump, secondTexLump;
    int count = 0, countCustom = 0, *eCount = 0;
    int i, numLumps, numPatchNames, origIndexBase;
    patchcompositetex_t** newTexDefs;
    boolean firstNull, isCustom;
    patchname_t* patchNames;
    int newNumTexDefs;
    int defLumpsSize;
    lumpnum_t* defLumps;

    pnames = F_CheckLumpNumForName2("PNAMES", true);
    if(-1 == pnames)
    {
        if(numDefs) *numDefs = 0;
        return NULL;
    }

    // Load the patch names from the PNAMES lump.
    patchNames = loadPatchNames(pnames, &numPatchNames);
    if(!patchNames)
    {
        Con_Message("Warning:loadPatchCompositeDefs: Unexpected error occured loading PNAMES.\n");
        if(numDefs) *numDefs = 0;
        return NULL;
    }

    /**
     * Precedence order of definitions is defined by id tech1 which processes
     * the TEXTURE1/2 lumps in the following order:
     *
     * (last)TEXTURE2 > (last)TEXTURE1
     */
    defLumpsSize = 0;
    defLumps = NULL;
    firstTexLump = W_CheckLumpNumForName2("TEXTURE1", true/*quiet please*/);
    secondTexLump = W_CheckLumpNumForName2("TEXTURE2", true/*quiet please*/);

    /**
     * In Doomsday we also process all other lumps named TEXTURE1/2 which
     * will not processed according to the predefined order.
     */
    numLumps = F_LumpCount();
    for(i = 0; i < numLumps; ++i)
    {
        const lumpinfo_t* info;

        // Will this be processed anyway?
        if(i == firstTexLump || i == secondTexLump) continue;

        info = F_FindInfoForLumpNum(i);
        assert(info);
        if(strnicmp(info->name, "TEXTURE1", 8) && strnicmp(info->name, "TEXTURE2", 8)) continue;

        defLumps = (lumpnum_t*)realloc(defLumps, sizeof *defLumps * ++defLumpsSize);
        if(!defLumps)
            Con_Error("loadPatchCompositeDefs: Failed on (re)allocation of %lu bytes enlarging definition lump list.", (unsigned long) sizeof *defLumps * defLumpsSize);
        defLumps[defLumpsSize-1] = i;
    }

    if(firstTexLump >= 0)
    {
        defLumps = (lumpnum_t*)realloc(defLumps, sizeof *defLumps * ++defLumpsSize);
        if(!defLumps)
            Con_Error("loadPatchCompositeDefs: Failed on (re)allocation of %lu bytes enlarging definition lump list.", (unsigned long) sizeof *defLumps * defLumpsSize);
        defLumps[defLumpsSize-1] = firstTexLump;
    }

    if(secondTexLump >= 0)
    {
        defLumps = (lumpnum_t*)realloc(defLumps, sizeof *defLumps * ++defLumpsSize);
        if(!defLumps)
            Con_Error("loadPatchCompositeDefs: Failed on (re)allocation of %lu bytes enlarging definition lump list.", (unsigned long) sizeof *defLumps * defLumpsSize);
        defLumps[defLumpsSize-1] = secondTexLump;
    }

    /**
     * Many PWADs include new TEXTURE1/2 lumps including the IWAD doomtexture
     * definitions, with new definitions appended. In order to correctly
     * determine whether a defined texture originates from an IWAD we must
     * compare all definitions against those in the IWAD and if matching,
     * they should be considered as IWAD resources, even though the doomtexture
     * definition does not come from an IWAD lump.
     */
    origIndexBase = 0;
    firstNull = true;
    for(i = 0; i < defLumpsSize; ++i)
    {
        lumpnum_t lumpNum = defLumps[i];

        // Read in the new texture defs.
        newTexDefs = readDoomTextureDefLump(lumpNum, patchNames, numPatchNames, &origIndexBase, firstNull, &newNumTexDefs);

        isCustom = F_LumpIsCustom(lumpNum);
        eList  = (!isCustom? &list  : &listCustom);
        eCount = (!isCustom? &count : &countCustom);
        if(*eList)
        {
            patchcompositetex_t** newList;
            size_t n;
            int i;

            // Merge with the existing doomtexturedefs.
            newList = (patchcompositetex_t**)malloc(sizeof *newList * ((*eCount) + newNumTexDefs));
            if(!newList)
                Con_Error("loadPatchCompositeDefs: Failed on allocation of %lu bytes for merged PatchComposite list.", (unsigned long) sizeof *newList * ((*eCount) + newNumTexDefs));

            n = 0;
            for(i = 0; i < *eCount; ++i)
                newList[n++] = (*eList)[i];
            for(i = 0; i < newNumTexDefs; ++i)
                newList[n++] = newTexDefs[i];

            free(*eList);
            free(newTexDefs);

            *eList = newList;
            *eCount += newNumTexDefs;
        }
        else
        {
            *eList = newTexDefs;
            *eCount = newNumTexDefs;
        }

        // No more "not-drawn" textures.
        firstNull = false;
    }

    if(listCustom)
    {
        // There are custom doomtexturedefs, cross compare with the IWAD
        // originals to see if they have been changed.
        patchcompositetex_t** newList;
        size_t n;

        i = 0;
        while(i < count)
        {
            patchcompositetex_t* orig = list[i];
            boolean hasReplacement = false;
            int j;

            for(j = 0; j < countCustom; ++j)
            {
                patchcompositetex_t* custom = listCustom[j];

                if(!Str_CompareIgnoreCase(&orig->name, Str_Text(&custom->name)))
                {
                    // This is a newer version of an IWAD doomtexturedef.
                    if(custom->flags & TXDF_CUSTOM)
                    {
                        hasReplacement = true; // Uses a non-IWAD patch.
                    }
                    else if(custom->height     == orig->height &&
                            custom->width      == orig->width  &&
                            custom->patchCount == orig->patchCount)
                    {
                        // Check the patches.
                        short k = 0;

                        while(k < orig->patchCount && !(custom->flags & TXDF_CUSTOM))
                        {
                            texpatch_t* origP   = orig->patches  + k;
                            texpatch_t* customP = custom->patches + k;

                            if(origP->lumpNum != customP->lumpNum &&
                               origP->offX != customP->offX &&
                               origP->offY != customP->offY)
                            {
                                custom->flags |= TXDF_CUSTOM;
                            }
                            else
                            {
                                k++;
                            }
                        }

                        hasReplacement = true;
                    }

                    // The non-drawable flag must pass to the replacement.
                    if(hasReplacement && (orig->flags & TXDF_NODRAW))
                        custom->flags |= TXDF_NODRAW;
                    break;
                }
            }

            if(hasReplacement)
            {
                // Let the PWAD "copy" override the IWAD original.
                if(orig->patches) free(orig->patches);
                free(orig);
                if(i < count - 1)
                    memmove(list + i, list + i + 1, sizeof *list * (count - i - 1));
                count--;
            }
            else
            {
                i++;
            }
        }

        // List contains only non-replaced doomtexturedefs, merge them.
        newList = (patchcompositetex_t**)malloc(sizeof *newList * (count + countCustom));
        if(!newList)
            Con_Error("loadPatchCompositeDefs: Failed on allocation of %lu bytes for the unique PatchComposite list.", (unsigned long) sizeof *newList * (count + countCustom));

        n = 0;
        for(i = 0; i < count; ++i)
            newList[n++] = list[i];
        for(i = 0; i < countCustom; ++i)
            newList[n++] = listCustom[i];

        free(list);
        free(listCustom);

        patchCompositeTextures = newList;
        patchCompositeTexturesCount = count + countCustom;
    }
    else
    {
        patchCompositeTextures = list;
        patchCompositeTexturesCount = count;
    }

    if(defLumps) free(defLumps);

    // We are finished with the patch names now.
    free(patchNames);

    if(numDefs) *numDefs = patchCompositeTexturesCount;
    return patchCompositeTextures;
}

/// \note Definitions for Textures that are not created successfully will be pruned from the set.
static void createTexturesForPatchCompositeDefs(patchcompositetex_t** defs, int count)
{
    Uri* uri = Uri_New();
    int i;
    assert(defs);

    Uri_SetScheme(uri, TN_TEXTURES_NAME);
    for(i = 0; i < count; ++i)
    {
        patchcompositetex_t* pcTex = defs[i];
        int flags = 0;
        texture_t* tex;

        if(pcTex->flags & TXDF_CUSTOM) flags |= TXF_CUSTOM;

        Uri_SetPath(uri, Str_Text(&pcTex->name));
        tex = Textures_CreateWithDimensions(uri, flags, pcTex->width, pcTex->height, (void*)pcTex);
        if(!tex)
        {
            Con_Message("Warning, failed creating Texture for new patch composite %s.\n", Str_Text(&pcTex->name));
            if(pcTex->patches) free(pcTex->patches);
            free(pcTex);
            defs[i] = NULL;
            continue;
        }
    }
    Uri_Delete(uri);
}

typedef struct {
    int minIdx, maxIdx;
} findpatchcompositeoriginalindexbounds_params_t;

static int findPatchCompositeOriginalIndexBounds(texture_t* tex, void* paramaters)
{
    patchcompositetex_t* pcTex= (patchcompositetex_t*)Texture_UserData(tex);
    findpatchcompositeoriginalindexbounds_params_t* p = (findpatchcompositeoriginalindexbounds_params_t*)paramaters;
    if(pcTex)
    {
        if(pcTex->origIndex < p->minIdx) p->minIdx = pcTex->origIndex;
        if(pcTex->origIndex > p->maxIdx) p->maxIdx = pcTex->origIndex;
    }
    return 0; // Continue iteration.
}

/// \assume patchCompositeOrigIndexMap has been initialized and is large enough!
static int insertPatchCompositeInOriginalIndexMap(texture_t* tex, void* paramaters)
{
    patchcompositetex_t* pcTex = (patchcompositetex_t*)Texture_UserData(tex);
    if(pcTex)
    {
        patchCompositeOrigIndexMap[pcTex->origIndex - patchCompositeOrigIndexBase] = tex;
    }
    return 0; // Continue iteration.
}

void R_RebuildPatchCompositeOriginalIndexMap(void)
{
    findpatchcompositeoriginalindexbounds_params_t p;

    // Determine the size of the LUT.
    p.minIdx = DDMAXINT;
    p.maxIdx = DDMININT;
    Textures_Iterate2(TN_TEXTURES, findPatchCompositeOriginalIndexBounds, (void*)&p);

    if(p.minIdx > p.maxIdx)
    {
        // None found.
        patchCompositeOrigIndexBase = 0;
        patchCompositeOrigIndexMapSize = 0;
    }
    else
    {
        patchCompositeOrigIndexBase = p.minIdx;
        patchCompositeOrigIndexMapSize = p.maxIdx - p.minIdx + 1;
    }

    // Construct and (re)populate the LUT.
    patchCompositeOrigIndexMap = (texture_t**)realloc(patchCompositeOrigIndexMap, sizeof *patchCompositeOrigIndexMap * patchCompositeOrigIndexMapSize);
    if(!patchCompositeOrigIndexMap && patchCompositeOrigIndexMapSize)
        Con_Error("R_RebuildPatchCompositeOriginalIndexMap: Failed on (re)allocation of %lu bytes resizing the map.", (unsigned long) sizeof *patchCompositeOrigIndexMap * patchCompositeOrigIndexMapSize);
    if(!patchCompositeOrigIndexMapSize) return;

    memset(patchCompositeOrigIndexMap, 0, sizeof *patchCompositeOrigIndexMap * patchCompositeOrigIndexMapSize);
    Textures_Iterate(TN_TEXTURES, insertPatchCompositeInOriginalIndexMap);
}

void R_DestroyPatchCompositeOriginalIndexMap(void)
{
    if(!patchCompositeOrigIndexMap) return;
    free(patchCompositeOrigIndexMap), patchCompositeOrigIndexMap = NULL;
    patchCompositeOrigIndexMapSize = 0;
    patchCompositeOrigIndexBase = 0;
}

void R_InitPatchComposites(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    patchcompositetex_t** defs;
    int defsCount;

    //Textures_ClearByNamespace(TN_TEXTURES);
    //assert(Textures_Count(TN_TEXTURES) == 0);

    VERBOSE( Con_Message("Initializing PatchComposite textures...\n") )

    // Load texture definitions from TEXTURE1/2 lumps.
    defs = loadPatchCompositeDefs(&defsCount);
    if(defs)
    {
        createTexturesForPatchCompositeDefs(defs, defsCount);
        free(defs);
    }

    R_RebuildPatchCompositeOriginalIndexMap();

    VERBOSE2( Con_Message("R_InitPatchComposites: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

typedef struct {
    int minIdx, maxIdx;
} findflatoriginalindexbounds_params_t;

static int findFlatOriginalIndexBounds(texture_t* tex, void* paramaters)
{
    flat_t* flat = (flat_t*)Texture_UserData(tex);
    findflatoriginalindexbounds_params_t* p = (findflatoriginalindexbounds_params_t*)paramaters;
    if(flat)
    {
        if(flat->origIndex < p->minIdx) p->minIdx = flat->origIndex;
        if(flat->origIndex > p->maxIdx) p->maxIdx = flat->origIndex;
    }
    return 0; // Continue iteration.
}

/// \assume flatOrigIndexMap has been initialized and is large enough!
static int insertFlatInOriginalIndexMap(texture_t* tex, void* paramaters)
{
    flat_t* flat = (flat_t*)Texture_UserData(tex);
    if(flat)
    {
        flatOrigIndexMap[flat->origIndex - flatOrigIndexBase] = tex;
    }
    return 0; // Continue iteration.
}

void R_RebuildFlatOriginalIndexMap(void)
{
    findflatoriginalindexbounds_params_t p;

    // Determine the size of the LUT.
    p.minIdx = DDMAXINT;
    p.maxIdx = DDMININT;
    Textures_Iterate2(TN_FLATS, findFlatOriginalIndexBounds, (void*)&p);

    if(p.minIdx > p.maxIdx)
    {
        // None found.
        flatOrigIndexBase = 0;
        flatOrigIndexMapSize = 0;
    }
    else
    {
        flatOrigIndexBase = p.minIdx;
        flatOrigIndexMapSize = p.maxIdx - p.minIdx + 1;
    }

    // Construct and (re)populate the LUT.
    flatOrigIndexMap = (texture_t**)realloc(flatOrigIndexMap, sizeof *flatOrigIndexMap * flatOrigIndexMapSize);
    if(!flatOrigIndexMap && flatOrigIndexMapSize)
        Con_Error("R_RebuildFlatOriginalIndexMap: Failed on (re)allocation of %lu bytes resizing the map.", (unsigned long) sizeof *flatOrigIndexMap * flatOrigIndexMapSize);
    if(!flatOrigIndexMapSize) return;

    memset(flatOrigIndexMap, 0, sizeof *flatOrigIndexMap * flatOrigIndexMapSize);
    Textures_Iterate(TN_FLATS, insertFlatInOriginalIndexMap);
}

void R_DestroyFlatOriginalIndexMap(void)
{
    if(!flatOrigIndexMap) return;
    free(flatOrigIndexMap), flatOrigIndexMap = NULL;
    flatOrigIndexMapSize = 0;
    flatOrigIndexBase = 0;
}

static boolean createFlatForLump(lumpnum_t lumpNum, flat_t** rFlat)
{
    const lumpinfo_t* info = F_FindInfoForLumpNum(lumpNum);
    boolean isNewFlat = false;
    texture_t* tex;
    flat_t* flat;
    int flags;
    Uri* uri;

    if(rFlat) *rFlat = NULL;

    // We can only perform some basic filtering of lumps at this time.
    if(!info || info->size == 0) return false;

    uri = Uri_New();
    Uri_SetScheme(uri, TN_FLATS_NAME);

    // Have we already encountered this name?
    Uri_SetPath(uri, info->name);
    tex = Textures_TextureForUri2(uri, true/*quiet please*/);
    if(!tex)
    {
        // A new flat.
        flat = (flat_t*)malloc(sizeof *flat);
        if(!flat)
            Con_Error("R_InitFlatTextures: Failed on allocation of %lu bytes for new Flat.", (unsigned long) sizeof *flat);
        isNewFlat = true;

        flags = 0;
        if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;

        tex = Textures_Create(uri, flags, (void*)flat);
        if(!tex)
        {
            ddstring_t* path = Uri_ToString(uri);
            Con_Message("Warning, failed creating Texture for new flat %s.\n", Str_Text(path));
            Str_Delete(path);
            free(flat);
        }
    }
    else
    {
        flat = (flat_t*)Texture_UserData(tex);
        flags = 0;
        if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;
        Texture_SetFlags(tex, flags);

        /// \fixme Update any Materials (and thus Surfaces) which reference this.
    }
    Uri_Delete(uri);

    // (Re)configure this flat.
    flat->lumpNum = lumpNum;
    if(rFlat) *rFlat = flat;

    return isNewFlat;
}

void R_InitFlatTextures(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    lumpnum_t origRangeFrom, origRangeTo;

    VERBOSE( Con_Message("Initializing Flat textures...\n") )

    origRangeFrom = W_CheckLumpNumForName("F_START");
    origRangeTo   = W_CheckLumpNumForName("F_END");

    if(origRangeFrom >= 0 && origRangeTo >= 0)
    {
        int i, numLumps, origIndexBase = 0;
        ddstack_t* stack;
        flat_t* flat;
        lumpnum_t n;

        // First add all flats between all flat marker lumps exclusive of the
        // range defined by the last marker lumps.
        origIndexBase = (int)origRangeTo;
        stack = Stack_New();
        numLumps = F_LumpCount();
        for(i = 0; i < numLumps; ++i)
        {
            const lumpinfo_t* info = F_FindInfoForLumpNum(i);
            assert(info);

            if(info->name[0] == 'F')
            {
                if(!strnicmp(info->name + 1, "_START", 6) ||
                   !strnicmp(info->name + 2, "_START", 6))
                {
                    // We've arrived at *a* sprite block.
                    Stack_Push(stack, NULL);
                    continue;
                }
                else if(!strnicmp(info->name + 1, "_END", 4) ||
                        !strnicmp(info->name + 2, "_END", 4))
                {
                    // The sprite block ends.
                    Stack_Pop(stack);
                    continue;
                }
            }

            if(!Stack_Height(stack)) continue;
            if(i >= origRangeFrom && i <= origRangeTo) continue;

            createFlatForLump(i, &flat);
            if(!flat) continue;
            flat->origIndex = origIndexBase++;
        }

        while(Stack_Height(stack)) { Stack_Pop(stack); }
        Stack_Delete(stack);

        // Now add all lumps in the primary (overridding) range.
        origRangeFrom += 1; // Skip over marker lump.
        for(n = origRangeFrom; n < origRangeTo; ++n)
        {
            createFlatForLump(n, &flat);
            if(!flat) continue;
            flat->origIndex = (int) (n - origRangeFrom);
        }
    }

    R_RebuildFlatOriginalIndexMap();

    VERBOSE2( Con_Message("R_InitFlatTextures: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

static boolean validSpriteName(const char* name)
{
    if(!name || !name[0]) return false;
    if(!name[4] || !name[5] || (name[6] && !name[7])) return false;
    // Indices 5 and 7 must be numbers (0-8).
    if(name[5] < '0' || name[5] > '8') return false;
    if(name[7] && (name[7] < '0' || name[7] > '8')) return false;
    // Its good!
    return true;
}

void R_InitSpriteTextures(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    int i, numLumps, origIndex = 0;
    ddstack_t* stack;
    Uri* uri;

    VERBOSE( Con_Message("Initializing Sprite textures...\n") )

    uri = Uri_New();
    Uri_SetScheme(uri, TN_SPRITES_NAME);
    stack = Stack_New();
    numLumps = F_LumpCount();
    /// \fixme Load order here does not respect id tech1 logic.
    for(i = 0; i < numLumps; ++i)
    {
        const lumpinfo_t* info = F_FindInfoForLumpNum((lumpnum_t)i);
        spritetex_t* sprTex;
        texture_t* tex;
        int flags;

        if(info->name[0] == 'S')
        {
            if(!strnicmp(info->name + 1, "_START", 6) ||
               !strnicmp(info->name + 2, "_START", 6))
            {
                // We've arrived at *a* sprite block.
                Stack_Push(stack, NULL);
                continue;
            }
            else if(!strnicmp(info->name + 1, "_END", 4) ||
                    !strnicmp(info->name + 2, "_END", 4))
            {
                // The sprite block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack)) continue;
        if(!validSpriteName(info->name)) continue;

        origIndex++;

        // Have we already encountered this name?
        Uri_SetPath(uri, info->name);
        tex = Textures_TextureForUri2(uri, true/*quiet please*/);
        if(!tex)
        {
            // A new sprite texture.
            sprTex = (spritetex_t*)malloc(sizeof *sprTex);
            if(!sprTex)
                Con_Error("R_InitSpriteTextures: Failed on allocation of %lu bytes for new SpriteTex.", (unsigned long) sizeof *sprTex);
            sprTex->offX = sprTex->offY = 0; // Deferred until texture load time.

            flags = 0;
            if(F_LumpIsCustom((lumpnum_t)i)) flags |= TXF_CUSTOM;

            tex = Textures_Create(uri, flags, (void*)sprTex);
            if(!tex)
            {
                ddstring_t* path = Uri_ComposePath(uri);
                Con_Message("Warning, failed creating Texture for sprite frame lump %s (#%i).\n", Str_Text(path), (lumpnum_t)i);
                Str_Delete(path);
                free(sprTex);
            }
        }
        else
        {
            sprTex = (spritetex_t*)Texture_UserData(tex);
            flags = 0;
            if(F_LumpIsCustom((lumpnum_t)i)) flags |= TXF_CUSTOM;
            Texture_SetFlags(tex, flags);

            // Sprite offsets may have changed so release any currently loaded
            // GL-textures for this, so the offsets will be updated.
            GL_ReleaseGLTexturesForTexture(tex);

            /// \fixme Update any Materials (and thus Surfaces) which reference this.
        }

        // (Re)configure this sprite.
        sprTex->lumpNum = (lumpnum_t)i;
    }

    while(Stack_Height(stack)) { Stack_Pop(stack); }
    Stack_Delete(stack);

    Uri_Delete(uri);

    VERBOSE2( Con_Message("R_InitSpriteTextures: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) )
}

texture_t* R_CreateSkinTex(const Uri* filePath, boolean isShinySkin)
{
    skinname_t* skin;
    texture_t* tex;
    char name[9];
    Uri* uri;

    if(!filePath || Str_IsEmpty(Uri_Path(filePath))) return 0;

    // Have we already created one for this?
    tex = R_FindModelSkinForFilePath(filePath);
    if(tex) return tex;

    if(M_NumDigits(Textures_Count(isShinySkin? TN_MODELREFLECTIONSKINS : TN_MODELSKINS)+1) > 8)
    {
#if _DEBUG
        Con_Message("Warning, failed creating SkinName (max:%i).\n", DDMAXINT);
#endif
        return NULL;
    }

    /**
     * A new skin name.
     */

    skin = (skinname_t*)malloc(sizeof *skin);
    if(!skin)
        Con_Error("R_CreateSkinTex: Failed on allocation of %lu bytes for new SkinName.", (unsigned long) sizeof *skin);

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, Textures_Count(isShinySkin? TN_MODELREFLECTIONSKINS : TN_MODELSKINS)+1);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, (isShinySkin? TN_MODELREFLECTIONSKINS_NAME : TN_MODELSKINS_NAME));
    tex = Textures_Create(uri, TXF_CUSTOM, (void*)skin);
    if(!tex)
    {
        Con_Message("Warning, failed creating new Texture for SkinName %s.\n", name);
        Uri_Delete(uri);
        free(skin);
        return NULL;
    }

    skin->path = Uri_NewCopy(filePath);

    Uri_Delete(uri);
    return tex;
}

static boolean expandSkinName(ddstring_t* foundPath, const char* skin, const char* modelfn)
{
    boolean found = false;
    ddstring_t searchPath;
    assert(foundPath && skin && skin[0]);

    Str_Init(&searchPath);

    // Try the "first choice" directory first.
    if(modelfn)
    {
        // The "first choice" directory is that in which the model file resides.
        directory_t* mydir = Dir_ConstructFromPathDir(modelfn);
        Str_Appendf(&searchPath, "%s%s", mydir->path, skin);
        found = 0 != F_FindResourceStr2(RC_GRAPHIC, &searchPath, foundPath);
        Dir_Delete(mydir);
    }

    if(!found)
    {   // Try the resource locator.
        Str_Clear(&searchPath); Str_Appendf(&searchPath, MODELS_RESOURCE_NAMESPACE_NAME":%s", skin);
        found = F_FindResourceStr2(RC_GRAPHIC, &searchPath, foundPath) != 0;
    }

    Str_Free(&searchPath);
    return found;
}

texture_t* R_RegisterModelSkin(ddstring_t* foundPath, const char* skin, const char* modelfn, boolean isShinySkin)
{
    texture_t* tex = NULL;
    if(skin && skin[0])
    {
        ddstring_t buf;
        if(!foundPath)
            Str_Init(&buf);

        if(expandSkinName(foundPath ? foundPath : &buf, skin, modelfn))
        {
            Uri* uri = Uri_NewWithPath2(foundPath ? Str_Text(foundPath) : Str_Text(&buf), RC_NULL);
            tex = R_CreateSkinTex(uri, isShinySkin);
            Uri_Delete(uri);
        }

        if(!foundPath)
            Str_Free(&buf);
    }
    return tex;
}

static int findModelSkinForFilePathWorker(texture_t* tex, void* paramaters)
{
    const Uri* filePath = (Uri*)paramaters;
    skinname_t* skin = (skinname_t*)Texture_UserData(tex);
    assert(tex && filePath);

    if(skin && skin->path && Uri_Equality(skin->path, filePath))
    {
        return (int)Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

texture_t* R_FindModelSkinForFilePath(const Uri* filePath)
{
    int result;
    if(!filePath || Str_IsEmpty(Uri_Path(filePath))) return NULL;

    result = Textures_Iterate2(TN_MODELSKINS, findModelSkinForFilePathWorker, (void*)filePath);
    if(!result)
        result = Textures_Iterate2(TN_MODELREFLECTIONSKINS, findModelSkinForFilePathWorker, (void*)filePath);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

void R_UpdateData(void)
{
    R_UpdateRawTexs();
    Cl_InitTranslations();
}

void R_InitTranslationTables(void)
{
    translationTables = Z_Calloc(256 * 3 * 7, PU_REFRESHTRANS, 0);
}

void R_UpdateTranslationTables(void)
{
    Z_FreeTags(PU_REFRESHTRANS, PU_REFRESHTRANS);
    R_InitTranslationTables();
}

/**
 * @return              @c true, if the given light decoration definition
 *                      is valid.
 */
boolean R_IsValidLightDecoration(const ded_decorlight_t *lightDef)
{
    return (lightDef &&
            (lightDef->color[0] != 0 || lightDef->color[1] != 0 ||
             lightDef->color[2] != 0));
}

boolean R_IsAllowedDecoration(ded_decor_t* def, boolean hasExternal, boolean isCustom)
{
    if(hasExternal) return (def->flags & DCRF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & DCRF_NO_IWAD ) == 0;
    return (def->flags & DCRF_PWAD) != 0;
}

boolean R_IsAllowedReflection(ded_reflection_t* def, boolean hasExternal, boolean isCustom)
{
    if(hasExternal) return (def->flags & REFF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & REFF_NO_IWAD ) == 0;
    return (def->flags & REFF_PWAD) != 0;
}

boolean R_IsAllowedDetailTex(ded_detailtexture_t* def, boolean hasExternal, boolean isCustom)
{
    if(hasExternal) return (def->flags & DTLF_EXTERNAL) != 0;
    if(!isCustom)   return (def->flags & DTLF_NO_IWAD ) == 0;
    return (def->flags & DTLF_PWAD) != 0;
}

static boolean isInList(void** list, size_t len, void* elm)
{
    size_t              n;

    if(!list || !elm || len == 0)
        return false;

    for(n = 0; n < len; ++n)
        if(list[n] == elm)
            return true;

    return false;
}

boolean findSpriteOwner(thinker_t* th, void* context)
{
    int                 i;
    mobj_t*             mo = (mobj_t*) th;
    spritedef_t*        sprDef = (spritedef_t*) context;

    if(mo->type >= 0 && mo->type < defs.count.mobjs.num)
    {
        //// \optimize Traverses the entire state list!
        for(i = 0; i < defs.count.states.num; ++i)
        {
            if(stateOwners[i] != &mobjInfo[mo->type])
                continue;

            if(&sprites[states[i].sprite] == sprDef)
                return false; // Found an owner!
        }
    }

    return true; // Keep looking...
}

/// \note Part of the Doomsday public API.
void R_PrecacheMobjNum(int num)
{
    const materialvariantspecification_t* spec;

    if(novideo || !((useModels && precacheSkins) || precacheSprites))
        return;

    if(num < 0 || num >= defs.count.mobjs.num)
        return;

    spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 1, 0, 0,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
    assert(spec);

    /// \optimize Traverses the entire state list!
    { int i;
    for(i = 0; i < defs.count.states.num; ++i)
    {
        state_t* state;

        if(stateOwners[i] != &mobjInfo[num])
            continue;
        state = &states[i];

        R_PrecacheSkinsForState(i);

        if(precacheSprites)
        {
            spritedef_t* sprDef = &sprites[state->sprite];
            int j;
            for(j = 0; j < sprDef->numFrames; ++j)
            {
                spriteframe_t* sprFrame = &sprDef->spriteFrames[j];
                int k;
                for(k = 0; k < 8; ++k)
                    Materials_Precache(sprFrame->mats[k], spec, true);
            }
        }
    }}
}

void R_PrecacheForMap(void)
{
    float startTime;

    // Don't precache when playing demo.
    if(isDedicated || playback)
        return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    startTime = Sys_GetSeconds();

    if(precacheMapMaterials)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        uint i, j;

        for(i = 0; i < numSideDefs; ++i)
        {
            sidedef_t* side = SIDE_PTR(i);

            if(side->SW_middlematerial)
                Materials_Precache(side->SW_middlematerial, spec, true);

            if(side->SW_topmaterial)
                Materials_Precache(side->SW_topmaterial, spec, true);

            if(side->SW_bottommaterial)
                Materials_Precache(side->SW_bottommaterial, spec, true);
        }

        for(i = 0; i < numSectors; ++i)
        {
            sector_t* sec = SECTOR_PTR(i);
            if(!sec->lineDefCount) continue;
            for(j = 0; j < sec->planeCount; ++j)
            {
                Materials_Precache(sec->SP_planematerial(j), spec, true);
            }
        }
    }

    if(precacheSprites)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_SPRITE, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
        int i;
        for(i = 0; i < numSprites; ++i)
        {
            spritedef_t* sprDef = &sprites[i];

            if(!P_IterateThinkers(gx.MobjThinker, 0x1 /* All mobjs are public */, findSpriteOwner, sprDef))
            {   // This sprite is used by some state of at least one mobj.
                int j;

                // Precache all the frames.
                for(j = 0; j < sprDef->numFrames; ++j)
                {
                    spriteframe_t* sprFrame = &sprDef->spriteFrames[j];
                    int k;
                    for(k = 0; k < 8; ++k)
                        Materials_Precache(sprFrame->mats[k], spec, true);
                }
            }
        }
    }

     // Sky models usually have big skins.
    R_SkyPrecache();

    // Precache model skins?
    if(useModels && precacheSkins)
    {
        // All mobjs are public.
        P_IterateThinkers(gx.MobjThinker, 0x1, R_PrecacheSkinsForMobj, NULL);
    }

    VERBOSE(Con_Message("Precaching took %.2f seconds.\n", Sys_GetSeconds() - startTime))
}

/**
 * Initialize an entire animation using the data in the definition.
 */
void R_InitAnimGroup(ded_group_t* def)
{
    int i, groupNumber = -1;
    for(i = 0; i < def->count.num; ++i)
    {
        ded_group_member_t* gm = &def->members[i];
        material_t* mat;

        if(!gm->material) continue;

        mat = Materials_MaterialForUri2(gm->material, true/*quiet please*/);
        if(!mat) continue;

        // Only create a group when the first texture is found.
        if(groupNumber == -1)
        {
            groupNumber = Materials_CreateAnimGroup(def->flags);
        }

        Materials_AddAnimGroupFrame(groupNumber, mat, gm->tics, gm->randomTics);
    }
}

texture_t* R_CreateDetailTextureFromDef(const ded_detailtexture_t* def)
{
    texture_t* tex;
    detailtex_t* dTex;
    char name[9];
    Uri* uri;

    // Have we already created one for this?
    tex = R_FindDetailTextureForFilePath(def->detailTex, def->isExternal);
    if(tex) return tex;

    if(M_NumDigits(Textures_Count(TN_DETAILS)+1) > 8)
    {
        Con_Message("Warning: failed to create new detail texture (max:%i).\n", DDMAXINT);
        return NULL;
    }

    /**
     * A new detail texture.
     */
    dTex = (detailtex_t*)malloc(sizeof *dTex);
    if(!dTex)
        Con_Error("R_CreateDetailTextureFromDef: Failed on allocation of %lu bytes for new DetailTex.", (unsigned long) sizeof *dTex);

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, Textures_Count(TN_DETAILS)+1);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_DETAILS_NAME);
    tex = Textures_Create(uri, TXF_CUSTOM, (void*)dTex);
    if(!tex)
    {
        Con_Message("Warning, failed creating new Texture for DetailTexture %s.\n", name);
        Uri_Delete(uri);
        free(dTex);
        return NULL;
    }

    dTex->isExternal = def->isExternal;
    dTex->path = def->detailTex;

    Uri_Delete(uri);
    return tex;
}

typedef struct {
    const Uri* path;
    boolean isExternal;
} finddetailtextureforfilepathworker_params_t;

static int findDetailTextureForFilePathWorker(texture_t* tex, void* paramaters)
{
    finddetailtextureforfilepathworker_params_t* p = (finddetailtextureforfilepathworker_params_t*)paramaters;
    detailtex_t* dTex = (detailtex_t*)Texture_UserData(tex);
    assert(tex && p);

    if(dTex->path && dTex->isExternal == p->isExternal && Uri_Equality(dTex->path, p->path))
    {
        return (int)Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

texture_t* R_FindDetailTextureForFilePath(const Uri* filePath, boolean isExternal)
{
    finddetailtextureforfilepathworker_params_t p;
    int result;
    if(!filePath) return NULL;

    p.path = filePath;
    p.isExternal = isExternal;
    result = Textures_Iterate2(TN_DETAILS, findDetailTextureForFilePathWorker, (void*)&p);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

texture_t* R_CreateLightMap(const Uri* filePath)
{
    texture_t* tex;
    lightmap_t* lmap;
    char name[9];
    Uri* uri;

    if(!filePath || Str_IsEmpty(Uri_Path(filePath)) || !Str_CompareIgnoreCase(Uri_Path(filePath), "-"))
        return NULL;

    // Have we already created one for this?
    tex = R_FindLightMapForFilePath(filePath);
    if(tex) return tex;

    if(M_NumDigits(Textures_Count(TN_LIGHTMAPS)+1) > 8)
    {
        Con_Message("Warning, failed creating new LightMap (max:%i).\n", DDMAXINT);
        return NULL;
    }

    /**
     * A new lightmap.
     */

    lmap = (lightmap_t*)malloc(sizeof *lmap);
    if(!lmap)
        Con_Error("R_CreateLightMap: Failed on allocation of %lu bytes for new LightMap.", (unsigned long) sizeof *lmap);

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, Textures_Count(TN_LIGHTMAPS)+1);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_LIGHTMAPS_NAME);
    tex = Textures_Create(uri, TXF_CUSTOM, (void*)lmap);
    if(!tex)
    {
        Con_Message("Warning, failed creating new Texture for LightMap %s.\n", name);
        Uri_Delete(uri);
        free(lmap);
        return NULL;
    }

    lmap->external = filePath;

    Uri_Delete(uri);
    return tex;
}

static int findLightMapTextureForFilePathWorker(texture_t* tex, void* paramaters)
{
    const Uri* filePath = (Uri*)paramaters;
    lightmap_t* lmap = (lightmap_t*)Texture_UserData(tex);
    assert(tex && filePath);

    if(lmap && lmap->external && Uri_Equality(lmap->external, filePath))
    {
        return (int)Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

texture_t* R_FindLightMapForFilePath(const Uri* filePath)
{
    int result;
    if(!filePath || !Str_CompareIgnoreCase(Uri_Path(filePath), "-")) return NULL;

    result = Textures_Iterate2(TN_LIGHTMAPS, findLightMapTextureForFilePathWorker, (void*)filePath);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

texture_t* R_CreateFlareTexture(const Uri* filePath)
{
    texture_t* tex;
    flaretex_t* fTex;
    char name[9];
    Uri* uri;

    if(!filePath || Str_IsEmpty(Uri_Path(filePath)) || !Str_CompareIgnoreCase(Uri_Path(filePath), "-"))
        return NULL;

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    if(Str_At(Uri_Path(filePath), 0) >= '0' && Str_At(Uri_Path(filePath), 0) <= '4' && !Str_At(Uri_Path(filePath), 1))
        return NULL;

    // Have we already created one for this?
    tex = R_FindFlareTextureForFilePath(filePath);
    if(tex) return tex;

    if(M_NumDigits(Textures_Count(TN_FLAREMAPS)+1) > 8)
    {
        Con_Message("Warning, failed creating new FlareTex (max:%i).\n", DDMAXINT);
        return NULL;
    }

    /**
     * A new flare texture.
     */

    fTex = (flaretex_t*)malloc(sizeof *fTex);
    if(!fTex)
        Con_Error("R_CreateFlareTexture: Failed on allocation of %lu bytes for new FlareTex.", (unsigned long) sizeof *fTex);

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, Textures_Count(TN_FLAREMAPS)+1);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_FLAREMAPS_NAME);
    tex = Textures_Create(uri, TXF_CUSTOM, (void*)fTex);
    if(!tex)
    {
        Con_Message("Warning, failed creating new Texture for FlareTex %s.\n", name);
        Uri_Delete(uri);
        free(fTex);
        return NULL;
    }

    fTex->external = filePath;

    Uri_Delete(uri);
    return tex;
}

static int findFlareTextureForFilePathWorker(texture_t* tex, void* paramaters)
{
    const Uri* filePath = (Uri*)paramaters;
    flaretex_t* fTex = (flaretex_t*)Texture_UserData(tex);
    assert(tex && filePath);

    if(fTex && fTex->external && Uri_Equality(fTex->external, filePath))
    {
        return (int)Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

texture_t* R_FindFlareTextureForFilePath(const Uri* filePath)
{
    int result;
    if(!filePath || !Str_CompareIgnoreCase(Uri_Path(filePath), "-")) return NULL;

    result = Textures_Iterate2(TN_FLAREMAPS, findFlareTextureForFilePathWorker, (void*)filePath);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

texture_t* R_CreateShinyTexture(const Uri* filePath)
{
    texture_t* tex;
    shinytex_t* sTex;
    char name[9];
    Uri* uri;

    // Have we already created one for this?
    tex = R_FindReflectionTextureForFilePath(filePath);
    if(tex) return tex;

    if(M_NumDigits(Textures_Count(TN_REFLECTIONS)+1) > 8)
    {
        Con_Message("Warning, failed creating new ShinyTex (max:%i).\n", DDMAXINT);
        return NULL;
    }

    /**
     * A new shiny texture.
     */

    sTex = (shinytex_t*)malloc(sizeof *sTex);
    if(!sTex)
        Con_Error("R_CreateShinyTexture: Failed on allocation of %lu bytes for new ShinyTex.", (unsigned long) sizeof *sTex);

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, Textures_Count(TN_REFLECTIONS)+1);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_REFLECTIONS_NAME);
    tex = Textures_Create(uri, TXF_CUSTOM, (void*)sTex);
    if(!tex)
    {
        Con_Message("Warning, failed creating new Texture for ShinyTex %s.\n", name);
        Uri_Delete(uri);
        free(sTex);
        return NULL;
    }

    sTex->external = filePath;

    Uri_Delete(uri);
    return tex;
}

static int findReflectionTextureForFilePathWorker(texture_t* tex, void* paramaters)
{
    const Uri* filePath = (Uri*)paramaters;
    shinytex_t* rTex = (shinytex_t*)Texture_UserData(tex);
    assert(tex && filePath);

    if(rTex && rTex->external && Uri_Equality(rTex->external, filePath))
    {
        return (int)Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

texture_t* R_FindReflectionTextureForFilePath(const Uri* filePath)
{
    int result;
    if(!filePath || !Str_IsEmpty(Uri_Path(filePath))) return NULL;

    result = Textures_Iterate2(TN_REFLECTIONS, findReflectionTextureForFilePathWorker, (void*)filePath);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

texture_t* R_CreateMaskTexture(const Uri* filePath, int width, int height)
{
    texture_t* tex;
    masktex_t* mTex;
    char name[9];
    Uri* uri;

    // Have we already created one for this?
    tex = R_FindMaskTextureForFilePath(filePath);
    if(tex) return tex;

    if(M_NumDigits(Textures_Count(TN_MASKS)+1) > 8)
    {
        Con_Message("Warning, failed to create mask image (max:%i).\n", DDMAXINT);
        return NULL;
    }

    /**
     * A new shiny texture.
     */

    mTex = (masktex_t*)malloc(sizeof *mTex);
    if(!mTex)
        Con_Error("R_CreateMaskTexture: Failed on allocation of %lu bytes for new MaskTex.", (unsigned long) sizeof *mTex);

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, Textures_Count(TN_MASKS)+1);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_MASKS_NAME);
    tex = Textures_CreateWithDimensions(uri, TXF_CUSTOM, width, height, (void*)mTex);
    if(!tex)
    {
        ddstring_t* path = Uri_ToString(filePath);
        Con_Message("Warning, failed creating Texture for mask image: %s\n", F_PrettyPath(Str_Text(path)));
        Str_Delete(path);
        Uri_Delete(uri);
        free(mTex);
        return NULL;
    }

    mTex->external = filePath;

    Uri_Delete(uri);
    return tex;
}

static int findMaskTextureForFilePathWorker(texture_t* tex, void* paramaters)
{
    const Uri* filePath = (Uri*)paramaters;
    masktex_t* mTex = (masktex_t*)Texture_UserData(tex);
    assert(tex && filePath);

    if(mTex && mTex->external && Uri_Equality(mTex->external, filePath))
    {
        return (int)Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

texture_t* R_FindMaskTextureForFilePath(const Uri* filePath)
{
    int result;
    if(!filePath || !Str_IsEmpty(Uri_Path(filePath))) return NULL;

    result = Textures_Iterate2(TN_MASKS, findMaskTextureForFilePathWorker, (void*)filePath);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

boolean R_DrawVLightVector(const vlight_t* light, void* context)
{
    float distFromViewer = fabs(*((float*)context));
    if(distFromViewer < 1600-8)
    {
        float alpha = 1 - distFromViewer / 1600, scale = 100;

        glBegin(GL_LINES);
            glColor4f(light->color[CR], light->color[CG], light->color[CB], alpha);
            glVertex3f(scale * light->vector[VX], scale * light->vector[VZ], scale * light->vector[VY]);
            glColor4f(light->color[CR], light->color[CG], light->color[CB], 0);
            glVertex3f(0, 0, 0);
        glEnd();
    }
    return true; // Continue iteration.
}

float RColor_AverageColor(rcolor_t* c)
{
    assert(c);
    return (c->red + c->green + c->blue) / 3;
}

float RColor_AverageColorMulAlpha(rcolor_t* c)
{
    assert(c);
    return (c->red + c->green + c->blue) / 3 * c->alpha;
}

void R_DestroyTextureLUTs(void)
{
    R_DestroyFlatOriginalIndexMap();
    R_DestroyPatchCompositeOriginalIndexMap();
}
