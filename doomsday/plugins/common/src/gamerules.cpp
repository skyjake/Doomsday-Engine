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

using namespace de;

GameRuleset::GameRuleset()
    : skill(0)
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

GameRuleset *GameRuleset::fromReader(reader_s *reader) // static
{
    GameRuleset *rules = new GameRuleset;
    rules->read(reader);
    return rules;
}

GameRuleset *GameRuleset::fromRecord(Record const &record, GameRuleset const *defaults) // static
{
    GameRuleset *rules = new GameRuleset;

    Record const *rec = &record;
    if(defaults)
    {
        Record *merged = defaults->toRecord();
        merged->copyMembersFrom(record);
        rec = merged;
    }

    /// @todo Info keys are converted to lowercase when parsed.
    if(!defaults || rec->has("skill"))           rules->skill           = rec->geti("skill");
#if !__JHEXEN__
    if(!defaults || rec->has("fast"))            rules->fast            = byte( rec->getb("fast") );
#endif
    if(!defaults || rec->has("deathmatch"))      rules->deathmatch      = byte( rec->geti("deathmatch") );
    if(!defaults || rec->has("noMonsters"))      rules->noMonsters      = byte( rec->getb("noMonsters") );
#if __JHEXEN__
    if(!defaults || rec->has("randomClasses"))   rules->randomClasses   = byte( rec->getb("randomClasses") );
#else
    if(!defaults || rec->has("respawnMonsters")) rules->respawnMonsters = byte( rec->getb("respawnMonsters") );
#endif

    if(rec != &record) delete rec;
    return rules;
}

Record *GameRuleset::toRecord() const
{
    Record *rec = new Record;

    rec->addNumber ("skill",           skill);
#if !__JHEXEN__
    rec->addBoolean("fast",            CPP_BOOL(fast));
#endif
    rec->addNumber ("deathmatch",      deathmatch);
    rec->addBoolean("noMonsters",      CPP_BOOL(noMonsters));
#if __JHEXEN__
    rec->addBoolean("randomClasses",   CPP_BOOL(randomClasses));
#else
    rec->addBoolean("respawnMonsters", CPP_BOOL(respawnMonsters));
#endif

    return rec;
}

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

String GameRuleset::description() const
{
    /// @todo Separate co-op behavior to new rules, avoiding netgame test.
    if(IS_NETGAME)
    {
        if(deathmatch == 2) return "Deathmatch2";
        if(deathmatch)      return "Deathmatch";
        return "Co-op";
    }
    return "Singleplayer";
}

void GameRuleset::write(writer_s *writer) const
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

void GameRuleset::read(reader_s *reader)
{
    DENG2_ASSERT(reader != 0);

    skill           = Reader_ReadByte(reader);
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

String GameRuleset::asText() const
{
    String str;
    QTextStream os(&str);
    os << "skillmode: " << int(skill);
    //os << " jumping: "  << (cfg.jumpEnabled ? "yes" : "no");
#if __JHEXEN__
    os << " random player classes: " << (randomClasses ? "yes" : "no");
#endif
    os << " monsters: " << (!noMonsters     ? "yes" : "no");
#if !__JHEXEN__
    os << " (fast: "    << (fast            ? "yes" : "no");
    os << " respawn: "  << (respawnMonsters ? "yes" : "no") << ")";
#endif
    return str;
}
