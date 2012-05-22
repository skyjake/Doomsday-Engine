/**\file r_data.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
#include "texture.h"
#include "materialvariant.h"
#include "texturevariant.h"
#include "font.h"

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

static rawtexhash_t rawtexhash[RAWTEX_HASH_SIZE];

static unsigned int numrendpolys = 0;
static unsigned int maxrendpolys = 0;
static rendpolydata_t** rendPolys;

static boolean initedColorPalettes = false;

static int numColorPalettes;
static colorpalette_t** colorPalettes;
static int numColorPaletteBinds;
static colorpalettebind_t* colorPaletteBinds;

static int numgroups;
static animgroup_t* groups;

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
        GL_ReleaseTexturesByColorPalette(id);
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
    ColorRawf*           rcolors;
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
 * Retrieves a batch of ColorRawf.
 * Possibly allocates new if necessary.
 *
 * @param num           The number of verts required.
 *
 * @return              Ptr to array of ColorRawf
 */
ColorRawf* R_AllocRendColors(uint num)
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
            return (ColorRawf*) rendPolys[idx]->data;
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
        Z_Malloc(sizeof(ColorRawf) * num, PU_MAP, 0);

    return (ColorRawf*) rendPolys[idx]->data;
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
    uint i;

    if(!rvertices) return;

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
 * @param vertices      Ptr to array of ColorRawf to mark unused.
 */
void R_FreeRendColors(ColorRawf* rcolors)
{
    uint i;

    if(!rcolors) return;

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
    uint i;

    if(!rtexcoords) return;

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

void Rtu_Init(rtexmapunit_t* rtu)
{
    assert(rtu);
    rtu->texture.gl.name = 0;
    rtu->texture.gl.magMode = GL_LINEAR;
    rtu->texture.flags = 0;
    rtu->blendMode = BM_NORMAL;
    rtu->opacity = 1;
    rtu->scale[0] = rtu->scale[1] = 1;
    rtu->offset[0] = rtu->offset[1] = 0;
}

boolean Rtu_HasTexture(const rtexmapunit_t* rtu)
{
    if(rtu->texture.flags & TUF_TEXTURE_IS_MANAGED)
        return TextureVariant_GLName(rtu->texture.variant) != 0;
    return rtu->texture.gl.name != 0;
}

void Rtu_SetScale(rtexmapunit_t* rtu, float s, float t)
{
    assert(rtu);
    rtu->scale[0] = s;
    rtu->scale[1] = t;
}

void Rtu_SetScalev(rtexmapunit_t* rtu, float const st[2])
{
    assert(st);
    Rtu_SetScale(rtu, st[0], st[0]);
}

void Rtu_Scale(rtexmapunit_t* rtu, float scalar)
{
    assert(rtu);
    rtu->scale[0] *= scalar;
    rtu->scale[1] *= scalar;
    rtu->offset[0] *= scalar;
    rtu->offset[1] *= scalar;
}

void Rtu_ScaleST(rtexmapunit_t* rtu, float const scalarST[2])
{
    assert(rtu);
    rtu->scale[0] *= scalarST[0];
    rtu->scale[1] *= scalarST[1];
    rtu->offset[0] *= scalarST[0];
    rtu->offset[1] *= scalarST[1];
}

void Rtu_SetOffset(rtexmapunit_t* rtu, float s, float t)
{
    assert(rtu);
    rtu->offset[0] = s;
    rtu->offset[1] = t;
}

void Rtu_SetOffsetv(rtexmapunit_t* rtu, float const st[2])
{
    assert(st);
    Rtu_SetOffset(rtu, st[0], st[1]);
}

void Rtu_TranslateOffset(rtexmapunit_t* rtu, float s, float t)
{
    assert(rtu);
    rtu->offset[0] += s;
    rtu->offset[1] += t;
}

void Rtu_TranslateOffsetv(rtexmapunit_t* rtu, float const st[2])
{
    assert(st);
    Rtu_TranslateOffset(rtu, st[0], st[1]);
}

void R_DivVerts(rvertex_t* dst, const rvertex_t* src, const walldivs_t* leftDivs,
    const walldivs_t* rightDivs)
{
#define COPYVERT(d, s)  (d)->pos[VX] = (s)->pos[VX]; \
    (d)->pos[VY] = (s)->pos[VY]; \
    (d)->pos[VZ] = (s)->pos[VZ];

    uint i, numL, numR;

    assert(leftDivs && rightDivs);

    numL = 1 + leftDivs->num;
    numR = 1 + rightDivs->num;

    // Right fan:
    COPYVERT(&dst[numL + 0], &src[0])
    COPYVERT(&dst[numL + 1], &src[3]);
    COPYVERT(&dst[numL + numR - 1], &src[2]);

    for(i = 0; i < rightDivs->num - 2; ++i)
    {
        dst[numL + 2 + i].pos[VX] = src[2].pos[VX];
        dst[numL + 2 + i].pos[VY] = src[2].pos[VY];
        dst[numL + 2 + i].pos[VZ] = rightDivs->nodes[rightDivs->num-1 - 1 - i].height;
    }

    // Left fan:
    COPYVERT(&dst[0], &src[3]);
    COPYVERT(&dst[1], &src[0]);
    COPYVERT(&dst[numL - 1], &src[1]);

    for(i = 0; i < leftDivs->num - 2; ++i)
    {
        dst[2 + i].pos[VX] = src[0].pos[VX];
        dst[2 + i].pos[VY] = src[0].pos[VY];
        dst[2 + i].pos[VZ] = leftDivs->nodes[1 + i].height;
    }

#undef COPYVERT
}

void R_DivTexCoords(rtexcoord_t* dst, const rtexcoord_t* src, const walldivs_t* leftDivs,
    const walldivs_t* rightDivs, float bL, float tL, float bR, float tR)
{
#define COPYTEXCOORD(d, s)    (d)->st[0] = (s)->st[0]; \
    (d)->st[1] = (s)->st[1];

    uint i, numL, numR;
    float height;

    assert(leftDivs && rightDivs);

    numL = 1 + leftDivs->num;
    numR = 1 + rightDivs->num;

    // Right fan:
    COPYTEXCOORD(&dst[numL + 0], &src[0]);
    COPYTEXCOORD(&dst[numL + 1], &src[3]);
    COPYTEXCOORD(&dst[numL + numR-1], &src[2]);

    height = tR - bR;
    for(i = 0; i < rightDivs->num - 2; ++i)
    {
        float inter = (rightDivs->nodes[rightDivs->num-1 - 1 - i].height - bR) / height;

        dst[numL + 2 + i].st[0] = src[3].st[0];
        dst[numL + 2 + i].st[1] = src[2].st[1] +
            (src[3].st[1] - src[2].st[1]) * inter;
    }

    // Left fan:
    COPYTEXCOORD(&dst[0], &src[3]);
    COPYTEXCOORD(&dst[1], &src[0]);
    COPYTEXCOORD(&dst[numL - 1], &src[1]);

    height = tL - bL;
    for(i = 0; i < leftDivs->num - 2; ++i)
    {
        float inter = (leftDivs->nodes[1 + i].height - bL) / height;

        dst[2 + i].st[0] = src[0].st[0];
        dst[2 + i].st[1] = src[0].st[1] +
            (src[1].st[1] - src[0].st[1]) * inter;
    }

#undef COPYTEXCOORD
}

void R_DivVertColors(ColorRawf* dst, const ColorRawf* src, const walldivs_t* leftDivs,
    const walldivs_t* rightDivs, float bL, float tL, float bR, float tR)
{
#define COPYVCOLOR(d, s)    (d)->rgba[CR] = (s)->rgba[CR]; \
    (d)->rgba[CG] = (s)->rgba[CG]; \
    (d)->rgba[CB] = (s)->rgba[CB]; \
    (d)->rgba[CA] = (s)->rgba[CA];

    uint i, numL, numR = 3 + (rightDivs->num-2);
    float height;

    assert(leftDivs && rightDivs);

    numL = 1 + leftDivs->num;
    numR = 1 + rightDivs->num;

    // Right fan:
    COPYVCOLOR(&dst[numL + 0], &src[0]);
    COPYVCOLOR(&dst[numL + 1], &src[3]);
    COPYVCOLOR(&dst[numL + numR-1], &src[2]);

    height = tR - bR;
    for(i = 0; i < rightDivs->num - 2; ++i)
    {
        uint c;
        float inter = (rightDivs->nodes[rightDivs->num-1 - 1 - i].height - bR) / height;

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
    for(i = 0; i < leftDivs->num - 2; ++i)
    {
        uint c;
        float inter = (leftDivs->nodes[1 + i].height - bL) / height;

        for(c = 0; c < 4; ++c)
        {
            dst[2 + i].rgba[c] = src[0].rgba[c] +
                (src[1].rgba[c] - src[0].rgba[c]) * inter;
        }
    }

#undef COPYVCOLOR
}

/// \note Part of the Doomsday public API.
int R_TextureUniqueId2(const Uri* uri, boolean quiet)
{
    textureid_t texId = Textures_ResolveUri2(uri, quiet);
    if(texId != NOTEXTUREID)
    {
        return Textures_UniqueId(texId);
    }
    if(!quiet)
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning: Unknown Texture \"%s\"\n", Str_Text(path));
        Str_Delete(path);
    }
    return -1;
}

/// \note Part of the Doomsday public API.
int R_TextureUniqueId(const Uri* uri)
{
    return R_TextureUniqueId2(uri, false);
}

void R_InitSystemTextures(void)
{
    static const struct texdef_s {
        const char* texPath;
        const char* resourcePath; ///< Percent-encoded.
    } defs[] = {
        { "unknown", GRAPHICS_RESOURCE_NAMESPACE_NAME":unknown" },
        { "missing", GRAPHICS_RESOURCE_NAMESPACE_NAME":missing" },
        { "bbox",    GRAPHICS_RESOURCE_NAMESPACE_NAME":bbox" },
        { "gray",    GRAPHICS_RESOURCE_NAMESPACE_NAME":gray" },
        { NULL, NULL }
    };
    Uri* uri = Uri_New(), *resourcePath = Uri_New();
    textureid_t texId;
    Texture* tex;
    uint i;

    VERBOSE( Con_Message("Initializing System textures...\n") )

    Uri_SetScheme(uri, TN_SYSTEM_NAME);
    for(i = 0; defs[i].texPath; ++i)
    {
        Uri_SetPath(uri, defs[i].texPath);
        Uri_SetUri2(resourcePath, defs[i].resourcePath);

        texId = Textures_Declare(uri, i+1/*1-based index*/, resourcePath);
        if(texId == NOTEXTUREID) continue; // Invalid uri?

        // Have we defined this yet?
        tex = Textures_ToTexture(texId);
        if(!tex && !Textures_Create(texId, TXF_CUSTOM, NULL))
        {
            ddstring_t* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining Texture for System texture \"%s\"\n", Str_Text(path));
            Str_Delete(path);
        }
    }
    Uri_Delete(resourcePath);
    Uri_Delete(uri);
}

static textureid_t findPatchTextureIdByName(const char* encodedName)
{
    textureid_t texId;
    Uri* uri;
    assert(encodedName && encodedName[0]);

    uri = Uri_NewWithPath2(encodedName, RC_NULL);
    Uri_SetScheme(uri, TN_PATCHES_NAME);
    texId = Textures_ResolveUri2(uri, true/*quiet please*/);
    Uri_Delete(uri);
    return texId;
}

/// \note Part of the Doomsday public API.
patchid_t R_DeclarePatch(const char* name)
{
    const doompatch_header_t* patch;
    abstractfile_t* fsObject;
    Uri* uri, *resourcePath;
    int lumpIdx, flags;
    ddstring_t encodedName;
    lumpnum_t lumpNum;
    textureid_t texId;
    Texture* tex;
    int uniqueId;
    patchtex_t* p;

    if(!name || !name[0])
    {
#if _DEBUG
        Con_Message("Warning:R_DeclarePatch: Invalid 'name' argument, ignoring.\n");
#endif
        return 0;
    }

    Str_Init(&encodedName);
    Str_PercentEncode(Str_Set(&encodedName, name));

    // Already defined as a patch?
    texId = findPatchTextureIdByName(Str_Text(&encodedName));
    if(texId)
    {
        Str_Free(&encodedName);
        /// \todo We should instead define Materials from patches and return the material id.
        return (patchid_t)Textures_UniqueId(texId);
    }

    lumpNum = F_CheckLumpNumForName2(name, true);
    if(lumpNum < 0)
    {
#if _DEBUG
        Con_Message("Warning:R_DeclarePatch: Failed to locate lump for patch '%s'.\n", name);
#endif
        return 0;
    }

    // Compose the resource name
    uri = Uri_NewWithPath2(TN_PATCHES_NAME":", RC_NULL);
    Uri_SetPath(uri, Str_Text(&encodedName));
    Str_Free(&encodedName);

    // Compose the path to the data resource.
    resourcePath = Uri_NewWithPath2("Lumps:", RC_NULL);
    Uri_SetPath(resourcePath, F_LumpName(lumpNum));

    uniqueId = Textures_Count(TN_PATCHES)+1; // 1-based index.
    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return 0; // Invalid uri?

    // Generate a new patch.
    p = (patchtex_t*)malloc(sizeof *p);
    if(!p)
        Con_Error("R_DeclarePatch: Failed on allocation of %lu bytes for new PatchTex.", (unsigned long) sizeof *p);
    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    p->flags = 0;
    if(monochrome)               p->flags |= PF_MONOCHROME;
    if(upscaleAndSharpenPatches) p->flags |= PF_UPSCALE_AND_SHARPEN;

    /**
     * @todo: Cannot be sure this is in Patch format until a load attempt
     * is made. We should not read this info here!
     */
    fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    patch = (const doompatch_header_t*) F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);
    p->offX = -SHORT(patch->leftOffset);
    p->offY = -SHORT(patch->topOffset);

    flags = 0;
    if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        Size2Raw size;
        size.width  = SHORT(patch->width);
        size.height = SHORT(patch->height);
        tex = Textures_CreateWithSize(texId, flags, &size, (void*)p);
        F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);

        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for Patch texture \"%s\"\n", name);
            free(p);
            return 0;
        }
    }
    else
    {
        patchtex_t* oldPatch = Texture_DetachUserData(tex);
        Size2Raw size;

        size.width  = SHORT(patch->width);
        size.height = SHORT(patch->height);

        Texture_SetFlags(tex, flags);
        Texture_SetSize(tex, &size);
        Texture_AttachUserData(tex, (void*)p);

        free(oldPatch);

        F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);
    }

    return uniqueId;
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info)
{
    Texture* tex;
    if(!info)
        Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");

    memset(info, 0, sizeof *info);
    tex = Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, id));
    if(tex)
    {
        const patchtex_t* pTex = (patchtex_t*)Texture_UserData(tex);
        assert(tex);

        // Ensure we have up to date information about this patch.
        GL_PreparePatchTexture(tex);

        info->id = id;
        info->flags.isCustom = Texture_IsCustom(tex);

        { averagealpha_analysis_t* aa = (averagealpha_analysis_t*)Texture_Analysis(tex, TA_ALPHA);
        info->flags.isEmpty = aa && FEQUAL(aa->alpha, 0);
        }

        info->geometry.size.width = Texture_Width(tex);
        info->geometry.size.height = Texture_Height(tex);
        info->geometry.origin.x = pTex->offX;
        info->geometry.origin.y = pTex->offY;
        /// @kludge:
        info->extraOffset[0] = info->extraOffset[1] = (pTex->flags & PF_UPSCALE_AND_SHARPEN)? -1 : 0;
        // Kludge end.
        return true;
    }
    if(id != 0)
    {
#if _DEBUG
        Con_Message("Warning:R_GetPatchInfo: Invalid Patch id #%i.\n", id);
#endif
    }
    return false;
}

/// \note Part of the Doomsday public API.
Uri* R_ComposePatchUri(patchid_t id)
{
    return Textures_ComposeUri(Textures_TextureForUniqueId(TN_PATCHES, id));
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
    rawtex_t* i;

    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
#if _DEBUG
        Con_Message("Warning:R_FindRawTex: LumpNum #%i out of bounds (%i), returning NULL.\n",
                lumpNum, F_LumpCount());
#endif
        return NULL;
    }

    for(i = RAWTEX_HASH(lumpNum)->first; i; i = i->next)
    {
        if(i->lumpNum == lumpNum)
            return i;
    }
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
    if(r) return r;

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
    abstractfile_t* file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    size_t lumpSize = F_LumpLength(lumpNum);
    patchname_t* names, *name;
    const uint8_t* lump;
    int i, numNames;

    if(lumpSize < 4)
    {
        ddstring_t* path = F_ComposeLumpPath(file, lumpIdx);
        Con_Message("Warning:loadPatchNames: \"%s\"(#%i) is not valid PNAMES data.\n",
                    F_PrettyPath(Str_Text(path)), lumpNum);
        Str_Delete(path);

        if(num) *num = 0;
        return NULL;
    }

    lump = F_CacheLump(file, lumpIdx, PU_APPSTATIC);
    numNames = LONG(*((const int*) lump));
    if(numNames <= 0)
    {
        F_CacheChangeTag(file, lumpIdx, PU_CACHE);

        if(num) *num = 0;
        return NULL;
    }

    if((unsigned)numNames > (lumpSize - 4) / 8)
    {
        // Lump is truncated.
        ddstring_t* path = F_ComposeLumpPath(file, lumpIdx);
        Con_Message("Warning:loadPatchNames: Patch '%s'(#%i) is truncated (%lu bytes, expected %lu).\n",
                    F_PrettyPath(Str_Text(path)), lumpNum, (unsigned long) lumpSize,
                    (unsigned long) (numNames * 8 + 4));
        Str_Delete(path);

        numNames = (int) ((lumpSize - 4) / 8);
    }

    names = (patchname_t*) calloc(1, sizeof(*names) * numNames);
    if(!names)
        Con_Error("loadPatchNames: Failed on allocation of %lu bytes for patch name list.", (unsigned long) sizeof(*names) * numNames);

    name = names;
    for(i = 0; i < numNames; ++i)
    {
        /// @todo Some filtering of invalid characters wouldn't go amiss...
        strncpy(*name, (const char*) (lump + 4 + i * 8), 8);
        name++;
    }

    F_CacheChangeTag(file, lumpIdx, PU_CACHE);

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

    patchInfo = (patchinfo_t*)calloc(1, sizeof(*patchInfo) * numPatchNames);
    if(!patchInfo)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for patch info.", (unsigned long) sizeof *patchInfo * numPatchNames);

    lumpSize = F_LumpLength(lumpNum);
    maptex1 = (int*) malloc(lumpSize);
    if(!maptex1)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for temporary copy of archived DOOM texture definitions.", (unsigned long) lumpSize);

    fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)maptex1, 0, lumpSize);

    numTexDefs = LONG(*maptex1);

    VERBOSE(
        ddstring_t* path = F_ComposeLumpPath(fsObject, lumpIdx);
        Con_Message("  Processing \"%s\"...\n", F_PrettyPath(Str_Text(path)));
        Str_Delete(path);
    )

    validTexDefs = (byte*) calloc(1, sizeof(*validTexDefs) * numTexDefs);
    if(!validTexDefs)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for valid texture record list.", (unsigned long) sizeof(*validTexDefs) * numTexDefs);

    texDefNumPatches = (short*) calloc(1, sizeof(*texDefNumPatches) * numTexDefs);
    if(!texDefNumPatches)
        Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for texture patch count record list.", (unsigned long) sizeof(*texDefNumPatches) * numTexDefs);

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
            ddstring_t* path = F_ComposeLumpPath(fsObject, lumpIdx);
            Con_Message("Warning: Invalid offset %lu for definition %i in \"%s\", ignoring.\n", (unsigned long) offset, i, F_PrettyPath(Str_Text(path)));
            Str_Delete(path);
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
                        Con_Message("Warning: Invalid Patch %i in texture definition \"%s\", ignoring.\n", (int) patchNum, mtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = F_CheckLumpNumForName2(*(patchNames + patchNum), true);
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning: Failed to locate Patch \"%s\", ignoring.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning: Missing Patch %i in texture definition \"%s\", ignoring.\n", (int) j, mtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning: Invalid patch count %i in texture definition \"%s\", ignoring.\n", (int) patchCount, mtexture->name);
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
                        Con_Message("Warning: Invalid Patch #%i in texture definition \"%s\", ignoring.\n", (int) patchNum, smtexture->name);
                        continue;
                    }
                    pinfo = patchInfo + patchNum;

                    if(!pinfo->flags.processed)
                    {
                        pinfo->lumpNum = F_CheckLumpNumForName2(*(patchNames + patchNum), true);
                        pinfo->flags.processed = true;
                        if(-1 == pinfo->lumpNum)
                        {
                            Con_Message("Warning: Failed locating Patch \"%s\", ignoring.\n", *(patchNames + patchNum));
                        }
                    }

                    if(-1 == pinfo->lumpNum)
                    {
                        Con_Message("Warning: Missing patch #%i in texture definition \"%s\", ignoring.\n", (int) j, smtexture->name);
                        continue;
                    }
                    ++n;
                }
            }
            else
            {
                Con_Message("Warning: Invalid patch count %i in texture definition \"%s\", ignoring.\n", (int) patchCount, smtexture->name);
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
        texDefs = (patchcompositetex_t**)malloc(sizeof(*texDefs) * numValidTexDefs);
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

                texDef = (patchcompositetex_t*) malloc(sizeof(*texDef));
                if(!texDef)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

                texDef->patchCount = texDefNumPatches[i];
                texDef->flags = 0;
                texDef->origIndex = (*origIndexBase) + i;

                Str_Init(&texDef->name);
                Str_PercentEncode(Str_StripRight(Str_PartAppend(&texDef->name, (const char*) mtexture->name, 0, 8)));

                texDef->size.width = SHORT(mtexture->width);
                texDef->size.height = SHORT(mtexture->height);

                texDef->patches = (texpatch_t*)malloc(sizeof(*texDef->patches) * texDef->patchCount);
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

                texDef = (patchcompositetex_t*) malloc(sizeof(*texDef));
                if(!texDef)
                    Con_Error("R_ReadTextureDefs: Failed on allocation of %lu bytes for new PatchComposite.", (unsigned long) sizeof *texDef);

                texDef->patchCount = texDefNumPatches[i];
                texDef->flags = 0;
                texDef->origIndex = (*origIndexBase) + i;

                Str_Init(&texDef->name);
                Str_PercentEncode(Str_StripRight(Str_PartAppend(&texDef->name, (const char*) smtexture->name, 0, 8)));

                texDef->size.width = SHORT(smtexture->width);
                texDef->size.height = SHORT(smtexture->height);

                texDef->patches = (texpatch_t*)malloc(sizeof(*texDef->patches) * texDef->patchCount);
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
        const char* lumpName;

        // Will this be processed anyway?
        if(i == firstTexLump || i == secondTexLump) continue;

        lumpName = F_LumpName(i);
        if(strnicmp(lumpName, "TEXTURE1", 8) && strnicmp(lumpName, "TEXTURE2", 8)) continue;

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
                    else if(custom->size.height == orig->size.height &&
                            custom->size.width  == orig->size.width  &&
                            custom->patchCount  == orig->patchCount)
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
                Str_Free(&orig->name);
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
    textureid_t texId;
    Texture* tex;
    int i;
    assert(defs);

    Uri_SetScheme(uri, TN_TEXTURES_NAME);
    for(i = 0; i < count; ++i)
    {
        patchcompositetex_t* pcTex = defs[i];
        int flags = 0;

        Uri_SetPath(uri, Str_Text(&pcTex->name));

        texId = Textures_Declare(uri, pcTex->origIndex, NULL);
        if(texId == NOTEXTUREID) continue; // Invalid uri?

        if(pcTex->flags & TXDF_CUSTOM) flags |= TXF_CUSTOM;

        tex = Textures_ToTexture(texId);
        if(tex)
        {
            patchcompositetex_t* oldPcTex = Texture_DetachUserData(tex);

            Texture_SetFlags(tex, flags);
            Texture_SetSize(tex, &pcTex->size);
            Texture_AttachUserData(tex, (void*)pcTex);

            Str_Free(&oldPcTex->name);
            if(oldPcTex->patches) free(oldPcTex->patches);
            free(oldPcTex);
        }
        else if(!Textures_CreateWithSize(texId, flags, &pcTex->size, (void*)pcTex))
        {
            Con_Message("Warning: Failed defining Texture for new patch composite '%s', ignoring.\n", Str_Text(&pcTex->name));
            Str_Free(&pcTex->name);
            if(pcTex->patches) free(pcTex->patches);
            free(pcTex);
            defs[i] = NULL;
        }
    }
    Uri_Delete(uri);
}

void R_InitPatchComposites(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    patchcompositetex_t** defs;
    int defsCount;

    VERBOSE( Con_Message("Initializing PatchComposite textures...\n") )

    // Load texture definitions from TEXTURE1/2 lumps.
    defs = loadPatchCompositeDefs(&defsCount);
    if(defs)
    {
        createTexturesForPatchCompositeDefs(defs, defsCount);
        free(defs);
    }

    VERBOSE2( Con_Message("R_InitPatchComposites: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

static Texture* createFlatForLump(lumpnum_t lumpNum, int uniqueId)
{
    Uri* uri, *resourcePath;
    ddstring_t flatName;
    textureid_t texId;
    Texture* tex;
    Size2Raw size;
    char name[9];
    int flags;

    // We can only perform some basic filtering of lumps at this time.
    if(F_LumpLength(lumpNum) == 0) return NULL;

    // Compose the resource name.
    Str_Init(&flatName);
    F_FileName(&flatName, F_LumpName(lumpNum));

    uri = Uri_NewWithPath2(TN_FLATS_NAME":", RC_NULL);
    Uri_SetPath(uri, Str_Text(&flatName));
    Str_Free(&flatName);

    // Compose the path to the data resource.
    // Note that we do not use the lump name and instead use the logical lump index
    // in the global LumpDirectory. This is necessary because of the way id tech 1
    // manages flat references in animations (intermediate frames are chosen by their
    // 'original indices' rather than by name).
    dd_snprintf(name, 9, "%i", lumpNum);
    resourcePath = Uri_NewWithPath2("LumpDir:", RC_NULL);
    Uri_SetPath(resourcePath, name);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(resourcePath);
    if(texId == NOTEXTUREID)
    {
        Uri_Delete(uri);
        return NULL; // Invalid uri?
    }

    // Have we already encountered this name?
    tex = Textures_ToTexture(texId);
    if(tex)
    {
        flags = 0;
        if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;
        Texture_SetFlags(tex, flags);
    }
    else
    {
        // A new flat.
        flags = 0;
        if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;

        /**
         * \kludge Assume 64x64 else when the flat is loaded it will inherit the
         * dimensions of the texture, which, if it has been replaced with a hires
         * version - will be much larger than it should be.
         *
         * \todo Always determine size from the lowres original.
         */
        size.width = size.height = 64;
        tex = Textures_CreateWithSize(texId, flags, &size, NULL);
        if(!tex)
        {
            ddstring_t* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining Texture for new flat '%s', ignoring.\n", Str_Text(path));
            Str_Delete(path);
            Uri_Delete(uri);
            return NULL;
        }
    }
    Uri_Delete(uri);

    return tex;
}

void R_InitFlatTextures(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    lumpnum_t origRangeFrom, origRangeTo;

    VERBOSE( Con_Message("Initializing Flat textures...\n") )

    origRangeFrom = W_CheckLumpNumForName2("F_START", true/*quiet please*/);
    origRangeTo   = W_CheckLumpNumForName2("F_END", true/*quiet please*/);

    if(origRangeFrom >= 0 && origRangeTo >= 0)
    {
        int i, numLumps, origIndexBase = 0;
        ddstack_t* stack;
        Texture* tex;
        lumpnum_t n;

        // First add all flats between all flat marker lumps exclusive of the
        // range defined by the last marker lumps.
        origIndexBase = (int)origRangeTo;
        stack = Stack_New();
        numLumps = F_LumpCount();
        for(i = 0; i < numLumps; ++i)
        {
            const char* lumpName = F_LumpName(i);

            if(lumpName[0] == 'F' && strlen(lumpName) >= 5)
            {
                if(!strnicmp(lumpName + 1, "_START", 6) ||
                   !strnicmp(lumpName + 2, "_START", 6))
                {
                    // We've arrived at *a* sprite block.
                    Stack_Push(stack, NULL);
                    continue;
                }
                else if(!strnicmp(lumpName + 1, "_END", 4) ||
                        !strnicmp(lumpName + 2, "_END", 4))
                {
                    // The sprite block ends.
                    Stack_Pop(stack);
                    continue;
                }
            }

            if(!Stack_Height(stack)) continue;
            if(i >= origRangeFrom && i <= origRangeTo) continue;

            tex = createFlatForLump(i, origIndexBase);
            if(tex) origIndexBase++;
        }

        while(Stack_Height(stack)) { Stack_Pop(stack); }
        Stack_Delete(stack);

        // Now add all lumps in the primary (overridding) range.
        origRangeFrom += 1; // Skip over marker lump.
        for(n = origRangeFrom; n < origRangeTo; ++n)
        {
            createFlatForLump(n, (int)(n - origRangeFrom));
        }
    }

    VERBOSE2( Con_Message("R_InitFlatTextures: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

static boolean validSpriteName(const ddstring_t* name)
{
    if(!name || Str_Length(name) < 5) return false;
    if(!Str_At(name, 4) || !Str_At(name, 5) || (Str_At(name, 6) && !Str_At(name, 7))) return false;
    // Indices 5 and 7 must be numbers (0-8).
    if(Str_At(name, 5) < '0' || Str_At(name, 5) > '8') return false;
    if(Str_At(name, 7) && (Str_At(name, 7) < '0' || Str_At(name, 7) > '8')) return false;
    // Its good!
    return true;
}

void R_DefineSpriteTexture(textureid_t texId)
{
    const Uri* resourceUri = Textures_ResourcePath(texId);
    Texture* tex = Textures_ToTexture(texId);

    // Have we already encountered this name?
    if(!tex)
    {
        // A new sprite texture.
        patchtex_t* pTex = (patchtex_t*)malloc(sizeof *pTex);
        if(!pTex)
            Con_Error("R_InitSpriteTextures: Failed on allocation of %lu bytes for new PatchTex.", (unsigned long) sizeof *pTex);
        pTex->offX = pTex->offY = 0; // Deferred until texture load time.

        tex = Textures_Create(texId, 0, (void*)pTex);
        if(!tex)
        {
            Uri* uri = Textures_ComposeUri(texId);
            ddstring_t* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining Texture for \"%s\", ignoring.\n", Str_Text(path));
            Str_Delete(path);
            Uri_Delete(uri);
            free(pTex);
        }
    }

    if(tex && resourceUri)
    {
        ddstring_t* resourcePath = Uri_Resolved(resourceUri);
        lumpnum_t lumpNum = F_CheckLumpNumForName2(Str_Text(resourcePath), true/*quiet please*/);
        int lumpIdx;
        abstractfile_t* file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
        const doompatch_header_t* patch = (const doompatch_header_t*) F_CacheLump(file, lumpIdx, PU_APPSTATIC);
        Size2Raw size;
        int flags;

        size.width  = SHORT(patch->width);
        size.height = SHORT(patch->height);
        Texture_SetSize(tex, &size);

        flags = 0;
        if(F_LumpIsCustom(lumpNum)) flags |= TXF_CUSTOM;
        Texture_SetFlags(tex, flags);

        F_CacheChangeTag(file, lumpIdx, PU_CACHE);
        Str_Delete(resourcePath);
    }
}

int RIT_DefineSpriteTexture(textureid_t texId, void* paramaters)
{
    R_DefineSpriteTexture(texId);
    return 0; // Continue iteration.
}

/// @todo Defer until necessary (sprite is first de-referenced).
static void defineAllSpriteTextures(void)
{
    Textures_IterateDeclared(TN_SPRITES, RIT_DefineSpriteTexture);
}

void R_InitSpriteTextures(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);
    int i, numLumps, uniqueId = 1/*1-based index*/;
    ddstring_t spriteName, decodedSpriteName;
    Uri* uri, *resourcePath;
    ddstack_t* stack;

    VERBOSE( Con_Message("Initializing Sprite textures...\n") )

    uri = Uri_NewWithPath2(TN_SPRITES_NAME":", RC_NULL);
    resourcePath = Uri_NewWithPath2("Lumps:", RC_NULL);

    Str_Init(&spriteName);
    Str_Init(&decodedSpriteName);

    stack = Stack_New();
    numLumps = F_LumpCount();

    /// @todo Order here does not respect id tech1 logic.
    for(i = 0; i < numLumps; ++i)
    {
        const char* lumpName = F_LumpName((lumpnum_t)i);
        textureid_t texId;

        if(lumpName[0] == 'S' && strlen(lumpName) >= 5)
        {
            if(!strnicmp(lumpName + 1, "_START", 6) ||
               !strnicmp(lumpName + 2, "_START", 6))
            {
                // We've arrived at *a* sprite block.
                Stack_Push(stack, NULL);
                continue;
            }
            else if(!strnicmp(lumpName + 1, "_END", 4) ||
                    !strnicmp(lumpName + 2, "_END", 4))
            {
                // The sprite block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack)) continue;

        F_FileName(&spriteName, lumpName);

        Str_Set(&decodedSpriteName, Str_Text(&spriteName));
        Str_PercentDecode(&decodedSpriteName);
        if(!validSpriteName(&decodedSpriteName)) continue;

        // Compose the resource name.
        Uri_SetPath(uri, Str_Text(&spriteName));

        // Compose the data resource path.
        Uri_SetPath(resourcePath, lumpName);

        texId = Textures_Declare(uri, uniqueId, resourcePath);
        if(texId == NOTEXTUREID) continue; // Invalid uri?
        uniqueId++;
    }

    while(Stack_Height(stack)) { Stack_Pop(stack); }
    Stack_Delete(stack);

    Uri_Delete(resourcePath);
    Uri_Delete(uri);
    Str_Free(&decodedSpriteName);
    Str_Free(&spriteName);

    // Define any as yet undefined sprite textures.
    defineAllSpriteTextures();

    VERBOSE2( Con_Message("R_InitSpriteTextures: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) )
}

Texture* R_CreateSkinTex(const Uri* filePath, boolean isShinySkin)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!filePath || Str_IsEmpty(Uri_Path(filePath))) return 0;

    // Have we already created one for this?
    if(!isShinySkin)
    {
        tex = R_FindModelSkinForResourcePath(filePath);
    }
    else
    {
        tex = R_FindModelReflectionSkinForResourcePath(filePath);
    }
    if(tex) return tex;

    uniqueId = Textures_Count(isShinySkin? TN_MODELREFLECTIONSKINS : TN_MODELSKINS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
#if _DEBUG
        Con_Message("Warning: Failed creating SkinName (max:%i), ignoring.\n", DDMAXINT);
#endif
        return NULL;
    }

    dd_snprintf(name, 9, "%-*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, (isShinySkin? TN_MODELREFLECTIONSKINS_NAME : TN_MODELSKINS_NAME));

    texId = Textures_Declare(uri, uniqueId, filePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, TXF_CUSTOM, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for ModelSkin '%s', ignoring.\n", name);
            return NULL;
        }
    }

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

Texture* R_RegisterModelSkin(ddstring_t* foundPath, const char* skin, const char* modelfn, boolean isShinySkin)
{
    Texture* tex = NULL;
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

static int findModelSkinForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindModelSkinForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;
    result = Textures_IterateDeclared2(TN_MODELSKINS, findModelSkinForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_FindModelReflectionSkinForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;
    result = Textures_IterateDeclared2(TN_MODELREFLECTIONSKINS, findModelSkinForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

void R_UpdateData(void)
{
    R_UpdateRawTexs();
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

int findSpriteOwner(thinker_t* th, void* context)
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
                return true; // Found an owner!
        }
    }

    return false; // Keep looking...
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

        R_PrecacheModelsForState(i);

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
    // Don't precache when playing demo.
    if(isDedicated || playback)
        return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    if(precacheMapMaterials)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        uint i, j;

        for(i = 0; i < NUM_SIDEDEFS; ++i)
        {
            SideDef* side = SIDE_PTR(i);

            if(side->SW_middlematerial)
                Materials_Precache(side->SW_middlematerial, spec, true);

            if(side->SW_topmaterial)
                Materials_Precache(side->SW_topmaterial, spec, true);

            if(side->SW_bottommaterial)
                Materials_Precache(side->SW_bottommaterial, spec, true);
        }

        for(i = 0; i < NUM_SECTORS; ++i)
        {
            Sector* sec = SECTOR_PTR(i);
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

            if(GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1/* All mobjs are public*/,
                                       findSpriteOwner, sprDef))
            {
                // This sprite is used by some state of at least one mobj.
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
        GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1, R_PrecacheModelsForMobj, NULL);
    }
}

Texture* R_CreateDetailTextureFromDef(const ded_detailtexture_t* def)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    // Have we already created one for this?
    tex = R_FindDetailTextureForResourcePath(def->detailTex);
    if(tex) return tex;

    uniqueId = Textures_Count(TN_DETAILS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: failed to create new detail texture (max:%i).\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%-*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_DETAILS_NAME);

    texId = Textures_Declare(uri, uniqueId, def->detailTex);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex && !Textures_Create(texId, TXF_CUSTOM, NULL))
    {
        Con_Message("Warning: Failed defining Texture for DetailTexture '%s', ignoring.\n", name);
        return NULL;
    }

    return tex;
}

static int findDetailTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindDetailTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;
    result = Textures_IterateDeclared2(TN_DETAILS, findDetailTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateLightMap(const Uri* resourcePath)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!resourcePath || Str_IsEmpty(Uri_Path(resourcePath)) || !Str_CompareIgnoreCase(Uri_Path(resourcePath), "-"))
        return NULL;

    // Have we already created one for this?
    tex = R_FindLightMapForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TN_LIGHTMAPS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring new LightMap (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%-*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_LIGHTMAPS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, TXF_CUSTOM, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for LightMap '%s', ignoring.\n", name);
            return NULL;
        }
    }
    return tex;
}

static int findLightMapTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindLightMapForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path)) || !Str_CompareIgnoreCase(Uri_Path(path), "-")) return NULL;

    result = Textures_IterateDeclared2(TN_LIGHTMAPS, findLightMapTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateFlareTexture(const Uri* resourcePath)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    if(!resourcePath || Str_IsEmpty(Uri_Path(resourcePath)) || !Str_CompareIgnoreCase(Uri_Path(resourcePath), "-"))
        return NULL;

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    if(Str_At(Uri_Path(resourcePath), 0) >= '0' && Str_At(Uri_Path(resourcePath), 0) <= '4' && !Str_At(Uri_Path(resourcePath), 1))
        return NULL;

    // Have we already created one for this?
    tex = R_FindFlareTextureForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TN_FLAREMAPS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring new FlareTex (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    // Create a texture for it.
    dd_snprintf(name, 9, "%-*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_FLAREMAPS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        tex = Textures_Create(texId, TXF_CUSTOM, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for flare texture '%s', ignoring.\n", name);
            return NULL;
        }
    }
    return tex;
}

static int findFlareTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindFlareTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path)) || !Str_CompareIgnoreCase(Uri_Path(path), "-")) return NULL;

    result = Textures_IterateDeclared2(TN_FLAREMAPS, findFlareTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateReflectionTexture(const Uri* resourcePath)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    // Have we already created one for this?
    tex = R_FindReflectionTextureForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TN_REFLECTIONS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring new ShinyTex (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%-*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_REFLECTIONS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(!tex)
    {
        // Create a texture for it.
        tex = Textures_Create(texId, TXF_CUSTOM, NULL);
        if(!tex)
        {
            Con_Message("Warning: Failed defining Texture for shiny texture '%s', ignoring.\n", name);
            return NULL;
        }
    }

    return tex;
}

static int findReflectionTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindReflectionTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;

    result = Textures_IterateDeclared2(TN_REFLECTIONS, findReflectionTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

Texture* R_CreateMaskTexture(const Uri* resourcePath, const Size2Raw* size)
{
    textureid_t texId;
    Texture* tex;
    char name[9];
    int uniqueId;
    Uri* uri;

    // Have we already created one for this?
    tex = R_FindMaskTextureForResourcePath(resourcePath);
    if(tex) return tex;

    uniqueId = Textures_Count(TN_MASKS)+1;
    if(M_NumDigits(uniqueId) > 8)
    {
        Con_Message("Warning: Failed declaring Mask texture (max:%i), ignoring.\n", DDMAXINT);
        return NULL;
    }

    dd_snprintf(name, 9, "%-*i", 8, uniqueId);
    uri = Uri_NewWithPath2(name, RC_NULL);
    Uri_SetScheme(uri, TN_MASKS_NAME);

    texId = Textures_Declare(uri, uniqueId, resourcePath);
    Uri_Delete(uri);
    if(texId == NOTEXTUREID) return NULL; // Invalid uri?

    tex = Textures_ToTexture(texId);
    if(tex)
    {
        Texture_SetSize(tex, size);
    }
    else
    {
        // Create a texture for it.
        tex = Textures_CreateWithSize(texId, TXF_CUSTOM, size, NULL);
        if(!tex)
        {
            ddstring_t* path = Uri_ToString(resourcePath);
            Con_Message("Warning: Failed defining Texture for mask texture \"%s\"\n", F_PrettyPath(Str_Text(path)));
            Str_Delete(path);
            return NULL;
        }
    }

    return tex;
}

static int findMaskTextureForResourcePathWorker(textureid_t texId, void* paramaters)
{
    const Uri* resourcePath = Textures_ResourcePath(texId);
    if(Uri_Equality(resourcePath, (const Uri*)paramaters))
    {
        return (int)texId;
    }
    return 0; // Continue iteration.
}

Texture* R_FindMaskTextureForResourcePath(const Uri* path)
{
    int result;
    if(!path || Str_IsEmpty(Uri_Path(path))) return NULL;

    result = Textures_IterateDeclared2(TN_MASKS, findMaskTextureForResourcePathWorker, (void*)path);
    if(!result) return NULL;
    return Textures_ToTexture((textureid_t)result);
}

static animgroup_t* getAnimGroup(int number)
{
    if(--number < 0 || number >= numgroups) return NULL;
    return &groups[number];
}

static boolean isInAnimGroup(const animgroup_t* group, textureid_t texId)
{
    int i;
    assert(group);

    if(texId == NOTEXTUREID) return false;
    for(i = 0; i < group->count; ++i)
    {
        if(group->frames[i].texture == texId)
            return true;
    }
    return false;
}

void R_ClearAnimGroups(void)
{
    int i;
    if(numgroups <= 0) return;

    for(i = 0; i < numgroups; ++i)
    {
        animgroup_t* grp = &groups[i];
        if(grp->frames) Z_Free(grp->frames);
    }
    Z_Free(groups), groups = NULL;
    numgroups = 0;
}

const animgroup_t* R_ToAnimGroup(int animGroupNum)
{
    animgroup_t* grp = getAnimGroup(animGroupNum);
#if _DEBUG
    if(!grp)
    {
        Con_Message("Warning:R_ToAnimGroup: Invalid group #%i, returning NULL.\n", animGroupNum);
    }
#endif
    return grp;
}

int R_AnimGroupCount(void)
{
    return numgroups;
}

/// \note Part of the Doomsday public API.
int R_CreateAnimGroup(int flags)
{
    animgroup_t* group;

    // Allocating one by one is inefficient but it doesn't really matter.
    groups = Z_Realloc(groups, sizeof *groups * ++numgroups, PU_APPSTATIC);
    if(!groups)
        Con_Error("R_CreateAnimGroup: Failed on (re)allocation of %lu bytes enlarging AnimGroup list.", (unsigned long) sizeof *groups * numgroups);
    group = &groups[numgroups-1];

    // Init the new group.
    memset(group, 0, sizeof *group);
    group->id = numgroups; // 1-based index.
    group->flags = flags;

    return group->id;
}

/// \note Part of the Doomsday public API.
void R_AddAnimGroupFrame(int groupNum, const Uri* texture, int tics, int randomTics)
{
    animgroup_t* group = getAnimGroup(groupNum);
    animframe_t* frame;
    textureid_t texId;

    if(!group)
    {
#if _DEBUG
        Con_Message("Warning:R_AddAnimGroupFrame: Unknown anim group #%i, ignoring.\n", groupNum);
#endif
        return;
    }

    texId = Textures_ResolveUri2(texture, true/*quiet please*/);
    if(texId == NOTEXTUREID)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(texture);
        Con_Message("Warning::R_AddAnimGroupFrame: Invalid texture uri \"%s\", ignoring.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return;
    }

    // Allocate a new animframe.
    group->frames = Z_Realloc(group->frames, sizeof *group->frames * ++group->count, PU_APPSTATIC);
    if(!group->frames)
        Con_Error("R_AddAnimGroupFrame: Failed on (re)allocation of %lu bytes enlarging AnimFrame list for group #%i.", (unsigned long) sizeof *group->frames * group->count, groupNum);

    frame = &group->frames[group->count - 1];

    frame->texture = texId;
    frame->tics = tics;
    frame->randomTics = randomTics;
}

boolean R_IsTextureInAnimGroup(const Uri* texture, int groupNum)
{
    animgroup_t* group = getAnimGroup(groupNum);
    if(!group) return false;
    return isInAnimGroup(group, Textures_ResolveUri2(texture, true/*quiet please*/));
}

font_t* R_CreateFontFromFile(const Uri* uri, const char* resourcePath)
{
    fontnamespaceid_t namespaceId;
    fontid_t fontId;
    int uniqueId;
    font_t* font;

    if(!uri || !resourcePath || !resourcePath[0] || !F_Access(resourcePath))
    {
#if _DEBUG
        Con_Message("Warning:R_CreateFontFromFile: Invalid Uri or ResourcePath reference, ignoring.\n");
        if(resourcePath) Con_Message("  Resource path: %s\n", resourcePath);
#endif
        return NULL;
    }

    namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Scheme(uri)));
    if(!VALID_FONTNAMESPACEID(namespaceId))
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning: Invalid font namespace in Font Uri \"%s\", ignoring.\n", Str_Text(path));
        Str_Delete(path);
        return NULL;
    }

    uniqueId = Fonts_Count(namespaceId)+1; // 1-based index.
    fontId = Fonts_Declare(uri, uniqueId/*, resourcePath*/);
    if(fontId == NOFONTID) return NULL; // Invalid uri?

    // Have we already encountered this name?
    font = Fonts_ToFont(fontId);
    if(font)
    {
        Fonts_RebuildFromFile(font, resourcePath);
    }
    else
    {
        // A new font.
        font = Fonts_CreateFromFile(fontId, resourcePath);
        if(!font)
        {
            ddstring_t* path = Uri_ToString(uri);
            Con_Message("Warning: Failed defining new Font for \"%s\", ignoring.\n", Str_Text(path));
            Str_Delete(path);
        }
    }
    return font;
}

font_t* R_CreateFontFromDef(ded_compositefont_t* def)
{
    fontnamespaceid_t namespaceId;
    fontid_t fontId;
    int uniqueId;
    font_t* font;

    if(!def || !def->uri)
    {
#if _DEBUG
        Con_Message("Warning:R_CreateFontFromDef: Invalid Definition or Uri reference, ignoring.\n");
#endif
        return NULL;
    }

    namespaceId = Fonts_ParseNamespace(Str_Text(Uri_Scheme(def->uri)));
    if(!VALID_FONTNAMESPACEID(namespaceId))
    {
        ddstring_t* path = Uri_ToString(def->uri);
        Con_Message("Warning: Invalid font namespace in Font Definition Uri \"%s\", ignoring.\n", Str_Text(path));
        Str_Delete(path);
        return NULL;
    }

    uniqueId = Fonts_Count(namespaceId)+1; // 1-based index.
    fontId = Fonts_Declare(def->uri, uniqueId);
    if(fontId == NOFONTID) return NULL; // Invalid uri?

    // Have we already encountered this name?
    font = Fonts_ToFont(fontId);
    if(font)
    {
        Fonts_RebuildFromDef(font, def);
    }
    else
    {
        // A new font.
        font = Fonts_CreateFromDef(fontId, def);
        if(!font)
        {
            ddstring_t* path = Uri_ToString(def->uri);
            Con_Message("Warning: Failed defining new Font for \"%s\", ignoring.\n", Str_Text(path));
            Str_Delete(path);
        }
    }
    return font;
}

boolean R_DrawVLightVector(const vlight_t* light, void* context)
{
    coord_t distFromViewer = fabs(*((coord_t*)context));
    if(distFromViewer < 1600-8)
    {
        float alpha = 1 - distFromViewer / 1600, scale = 100;

        LIBDENG_ASSERT_IN_MAIN_THREAD();
        LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

        glBegin(GL_LINES);
            glColor4f(light->color[CR], light->color[CG], light->color[CB], alpha);
            glVertex3f(scale * light->vector[VX], scale * light->vector[VZ], scale * light->vector[VY]);
            glColor4f(light->color[CR], light->color[CG], light->color[CB], 0);
            glVertex3f(0, 0, 0);
        glEnd();
    }
    return true; // Continue iteration.
}

float ColorRawf_AverageColor(ColorRawf* c)
{
    assert(c);
    return (c->red + c->green + c->blue) / 3;
}

float ColorRawf_AverageColorMulAlpha(ColorRawf* c)
{
    assert(c);
    return (c->red + c->green + c->blue) / 3 * c->alpha;
}
