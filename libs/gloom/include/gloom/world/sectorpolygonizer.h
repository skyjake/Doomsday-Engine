/** @file sectorpolygonizer.h  Construct polygon(s) for a sector.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOM_SECTORPOLYGONIZER_H
#define GLOOM_SECTORPOLYGONIZER_H

#include "gloom/world/map.h"

#include <de/list.h>

namespace gloom {

class SectorPolygonizer
{
public:
    SectorPolygonizer(Map &map);

    void polygonize(ID sector, const de::List<ID> &boundaryLines);

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_SECTORPOLYGONIZER_H
