/** @file colorpalette.c Color Palette.
 *
 * @author Copyright &copy; 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "m_misc.h" // for M_ReadBits

#include "resource/colorpalette.h"

#define RGB18(r, g, b)      ((r)+((g)<<6)+((b)<<12))

static void prepareNearestLUT(colorpalette_t* pal);
static void prepareColorTable(colorpalette_t* pal, const int compOrder[3],
    const uint8_t compBits[3], const uint8_t* colorData, int colorCount);

colorpalette_t* ColorPalette_New(void)
{
    colorpalette_t* pal = (colorpalette_t*) malloc(sizeof(*pal));
    if(NULL == pal)
        Con_Error("ColorPalette::Construct: Failed on allocation of %lu bytes for "
            "new ColorPalette.", (unsigned long) sizeof(*pal));

    pal->_flags = CPF_UPDATE_18TO8; // Defer creation of the nearest color LUT.
    pal->_colorCount = 0;
    pal->_colorData = NULL;
    pal->_18To8LUT = NULL;
    return pal;
}

colorpalette_t* ColorPalette_NewWithColorTable(const int compOrder[3],
    const uint8_t compBits[3], const uint8_t* colorData, int colorCount)
{
    assert(compOrder && compBits);
    {
    colorpalette_t* pal = ColorPalette_New();
    if(colorCount > 0 && NULL != colorData)
        prepareColorTable(pal, compOrder, compBits, colorData, colorCount);
    return pal;
    }
}

void ColorPalette_Delete(colorpalette_t* pal)
{
    assert(pal);
    if(pal->_colorData)
        free(pal->_colorData);
    if(pal->_18To8LUT)
        free(pal->_18To8LUT);
    free(pal);
}

void ColorPalette_ReplaceColorTable(colorpalette_t* pal, const int compOrder[3],
    const uint8_t compBits[3], const uint8_t* colorData, int colorCount)
{
    prepareColorTable(pal, compOrder, compBits, colorData, colorCount);
}

void ColorPalette_Color(const colorpalette_t* pal, int colorIdx, uint8_t rgb[3])
{
    assert(pal && rgb);
    {
    size_t offset;

#if _DEBUG
    if(colorIdx < 0 || colorIdx >= pal->_colorCount)
        Con_Message("Warning:ColorPalette::Color: ColorIdx %u out of range [0...%u].\n",
            colorIdx, pal->_colorCount);
#endif

    if(0 == pal->_colorCount)
    {
        rgb[CR] = rgb[CG] = rgb[CB] = 0;
        return;
    }

    offset = 3 * (size_t)MINMAX_OF(0, colorIdx, pal->_colorCount-1);
    rgb[CR] = pal->_colorData[offset + CR];
    rgb[CG] = pal->_colorData[offset + CG];
    rgb[CB] = pal->_colorData[offset + CB];
    }
}

int ColorPalette_NearestIndex(colorpalette_t* pal, uint8_t red, uint8_t green, uint8_t blue)
{
    assert(pal);
    if(0 == pal->_colorCount)
        return -1;
    // Ensure we've prepared the 18 to 8 table.
    prepareNearestLUT(pal);
    return pal->_18To8LUT[RGB18(red >> 2, green >> 2, blue >> 2)];
}

int ColorPalette_NearestIndexv(colorpalette_t* pal, const uint8_t rgb[3])
{
    return ColorPalette_NearestIndex(pal, rgb[CR], rgb[CG], rgb[CB]);
}

static void prepareColorTable(colorpalette_t* pal, const int compOrder[3],
    const uint8_t compBits[3], const uint8_t* colorData, int colorCount)
{
    assert(pal);
    {
    uint8_t order[3], bits[3], cb;
    const uint8_t* src;

    // Ensure input is in range.
    order[0] = MINMAX_OF(0, compOrder[0], 2);
    order[1] = MINMAX_OF(0, compOrder[1], 2);
    order[2] = MINMAX_OF(0, compOrder[2], 2);

    bits[CR] = MIN_OF(compBits[CR], COLORPALETTE_MAX_COMPONENT_BITS);
    bits[CG] = MIN_OF(compBits[CG], COLORPALETTE_MAX_COMPONENT_BITS);
    bits[CB] = MIN_OF(compBits[CB], COLORPALETTE_MAX_COMPONENT_BITS);

    if(NULL != pal->_colorData)
    {
        free(pal->_colorData); pal->_colorData = NULL;
        pal->_colorCount = 0;
    }

    if(NULL != pal->_18To8LUT)
    {
        free(pal->_18To8LUT); pal->_18To8LUT = NULL;
        pal->_flags |= CPF_UPDATE_18TO8;
    }

    pal->_colorCount = colorCount;
    if(NULL == (pal->_colorData = (uint8_t*) malloc(pal->_colorCount * 3 * sizeof(uint8_t))))
        Con_Error("ColorPalette::prepareColorTable: Failed on allocation of %lu bytes for "
            "color table.", (unsigned long) (pal->_colorCount * 3 * sizeof(uint8_t)));

    // Already in the format we want?
    if(8 == bits[CR] && 8 == bits[CG] && 8 == bits[CB])
    {   // Great! Just copy it as-is.
        memcpy(pal->_colorData, colorData, pal->_colorCount * 3);

        // Do we need to adjust the order?
        if(0 != order[CR]|| 1 != order[CG] || 2 != order[CB])
        {
            int i;
            for(i = 0; i < pal->_colorCount; ++i)
            {
                uint8_t* dst = &pal->_colorData[i * 3];
                uint8_t tmp[3];

                tmp[0] = dst[0];
                tmp[1] = dst[1];
                tmp[2] = dst[2];

                dst[CR] = tmp[order[CR]];
                dst[CG] = tmp[order[CG]];
                dst[CB] = tmp[order[CB]];
            }
        }
        return;
    }

    // Conversion is necessary.
    src = colorData;
    cb = 0;
    { int i;
    for(i = 0; i < pal->_colorCount; ++i)
    {
        uint8_t* dst = &pal->_colorData[i * 3];
        int tmp[3];

        tmp[CR] = tmp[CG] = tmp[CB] = 0;

        M_ReadBits(bits[order[CR]], &src, &cb, (uint8_t*) &(tmp[order[CR]]));
        M_ReadBits(bits[order[CG]], &src, &cb, (uint8_t*) &(tmp[order[CG]]));
        M_ReadBits(bits[order[CB]], &src, &cb, (uint8_t*) &(tmp[order[CB]]));

        // Need to do any scaling?
        if(8 != bits[CR])
        {
            if(bits[CR] < 8)
                tmp[CR] <<= 8 - bits[CR];
            else
                tmp[CR] >>= bits[CR] - 8;
        }

        if(8 != bits[CG])
        {
            if(bits[CG] < 8)
                tmp[CG] <<= 8 - bits[CG];
            else
                tmp[CG] >>= bits[CG] - 8;
        }

        if(8 != bits[CB])
        {
            if(bits[CB] < 8)
                tmp[CB] <<= 8 - bits[CB];
            else
                tmp[CB] >>= bits[CB] - 8;
        }

        // Store the final color.
        dst[CR] = (uint8_t) MINMAX_OF(0, tmp[CR], 255);
        dst[CG] = (uint8_t) MINMAX_OF(0, tmp[CG], 255);
        dst[CB] = (uint8_t) MINMAX_OF(0, tmp[CB], 255);
    }}
    }
}

/// @note A time-consuming operation.
static void prepareNearestLUT(colorpalette_t* pal)
{
#define SIZEOF18TO8         (sizeof(int) * 262144)

    assert(pal);
    if((pal->_flags & CPF_UPDATE_18TO8) || !pal->_18To8LUT)
    {
        int r, g, b, i, nearest, smallestDiff;

        if(!pal->_18To8LUT)
        {
            if(NULL == (pal->_18To8LUT = (int*) malloc(SIZEOF18TO8)))
                Con_Error("ColorPalette::prepareNearestLUT: Failed on allocation of %lu bytes for "
                    "lookup table.", (unsigned long) SIZEOF18TO8);
        }

        for(r = 0; r < 64; ++r)
        {
            for(g = 0; g < 64; ++g)
            {
                for(b = 0; b < 64; ++b)
                {
                    nearest = 0;
                    smallestDiff = DDMAXINT;
                    for(i = 0; i < pal->_colorCount; ++i)
                    {
                        const uint8_t* rgb = &pal->_colorData[i * 3];
                        int diff =
                            (rgb[CR] - (r << 2)) * (rgb[CR] - (r << 2)) +
                            (rgb[CG] - (g << 2)) * (rgb[CG] - (g << 2)) +
                            (rgb[CB] - (b << 2)) * (rgb[CB] - (b << 2));

                        if(diff < smallestDiff)
                        {
                            smallestDiff = diff;
                            nearest = i;
                        }
                    }

                    pal->_18To8LUT[RGB18(r, g, b)] = nearest;
                }
            }
        }

        pal->_flags &= ~CPF_UPDATE_18TO8;

        /*if(ArgCheck("-dump_pal18to8"))
        {
            FILE* file = fopen("colorpalette18To8.lmp", "wb");
            fwrite(pal->_18To8LUT, SIZEOF18TO8, 1, file);
            fclose(file);
        }*/
    }

#undef SIZEOF18TO8
}
