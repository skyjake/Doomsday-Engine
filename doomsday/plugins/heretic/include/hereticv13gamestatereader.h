/** @file hereticv13gamestatereader.h  Heretic ver 1.3 save game reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBHERETIC_HERETICV13_GAMESTATEREADER
#define LIBHERETIC_HERETICV13_GAMESTATEREADER

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include <de/game/IGameStateReader>

/**
 * Heretic ver 1.3 saved game state reader.
 *
 * @ingroup libheretic
 */
class HereticV13GameStateReader : public de::IGameStateReader
{
public:
    HereticV13GameStateReader();
    ~HereticV13GameStateReader();

    static de::IGameStateReader *make();
    static bool recognize(de::Path const &stateFilePath, de::SessionMetadata &metadata);

    void read(de::Path const &stateFilePath, de::Path const &mapStateFilePath,
              de::SessionMetadata const &metadata);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBHERETIC_HERETICV13_GAMESTATEREADER
