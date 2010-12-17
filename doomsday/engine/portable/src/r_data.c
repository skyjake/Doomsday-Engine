/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * r_data.c: Data Structures and Constants for Refresh
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

#include "m_stack.h"

// MACROS ------------------------------------------------------------------

#define RAWTEX_HASH_SIZE    128
#define RAWTEX_HASH(x)      (rawtexhash + (((unsigned) x) & (RAWTEX_HASH_SIZE - 1)))

#define COLORPALETTENAME_MAXLEN     (32)

// TYPES -------------------------------------------------------------------

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
 * DGL color palette name bindings.
 * These are mainly intended as a public API convenience.
 * Internally, we are only interested in the associated DGL name(s).
 */
typedef struct {
    char            name[COLORPALETTENAME_MAXLEN+1];
    DGLuint         id;
} colorpalettebind_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

int gameDataFormat; // Use a game-specifc data format where applicable.

extern boolean mapSetup; // We are currently setting up a map.

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte precacheSkins = true;
byte precacheSprites = true;

int numFlats = 0;
flat_t** flats = NULL;

int numSpriteTextures = 0;
spritetex_t** spriteTextures = NULL;

uint numDoomPatchDefs = 0;
patchtex_t** doomPatchDefs = NULL;

int numDetailTextures = 0;
detailtex_t** detailTextures = NULL;

int numLightMaps = 0;
lightmap_t** lightMaps = NULL;

int numFlareTextures = 0;
flaretex_t** flareTextures = NULL;

int numShinyTextures = 0;
shinytex_t** shinyTextures = NULL;

int numMaskTextures = 0;
masktex_t** maskTextures = NULL;

// Glowing textures are always rendered fullbright.
float glowingTextures = 1.0f;

byte rendInfoRPolys = 0;

// Skinnames will only *grow*. It will never be destroyed, not even
// at resets. The skin textures themselves will be deleted, though.
// This is because we want to have permanent ID numbers for skins,
// and the ID numbers are the same as indices to the skinNames array.
// Created in r_model.c, when registering the skins.
uint numSkinNames = 0;
skinname_t* skinNames = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int numDoomTextureDefs;
static doomtexturedef_t** doomTextureDefs;

static rawtexhash_t rawtexhash[RAWTEX_HASH_SIZE];

static unsigned int numrendpolys = 0;
static unsigned int maxrendpolys = 0;
static rendpolydata_t** rendPolys;

static colorpalettebind_t* colorPalettes = NULL;
static uint numColorPalettes = 0;
static colorpaletteid_t defaultColorPalette = 0;

// CODE --------------------------------------------------------------------

static colorpaletteid_t colorPaletteNumForName(const char* name)
{
    uint                i;

    // Linear search (sufficiently fast enough given the probably small set
    // and infrequency of searches).
    for(i = 0; i < numColorPalettes; ++i)
    {
        colorpalettebind_t* pal = &colorPalettes[i];

        if(!strnicmp(pal->name, name, COLORPALETTENAME_MAXLEN))
            return i + 1; // Already registered. 1-based index.
    }

    return 0; // Not found.
}

/**
 * Given a colorpalette id return the associated DGL color palette num.
 * If the specified id cannot be found, the default color palette will be
 * returned instead.
 *
 * @param id            Id of the color palette to be prepared.
 *
 * @return              The DGL palette number iff found else @c NULL.
 */
DGLuint R_GetColorPalette(colorpaletteid_t id)
{
    if(!id || id - 1 >= numColorPalettes)
    {
        if(numColorPalettes)
            return colorPalettes[defaultColorPalette-1].id;

        return 0;
    }

    return colorPalettes[id-1].id;
}

/**
 * Change the default color palette.
 *
 * @param id            Id of the color palette to make default.
 *
 * @return              @c true iff successful, else @c NULL.
 */
boolean R_SetDefaultColorPalette(colorpaletteid_t id)
{
    if(id && id - 1 < numColorPalettes)
    {
        defaultColorPalette = id;
        return true;
    }

    VERBOSE(Con_Message("R_SetDefaultColorPalette: Invalid id %u.\n", id))
    return false;
}

/**
 * Add a new (named) color palette.
 * \note Part of the Doomsday public API.
 *
 * \design The idea with the two-teered implementation is to allow maximum
 * flexibility. Within the engine we can create new palettes and manipulate
 * them directly via the DGL interface. The underlying implementation is
 * wrapped in a similar way to the materials so that publically, there is a
 * set of (eternal) names and unique identifiers that survive engine and
 * GL resets.
 *
 * @param fmt           Format string describes the format of @p data.
 *                      Expected form: "C#C#C"
 *                          C = color component, one of R, G, B.
 *                          # = bits per component.
 * @param name          Unique name by which the palette will be known.
 * @param data          Buffer containing the src color data.
 * @param num           Number of sets of components).
 *
 * @return              Color palette id.
 */
colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name,
                                      const byte* data, ushort num)
{
#define MAX_BPC         (16) // Max bits per component.

    static const char*  compNames[] = { "red", "green", "blue" };
    colorpaletteid_t    id;
    colorpalettebind_t* pal;
    const char*         c, *end;
    int                 i, pos, compOrder[3];
    byte                compSize[3];

    if(!name || !name[0])
        Con_Error("R_CreateColorPalette: Color palettes require a name.\n");

    if(strlen(name) > COLORPALETTENAME_MAXLEN)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", "
                  "color palette name can be at most %i characters long.\n",
                  name, COLORPALETTENAME_MAXLEN);

    if(!fmt || !fmt[0])
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", "
                  "missing format string.\n", name);

    if(!data || !num)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", "
                  "cannot create a zero-sized palette.\n", name);

    // All arguments supplied. Parse the format string.
    memset(compOrder, -1, sizeof(compOrder));
    pos = 0;
    end = fmt + (strlen(fmt) - 1);
    c = fmt;
    do
    {
        int               comp = -1;

        if(*c == 'R' || *c == 'r')
            comp = CR;
        else if(*c == 'G' || *c == 'g')
            comp = CG;
        else if(*c == 'B' || *c == 'b')
            comp = CB;

        if(comp != -1)
        {
            // Have encountered this component yet?
            if(compOrder[comp] == -1)
            {   // No.
                const char*         start;
                size_t              numDigits;

                compOrder[comp] = pos++;

                // Read the number of bits.
                start = ++c;
                while((*c >= '0' && *c <= '9') && ++c < end);

                numDigits = c - start;
                if(numDigits != 0 && numDigits <= 2)
                {
                    char                buf[3];

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

        Con_Error("R_CreateColorPalette: Failed creating \"%s\", "
                  "invalid character '%c' in format string at "
                  "position %u.\n", name, *c, (unsigned int) (c - fmt));
    } while(++c <= end);

    if(pos != 3)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", "
                  "incomplete format specification.\n", name);

    // Check validity of bits per component.
    for(i = 0; i < 3; ++i)
        if(compSize[i] == 0 || compSize[i] > MAX_BPC)
        {
            Con_Error("R_CreateColorPalette: Failed creating \"%s\", "
                      "unsupported bit size %i for %s component.\n",
                      name, compSize[i], compNames[compOrder[i]]);
        }

    if((id = colorPaletteNumForName(name)))
    {   // We are replacing an existing palette.
        pal = &colorPalettes[id-1];

        // Destroy the existing DGL color palette.
        GL_DeleteColorPalettes(1, &pal->id);
        pal->id = 0;
    }
    else
    {   // We are creating a new palette.
        if(!numColorPalettes)
        {
            colorPalettes = Z_Malloc(
                ++numColorPalettes * sizeof(colorpalettebind_t), PU_STATIC, 0);

            defaultColorPalette = numColorPalettes;
        }
        else
            colorPalettes = Z_Realloc(&colorPalettes,
                ++numColorPalettes * sizeof(colorpalettebind_t), PU_STATIC);

        pal = &colorPalettes[numColorPalettes-1];
        memset(pal, 0, sizeof(*pal));

        strncpy(pal->name, name, COLORPALETTENAME_MAXLEN);

        id = numColorPalettes; // 1-based index.
    }

    // Generate the actual DGL color palette.
    pal->id = GL_CreateColorPalette(compOrder, compSize, data, num);

    return id; // 1-based index.

#undef MAX_BPC
}

/**
 * Given a color palette name, look up the associated identifier.
 * \note Part of the Doomsday public API.
 *
 * @param name          Unique name of the palette to locate.
 * @return              Iff found, identifier of the palette associated
 *                      with this name, ELSE @c 0.
 */
colorpaletteid_t R_GetColorPaletteNumForName(const char* name)
{
    if(name && name[0] && strlen(name) <= COLORPALETTENAME_MAXLEN)
        return colorPaletteNumForName(name);

    return 0;
}

/**
 * Given a color palette id, look up the specified unique name.
 * \note Part of the Doomsday public API.
 *
 * @param id            Id of the color palette to locate.
 * @return              Iff found, pointer to the unique name associated
 *                      with the specified id, ELSE @c NULL.
 */
const char* R_GetColorPaletteNameForNum(colorpaletteid_t id)
{
    if(id && id - 1 < numColorPalettes)
        return colorPalettes[id-1].name;

    return NULL;
}

/**
 * Given a color palette index, calculate the equivilent RGB color.
 * \note Part of the Doomsday public API.
 *
 * @param id            Id of the color palette to use.
 * @param rgb           Final color will be written back here.
 * @param idx           Color Palette, color index.
 * @param correctGamma  @c TRUE if the current gamma ramp should be applied.
 */
void R_GetColorPaletteRGBf(colorpaletteid_t id, float rgb[3], int idx,
                           boolean correctGamma)
{
    if(rgb)
    {
        if(idx >= 0)
        {
            DGLuint             pal = R_GetColorPalette(id);
            DGLubyte            rgbUBV[3];

            GL_GetColorPaletteRGB(pal, rgbUBV, idx);

            if(correctGamma)
            {
                rgbUBV[CR] = gammaTable[rgbUBV[CR]];
                rgbUBV[CG] = gammaTable[rgbUBV[CG]];
                rgbUBV[CB] = gammaTable[rgbUBV[CB]];
            }

            rgb[CR] = rgbUBV[CR] * reciprocal255;
            rgb[CG] = rgbUBV[CG] * reciprocal255;
            rgb[CB] = rgbUBV[CB] * reciprocal255;

            return;
        }

        rgb[CR] = rgb[CG] = rgb[CB] = 0;
    }
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

void R_ShutdownData(void)
{
    Materials_Shutdown();
}

/**
 * Registers the "system" textures that are part of the Doomsday data resource package.
 * @fixme A nuisance really. Why not simply read the contents of e.g., "}doomsday/data/graphics/"
 */
void R_InitSystemTextures(void)
{
    struct ddtexdef_s {
        char name[9];
        uint id;
    } static const ddtexdefs[NUM_DD_TEXTURES] = {
        { "DDT_UNKN", DDT_UNKNOWN },
        { "DDT_MISS", DDT_MISSING },
        { "DDT_BBOX", DDT_BBOX },
        { "DDT_GRAY", DDT_GRAY }
    };
    { uint i;
    for(i = 0; i < NUM_DD_TEXTURES; ++i)
    {
        GL_CreateGLTexture(ddtexdefs[i].name, ddtexdefs[i].id, GLT_SYSTEM);
    }}
}

static patchtex_t* getPatchTex(patchid_t id)
{
    if(id != 0 && id <= numDoomPatchDefs)
        return doomPatchDefs[id-1];
    return NULL;
}

static patchid_t patchForName(const char* name)
{
    const gltexture_t* glTex;
    if((glTex = GL_GetGLTextureByName(name, GLT_DOOMPATCH)))
        return glTex->ofTypeID;
    return 0;
}

/**
 * Returns a patchtex_t* for the given lump, if one already exists.
 */
patchtex_t* R_FindPatchTex(patchid_t id)
{
    patchtex_t* patchTex = getPatchTex(id);
    if(!patchTex)
        Con_Error("R_FindPatchText: Unknown patch %i.", id);
    return patchTex;
}

/**
 * Get a patchtex_t data structure for a patch specified with a WAD lump
 * number. Allocates a new patchtex_t if it hasn't been loaded yet.
 */
patchid_t R_RegisterAsPatch(const char* name)
{
    assert(name);
    {
    const lumppatch_t* patch;
    lumpnum_t lump;
    patchid_t id;
    patchtex_t* p;

    if(!name[0])
        return 0;

    // Already defined as a patch?
    if((id = patchForName(name)) != 0)
        return id;

    if((lump = W_CheckNumForName(name)) == -1)
        return 0;

    /// \fixme What about min lump size?
    patch = (const lumppatch_t*) W_CacheLumpNum(lump, PU_STATIC);

    // An entirely new patch.
    p = Z_Calloc(sizeof(*p), PU_STATIC, NULL);
    p->lump = lump;
    /**
     * \fixme: Cannot be sure this is in Patch format until a load attempt
     * is made. We should not read this info here!
     */
    p->offX = -SHORT(patch->leftOffset);
    p->offY = -SHORT(patch->topOffset);
    p->width = SHORT(patch->width);
    p->height = SHORT(patch->height);
    p->isCustom = !W_LumpFromIWAD(lump);

    // Take a copy of the current patch loading state so that future texture
    // loads will produce the same results.
    if(monochrome)
        p->flags |= PF_MONOCHROME;
    if(upscaleAndSharpenPatches)
        p->flags |= PF_UPSCALE_AND_SHARPEN;

    // Register a gltexture for this.
    {
    const gltexture_t* glTex = GL_CreateGLTexture(name, ++numDoomPatchDefs, GLT_DOOMPATCH);
    p->texId = glTex->id;
    }

    // Add it to the pointer array.
    doomPatchDefs = Z_Realloc(doomPatchDefs, sizeof(patchtex_t*) * numDoomPatchDefs, PU_STATIC);
    doomPatchDefs[numDoomPatchDefs-1] = p;

    W_ChangeCacheTag(lump, PU_CACHE);
    return numDoomPatchDefs;
    }
}

boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info)
{
    if(!info)
        Con_Error("R_GetPatchInfo: Argument 'info' cannot be NULL.");
    {
    const patchtex_t* p;
    memset(info, 0, sizeof(*info));
    if((p = getPatchTex(id)) != NULL)
    {
        info->id = id;
        info->width = p->width;
        info->height = p->height;
        info->offset = p->offX;
        info->topOffset = p->offY;
        info->isCustom = p->isCustom;
        /// \kludge:
        info->extraOffset[0] = info->extraOffset[1] = (p->flags & PF_UPSCALE_AND_SHARPEN)? -1 : 0;
        return true;
    }
    VERBOSE(Con_Message("R_GetPatchInfo: Warning, unknown Patch %i.\n", id));
    return false;
    }
}

patchid_t R_PrecachePatch(const char* name, patchinfo_t* info)
{
    if(!name)
        Con_Error("R_PrecachePatch: Argument 'name' cannot be NULL.");

    if(info)
        memset(info, 0, sizeof(patchinfo_t));

    if(isDedicated)
        return 0;

    {patchid_t patch;
    if((patch = R_RegisterAsPatch(name)) != 0)
    {
        GL_PreparePatch(getPatchTex(patch));
        if(info)
            R_GetPatchInfo(patch, info);
    }
    else
        VERBOSE(Con_Message("R_PrecachePatch: Warning, unknown Patch %s.\n", name));
    return patch;
    }
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
    list = Z_Malloc(sizeof(r) * (num + 1), PU_STATIC, NULL);

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
rawtex_t* R_FindRawTex(lumpnum_t lump)
{
    if(lump >= W_NumLumps())
    {
        Con_Error("R_FindRawTex: lump = %i out of bounds (%i).\n", lump, W_NumLumps());
    }

    { rawtex_t* i;
    for(i = RAWTEX_HASH(lump)->first; i; i = i->next)
    {
        if(i->lump == lump)
            return i;
    }}
    return 0;
}

/**
 * Get a rawtex_t data structure for a raw texture specified with a WAD lump
 * number. Allocates a new rawtex_t if it hasn't been loaded yet.
 */
rawtex_t* R_GetRawTex(lumpnum_t lump)
{
    rawtex_t* r = 0;
    rawtexhash_t* hash = 0;

    if(lump >= W_NumLumps())
    {
        Con_Error("R_GetRawTex: lump = %i out of bounds (%i).\n", lump, W_NumLumps());
    }

    r = R_FindRawTex(lump);

    if(!lump)
        return NULL;

    // Check if this lump has already been loaded as a rawtex.
    if(r)
        return r;

    // Hmm, this is an entirely new rawtex.
    r = Z_Calloc(sizeof(*r), PU_PATCH, 0);
    hash = RAWTEX_HASH(lump);

    // Link to the hash.
    r->next = hash->first;
    hash->first = r;

    // Init the new one.
    r->lump = lump;
    return r;
}

static lumpnum_t* loadPatchList(lumpnum_t lump, size_t* num)
{
    char name[9], *names;
    lumpnum_t* patchLumpList;
    size_t i, numPatches, lumpSize = W_LumpLength(lump);

    names = M_Malloc(lumpSize);
    W_ReadLump(lump, names);

    numPatches = LONG(*((int *) names));
    if(numPatches > (lumpSize - 4) / 8)
    {   // Lump is truncated.
        Con_Message("loadPatchList: Warning, lump '%s' truncated (%lu bytes, "
                    "expected %lu).\n", W_LumpName(lump),
                    (unsigned long) lumpSize,
                    (unsigned long) (numPatches * 8 + 4));
        numPatches = (lumpSize - 4) / 8;
    }

    patchLumpList = M_Malloc(numPatches * sizeof(*patchLumpList));
    for(i = 0; i < numPatches; ++i)
    {
        memset(name, 0, sizeof(name));
        strncpy(name, names + 4 + i * 8, 8);
        patchLumpList[i] = W_CheckNumForName(name);
    }

    M_Free(names);

    if(num)
        *num = numPatches;

    return patchLumpList;
}

/**
 * Read DOOM and Strife format texture definitions from the specified lump.
 */
static doomtexturedef_t** readDoomTextureDefLump(lumpnum_t lump,
                                                 lumpnum_t* patchlookup,
                                                 size_t numPatches,
                                                 boolean firstNull, int* numDefs)
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

    int                 i;
    int*                maptex1;
    size_t              lumpSize, offset, n, numValidPatchRefs;
    int*                directory;
    void*               storage;
    byte*               validTexDefs;
    short*              texDefNumPatches;
    int                 numTexDefs, numValidTexDefs;
    doomtexturedef_t**  texDefs = NULL;

    lumpSize = W_LumpLength(lump);
    maptex1 = M_Malloc(lumpSize);
    W_ReadLump(lump, maptex1);

    numTexDefs = LONG(*maptex1);

    VERBOSE(Con_Message("R_ReadTextureDefs: Processing lump '%s'...\n",
                        W_LumpName(lump)));

    validTexDefs = M_Calloc(numTexDefs * sizeof(byte));
    texDefNumPatches = M_Calloc(numTexDefs * sizeof(*texDefNumPatches));

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
            Con_Message("R_ReadTextureDefs: Bad offset %lu for definition "
                        "%i in lump '%s'.\n", (unsigned long) offset, i,
                        W_LumpName(lump));
            continue;
        }

        if(gameDataFormat == 0)
        {   // DOOM format.
            maptexture_t*       mtexture =
                (maptexture_t *) ((byte *) maptex1 + offset);
            short               j, n, patchCount = SHORT(mtexture->patchCount);

            n = 0;
            if(patchCount > 0)
            {
                mappatch_t*         mpatch = &mtexture->patches[0];

                for(j = 0; j < patchCount; ++j, mpatch++)
                {
                    short               patchNum = SHORT(mpatch->patch);

                    if(patchNum < 0 || (unsigned) patchNum >= numPatches)
                    {
                        Con_Message("R_ReadTextureDefs: Invalid patch %i in "
                                    "texture '%s'.\n", (int) patchNum,
                                    mtexture->name);
                        continue;
                    }

                    if(patchlookup[patchNum] == -1)
                    {
                        Con_Message("R_ReadTextureDefs: Missing patch %i in "
                                    "texture '%s'.\n", (int) j, mtexture->name);
                        continue;
                    }

                    n++;
                }
            }
            else
            {
                Con_Message("R_ReadTextureDefs: Invalid patchcount %i in "
                            "texture '%s'.\n", (int) patchCount,
                            mtexture->name);
            }

            texDefNumPatches[i] = n;
            numValidPatchRefs += n;
        }
        else if(gameDataFormat == 3)
        {   // Strife format.
            strifemaptexture_t* smtexture =
                (strifemaptexture_t *) ((byte *) maptex1 + offset);
            short               j, n, patchCount = SHORT(smtexture->patchCount);

            n = 0;
            if(patchCount > 0)
            {
                strifemappatch_t*   smpatch = &smtexture->patches[0];
                for(j = 0; j < patchCount; ++j, smpatch++)
                {
                    short               patchNum = SHORT(smpatch->patch);

                    if(patchNum < 0 || (unsigned) patchNum >= numPatches)
                    {
                        Con_Message("R_ReadTextureDefs: Invalid patch %i in "
                                    "texture '%s'.\n", (int) patchNum,
                                    smtexture->name);
                        continue;
                    }

                    if(patchlookup[patchNum] == -1)
                    {
                        Con_Message("R_ReadTextureDefs: Missing patch %i in "
                                    "texture '%s'.\n", (int) j, smtexture->name);
                        continue;
                    }

                    n++;
                }
            }
            else
            {
                Con_Message("R_ReadTextureDefs: Invalid patchcount %i in "
                            "texture '%s'.\n", (int) patchCount,
                            smtexture->name);
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
        storage = Z_Calloc(sizeof(doomtexturedef_t) * numValidTexDefs +
                           sizeof(texpatch_t) * numValidPatchRefs, PU_REFRESHTEX, 0);
        directory = maptex1 + 1;
        n = 0;
        for(i = 0; i < numTexDefs; ++i, directory++)
        {
            doomtexturedef_t* texDef;
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
                strncpy(texDef->name, (const char*) mtexture->name, 8);
                strupr(texDef->name);
                texDef->width = SHORT(mtexture->width);
                texDef->height = SHORT(mtexture->height);
                storage = (byte *) storage + sizeof(doomtexturedef_t) + sizeof(texpatch_t) * texDef->patchCount;

                mpatch = &mtexture->patches[0];
                patch = &texDef->patches[0];
                for(j = 0; j < SHORT(mtexture->patchCount); ++j, mpatch++)
                {
                    short patchNum = SHORT(mpatch->patch);

                    if(patchNum < 0 || (unsigned) patchNum >= numPatches ||
                       patchlookup[patchNum] == -1)
                        continue;

                    patch->offX = SHORT(mpatch->originX);
                    patch->offY = SHORT(mpatch->originY);
                    patch->lump = patchlookup[patchNum];
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
                strncpy(texDef->name, (const char*) smtexture->name, 8);
                strupr(texDef->name);
                texDef->width = SHORT(smtexture->width);
                texDef->height = SHORT(smtexture->height);
                storage = (byte*) storage + sizeof(doomtexturedef_t) + sizeof(texpatch_t) * texDef->patchCount;

                smpatch = &smtexture->patches[0];
                patch = &texDef->patches[0];
                for(j = 0; j < SHORT(smtexture->patchCount); ++j, smpatch++)
                {
                    short patchNum = SHORT(smpatch->patch);

                    if(patchNum < 0 || (unsigned) patchNum >= numPatches ||
                       patchlookup[patchNum] == -1)
                        continue;

                    patch->offX = SHORT(smpatch->originX);
                    patch->offY = SHORT(smpatch->originY);
                    patch->lump = patchlookup[patchNum];
                    patch++;
                }
            }

            /**
             * DOOM.EXE had a bug in the way textures were managed resulting in
             * the first texture being used dually as a "NULL" texture.
             */
            if(firstNull && i == 0)
                texDef->flags |= TXDF_NODRAW;

            /**
             * Is this a non-IWAD texture?
             * At this stage we assume it is an IWAD texture definition
             * unless one of the patches is not.
             */
            texDef->flags |= TXDF_IWAD;
            j = 0;
            while(j < texDef->patchCount && (texDef->flags & TXDF_IWAD))
            {
                if(!W_LumpFromIWAD(texDef->patches[j].lump))
                    texDef->flags &= ~TXDF_IWAD;
                else
                    j++;
            }

            // Add it to the list.
            texDefs[n++] = texDef;
        }
    }

    VERBOSE(Con_Message("  Loaded %i of %i definitions.\n",
                        numValidTexDefs, numTexDefs));

    // Free all temporary storage.
    M_Free(validTexDefs);
    M_Free(texDefNumPatches);
    M_Free(maptex1);

    if(numDefs)
        *numDefs = numValidTexDefs;

    return texDefs;
}

static void loadDoomTextureDefs(void)
{
    lumpnum_t i, pnamesLump, *patchLumpList;
    doomtexturedef_t** list = 0, **listCustom = 0, ***eList = 0;
    int count = 0, countCustom = 0, *eCount = 0;
    size_t numPatches;
    boolean firstNull;

    if((pnamesLump = W_CheckNumForName2("PNAMES", true)) == -1)
        return;

    // Load the patch names from the PNAMES lump.
    patchLumpList = loadPatchList(pnamesLump, &numPatches);
    if(!patchLumpList)
        Con_Error("loadDoomTextureDefs: Error loading PNAMES.");

    /**
     * Many PWADs include new TEXTURE1/2 lumps including the IWAD doomtexture
     * definitions, with new definitions appended. In order to correctly
     * determine whether a defined texture originates from an IWAD we must
     * compare all definitions against those in the IWAD and if matching,
     * they should be considered as IWAD resources, even though the doomtexture
     * definition does not come from an IWAD lump.
     */
    firstNull = true;
    for(i = 0; i < W_NumLumps(); ++i)
    {
        char name[9];

        memset(name, 0, sizeof(name));
        strncpy(name, W_LumpName(i), 8);
        strupr(name); // Case insensitive.

        if(!strncmp(name, "TEXTURE1", 8) || !strncmp(name, "TEXTURE2", 8))
        {
            boolean isFromIWAD = W_LumpFromIWAD(i);
            int newNumTexDefs;
            doomtexturedef_t** newTexDefs;

            // Read in the new texture defs.
            newTexDefs = readDoomTextureDefLump(i, patchLumpList, numPatches, firstNull, &newNumTexDefs);

            eList = (isFromIWAD? &list : &listCustom);
            eCount = (isFromIWAD? &count : &countCustom);
            if(*eList)
            {
                doomtexturedef_t** newList;
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
        doomtexturedef_t** newList;
        size_t n;

        i = 0;
        while(i < count)
        {
            doomtexturedef_t* orig = list[i];
            boolean hasReplacement = false;
            int j;

            for(j = 0; j < countCustom; ++j)
            {
                doomtexturedef_t* custom = listCustom[j];

                if(!strncmp(orig->name, custom->name, 8))
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

                            if(origP->lump != customP->lump &&
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

        doomTextureDefs = newList;
        numDoomTextureDefs = count + countCustom;
    }
    else
    {
        doomTextureDefs = list;
        numDoomTextureDefs = count;
    }

    // We're finished with the patch lump list now.
    M_Free(patchLumpList);
}

/**
 * Reads in the texture defs and creates materials for them.
 */
void R_InitTextures(void)
{
    uint startTime = Sys_GetRealTime();

    numDoomTextureDefs = 0;
    doomTextureDefs = NULL;

    // Load texture definitions from TEXTURE1/2 lumps.
    loadDoomTextureDefs();

    if(numDoomTextureDefs > 0)
    {
        // Create materials for the defined textures.
        int i;
        for(i = 0; i < numDoomTextureDefs; ++i)
        {
            doomtexturedef_t* texDef = doomTextureDefs[i];
            const gltexture_t* tex = GL_CreateGLTexture(texDef->name, i, GLT_DOOMTEXTURE);
            // Create a material for this texture.
            Materials_New(MN_TEXTURES, texDef->name, texDef->width, texDef->height, ((texDef->flags & TXDF_NODRAW)? MATF_NO_DRAW : 0), tex->id, 0, 0);
        }
    }

    VERBOSE( Con_Message("R_InitTextures: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

doomtexturedef_t* R_GetDoomTextureDef(int num)
{
    if(num < 0 || num >= numDoomTextureDefs)
    {
#if _DEBUG
        Con_Error("R_GetDoomTextureDef: Invalid def num %i.", num);
#endif
        return NULL;
    }

    return doomTextureDefs[num];
}

/**
 * Returns the new flat index.
 */
static int R_NewFlat(lumpnum_t lump)
{
    int i;
    flat_t** newlist, *ptr;

    for(i = 0; i < numFlats; ++i)
    {
        ptr = flats[i];

        // Is this lump already entered?
        if(ptr->lump == lump)
            return i;

        // Is this a known identifer? Newer idents overide old.
        if(!strnicmp(W_LumpName(ptr->lump), W_LumpName(lump), 8))
        {
            ptr->lump = lump;
            return i;
        }
    }

    newlist = Z_Malloc(sizeof(flat_t*) * ++numFlats, PU_REFRESHTEX, 0);
    if(numFlats > 1)
    {
        for(i = 0; i < numFlats -1; ++i)
            newlist[i] = flats[i];

        Z_Free(flats);
    }
    flats = newlist;
    ptr = flats[numFlats - 1] = Z_Calloc(sizeof(flat_t), PU_REFRESHTEX, 0);
    ptr->lump = lump;
    ptr->width = 64; /// \fixme not all flats are 64 texels in width!
    ptr->height = 64; /// \fixme not all flats are 64 texels in height!

    return numFlats - 1;
}

void R_InitFlats(void)
{
    uint startTime = Sys_GetRealTime();
    ddstack_t* stack = Stack_New();
    int i;

    numFlats = 0;

    for(i = 0; i < W_NumLumps(); ++i)
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

        R_NewFlat(i);
    }

    while(Stack_Height(stack))
        Stack_Pop(stack);
    Stack_Delete(stack);

    for(i = 0; i < numFlats; ++i)
    {
        const flat_t* flat = flats[i];
        const gltexture_t* tex;

        tex = GL_CreateGLTexture(W_LumpName(flat->lump), i, GLT_FLAT);

        // Create a material for this flat.
        // \note that width = 64, height = 64 regardless of the flat dimensions.
        Materials_New(MN_FLATS, W_LumpName(flat->lump), 64, 64, 0, tex->id, 0, 0);
    }

    VERBOSE( Con_Message("R_InitFlats: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

uint R_CreateSkinTex(const char* skin, boolean isShinySkin)
{
    char realPath[256], name[9];
    const gltexture_t* glTex;
    skinname_t* st;
    int id;

    if(!skin[0])
        return 0;

    // Convert the given skin file to a full pathname.
    // \fixme Why is this done here and not during init??
    _fullpath(realPath, skin, 255);

    // Have we already created one for this?
    if((id = R_GetSkinNumForName(realPath)))
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

    // Create a gltexture for it.
    dd_snprintf(name, 9, "%-*i", 8, numSkinNames + 1);

    glTex = GL_CreateGLTexture(name, numSkinNames, (isShinySkin? GLT_MODELSHINYSKIN : GLT_MODELSKIN));

    skinNames = M_Realloc(skinNames, sizeof(skinname_t) * ++numSkinNames);
    st = skinNames + (numSkinNames - 1);

    strncpy(st->path, realPath, FILENAME_T_MAXLEN);
    st->id = glTex->id;

    if(verbose)
    {
        Con_Message("SkinTex: %s => %li\n", M_PrettyPath(skin), (long) (1 + (st - skinNames)));
    }
    return 1 + (st - skinNames); // 1-based index.
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
        directory_t mydir;
        memset(&mydir, 0, sizeof(mydir));
        Dir_FileDir(modelfn, &mydir);
        Str_Appendf(&searchPath, "Models:%s%s", mydir.path, skin);
        found = F_FindResource2(RC_GRAPHIC, Str_Text(&searchPath), foundPath);
    }

    if(!found)
    {   // Try the resource locator.
        Str_Clear(&searchPath); Str_Appendf(&searchPath, "Models:%s", skin);
        found = F_FindResource2(RC_GRAPHIC, Str_Text(&searchPath), foundPath);
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
            result = R_CreateSkinTex(foundPath ? Str_Text(foundPath) : Str_Text(&buf), isShinySkin);
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

uint R_GetSkinNumForName(const char* path)
{
    uint                i;

    for(i = 0; i < numSkinNames; ++i)
        if(!stricmp(skinNames[i].path, path))
            return i + 1; // 1-based index.

    return 0;
}

/**
 * This is called at final shutdown.
 */
void R_DestroySkins(void)
{
    M_Free(skinNames);
    skinNames = 0;
    numSkinNames = 0;
}

void R_UpdateTexturesAndFlats(void)
{
    Z_FreeTags(PU_REFRESHTEX, PU_REFRESHTEX);
}

void R_InitRawTexs(void)
{
    memset(rawtexhash, 0, sizeof(rawtexhash));
}

void R_UpdateRawTexs(void)
{
    Z_FreeTags(PU_PATCH, PU_PATCH);
    memset(rawtexhash, 0, sizeof(rawtexhash));
    R_InitRawTexs();
}

void R_UpdateData(void)
{
    R_UpdateRawTexs();
    Cl_InitTranslations();
}

void R_InitTranslationTables(void)
{
    // Allocate translation tables
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

/**
 * @return              @c true, if the given decoration works under the
 *                      specified circumstances.
 */
boolean R_IsAllowedDecoration(ded_decor_t* def, material_t* mat,
                              boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & DCRF_EXTERNAL) != 0;
    }

    if(!(mat->flags & MATF_CUSTOM))
        return !(def->flags & DCRF_NO_IWAD);

    return (def->flags & DCRF_PWAD) != 0;
}

/**
 * @return              @c true, if the given reflection works under the
 *                      specified circumstances.
 */
boolean R_IsAllowedReflection(ded_reflection_t* def, material_t* mat,
                              boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & REFF_EXTERNAL) != 0;
    }

    if(!(mat->flags & MATF_CUSTOM))
        return !(def->flags & REFF_NO_IWAD);

    return (def->flags & REFF_PWAD) != 0;
}

/**
 * @return              @c true, if the given decoration works under the
 *                      specified circumstances.
 */
boolean R_IsAllowedDetailTex(ded_detailtexture_t* def, material_t* mat,
                             boolean hasExternal)
{
    if(hasExternal)
    {
        return (def->flags & DTLF_EXTERNAL) != 0;
    }

    if(!(mat->flags & MATF_CUSTOM))
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

/**
 * Prepare all the relevant resources for the specified mobjtype.
 *
 * \note Part of the Doomsday public API.
 */
void R_PrecacheMobjNum(int num)
{
    int                 i;

    if(!((useModels && precacheSkins) || precacheSprites))
        return;

    if(num < 0 || num >= defs.count.mobjs.num)
        return;

    //// \optimize Traverses the entire state list!
    for(i = 0; i < defs.count.states.num; ++i)
    {
        state_t*            state;

        if(stateOwners[i] != &mobjInfo[num])
            continue;
        state = &states[i];

        R_PrecacheSkinsForState(i);

        if(precacheSprites)
        {
            int                 j;
            spritedef_t*        sprDef = &sprites[state->sprite];

            for(j = 0; j < sprDef->numFrames; ++j)
            {
                int                 k;
                spriteframe_t*      sprFrame = &sprDef->spriteFrames[j];

                for(k = 0; k < 8; ++k)
                    Materials_Precache(sprFrame->mats[k], true);
            }
        }
    }
}

/**
 * Prepare all relevant skins, textures, flats and sprites.
 * Doesn't unload anything, though (so that if there's enough
 * texture memory it will be used more efficiently). That much trust
 * is placed in the GL/D3D drivers. The prepared textures are also bound
 * here once so they should be ready for use ASAP.
 */
void R_PrecacheMap(void)
{
    uint i, j;
    sector_t* sec;
    sidedef_t* side;
    float startTime;

    // Don't precache when playing demo.
    if(isDedicated || playback)
        return;

    // Precaching from 100 to 200.
    Con_SetProgress(100);

    startTime = Sys_GetSeconds();

    // Always precache all materials used on world surfaces.
    for(i = 0; i < numSideDefs; ++i)
    {
        side = SIDE_PTR(i);
        Materials_Precache(side->SW_topmaterial, true);
        Materials_Precache(side->SW_middlematerial, true);
        Materials_Precache(side->SW_bottommaterial, true);
    }
    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);
        for(j = 0; j < sec->planeCount; ++j)
            Materials_Precache(sec->SP_planematerial(j), true);
    }

    // Precache sprites?
    if(precacheSprites)
    {
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
                        Materials_Precache(sprFrame->mats[k], true);
                }
            }
        }
    }

    // \fixme Precache sky materials!

    for(i = 0; i < Materials_Count(); ++i)
    {
        material_t* mat = Materials_ToMaterial(i+1);
        if(mat->inFlags & MATIF_PRECACHE)
        {
            material_snapshot_t ms;
            boolean hasDecorations;
            uint j;

            // Just this one material.
            Materials_Prepare(&ms, mat, true, 0);
            hasDecorations = ms.decorated;

            if(ms.glowing > 0 || hasDecorations)
            {
                for(j = 0; j < numSideDefs; ++j)
                {
                    sidedef_t* side = &sideDefs[j];
                    if(side->SW_middlematerial == mat)
                    {
                        if(ms.glowing > 0)
                            R_SurfaceListAdd(glowingSurfaceList, &side->SW_middlesurface);
                        if(hasDecorations)
                            R_SurfaceListAdd(decoratedSurfaceList, &side->SW_middlesurface);
                    }
                    if(side->SW_bottommaterial == mat)
                    {
                        if(ms.glowing > 0)
                            R_SurfaceListAdd(glowingSurfaceList, &side->SW_bottomsurface);
                        if(hasDecorations)
                            R_SurfaceListAdd(decoratedSurfaceList, &side->SW_bottomsurface);
                    }
                    if(side->SW_topmaterial == mat)
                    {
                        if(ms.glowing > 0)
                            R_SurfaceListAdd(glowingSurfaceList, &side->SW_topsurface);
                        if(hasDecorations)
                            R_SurfaceListAdd(decoratedSurfaceList, &side->SW_topsurface);
                    }
                }
                for(j = 0; j < numSectors; ++j)
                {
                    sector_t* sec = &sectors[j];
                    uint k;
                    for(k = 0; k < sec->planeCount; ++k)
                    {
                        plane_t* pln = sec->planes[k];
                        if(pln->PS_material == mat)
                        {
                            if(ms.glowing > 0)
                                R_SurfaceListAdd(glowingSurfaceList, &pln->surface);
                            if(hasDecorations)
                                R_SurfaceListAdd(decoratedSurfaceList, &pln->surface);
                        }
                    }
                }
            }
        }
    }

    // Precache model skins?
    if(useModels && precacheSkins)
    {
        // All mobjs are public.
        P_IterateThinkers(gx.MobjThinker, 0x1, R_PrecacheSkinsForMobj, NULL);
    }

    // Sky models usually have big skins.
    R_PrecacheSky();

    VERBOSE(Con_Message("Precaching took %.2f seconds.\n", Sys_GetSeconds() - startTime))
}

/**
 * Initialize an entire animation using the data in the definition.
 */
void R_InitAnimGroup(ded_group_t* def)
{
    int                 i, groupNumber = -1;
    materialnum_t       num;

    for(i = 0; i < def->count.num; ++i)
    {
        ded_group_member_t *gm = &def->members[i];

        num = Materials_CheckNumForName(gm->material.name, gm->material.mnamespace);

        if(!num)
            continue;

        // Only create a group when the first texture is found.
        if(groupNumber == -1)
        {
            // Create a new animation group.
            groupNumber = Materials_CreateAnimGroup(def->flags);
        }

        Materials_AddAnimGroupFrame(groupNumber, num, gm->tics, gm->randomTics);
    }
}

detailtex_t* R_CreateDetailTexture(const ded_detailtexture_t* def)
{
    char                name[9];
    const gltexture_t*  glTex;
    detailtex_t*        dTex;
    lumpnum_t           lump = W_CheckNumForName(def->detailLump.path);
    const char*         external =
        (def->isExternal? def->detailLump.path : NULL);

    // Have we already created one for this?
    if((dTex = R_GetDetailTexture(lump, external)))
        return NULL;

    if(M_NumDigits(numDetailTextures + 1) > 8)
    {
#if _DEBUG
Con_Message("R_CreateDetailTexture: Too many detail textures!\n");
#endif
        return NULL;
    }

    /**
     * A new detail texture.
     */

    // Create a gltexture for it.
    dd_snprintf(name, 9, "%-*i", 8, numDetailTextures + 1);

    glTex = GL_CreateGLTexture(name, numDetailTextures, GLT_DETAIL);

    dTex = M_Malloc(sizeof(*dTex));
    dTex->id = glTex->id;
    dTex->lump = lump;
    dTex->external = external;

    // Add it to the list.
    detailTextures =
        M_Realloc(detailTextures, sizeof(detailtex_t*) * ++numDetailTextures);
    detailTextures[numDetailTextures-1] = dTex;

    return dTex;
}

detailtex_t* R_GetDetailTexture(lumpnum_t lump, const char* external)
{
    int                 i;

    for(i = 0; i < numDetailTextures; ++i)
    {
        detailtex_t*        dTex = detailTextures[i];

        if(dTex->lump == lump &&
           ((dTex->external == NULL && external == NULL) ||
             (dTex->external && external && !stricmp(dTex->external, external))))
            return dTex;
    }

    return NULL;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyDetailTextures(void)
{
    int                 i;

    for(i = 0; i < numDetailTextures; ++i)
    {
        M_Free(detailTextures[i]);
    }

    if(detailTextures)
        M_Free(detailTextures);
    detailTextures = NULL;
    numDetailTextures = 0;
}

lightmap_t* R_CreateLightMap(const ded_lightmap_t* def)
{
    char                name[9];
    const gltexture_t*  glTex;
    lightmap_t*         lmap;

    if(!def->id[0] || def->id[0] == '-')
        return NULL; // Not a lightmap

    // Have we already created one for this?
    if((lmap = R_GetLightMap(def->id)))
        return NULL;

    if(M_NumDigits(numLightMaps + 1) > 8)
    {
#if _DEBUG
Con_Message("R_CreateLightMap: Too many lightmaps!\n");
#endif
        return NULL;
    }

    /**
     * A new lightmap.
     */

    // Create a gltexture for it.
    dd_snprintf(name, 9, "%-*i", 8, numLightMaps + 1);

    glTex = GL_CreateGLTexture(name, numLightMaps, GLT_LIGHTMAP);

    lmap = M_Malloc(sizeof(*lmap));
    lmap->id = glTex->id;
    lmap->external = def->id;

    // Add it to the list.
    lightMaps = M_Realloc(lightMaps, sizeof(lightmap_t*) * ++numLightMaps);
    lightMaps[numLightMaps-1] = lmap;

    return lmap;
}

lightmap_t* R_GetLightMap(const char* external)
{
    int                 i;

    if(external && external[0] && external[0] != '-')
    {
        for(i = 0; i < numLightMaps; ++i)
        {
            lightmap_t*         lmap = lightMaps[i];

            if(!stricmp(lmap->external, external))
                return lmap;
        }
    }

    return NULL;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyLightMaps(void)
{
    int                 i;

    for(i = 0; i < numLightMaps; ++i)
    {
        M_Free(lightMaps[i]);
    }

    if(lightMaps)
        M_Free(lightMaps);
    lightMaps = NULL;
    numLightMaps = 0;
}

flaretex_t* R_CreateFlareTexture(const ded_flaremap_t* def)
{
    flaretex_t*         fTex;
    char                name[9];
    const gltexture_t*  glTex;

    if(!def->id || !def->id[0] || def->id[0] == '-')
        return NULL; // Not a flare texture.

    // Perhaps a "built-in" flare texture id?
    // Try to convert the id to a system flare tex constant idx
    if(def->id[0] >= '0' && def->id[0] <= '4' && !def->id[1])
        return NULL; // Don't create a flaretex for this

    // Have we already created one for this?
    if((fTex = R_GetFlareTexture(def->id)))
        return NULL;

    if(M_NumDigits(numFlareTextures + 1) > 8)
    {
#if _DEBUG
Con_Message("R_CreateFlareTexture: Too many flare textures!\n");
#endif
        return NULL;
    }

    /**
     * A new flare texture.
     */
    // Create a gltexture for it.
    dd_snprintf(name, 9, "%-*i", 8, numFlareTextures + 1);

    glTex = GL_CreateGLTexture(name, numFlareTextures, GLT_FLARE);

    fTex = M_Malloc(sizeof(*fTex));
    fTex->external = def->id;
    fTex->id = glTex->id;

    // Add it to the list.
    flareTextures =
        M_Realloc(flareTextures, sizeof(flaretex_t*) * ++numFlareTextures);
    flareTextures[numFlareTextures-1] = fTex;

    return fTex;
}

flaretex_t* R_GetFlareTexture(const char* external)
{
    int                 i;

    if(!external || !external[0] || external[0] == '-')
        return NULL;

    for(i = 0; i < numFlareTextures; ++i)
    {
        flaretex_t*         fTex = flareTextures[i];

        if(!stricmp(fTex->external, external))
            return fTex;
    }

    return NULL;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyFlareTextures(void)
{
    int                 i;

    for(i = 0; i < numFlareTextures; ++i)
    {
        M_Free(flareTextures[i]);
    }

    if(flareTextures)
        M_Free(flareTextures);
    flareTextures = NULL;
    numFlareTextures = 0;
}

shinytex_t* R_CreateShinyTexture(const ded_reflection_t* def)
{
    char                name[9];
    const gltexture_t*  glTex;
    shinytex_t*         sTex;

    // Have we already created one for this?
    if((sTex = R_GetShinyTexture(def->shinyMap.path)))
        return NULL;

    if(M_NumDigits(numShinyTextures + 1) > 8)
    {
#if _DEBUG
Con_Message("R_CreateShinyTexture: Too many shiny textures!\n");
#endif
        return NULL;
    }

    /**
     * A new shiny texture.
     */

    // Create a gltexture for it.
    dd_snprintf(name, 9, "%-*i", 8, numShinyTextures + 1);

    glTex = GL_CreateGLTexture(name, numShinyTextures, GLT_SHINY);

    sTex = M_Malloc(sizeof(*sTex));
    sTex->id = glTex->id;
    sTex->external = def->shinyMap.path;

    // Add it to the list.
    shinyTextures =
        M_Realloc(shinyTextures, sizeof(shinytex_t*) * ++numShinyTextures);
    shinyTextures[numShinyTextures-1] = sTex;

    return sTex;
}

shinytex_t* R_GetShinyTexture(const char* external)
{
    int                 i;

    if(external && external[0])
        for(i = 0; i < numShinyTextures; ++i)
        {
            shinytex_t*         sTex = shinyTextures[i];

            if(!stricmp(sTex->external, external))
                return sTex;
        }

    return NULL;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyShinyTextures(void)
{
    int                 i;

    for(i = 0; i < numShinyTextures; ++i)
    {
        M_Free(shinyTextures[i]);
    }

    if(shinyTextures)
        M_Free(shinyTextures);
    shinyTextures = NULL;
    numShinyTextures = 0;
}

masktex_t* R_CreateMaskTexture(const ded_reflection_t* def)
{
    char                name[9];
    const gltexture_t*  glTex;
    masktex_t*          mTex;

    // Have we already created one for this?
    if((mTex = R_GetMaskTexture(def->maskMap.path)))
        return NULL;

    if(M_NumDigits(numMaskTextures + 1) > 8)
    {
#if _DEBUG
Con_Message("R_CreateMaskTexture: Too many mask textures!\n");
#endif
        return NULL;
    }

    /**
     * A new shiny texture.
     */

    // Create a gltexture for it.
    dd_snprintf(name, 9, "%-*i", 8, numMaskTextures + 1);

    glTex = GL_CreateGLTexture(name, numMaskTextures, GLT_MASK);

    mTex = M_Malloc(sizeof(*mTex));
    mTex->id = glTex->id;
    mTex->external = def->maskMap.path;
    mTex->width = def->maskWidth;
    mTex->height = def->maskHeight;

    // Add it to the list.
    maskTextures =
        M_Realloc(maskTextures, sizeof(masktex_t*) * ++numMaskTextures);
    maskTextures[numMaskTextures-1] = mTex;

    return mTex;
}

masktex_t* R_GetMaskTexture(const char* external)
{
    int                 i;

    if(external && external[0])
        for(i = 0; i < numMaskTextures; ++i)
        {
            masktex_t*      mTex = maskTextures[i];

            if(!stricmp(mTex->external, external))
                return mTex;
        }

    return NULL;
}

/**
 * This is called at final shutdown.
 */
void R_DestroyMaskTextures(void)
{
    int                 i;

    for(i = 0; i < numMaskTextures; ++i)
    {
        M_Free(maskTextures[i]);
    }

    if(maskTextures)
        M_Free(maskTextures);
    maskTextures = NULL;
    numMaskTextures = 0;
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
