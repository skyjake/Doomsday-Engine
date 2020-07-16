/** @file hereticv13mapstatereader.h  Heretic ver 1.3 saved game map state reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef LIBHERETIC_HERETICV13_MAPSTATEREADER
#define LIBHERETIC_HERETICV13_MAPSTATEREADER

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include <doomsday/gamestatefolder.h>

/**
 * Heretic ver 1.3 saved game map state reader.
 *
 * @ingroup libheretic
 */
class HereticV13MapStateReader : public GameStateFolder::MapStateReader
{
public:
    HereticV13MapStateReader(const GameStateFolder &session);
    ~HereticV13MapStateReader();

    void read(const de::String &mapUriStr);

    thinker_t *thinkerForPrivateId(de::Id::Type id) const override;

private:
    DE_PRIVATE(d)
};

#endif // LIBHERETIC_HERETICV13_MAPSTATEREADER
