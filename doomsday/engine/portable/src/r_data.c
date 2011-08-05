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
byte precacheSprites = true;

byte* translationTables = NULL;

uint patchTexturesCount = 0;
patchtex_t** patchTextures = NULL;

int sysTexturesCount = 0;
systex_t** sysTextures = NULL;

int lightmapTexturesCount = 0;
lightmap_t** lightmapTextures = NULL;

int flareTexturesCount = 0;
flaretex_t** flareTextures = NULL;

int shinyTexturesCount = 0;
shinytex_t** shinyTextures = NULL;

int maskTexturesCount = 0;
masktex_t** maskTextures = NULL;

// Skinnames will only *grow*. It will never be destroyed, not even
// at resets. The skin textures themselves will be deleted, though.
// This is because we want to have permanent ID numbers for skins,
// and the ID numbers are the same as indices to the skinNames array.
// Created in r_model.c, when registering the skins.
uint numSkinNames = 0;
skinname_t* skinNames = NULL;

int gameDataFormat; // Use a game-specifc data format where applicable.
byte rendInfoRPolys = 0;

colorpaletteid_t defaultColorPalette = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int spriteTexturesCount = 0;
static spritetex_t** spriteTextures = NULL;

static int flatTexturesCount = 0;
static flat_t** flatTextures = NULL;

static int patchCompositeTexturesCount;
static patchcompositetex_t** patchCompositeTextures;

static int detailTexturesCount = 0;
static detailtex_t** detailTextures = NULL;

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
    colorpalette_t* pal = ColorPalette_Construct(compOrder, compSize, data, num);

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
            ColorPalette_Destruct(colorPalettes[idx]);
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
            ColorPalette_Destruct(colorPalettes[i]);
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

void R_InitSystemTextures(void)
{
    struct texdef_s {
        const char* name;
        const char* path;
    } static const defs[] = {
        { "unknown", GRAPHICS_RESOURCE_NAMESPACE_NAME":unknown" },
        { "missing", GRAPHICS_RESOURCE_NAMESPACE_NAME":missing" },
        { "bbox",    GRAPHICS_RESOURCE_NAMESPACE_NAME":bbox" },
        { "gray",    GRAPHICS_RESOURCE_NAMESPACE_NAME":gray" },
        { NULL, NULL }
    };
    { uint i;
    for(i = 0; defs[i].name; ++i)
    {
        const texture_t* glTex = GL_CreateTexture(defs[i].name, i, TN_SYSTEM);
        systex_t* sysTex;
    
        sysTex = malloc(sizeof(*sysTex));
        sysTex->id = Texture_Id(glTex);
        sysTex->external = Uri_Construct(defs[i].path);

        // Add it to the list.
        sysTextures = realloc(sysTextures, sizeof(systex_t*) * ++sysTexturesCount);
        sysTextures[sysTexturesCount-1] = sysTex;
    }}
}

void R_DestroySystemTextures(void)
{
    { int i;
    for(i = 0; i < sysTexturesCount; ++i)
    {
        systex_t* rec = sysTextures[i];
        Uri_Destruct(rec->external);
        free(rec);
    }}

    if(sysTextures)
        free(sysTextures);
    sysTextures = NULL;
    sysTexturesCount = 0;
}

static patchtex_t* getPatchTex(patchid_t id)
{
    if(id != 0 && id <= patchTexturesCount)
        return patchTextures[id-1];
    return NULL;
}

static patchid_t findPatchTextureByName(const char* name)
{
    assert(name && name[0]);
    {
    const texture_t* glTex;
    dduri_t* uri = Uri_Construct2(name, RC_NULL);
    Uri_SetScheme(uri, TN_PATCHES_NAME);
    glTex = GL_TextureByUri(uri);
    Uri_Destruct(uri);
    if(glTex == NULL)
        return 0;
    return (patchid_t) Texture_TypeIndex(glTex);
    }
}

/**
 * Returns a patchtex_t* for the given lump, if one already exists.
 */
patchtex_t* R_PatchTextureByIndex(patchid_t id)
{
    patchtex_t* patchTex = getPatchTex(id);
    if(!patchTex)
        Con_Error("R_PatchTextureByIndex: Unknown patch %i.", id);
    return patchTex;
}

void R_ClearPatchTexs(void)
{
    uint i;
    for(i = 0; i < patchTexturesCount; ++i)
    {
        patchtex_t* patchTex = patchTextures[i];
        Str_Free(&patchTex->name);
    }
    Z_FreeTags(PU_PATCH, PU_PATCH);
    patchTextures = 0;
    patchTexturesCount = 0;
}

/**
 * Get a patchtex_t data structure for a patch specified with a WAD lump
 * number. Allocates a new patchtex_t if it hasn't been loaded yet.
 */
patchid_t R_RegisterPatch(const char* name)
{
    assert(name);
    {
    const doompatch_header_t* patch;
    lumpnum_t lumpNum;
    patchid_t id;
    patchtex_t* p;

    if(!name[0])
        return 0;

    // Already defined as a patch?
    if(0 != (id = findPatchTextureByName(name)))
        return id;

    if(-1 == (lumpNum = W_CheckLumpNumForName2(name, true)))
    {
        Con_Message("Warning:R_RegisterPatch: Failed to locate lump for patch '%s'.\n", name);
        return 0;
    }

    /// \fixme What about min lumpNum size?
    patch = (const doompatch_header_t*) W_CacheLump(lumpNum, PU_APPSTATIC);

    // An entirely new patch.
    p = Z_Calloc(sizeof(*p), PU_PATCH, NULL);
    Str_Init(&p->name);
    Str_Set(&p->name, name);
    p->lumpNum = lumpNum;
    /**
     * \fixme: Cannot be sure this is in Patch format until a load attempt
     * is made. We should not read this info here!
     */
    p->offX = -SHORT(patch->leftOffset);
    p->offY = -SHORT(patch->topOffset);
    p->isCustom = !W_LumpIsFromIWAD(lumpNum);

    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    if(monochrome)
        p->flags |= PF_MONOCHROME;
    if(upscaleAndSharpenPatches)
        p->flags |= PF_UPSCALE_AND_SHARPEN;

    // Register a texture for this.
    { const texture_t* glTex = GL_CreateTexture2(name, ++patchTexturesCount,
        TN_PATCHES, SHORT(patch->width), SHORT(patch->height));
    p->texId = Texture_Id(glTex);
    }

    // Add it to the pointer array.
    patchTextures = Z_Realloc(patchTextures, sizeof(patchtex_t*) * patchTexturesCount, PU_PATCH);
    patchTextures[patchTexturesCount-1] = p;

    W_CacheChangeTag(lumpNum, PU_CACHE);
    return patchTexturesCount;
    }
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info)
{
    const patchtex_t* patch;
    if(!info)
        Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");

    memset(info, 0, sizeof(*info));
    patch = getPatchTex(id);
    if(NULL != patch)
    {
        texture_t* tex = GL_ToTexture(patch->texId);
        assert(NULL != tex);
        info->id = id;
        info->width = Texture_Width(tex);
        info->height = Texture_Height(tex);
        info->offset = patch->offX;
        info->topOffset = patch->offY;
        info->isCustom = patch->isCustom;
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

const ddstring_t* R_GetPatchName(patchid_t id)
{
    const patchtex_t* patch = getPatchTex(id);
    if(NULL != patch)
    {
        return &patch->name;
    }
    if(id != 0)
    {
        VERBOSE( Con_Message("Warning:R_GetPatchName: Invalid Patch id #%u.\n", (uint)id) )
    }
    return NULL;
}

patchid_t R_PrecachePatch(const char* name, patchinfo_t* info)
{
    patchid_t patchId;

    if(info)
        memset(info, 0, sizeof(patchinfo_t));

    if(NULL == name || !name[0])
    {
        Con_Message("Warning:R_PrecachePatch: Invalid 'name' argument, ignoring.\n");
        return 0;
    }

    patchId = R_RegisterPatch(name);
    if(0 != patchId)
    {
        GL_PreparePatch(getPatchTex(patchId));
        if(info)
        {
            R_GetPatchInfo(patchId, info);
        }
    }
    return patchId;
}

/**
 * Returns a NULL-terminated array of pointers to all the rawtexs.
 * The array must be freed with Z_Free.
 */
rawtex_t** R_CollectRawTexs(int* count)
{
    int                 i, num;
    rawtex_t*           r, **list;

    // First count the number of patchtexs.
    for(num = 0, i = 0; i < RAWTEX_HASH_SIZE; ++i)
        for(r = rawtexhash[i].first; r; r = r->next)
            num++;

    // Tell this to the caller.
    if(count)
        *count = num;

    // Allocate the array, plus one for the terminator.
    list = Z_Malloc(sizeof(r) * (num + 1), PU_APPSTATIC, NULL);

    // Collect the pointers.
    for(num = 0, i = 0; i < RAWTEX_HASH_SIZE; ++i)
        for(r = rawtexhash[i].first; r; r = r->next)
            list[num++] = r;

    // Terminate.
    list[num] = NULL;

    return list;
}

/**
 * Returns a rawtex_t* for the given lump, if one already exists.
 */
rawtex_t* R_FindRawTex(lumpnum_t lumpNum)
{
    if(-1 == lumpNum || lumpNum >= W_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_FindRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, W_LumpCount());
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

/**
 * Get a rawtex_t data structure for a raw texture specified with a WAD lump
 * number. Allocates a new rawtex_t if it hasn't been loaded yet.
 */
rawtex_t* R_GetRawTex(lumpnum_t lumpNum)
{
    rawtexhash_t* hash = 0;
    rawtex_t* r;

    if(-1 == lumpNum || lumpNum >= W_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_GetRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, W_LumpCount());
#endif
        return NULL;
    }

    // Check if this lumpNum has already been loaded as a rawtex.
    r = R_FindRawTex(lumpNum);
    if(NULL != r) return r;

    // Hmm, this is an entirely new rawtex.
    r = Z_Calloc(sizeof(*r), PU_REFRESHRAW, 0);
    Str_Init(&r->name);
    Str_Set(&r->name, W_LumpName(lumpNum));
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

static void clearPatchComposites(void)
{
    int i;

    if(patchCompositeTexturesCount == 0) return;

    for(i = 0; i < patchCompositeTexturesCount; ++i)
    {
        patchcompositetex_t* pcomTex = patchCompositeTextures[i];
        Str_Free(&pcomTex->name);
    }
    Z_FreeTags(PU_REFRESHTEX, PU_REFRESHTEX);
    patchCompositeTextures = NULL;
    patchCompositeTexturesCount = 0;
}

static patchname_t* loadPatchNames(lumpnum_t lumpNum, int* num)
{
    size_t lumpSize = W_LumpLength(lumpNum);
    const char* lump = W_CacheLump(lumpNum, PU_APPSTATIC);
    patchname_t* names, *name;
    int numNames;

    if(lumpSize < 4)
    {
        Con_Message("Warning:loadPatchNames: Lump '%s'(#%i) is not valid PNAMES data.\n",
            W_LumpName(lumpNum), lumpNum);
        if(NULL != num) *num = 0;
        return NULL;
    }

    numNames = LONG(*((int*) lump));
    if(numNames <= 0)
    {
        if(NULL != num) *num = 0;
        return NULL;
    }

    if((unsigned)numNames > (lumpSize - 4) / 8)
    {   // Lump is truncated.
        Con_Message("Warning:loadPatchNames: Lump '%s'(#%i) truncated (%lu bytes, expected %lu).\n",
            W_LumpName(lumpNum), lumpNum, (unsigned long) lumpSize, (unsigned long) (numNames * 8 + 4));
        numNames = (int) ((lumpSize - 4) / 8);
    }

    names = (patchname_t*) calloc(1, numNames * sizeof(*names));
    if(NULL == names)
        Con_Error("loadPatchNames: Failed on allocation of %lu bytes for patch name list.",
            (unsigned long) (numNames * sizeof(*names)));

    name = names;
    { int i;
    for(i = 0; i < numNames; ++i)
    {
        /// \fixme Some filtering of invalid characters wouldn't go amiss...
        strncpy(*name, lump + 4 + i * 8, 8);
        name++;
    }}

    W_CacheChangeTag(lumpNum, PU_CACHE);

    if(NULL != num)
        *num = numNames;

    return names;
}

/**
 * Read DOOM and Strife format texture definitions from the specified lump.
 */
static patchcompositetex_t** readDoomTextureDefLump(lumpnum_t lumpNum,
    patchname_t* patchNames, int numPatchNames, boolean firstNull, int* numDefs)
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
    short* texDefNumPatches;
    patchinfo_t* patchInfo;
    byte* validTexDefs;
    int* directory;
    void* storage;
    int* maptex1;
    int i;

    patchInfo = (patchinfo_t*)calloc(1, sizeof(*patchInfo) * numPatchNames);
    if(NULL == patchInfo)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for patch info.",
            (unsigned long) (sizeof(*patchInfo) * numPatchNames));

    lumpSize = W_LumpLength(lumpNum);
    maptex1 = (int*) malloc(lumpSize);
    if(NULL == maptex1)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for temporary copy of "
            "archived DOOM texture definitions.", (unsigned long) lumpSize);
    W_ReadLump(lumpNum, (char*)maptex1);

    numTexDefs = LONG(*maptex1);

    VERBOSE( Con_Message("  Processing lump \"%s\"...\n", W_LumpName(lumpNum)) )

    validTexDefs = (byte*) calloc(1, numTexDefs * sizeof(*validTexDefs));
    if(NULL == validTexDefs)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for valid texture record list.",
            (unsigned long) (numTexDefs * sizeof(*validTexDefs)));

    texDefNumPatches = (short*) calloc(1, numTexDefs * sizeof(*texDefNumPatches));
    if(NULL == texDefNumPatches)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for texture patch count record list.",
            (unsigned long) (numTexDefs * sizeof(*texDefNumPatches)));

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
            Con_Message("Warning:R_ReadTextureDefs: Bad offset %lu for definition %i in lump \"%s\".\n",
                (unsigned long) offset, i, W_LumpName(lumpNum));
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
                        Con_Message("Warning:R_ReadTextureDefs: Invalid patch %i in texture '%s'.\n",
                            (int) patchNum, mtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = W_CheckLumpNumForName2(*(patchNames + patchNum), true);
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning:R_ReadTextureDefs: Failed to locate patch '%s'.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning:R_ReadTextureDefs: Missing patch %i in texture '%s'.\n",
                            (int) j, mtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning:R_ReadTextureDefs: Invalid patchcount %i in texture '%s'.\n",
                    (int) patchCount, mtexture->name);
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
                        Con_Message("Warning:R_ReadTextureDefs: Invalid patch %i in texture '%s'.\n",
                            (int) patchNum, smtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = W_CheckLumpNumForName2(*(patchNames + patchNum), true);
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning:R_ReadTextureDefs: Failed to locate patch '%s'.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning:R_ReadTextureDefs: Missing patch %i in texture '%s'.\n",
                            (int) j, smtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning:R_ReadTextureDefs: Invalid patchcount %i in texture '%s'.\n",
                    (int) patchCount, smtexture->name);
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
        texDefs = Z_Malloc(numValidTexDefs * sizeof(*texDefs), PU_REFRESHTEX, 0);
        storage = Z_Calloc(sizeof(patchcompositetex_t) * numValidTexDefs +
                           sizeof(texpatch_t) * numValidPatchRefs, PU_REFRESHTEX, 0);
        directory = maptex1 + 1;
        n = 0;
        for(i = 0; i < numTexDefs; ++i, directory++)
        {
            patchcompositetex_t* texDef;
            short j;

            if(!validTexDefs[i])
                continue;

            offset = LONG(*directory);

            // Read and create the texture def.
            if(gameDataFormat == 0)
            {   // DOOM format.
                texpatch_t* patch;
                mappatch_t* mpatch;
                maptexture_t* mtexture = (maptexture_t*) ((byte*) maptex1 + offset);

                texDef = storage;
                texDef->patchCount = texDefNumPatches[i];
                Str_Init(&texDef->name);
                Str_PartAppend(&texDef->name, (const char*) mtexture->name, 0, 8);
                texDef->width = SHORT(mtexture->width);
                texDef->height = SHORT(mtexture->height);
                storage = (byte *) storage + sizeof(patchcompositetex_t) + sizeof(texpatch_t) * texDef->patchCount;

                mpatch = &mtexture->patches[0];
                patch = &texDef->patches[0];
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

                texDef = storage;
                texDef->patchCount = texDefNumPatches[i];
                Str_Init(&texDef->name);
                Str_PartAppend(&texDef->name, (const char*) smtexture->name, 0, 8);
                texDef->width = SHORT(smtexture->width);
                texDef->height = SHORT(smtexture->height);
                storage = (byte*) storage + sizeof(patchcompositetex_t) + sizeof(texpatch_t) * texDef->patchCount;

                smpatch = &smtexture->patches[0];
                patch = &texDef->patches[0];
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

            /**
             * Vanilla DOOM's implementation of the texture collection has a flaw which
             * results in the first texture being used dually as a "NULL" texture.
             */
            if(firstNull && i == 0)
                texDef->flags |= TXDF_NODRAW;

            /**
             * Is this a non-IWAD texture?
             * At this stage we assume it is an IWAD texture definition unless one of the
             * patches is not.
             */
            texDef->flags |= TXDF_IWAD;
            j = 0;
            while(j < texDef->patchCount && (texDef->flags & TXDF_IWAD))
            {
                if(!W_LumpIsFromIWAD(texDef->patches[j].lumpNum))
                    texDef->flags &= ~TXDF_IWAD;
                else
                    j++;
            }

            // Add it to the list.
            texDefs[n++] = texDef;
        }
    }

    VERBOSE2( Con_Message("  Loaded %i of %i definitions.\n", numValidTexDefs, numTexDefs) )

    // Free all temporary storage.
    free(validTexDefs);
    free(texDefNumPatches);
    free(maptex1);

    if(numDefs)
        *numDefs = numValidTexDefs;

    return texDefs;
}

static void loadPatchCompositeDefs(void)
{
    patchcompositetex_t** list = 0, **listCustom = 0, ***eList = 0;
    int count = 0, countCustom = 0, *eCount = 0;
    lumpnum_t i, pnamesLump;
    patchname_t* patchNames;
    int numPatchNames;
    boolean firstNull;

    if(-1 == (pnamesLump = W_CheckLumpNumForName2("PNAMES", true)))
        return;

    // Load the patch names from the PNAMES lump.
    patchNames = loadPatchNames(pnamesLump, &numPatchNames);
    if(NULL == patchNames)
    {
        Con_Message("Warning:loadPatchCompositeDefs: Unexpected error occured loading PNAMES.\n");
        return;
    }

    /**
     * Many PWADs include new TEXTURE1/2 lumps including the IWAD doomtexture
     * definitions, with new definitions appended. In order to correctly
     * determine whether a defined texture originates from an IWAD we must
     * compare all definitions against those in the IWAD and if matching,
     * they should be considered as IWAD resources, even though the doomtexture
     * definition does not come from an IWAD lump.
     */
    firstNull = true;
    for(i = 0; i < W_LumpCount(); ++i)
    {
        char name[9];

        memset(name, 0, sizeof(name));
        strncpy(name, W_LumpName(i), 8);
        strupr(name); // Case insensitive.

        if(!strncmp(name, "TEXTURE1", 8) || !strncmp(name, "TEXTURE2", 8))
        {
            boolean isFromIWAD = W_LumpIsFromIWAD(i);
            int newNumTexDefs;
            patchcompositetex_t** newTexDefs;

            // Read in the new texture defs.
            newTexDefs = readDoomTextureDefLump(i, patchNames, numPatchNames, firstNull, &newNumTexDefs);

            eList = (isFromIWAD? &list : &listCustom);
            eCount = (isFromIWAD? &count : &countCustom);
            if(*eList)
            {
                patchcompositetex_t** newList;
                size_t n;
                int i;

                // Merge with the existing doomtexturedefs.
                newList = Z_Malloc(sizeof(*newList) * ((*eCount) + newNumTexDefs), PU_REFRESHTEX, 0);
                n = 0;
                for(i = 0; i < *eCount; ++i)
                    newList[n++] = (*eList)[i];
                for(i = 0; i < newNumTexDefs; ++i)
                    newList[n++] = newTexDefs[i];

                Z_Free(*eList);
                Z_Free(newTexDefs);

                *eList = newList;
                *eCount += newNumTexDefs;
            }
            else
            {
                *eList = newTexDefs;
                *eCount = newNumTexDefs;
            }

            // No more NULL textures.
            firstNull = false;
        }
    }

    if(listCustom)
    {   // There are custom doomtexturedefs, cross compare with the IWAD
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
                {   // This is a newer version of an IWAD doomtexturedef.
                    if(!(custom->flags & TXDF_IWAD))
                        hasReplacement = true; // Uses a non-IWAD patch.
                    else if(custom->height     == orig->height &&
                            custom->width      == orig->width  &&
                            custom->patchCount == orig->patchCount)
                    {   // Check the patches.
                        short k = 0;

                        while(k < orig->patchCount && (custom->flags & TXDF_IWAD))
                        {
                            texpatch_t* origP   = orig->patches  + k;
                            texpatch_t* customP = custom->patches + k;

                            if(origP->lumpNum != customP->lumpNum &&
                               origP->offX != customP->offX &&
                               origP->offY != customP->offY)
                            {
                                custom->flags &= ~TXDF_IWAD;
                            }
                            else
                                k++;
                        }

                        hasReplacement = true;
                    }

                    break;
                }
            }

            if(hasReplacement)
            {   // Let the PWAD "copy" override the IWAD original.
                int n;
                for(n = i + 1; n < count; ++n)
                    list[n-1] = list[n];
                count--;
            }
            else
                i++;
        }

        // List contains only non-replaced doomtexturedefs, merge them.
        newList = Z_Malloc(sizeof(*newList) * (count + countCustom), PU_REFRESHTEX, 0);

        n = 0;
        for(i = 0; i < count; ++i)
            newList[n++] = list[i];
        for(i = 0; i < countCustom; ++i)
            newList[n++] = listCustom[i];

        Z_Free(list);
        Z_Free(listCustom);

        patchCompositeTextures = newList;
        patchCompositeTexturesCount = count + countCustom;
    }
    else
    {
        patchCompositeTextures = list;
        patchCompositeTexturesCount = count;
    }

    // We're finished with the patch names now.
    free(patchNames);
}

static void createTexturesForPatchCompositeDefs(void)
{
    int i;
    for(i = 0; i < patchCompositeTexturesCount; ++i)
    {
        patchcompositetex_t* pcomTex = patchCompositeTextures[i];       
        const texture_t* tex = GL_CreateTexture2(Str_Text(&pcomTex->name), i, TN_TEXTURES,
            pcomTex->width, pcomTex->height);
        if(NULL == tex)
        {
            Con_Message("Warning, failed creating Texture for new patch composite %s.\n",
                Str_Text(&pcomTex->name));
            continue;
        }
        pcomTex->texId = Texture_Id(tex);
    }
}

int R_PatchCompositeCount(void)
{
    return patchCompositeTexturesCount;
}

/**
 * Reads in the texture defs and creates materials for them.
 */
void R_InitPatchComposites(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);

    clearPatchComposites();
    VERBOSE( Con_Message("Initializing PatchComposites...\n") )

    // Load texture definitions from TEXTURE1/2 lumps.
    loadPatchCompositeDefs();
    createTexturesForPatchCompositeDefs();

    VERBOSE2( Con_Message("R_InitPatchComposites: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

patchcompositetex_t* R_PatchCompositeTextureByIndex(int num)
{
    if(num < 0 || num >= patchCompositeTexturesCount)
    {
#if _DEBUG
        Con_Error("R_PatchCompositeTextureByIndex: Invalid def num %i.", num);
#endif
        return NULL;
    }

    return patchCompositeTextures[num];
}

int R_FlatTextureCount(void)
{
    return flatTexturesCount;
}

flat_t* R_FlatTextureByIndex(int idx)
{
    assert(idx >= 0 && idx < flatTexturesCount);
    return flatTextures[idx];
}

int R_FindFlatIdxForName(const char* name)
{
    if(name && name[0])
    {
        int i;
        for(i = 0; i < flatTexturesCount; ++i)
        {
            if(!Str_CompareIgnoreCase(&flatTextures[i]->name, name))
                return i;
        }
    }
    return -1;
}

/// @return  Associated flat index.
static int R_NewFlat(const char* name, lumpnum_t lumpNum, boolean isCustom)
{
    assert(name && name[0]);

    // Is this a known identifer?
    { int idx;
    if(-1 != (idx = R_FindFlatIdxForName(name)))
    {
        // Update metadata and move the flat to the end (relative indices must be respected).
        flatTextures[idx]->isCustom = isCustom;
        flatTextures[idx]->lumpNum = lumpNum;
        if(idx != flatTexturesCount-1)
        {
            flat_t* ptr = flatTextures[idx];
            memmove(flatTextures+idx, flatTextures+idx+1, sizeof(flat_t*) * (flatTexturesCount-1-idx));
            flatTextures[flatTexturesCount-1] = ptr;
        }
        return flatTexturesCount-1;
    }}

    // A new flat.
    flatTextures = Z_Realloc(flatTextures, sizeof(flat_t*) * ++flatTexturesCount, PU_REFRESHFLAT);
    { flat_t* flat = flatTextures[flatTexturesCount - 1] = Z_Malloc(sizeof(*flat), PU_REFRESHFLAT, 0);
    Str_Init(&flat->name);
    Str_Set(&flat->name, name);
    flat->lumpNum = lumpNum;
    flat->isCustom = isCustom;
    flat->texId = 0;
    }
    return flatTexturesCount - 1;
}

/**
 * Free all memory acquired for Flat textures.
 * \note  Does nothing about any Textures or Materials created from these!
 */
static void clearFlatTextures(void)
{
    int i;
    if(0 == flatTexturesCount) return;

    for(i = 0; i < flatTexturesCount; ++i)
    {
        flat_t* flat = flatTextures[i];
        Str_Free(&flat->name);
    }
    Z_FreeTags(PU_REFRESHFLAT, PU_REFRESHFLAT);
    flatTextures = NULL;
    flatTexturesCount = 0;
}

void R_InitFlatTextures(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    ddstack_t* stack = Stack_New();

    clearFlatTextures();
    assert(flatTexturesCount == 0);

    VERBOSE( Con_Message("Initializing Flats...\n") )
    { int i, numLumps = W_LumpCount();
    for(i = 0; i < numLumps; ++i)
    {
        const char* name = W_LumpName(i);

        if(name[0] == 'F')
        {
            if(!strnicmp(name + 1, "_START", 6) ||
               !strnicmp(name + 2, "_START", 6))
            {
                // We've arrived at *a* flat block.
                Stack_Push(stack, NULL);
                continue;
            }
            else if(!strnicmp(name + 1, "_END", 4) ||
                    !strnicmp(name + 2, "_END", 4))
            {
                // The flat block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack))
            continue;

        R_NewFlat(name, (lumpnum_t)i, !W_LumpIsFromIWAD(i));
    }}

    while(Stack_Height(stack))
        Stack_Pop(stack);
    Stack_Delete(stack);

    // Create Textures for all known flats.
    { int i;
    for(i = 0; i < flatTexturesCount; ++i)
    {
        flat_t* flat = flatTextures[i];
        const texture_t* tex = GL_CreateTexture2(Str_Text(&flat->name), i, TN_FLATS, 64, 64);
        if(NULL == tex)
        {
            Con_Message("Warning, failed creating Texture for new flat %s.\n", Str_Text(&flat->name));
            continue;
        }
        flat->texId = Texture_Id(tex);
    }}

    VERBOSE2( Con_Message("R_InitFlatTextures: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

/**
 * Free all memory acquired for SpriteTextures.
 * \note  Does nothing about any Textures or Materials created from these!
 */
static void clearSpriteTextures(void)
{
    int i;
    if(0 == spriteTexturesCount) return;

    for(i = 0; i < spriteTexturesCount; ++i)
    {
        spritetex_t* sprTex = spriteTextures[i];
        Str_Free(&sprTex->name);
    }
    Z_FreeTags(PU_SPRITE, PU_SPRITE);
    spriteTextures = NULL;
    spriteTexturesCount = 0;
}

int R_SpriteTextureCount(void)
{
    return spriteTexturesCount;
}

spritetex_t* R_SpriteTextureByIndex(int idx)
{
    assert(idx >= 0 && idx < spriteTexturesCount);
    return spriteTextures[idx];
}

void R_InitSpriteTextures(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    ddstack_t* stack;

    VERBOSE( Con_Message("Initializing Sprites...\n") )

    clearSpriteTextures();
    assert(spriteTexturesCount == 0);

    /**
     * Step 1: Collection.
     */
    stack = Stack_New();
    { int i, numLumps = W_LumpCount();
    for(i = 0; i < numLumps; ++i)
    {
        const char* name = W_LumpName(i);
        spritetex_t* sprTex = NULL;

        if(name[0] == 'S')
        {
            if(!strnicmp(name + 1, "_START", 6) ||
               !strnicmp(name + 2, "_START", 6))
            {
                // We've arrived at *a* sprite block.
                Stack_Push(stack, NULL);
                continue;
            }
            else if(!strnicmp(name + 1, "_END", 4) ||
                    !strnicmp(name + 2, "_END", 4))
            {
                // The sprite block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack))
            continue;

        // Is this a known identifer?
        { int j;
        for(j = 0; j < spriteTexturesCount; ++j)
        {
            if(!Str_CompareIgnoreCase(&spriteTextures[j]->name, name))
            {
                // Update metadata and move the sprite to the end (relative indices must be respected).
                spriteTextures[j]->isCustom = W_LumpIsFromIWAD(i);
                spriteTextures[j]->lumpNum = (lumpnum_t) i;
                if(j != spriteTexturesCount-1)
                {
                    spritetex_t* ptr = spriteTextures[j];
                    memmove(spriteTextures+j, spriteTextures+j+1, sizeof(spritetex_t*) * (spriteTexturesCount-1-j));
                    spriteTextures[spriteTexturesCount-1] = ptr;
                }
                sprTex = spriteTextures[spriteTexturesCount-1];
            }
        }}

        if(NULL != sprTex)
            continue;

        // A new sprite texture.
        spriteTextures = Z_Realloc(spriteTextures, sizeof(spritetex_t*) * ++spriteTexturesCount, PU_SPRITE);
        sprTex = spriteTextures[spriteTexturesCount-1] = Z_Calloc(sizeof(spritetex_t), PU_SPRITE, 0);

        Str_Init(&sprTex->name);
        Str_Set(&sprTex->name, name);
        sprTex->lumpNum = (lumpnum_t)i;
        sprTex->isCustom = W_LumpIsFromIWAD(i);
    }}

    while(Stack_Height(stack))
        Stack_Pop(stack);
    Stack_Delete(stack);

    /**
     * Step 2: Create Textures for all known sprite textures.
     */
    { int i;
    for(i = 0; i < spriteTexturesCount; ++i)
    {
        spritetex_t* sprTex = spriteTextures[i];
        const doompatch_header_t* patch;
        const texture_t* tex;

        /// \fixme Do NOT assume this is in DOOM's Patch format. Defer until prepare-time.
        if(W_LumpLength(sprTex->lumpNum) < sizeof(doompatch_header_t))
        {
            Con_Message("Warning, sprite frame lump %s (#%i) does not appear to be a valid Patch.\n",
                Str_Text(&sprTex->name), sprTex->lumpNum);
            sprTex->offX = sprTex->offY = 0;
            sprTex->lumpNum = -1;
            continue;
        }

        patch = (const doompatch_header_t*) W_CacheLump(sprTex->lumpNum, PU_APPSTATIC);

        tex = GL_CreateTexture2(Str_Text(&sprTex->name), i, TN_SPRITES, SHORT(patch->width), SHORT(patch->height));
        if(NULL == tex)
        {
            Con_Message("Warning, failed creating Texture for sprite frame lump %s (#%i).\n",
                Str_Text(&sprTex->name), sprTex->lumpNum);
            sprTex->offX = sprTex->offY = 0;
            sprTex->lumpNum = -1;
            W_CacheChangeTag(sprTex->lumpNum, PU_CACHE);
            continue;
        }
        sprTex->texId = Texture_Id(tex);
        sprTex->offX = SHORT(patch->leftOffset);
        sprTex->offY = SHORT(patch->topOffset);
        W_CacheChangeTag(sprTex->lumpNum, PU_CACHE);
    }}

    VERBOSE2(
        Con_Message("R_InitSpriteTextures: Done in %.2f seconds.\n",
            (Sys_GetRealTime() - startTime) / 1000.0f) );
}

uint R_CreateSkinTex(const dduri_t* skin, boolean isShinySkin)
{
    assert(skin);
    {
    char name[9];
    const texture_t* glTex;
    skinname_t* st;
    int id;

    if(Str_IsEmpty(Uri_Path(skin)))
        return 0;

    // Have we already created one for this?
    if(0 != (id = R_GetSkinNumForName(skin)))
        return id;

    if(M_NumDigits(numSkinNames + 1) > 8)
    {
#if _DEBUG
Con_Message("R_GetSkinTex: Too many model skins!\n");
#endif
        return 0;
    }

    /**
     * A new skin name.
     */

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, numSkinNames + 1);

    glTex = GL_CreateTexture(name, numSkinNames, (isShinySkin? TN_MODELREFLECTIONSKINS : TN_MODELSKINS));

    skinNames = M_Realloc(skinNames, sizeof(skinname_t) * ++numSkinNames);
    st = skinNames + (numSkinNames - 1);

    st->path = Uri_ConstructCopy(skin);
    st->id = Texture_Id(glTex);

    if(verbose)
    {
        ddstring_t* searchPath = Uri_ToString(skin);
        Con_Message("SkinTex: \"%s\" -> %li\n", F_PrettyPath(Str_Text(searchPath)),
            (long) (1 + (st - skinNames)));
        Str_Delete(searchPath);
    }
    return 1 + (st - skinNames); // 1-based index.
    }
}

static boolean expandSkinName(ddstring_t* foundPath, const char* skin, const char* modelfn)
{
    assert(foundPath && skin && skin[0]);
    {
    boolean found = false;
    ddstring_t searchPath;
    Str_Init(&searchPath);

    // Try the "first choice" directory first.
    if(modelfn)
    {
        // The "first choice" directory is that in which the model file resides.
        directory_t* mydir = Dir_ConstructFromPathDir(modelfn);
        Str_Appendf(&searchPath, "%s%s", mydir->path, skin);
        found = 0 != F_FindResourceStr2(RC_GRAPHIC, &searchPath, foundPath);
        Dir_Destruct(mydir);
    }

    if(!found)
    {   // Try the resource locator.
        Str_Clear(&searchPath); Str_Appendf(&searchPath, MODELS_RESOURCE_NAMESPACE_NAME":%s", skin);
        found = F_FindResourceStr2(RC_GRAPHIC, &searchPath, foundPath) != 0;
    }

    Str_Free(&searchPath);
    return found;
    }
}

/**
 * Registers a new skin name.
 */
uint R_RegisterSkin(ddstring_t* foundPath, const char* skin, const char* modelfn, boolean isShinySkin)
{
    if(skin && skin[0])
    {
        uint result = 0;
        ddstring_t buf;
        if(!foundPath)
            Str_Init(&buf);
        if(expandSkinName(foundPath ? foundPath : &buf, skin, modelfn))
        {
            dduri_t* uri = Uri_Construct2(foundPath ? Str_Text(foundPath) : Str_Text(&buf), RC_NULL);
            result = R_CreateSkinTex(uri, isShinySkin);
            Uri_Destruct(uri);
        }
        if(!foundPath)
            Str_Free(&buf);
        return result;
    }
    return 0;
}

const skinname_t* R_GetSkinNameByIndex(uint id)
{
    if(!id || id > numSkinNames)
        return NULL;

    return &skinNames[id-1];
}

uint R_GetSkinNumForName(const dduri_t* path)
{
    uint i;
    for(i = 0; i < numSkinNames; ++i)
        if(Uri_Equality(skinNames[i].path, path))
            return i + 1; // 1-based index.
    return 0;
}

void R_DestroySkins(void)
{
    if(0 == numSkinNames)
        return;

    R_ReleaseGLTexturesForSkins();

    { uint i;
    for(i = 0; i < numSkinNames; ++i)
        Uri_Destruct(skinNames[i].path);
    }
    M_Free(skinNames);
    skinNames = NULL;
    numSkinNames = 0;
}

void R_ReleaseGLTexturesForSkins(void)
{
    uint i;
    for(i = 0; i < numSkinNames; ++i)
    {
        skinname_t* sn = skinNames + i;
        texture_t* tex = GL_ToTexture(sn->id);
        if(tex == NULL) continue;

        GL_ReleaseGLTexturesForTexture(tex);
    }
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

boolean R_IsAllowedDecoration(ded_decor_t* def, const material_t* mat,
    boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & DCRF_EXTERNAL) != 0;
    }

    if(!Material_IsCustom(mat))
        return !(def->flags & DCRF_NO_IWAD);

    return (def->flags & DCRF_PWAD) != 0;
}

boolean R_IsAllowedReflection(ded_reflection_t* def, const material_t* mat,
    boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & REFF_EXTERNAL) != 0;
    }

    if(!Material_IsCustom(mat))
        return !(def->flags & REFF_NO_IWAD);

    return (def->flags & REFF_PWAD) != 0;
}

boolean R_IsAllowedDetailTex(ded_detailtexture_t* def, const material_t* mat,
    boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & DTLF_EXTERNAL) != 0;
    }

    if(!Material_IsCustom(mat))
        return !(def->flags & DTLF_NO_IWAD);

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
    materialvariantspecification_t* spec;

    if(!((useModels && precacheSkins) || precacheSprites))
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
                    Materials_Precache(sprFrame->mats[k], spec);
            }
        }
    }}
}

void R_PrecacheForMap(void)
{
    materialvariantspecification_t* spec;
    float startTime;

    // Don't precache when playing demo.
    if(isDedicated || playback)
        return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    startTime = Sys_GetSeconds();
    spec = Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0,
        GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);

    { uint i;
    for(i = 0; i < numSideDefs; ++i)
    {
        sidedef_t* side = SIDE_PTR(i);

        if(NULL != side->SW_middlematerial)
            Materials_Precache(side->SW_middlematerial, spec);

        if(NULL != side->SW_topmaterial)
            Materials_Precache(side->SW_topmaterial, spec);

        if(NULL != side->SW_bottommaterial)
            Materials_Precache(side->SW_bottommaterial, spec);
    }}

    { uint i;
    for(i = 0; i < numSectors; ++i)
    {
        sector_t* sec = SECTOR_PTR(i);
        if(0 == sec->lineDefCount)
            continue;
        { uint j;
        for(j = 0; j < sec->planeCount; ++j)
        {
            Materials_Precache(sec->SP_planematerial(j), spec);
        }}
    }}

    // Precache sprites?
    if(precacheSprites)
    {
        int i;
        spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 1, 0, 0,
            GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
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
                        Materials_Precache(sprFrame->mats[k], spec);
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
        materialnum_t num;

        if(!gm->material)
            continue;

        if((num = Materials_IndexForUri(gm->material)) != 0)
        {
            // Only create a group when the first texture is found.
            if(groupNumber == -1)
            {
                groupNumber = Materials_CreateAnimGroup(def->flags);
            }

            Materials_AddAnimGroupFrame(groupNumber, num, gm->tics, gm->randomTics);
        }
    }
}

detailtex_t* R_CreateDetailTextureFromDef(const ded_detailtexture_t* def)
{
    const texture_t* glTex;
    detailtex_t* dTex;
    char name[9];

    // Have we already created one for this?
    if(NULL != (dTex = R_FindDetailTextureForName(def->detailTex, def->isExternal)))
        return dTex;

    if(M_NumDigits(detailTexturesCount + 1) > 8)
    {
        Con_Message("Warning: failed to create new detail texture (max:%i).\n", DDMAXINT);
        return NULL;
    }

    /**
     * A new detail texture.
     */

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, detailTexturesCount + 1);

    glTex = GL_CreateTexture(name, detailTexturesCount, TN_DETAILS);

    dTex = M_Malloc(sizeof(*dTex));
    dTex->id = Texture_Id(glTex);
    dTex->isExternal = def->isExternal;
    dTex->filePath = def->detailTex;

    // Add it to the list.
    detailTextures = M_Realloc(detailTextures, sizeof(detailtex_t*) * ++detailTexturesCount);
    detailTextures[detailTexturesCount-1] = dTex;

    return dTex;
}

detailtex_t* R_FindDetailTextureForName(const dduri_t* filePath, boolean isExternal)
{
    if(!filePath)
        return 0;
    { int i;
    for(i = 0; i < detailTexturesCount; ++i)
    {
        detailtex_t* dTex = detailTextures[i];
        if(dTex->isExternal == isExternal && Uri_Equality(dTex->filePath, filePath))
            return dTex;
    }}
    return 0;
}

detailtex_t* R_DetailTextureByIndex(int idx)
{
    if(idx >= 0 && idx < detailTexturesCount)
        return detailTextures[idx];
    Con_Error("R_DetailTextureByIndex: Failed to locate by index #%i.", idx);
    return NULL;
}

int R_DetailTextureCount(void)
{
    return detailTexturesCount;
}

void R_DestroyDetailTextures(void)
{
    { int i;
    for(i = 0; i < detailTexturesCount; ++i)
    {
        M_Free(detailTextures[i]);
    }}

    if(detailTextures)
        M_Free(detailTextures);
    detailTextures = NULL;
    detailTexturesCount = 0;
}

lightmap_t* R_CreateLightMap(const dduri_t* path)
{
    const texture_t* glTex;
    lightmap_t* lmap;
    char name[9];

    if(!path || Str_IsEmpty(Uri_Path(path)) || !Str_CompareIgnoreCase(Uri_Path(path), "-"))
        return 0; // Not a lightmap

    // Have we already created one for this?
    if(NULL != (lmap = R_GetLightMap(path)))
        return lmap;

    if(M_NumDigits(lightmapTexturesCount + 1) > 8)
    {
        Con_Message("Warning: Failed to create new lightmap (max:%i).\n", DDMAXINT);
        return 0;
    }

    /**
     * A new lightmap.
     */

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, lightmapTexturesCount + 1);

    glTex = GL_CreateTexture(name, lightmapTexturesCount, TN_LIGHTMAPS);

    lmap = M_Malloc(sizeof(*lmap));
    lmap->id = Texture_Id(glTex);
    lmap->external = path;

    // Add it to the list.
    lightmapTextures = M_Realloc(lightmapTextures, sizeof(lightmap_t*) * ++lightmapTexturesCount);
    lightmapTextures[lightmapTexturesCount-1] = lmap;

    return lmap;
}

lightmap_t* R_GetLightMap(const dduri_t* uri)
{
    if(uri && Str_CompareIgnoreCase(Uri_Path(uri), "-"))
    {
        int i;
        for(i = 0; i < lightmapTexturesCount; ++i)
        {
            lightmap_t* lmap = lightmapTextures[i];

            if(!lmap->external) continue;

            if(Uri_Equality(lmap->external, uri))
                return lmap;
        }
    }
    return 0;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyLightMaps(void)
{
    { int i;
    for(i = 0; i < lightmapTexturesCount; ++i)
    {
        M_Free(lightmapTextures[i]);
    }}

    if(lightmapTextures)
        M_Free(lightmapTextures);
    lightmapTextures = NULL;
    lightmapTexturesCount = 0;
}

flaretex_t* R_CreateFlareTexture(const dduri_t* path)
{
    const texture_t* glTex;
    flaretex_t* fTex;
    char name[9];

    if(!path || Str_IsEmpty(Uri_Path(path)) || !Str_CompareIgnoreCase(Uri_Path(path), "-"))
        return 0; // Not a flare texture.

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    if(Str_At(Uri_Path(path), 0) >= '0' && Str_At(Uri_Path(path), 0) <= '4' && !Str_At(Uri_Path(path), 1))
        return 0; // Don't create a flaretex for this

    // Have we already created one for this?
    if(NULL != (fTex = R_GetFlareTexture(path)))
        return fTex;

    if(M_NumDigits(flareTexturesCount + 1) > 8)
    {
        Con_Message("Warning: Failed to create new flare texture (max:%i).\n", DDMAXINT);
        return 0;
    }

    /**
     * A new flare texture.
     */
    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, flareTexturesCount + 1);

    glTex = GL_CreateTexture(name, flareTexturesCount, TN_FLAREMAPS);

    fTex = M_Malloc(sizeof(*fTex));
    fTex->external = path;
    fTex->id = Texture_Id(glTex);

    // Add it to the list.
    flareTextures = M_Realloc(flareTextures, sizeof(flaretex_t*) * ++flareTexturesCount);
    flareTextures[flareTexturesCount-1] = fTex;

    return fTex;
}

flaretex_t* R_GetFlareTexture(const dduri_t* uri)
{
    if(uri && Str_CompareIgnoreCase(Uri_Path(uri), "-"))
    {
        int i;
        for(i = 0; i < flareTexturesCount; ++i)
        {
            flaretex_t* fTex = flareTextures[i];

            if(!fTex->external) continue;

            if(Uri_Equality(fTex->external, uri))
                return fTex;
        }
    }
    return 0;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyFlareTextures(void)
{
    { int i;
    for(i = 0; i < flareTexturesCount; ++i)
    {
        M_Free(flareTextures[i]);
    }}

    if(flareTextures)
        M_Free(flareTextures);
    flareTextures = NULL;
    flareTexturesCount = 0;
}

shinytex_t* R_CreateShinyTexture(const dduri_t* uri)
{
    const texture_t* glTex;
    shinytex_t* sTex;
    char name[9];

    // Have we already created one for this?
    if(NULL != (sTex = R_FindShinyTextureForName(uri)))
        return sTex;

    if(M_NumDigits(shinyTexturesCount + 1) > 8)
    {
        Con_Message("Warning: Failed to create new shiny texture (max:%i).\n", DDMAXINT);
        return 0;
    }

    /**
     * A new shiny texture.
     */

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, shinyTexturesCount + 1);

    glTex = GL_CreateTexture(name, shinyTexturesCount, TN_REFLECTIONS);

    sTex = M_Malloc(sizeof(*sTex));
    sTex->id = Texture_Id(glTex);
    sTex->external = uri;

    // Add it to the list.
    shinyTextures = M_Realloc(shinyTextures, sizeof(shinytex_t*) * ++shinyTexturesCount);
    shinyTextures[shinyTexturesCount-1] = sTex;

    return sTex;
}

shinytex_t* R_FindShinyTextureForName(const dduri_t* uri)
{
    if(uri && !Str_IsEmpty(Uri_Path(uri)))
    {
        int i;
        for(i = 0; i < shinyTexturesCount; ++i)
        {
            shinytex_t* sTex = shinyTextures[i];

            if(!sTex->external) continue;

            if(Uri_Equality(sTex->external, uri))
                return sTex;
        }
    }
    return 0;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyShinyTextures(void)
{
    { int i;
    for(i = 0; i < shinyTexturesCount; ++i)
    {
        M_Free(shinyTextures[i]);
    }}

    if(shinyTextures)
        M_Free(shinyTextures);
    shinyTextures = NULL;
    shinyTexturesCount = 0;
}

masktex_t* R_CreateMaskTexture(const dduri_t* uri, int width, int height)
{
    const texture_t* glTex;
    masktex_t* mTex;
    char name[9];

    // Have we already created one for this?
    if(NULL != (mTex = R_FindMaskTextureForName(uri)))
        return mTex;

    if(M_NumDigits(maskTexturesCount + 1) > 8)
    {
        Con_Message("Warning, failed to create mask image (max:%i).\n", DDMAXINT);
        return 0;
    }

    /**
     * A new shiny texture.
     */

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, maskTexturesCount + 1);

    glTex = GL_CreateTexture2(name, maskTexturesCount, TN_MASKS, width, height);
    if(NULL == glTex)
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, failed creating Texture for mask image: %s\n", F_PrettyPath(Str_Text(path)));
        Str_Delete(path);
        return 0;
    }

    mTex = M_Malloc(sizeof(*mTex));
    mTex->id = Texture_Id(glTex);
    mTex->external = uri;

    // Add it to the list.
    maskTextures = M_Realloc(maskTextures, sizeof(masktex_t*) * ++maskTexturesCount);
    maskTextures[maskTexturesCount-1] = mTex;

    return mTex;
}

masktex_t* R_FindMaskTextureForName(const dduri_t* uri)
{
    if(uri && !Str_IsEmpty(Uri_Path(uri)))
    {
        int i;
        for(i = 0; i < maskTexturesCount; ++i)
        {
            masktex_t* mTex = maskTextures[i];

            if(!mTex->external) continue;

            if(Uri_Equality(mTex->external, uri))
                return mTex;
        }
    }
    return 0;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyMaskTextures(void)
{
    { int i;
    for(i = 0; i < maskTexturesCount; ++i)
    {
        M_Free(maskTextures[i]);
    }}

    if(maskTextures)
        M_Free(maskTextures);
    maskTextures = NULL;
    maskTexturesCount = 0;
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
