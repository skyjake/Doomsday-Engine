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

void GameRuleset_Write(GameRuleset const *rules, Writer *writer)
{
    DENG_ASSERT(rules != 0 && writer != 0);
    Writer_WriteByte(writer, rules->skill);
    Writer_WriteByte(writer, rules->deathmatch);
#if !__JHEXEN__
    Writer_WriteByte(writer, rules->fast);
#endif
    Writer_WriteByte(writer, rules->noMonsters);
#if __JHEXEN__
    Writer_WriteByte(writer, rules->randomClasses);
#else
    Writer_WriteByte(writer, rules->respawnMonsters);
#endif
}

void GameRuleset_Read(GameRuleset *rules, Reader *reader)
{
    DENG_ASSERT(rules != 0 && reader != 0);

    rules->skill           = (skillmode_t) Reader_ReadByte(reader);
    // Interpret skill modes outside the normal range as "spawn no things".
    if(rules->skill < SM_BABY || rules->skill >= NUM_SKILL_MODES)
    {
        rules->skill = SM_NOTHINGS;
    }

    rules->deathmatch      = Reader_ReadByte(reader);
#if !__JHEXEN__
    rules->fast            = Reader_ReadByte(reader);
#endif
    rules->noMonsters      = Reader_ReadByte(reader);
#if __JHEXEN__
    rules->randomClasses   = Reader_ReadByte(reader);
#else
    rules->respawnMonsters = Reader_ReadByte(reader);
#endif
}
