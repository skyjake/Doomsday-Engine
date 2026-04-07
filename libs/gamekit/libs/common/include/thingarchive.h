/** @file thingarchive.h  Map save state thing archive.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_THINGARCHIVE_H
#define LIBCOMMON_THINGARCHIVE_H

#include "common.h"

/**
 * @ingroup libcommon
 */
class ThingArchive
{
public:
    /// Unique identifier associated with each archived thing.
#if __JHEXEN__
    typedef int SerialId;
#else
    typedef ushort SerialId;
#endif

public:
    ThingArchive(int version = 0);

    int version() const;

    bool excludePlayers() const;

    uint size() const;

    void clear();

    void initForLoad(uint size);

    void initForSave(bool excludePlayers);

    /**
     * To be called when writing a game state to acquire a unique identifier for
     * the specified @a mobj from the thing archive. If the given mobj is already
     * present in the archived, the existing archive Id is returned.
     *
     * @param mobj  Mobj to lookup the archive Id for.
     *
     * @return  Identifier for the specified mobj (may be zero).
     */
    SerialId serialIdFor(const struct mobj_s *mobj);

    void insert(const struct mobj_s *mobj, SerialId serialId);

    /**
     * To be called after reading a game state has been read to lookup a pointer
     * to the mobj which is associated with the specified @a thingId.
     *
     * @param serialId  Unique identifier for the mobj in the thing archive.
     *
     * @return  Pointer to the associated mobj; otherwise @c 0 (not archived).
     */
    struct mobj_s *mobj(SerialId serialId, void *address);

private:
    DE_PRIVATE(d)
};

#endif // LIBCOMMON_P_ACTOR_H
