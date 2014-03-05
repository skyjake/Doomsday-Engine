/** @file gamerules.h  Game rule set.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_GAMERULES_H
#define DENG_CLIENT_GAMERULES_H

#ifdef __cplusplus

#include "doomsday.h"
#include <de/Record>
#include <de/String>

/**
 * @todo Separate behaviors so that each rule is singular.
 */
class GameRuleset
{
public:
    int skill;
#if !__JHEXEN__
    byte fast;
#endif
    byte deathmatch;
    byte noMonsters;
#if __JHEXEN__
    byte randomClasses;
#else
    byte respawnMonsters;
#endif

public:
    GameRuleset();
    GameRuleset(GameRuleset const &other);

    static GameRuleset *fromReader(Reader *reader);
    static GameRuleset *fromRecord(de::Record const &rec);

    GameRuleset &operator = (GameRuleset const &other);

    de::String description() const;

    de::Record *toRecord() const;

    void write(Writer *writer) const;
    void read(Reader *reader);

    de::String asText() const;
};

#else
typedef void *GameRuleset;
#endif

#endif // DENG_CLIENT_GAMERULES_H
