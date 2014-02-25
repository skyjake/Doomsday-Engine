/** @file gamerules.cpp  Game rule set.
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

#include "common.h"
#include "gamerules.h"

GameRuleset::GameRuleset()
    : skill(skillmode_t(0))
#if !__JHEXEN__
    , fast(0)
#endif
    , deathmatch(0)
    , noMonsters(0)
#if __JHEXEN__
    , randomClasses(0)
#else
    , respawnMonsters(0)
#endif
{}

GameRuleset::GameRuleset(GameRuleset const &other)
    : skill          (other.skill)
#if !__JHEXEN__
    , fast           (other.fast)
#endif
    , deathmatch     (other.deathmatch)
    , noMonsters     (other.noMonsters)
#if __JHEXEN__
    , randomClasses  (other.randomClasses)
#else
    , respawnMonsters(other.respawnMonsters)
#endif
{}

GameRuleset &GameRuleset::operator = (GameRuleset const &other)
{
    skill           = other.skill;
#if !__JHEXEN__
    fast            = other.fast;
#endif
    deathmatch      = other.deathmatch;
    noMonsters      = other.noMonsters;
#if __JHEXEN__
    randomClasses   = other.randomClasses;
#else
    respawnMonsters = other.respawnMonsters;
#endif
    return *this;
}

void GameRuleset::write(Writer *writer) const
{
    DENG2_ASSERT(writer != 0);
    Writer_WriteByte(writer, skill);
    Writer_WriteByte(writer, deathmatch);
#if !__JHEXEN__
    Writer_WriteByte(writer, fast);
#endif
    Writer_WriteByte(writer, noMonsters);
#if __JHEXEN__
    Writer_WriteByte(writer, randomClasses);
#else
    Writer_WriteByte(writer, respawnMonsters);
#endif
}

void GameRuleset::read(Reader *reader)
{
    DENG2_ASSERT(reader != 0);

    skill           = (skillmode_t) Reader_ReadByte(reader);
    // Interpret skill modes outside the normal range as "spawn no things".
    if(skill < SM_BABY || skill >= NUM_SKILL_MODES)
    {
        skill = SM_NOTHINGS;
    }

    deathmatch      = Reader_ReadByte(reader);
#if !__JHEXEN__
    fast            = Reader_ReadByte(reader);
#endif
    noMonsters      = Reader_ReadByte(reader);
#if __JHEXEN__
    randomClasses   = Reader_ReadByte(reader);
#else
    respawnMonsters = Reader_ReadByte(reader);
#endif
}

// C wrapper API ---------------------------------------------------------------


skillmode_t GameRuleset_Skill(GameRuleset const *rules)
{
    DENG2_ASSERT(rules != 0);
    return rules->skill;
}

#if !__JHEXEN__
byte GameRuleset_Fast(GameRuleset const *rules)
{
    DENG2_ASSERT(rules != 0);
    return rules->fast;
}
#endif

byte GameRuleset_Deathmatch(GameRuleset const *rules)
{
    DENG2_ASSERT(rules != 0);
    return rules->deathmatch;
}

byte GameRuleset_NoMonsters(GameRuleset const *rules)
{
    DENG2_ASSERT(rules != 0);
    return rules->noMonsters;
}

#if __JHEXEN__
byte GameRuleset_RandomClasses(GameRuleset const *rules)
{
    DENG2_ASSERT(rules != 0);
    return rules->randomClasses;
}
#endif

#if !__JHEXEN__
byte GameRuleset_RespawnMonsters(GameRuleset const *rules)
{
    DENG2_ASSERT(rules != 0);
    return rules->respawnMonsters;
}
#endif
