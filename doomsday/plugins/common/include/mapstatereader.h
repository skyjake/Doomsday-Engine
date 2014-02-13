/** @file mapstatereader.h  Saved map state reader.
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

#ifndef LIBCOMMON_MAPSTATEREADER_H
#define LIBCOMMON_MAPSTATEREADER_H

#include "common.h"
#include "dmu_archiveindex.h"
#include <de/Error>

/**
 * Performs saved game map state deserialization.
 *
 * @ingroup libcommon
 * @see MapStateWriter
 */
class MapStateReader
{
public:
    /// Base class for read-related errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    /**
     * @param saveVersion  Logical saved state version number.
     */
    MapStateReader(int saveVersion);

    /**
     * Deserialize the saved map state using the specified @a reader.
     */
    void read(Reader *reader);

    /**
     * Returns the logical map version of the saved map state.
     */
    int mapVersion();

    /**
     * Returns the reader to use when deserializing the saved map state.
     */
    Reader *reader();

    /**
     * Finds and returns a material with the identifier @a serialId.
     *
     * @param serialId  Unique identifier for the material in the material archive.
     * @param group     Used with previous versions of the material archive, which
     *                  separated materials into groups (0= Flats 1= Textures).
     *
     * @return  Pointer to the associated material; otherwise @c 0 (not archived).
     */
    Material *material(materialarchive_serialid_t serialId, int group);

    /**
     * Provides access to the side archive to use when deserializing the map state.
     */
    dmu_lib::SideArchive &sideArchive();

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_MAPSTATEREADER_H
