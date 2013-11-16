/** @file colorpalettes.cpp  Color palette resource collection
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h" // Must be first because of dd_share.h
#include "resource/colorpalettes.h"

#include "de_console.h"
#include "render/r_main.h" // texGammaLut
#ifdef __CLIENT__
#  include "gl/gl_texmanager.h"
#endif
#include <de/Log>
#include <de/memory.h>
#include <de/memoryzone.h>
#include <QList>
#include <QtAlgorithms>

using namespace de;

byte *translationTables;

void R_InitTranslationTables()
{
    // The translation tables consist of a number of translation maps, each
    // containing 256 palette indices.
    translationTables = (byte *) Z_Calloc(NUM_TRANSLATION_TABLES * 256, PU_REFRESHTRANS, 0);
}

void R_UpdateTranslationTables()
{
    Z_FreeTags(PU_REFRESHTRANS, PU_REFRESHTRANS);
    R_InitTranslationTables();
}

byte const *R_TranslationTable(int tclass, int tmap)
{
    // Is translation unnecessary?
    if(!tclass && !tmap) return 0;

    int trans = de::max(0, NUM_TRANSLATION_MAPS_PER_CLASS * tclass + tmap - 1);
    LOG_DEBUG("tclass=%i tmap=%i => TransPal# %i") << tclass << tmap << trans;

    return translationTables + trans * 256;
}

#define COLORPALETTENAME_MAXLEN     (32)

/**
 * Color palette name binding. Mainly intended as a public API convenience.
 * Internally we are only interested in the associated palette indices.
 */
struct ColorPaletteBind
{
    char name[COLORPALETTENAME_MAXLEN+1];
    uint idx;
};

colorpaletteid_t defaultColorPalette;

typedef QList<ColorPalette *> ColorPalettes;
static ColorPalettes palettes;

static int nameBindingsCount;
static ColorPaletteBind *nameBindings;

static bool inited = false;

static colorpaletteid_t id(char const *name)
{
    DENG2_ASSERT(inited);

    // Linear search (sufficiently fast enough given the probably small set
    // and infrequency of searches).
    for(int i = 0; i < nameBindingsCount; ++i)
    {
        ColorPaletteBind *pal = &nameBindings[i];
        if(!qstrnicmp(pal->name, name, COLORPALETTENAME_MAXLEN))
        {
            return i + 1; // Already registered. 1-based index.
        }
    }

    return 0; // Not found.
}

static int newPalette(int const compOrder[3], uint8_t const compSize[3],
    uint8_t const *data, ushort num)
{
    DENG2_ASSERT(inited);

    palettes.append(new ColorPalette(compOrder, compSize, data, num));
    return palettes.count(); //1-based index.
}

void R_InitColorPalettes()
{
    if(inited)
    {
        // Re-init.
        R_DestroyColorPalettes();
        return;
    }

    nameBindings = 0;
    nameBindingsCount = 0;
    defaultColorPalette = 0;

    inited = true;
}

void R_DestroyColorPalettes()
{
    if(!inited) return;

    qDeleteAll(palettes);
    palettes.clear();

    if(nameBindings)
    {
        M_Free(nameBindings); nameBindings = 0;
        nameBindingsCount = 0;
    }

    defaultColorPalette = 0;
}

int R_ColorPaletteCount()
{
    DENG2_ASSERT(inited);
    return palettes.count();
}

ColorPalette *R_ToColorPalette(colorpaletteid_t id)
{
    DENG2_ASSERT(inited);
    if(id == 0 || id - 1 >= (unsigned) nameBindingsCount)
    {
        id = defaultColorPalette;
    }

    if(id != 0 && nameBindingsCount > 0)
    {
        return palettes.at(nameBindings[id-1].idx - 1);
    }
    return 0;
}

ColorPalette *R_GetColorPaletteByIndex(int paletteIdx)
{
    DENG2_ASSERT(inited);
    if(paletteIdx > 0 && palettes.count() >= paletteIdx)
    {
        return palettes.at(paletteIdx - 1);
    }
    Con_Error("R_GetColorPaletteByIndex: Failed to locate palette for index #%i", paletteIdx);
    exit(1); // Unreachable.
}

boolean R_SetDefaultColorPalette(colorpaletteid_t id)
{
    DENG2_ASSERT(inited);
    if(id - 1 < (unsigned) nameBindingsCount)
    {
        defaultColorPalette = id;
        return true;
    }
    LOG_VERBOSE("R_SetDefaultColorPalette: Invalid id %u.") << id;
    return false;
}

#undef R_CreateColorPalette
DENG_EXTERN_C colorpaletteid_t R_CreateColorPalette(char const *fmt, char const *name,
    uint8_t const *colorData, int colorCount)
{
    DENG2_ASSERT(name != 0 && fmt != 0 && colorData != 0);

    try
    {
        int compOrder[3];
        uint8_t compSize[3];
        ColorPalette::parseColorFormat(fmt, compOrder, compSize);

        colorpaletteid_t paletteId = id(name);
        ColorPaletteBind *bind;
        if(paletteId)
        {
            // Replacing an existing palette.
            bind = &nameBindings[paletteId - 1];
            R_GetColorPaletteByIndex(bind->idx)->replaceColorTable(compOrder, compSize, colorData, colorCount);

#ifdef __CLIENT__
            GL_ReleaseTexturesByColorPalette(paletteId);
#endif
        }
        else
        {
            // A new palette.
            nameBindings = (ColorPaletteBind *) M_Realloc(nameBindings, (nameBindingsCount + 1) * sizeof(ColorPaletteBind));

            bind = &nameBindings[nameBindingsCount];
            std::memset(bind, 0, sizeof(*bind));

            strncpy(bind->name, name, COLORPALETTENAME_MAXLEN);

            paletteId = (colorpaletteid_t) ++nameBindingsCount; // 1-based index.
            if(nameBindingsCount == 1)
            {
                defaultColorPalette = colorpaletteid_t(nameBindingsCount);
            }
        }

        // Generate the color palette.
        bind->idx = newPalette(compOrder, compSize, colorData, colorCount);

        return paletteId; // 1-based index.
    }
    catch(ColorPalette::ColorFormatError const &er)
    {
        LOG_WARNING("Error when creating/replacing color palette '%s':\n")
            << name << er.asText();
    }

    return 0;
}

#undef R_GetColorPaletteNumForName
DENG_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(char const *name)
{
    if(name && name[0] && qstrlen(name) <= COLORPALETTENAME_MAXLEN)
    {
        return id(name);
    }

    return 0;
}

#undef R_GetColorPaletteNameForNum
DENG_EXTERN_C char const *R_GetColorPaletteNameForNum(colorpaletteid_t id)
{
    if(id != 0 && id - 1 < (unsigned)nameBindingsCount)
    {
        return nameBindings[id-1].name;
    }

    return 0;
}

#undef R_GetColorPaletteRGBubv
DENG_EXTERN_C void R_GetColorPaletteRGBubv(colorpaletteid_t paletteId, int colorIdx, uint8_t rgb[3],
    boolean applyTexGamma)
{
    if(!rgb)
    {
        Con_Error("R_GetColorPaletteRGBubv: Invalid arguments (rgb==NULL).");
    }

    if(colorIdx < 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }

    if(ColorPalette *palette = R_ToColorPalette(paletteId))
    {
        Vector3ub palColor = palette->color(colorIdx);
        rgb[0] = palColor.x;
        rgb[1] = palColor.y;
        rgb[2] = palColor.z;
        if(applyTexGamma)
        {
            rgb[0] = texGammaLut[rgb[0]];
            rgb[1] = texGammaLut[rgb[1]];
            rgb[2] = texGammaLut[rgb[2]];
        }
        return;
    }

    LOG_WARNING("R_GetColorPaletteRGBubv: Failed to locate ColorPalette for id %i.") << paletteId;
}

#undef R_GetColorPaletteRGBf
DENG_EXTERN_C void R_GetColorPaletteRGBf(colorpaletteid_t paletteId, int colorIdx, float rgb[3],
    boolean applyTexGamma)
{
    if(!rgb)
    {
        Con_Error("R_GetColorPaletteRGBf: Invalid arguments (rgb==NULL).");
    }

    if(colorIdx < 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }

    if(ColorPalette *palette = R_ToColorPalette(paletteId))
    {
        if(applyTexGamma)
        {
            Vector3ub palColor = palette->color(colorIdx);
            rgb[0] = texGammaLut[palColor.x] * reciprocal255;
            rgb[1] = texGammaLut[palColor.y] * reciprocal255;
            rgb[2] = texGammaLut[palColor.z] * reciprocal255;
        }
        else
        {
            Vector3f palColor = palette->colorf(colorIdx);
            rgb[0] = palColor.x;
            rgb[1] = palColor.y;
            rgb[2] = palColor.z;
        }
        return;
    }

    LOG_WARNING("R_GetColorPaletteRGBf: Failed to locate ColorPalette for id %i.") << paletteId;
}
