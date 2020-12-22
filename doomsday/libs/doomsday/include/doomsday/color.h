/**
 * @file color.h Color
 *
 * @authors Copyright &copy; 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include "libdoomsday.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ColorRawf. Color Raw (f)loating point. Is intended as a handy POD
 * structure for easy manipulation of four component, floating point
 * color plus alpha value sets.
 *
 * @ingroup data
 */
typedef struct LIBDOOMSDAY_PUBLIC ColorRawf_s {
    union {
        // Straight RGBA vector representation.
        float rgba[4];
        // Hybrid RGB plus alpha component representation.
        struct {
            float rgb[3];
            float alpha;
        };
        // Component-wise representation.
        struct {
            float red;
            float green;
            float blue;
            float _alpha;
        };
    };
#ifdef __cplusplus
    ColorRawf_s(float r = 0.f, float g = 0.f, float b = 0.f, float a = 0.f)
        : red(r), green(g), blue(b), _alpha(a) {}
#endif
} ColorRawf;

LIBDOOMSDAY_PUBLIC float ColorRawf_AverageColor(ColorRawf* color);
LIBDOOMSDAY_PUBLIC float ColorRawf_AverageColorMulAlpha(ColorRawf* color);

#ifdef __cplusplus
} // extern "C"
#endif
