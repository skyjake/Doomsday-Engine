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

#include "saveinfo.h"
#include "gamestatereader.h"

/**
 * Heretic ver 1.3 saved game state reader.
 *
 * @ingroup libheretic
 */
class HereticV13GameStateReader : public IGameStateReader
{
public:
    HereticV13GameStateReader();
    ~HereticV13GameStateReader();

    /**
     * Determines whether the resource file on @a path is interpretable as a game state which can
     * be loaded with a HereticV13GameStateReader.
     *
     * @param info  SaveInfo to attempt to read game session header into.
     * @param path  Path to the resource file to be recognized.
     */
    static bool recognize(SaveInfo &info, Str const *path);

    void read(SaveInfo &info, Str const *path);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBHERETIC_HERETICV13_GAMESTATEREADER
