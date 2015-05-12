/** @file vectorlightdata.h  Vector light source data.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RENDER_VECTORLIGHTDATA_H
#define CLIENT_RENDER_VECTORLIGHTDATA_H

#include <de/Vector>

/**
 * POD for a vector light source affection.
 * @ingroup render
 */
struct VectorLightData
{
    de::dfloat approxDist;   ///< Only an approximation.
    de::Vector3f direction;  ///< Normalized vector from light origin to illumination point.
    de::Vector3f color;      ///< How intense the light is (0..1, RGB).
    de::dfloat offset;
    de::dfloat lightSide;
    de::dfloat darkSide;     ///< Factors for world light.
    bool affectedByAmbient;
};

#endif  // CLIENT_RENDER_VECTORLIGHTDATA_H
