/** @file texgamma.c  Texture color gamma mapping.
 *
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/legacy/texgamma.h"

#include <math.h>

static byte texGammaLut[256];

void R_BuildTexGammaLut(float texGamma)
{
    float invGamma = 1.0f - MINMAX_OF(0.f, texGamma, 1.f); // Clamp to a sane range.

    for (int i = 0; i < 256; ++i)
    {
        texGammaLut[i] = (byte) (255.0f * powf((float) i / 255.0f, invGamma));
    }
}

byte R_TexGammaLut(byte colorValue)
{
    return texGammaLut[colorValue];
}
