/** @file mapstatewriter.h  Saved map state writer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_MAPSTATEWRITER_H
#define LIBCOMMON_MAPSTATEWRITER_H

#include "common.h"

/**
 * Performs saved game map state serialization.
 *
 * @ingroup libcommon
 * @see MapStateReader
 */
class MapStateWriter
{
public:
    /**
     * @param excludePlayers  Exclude player data. Used by Hexen when serializing hubs.
     */
    MapStateWriter(bool excludePlayers = false);

    /**
     * Serialize the map state using the specified @a reader.
     */
    void write(Writer *writer);

    /**
     * Returns a unique SerialId for the specified @a material.
     */
    materialarchive_serialid_t serialIdFor(Material *material);

    /**
     * Returns the writer to use when serializing the map state.
     */
    Writer *writer();

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_MAPSTATEWRITER_H
