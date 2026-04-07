/** @file lineopening.cpp
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/lineopening.h"
#include "doomsday/world/line.h"
#include "doomsday/world/sector.h"

using namespace de;

lineopening_s::lineopening_s(const world::Line &line)
{
    if (!line.back().hasSector())
    {
        top = bottom = range = lowFloor = 0;
        return;
    }

    const auto *frontSector = line.front().sectorPtr();
    const auto *backSector  = line.back().sectorPtr();
    DE_ASSERT(frontSector);

    if (backSector && backSector->ceiling().height() < frontSector->ceiling().height())
    {
        top = float(backSector->ceiling().height());
    }
    else
    {
        top = float(frontSector->ceiling().height());
    }

    if (backSector && backSector->floor().height() > frontSector->floor().height())
    {
        bottom = float(backSector->floor().height());
    }
    else
    {
        bottom = float(frontSector->floor().height());
    }

    range = top - bottom;

    // Determine the "low floor".
    if (backSector && frontSector->floor().height() > backSector->floor().height())
    {
        lowFloor = float(backSector->floor().height());
    }
    else
    {
        lowFloor = float(frontSector->floor().height());
    }
}

lineopening_s &lineopening_s::operator=(const lineopening_s &other)
{
    top      = other.top;
    bottom   = other.bottom;
    range    = other.range;
    lowFloor = other.lowFloor;
    return *this;
}
