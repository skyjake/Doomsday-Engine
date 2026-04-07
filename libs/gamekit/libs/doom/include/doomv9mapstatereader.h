/** @file doomv9mapstatereader.h  Doom ver 1.9 saved game map state reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBDOOM_DOOMV9_MAPSTATEREADER
#define LIBDOOM_DOOMV9_MAPSTATEREADER

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include <doomsday/gamestatefolder.h>

/**
 * Doom ver 1.9 saved game map state reader.
 *
 * @ingroup libdoom
 */
class DoomV9MapStateReader : public GameStateFolder::MapStateReader
{
public:
    DoomV9MapStateReader(const GameStateFolder &session);
    ~DoomV9MapStateReader();

    void read(const de::String &mapUriStr);

    thinker_t *thinkerForPrivateId(de::Id::Type id) const override;

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOM_DOOMV9_MAPSTATEREADER
