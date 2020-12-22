/** @file gamerules.cpp  Game rule set.
 *
 * @authors Copyright © 2003-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "gamesession.h"

using namespace de;

/*
 * These keys are used for serialization, so if changed, the only keys still need to be changed
 * when reading data.
 */
const char *GameRules::VAR_skill           = "skill";
const char *GameRules::VAR_fast            = "fast";
const char *GameRules::VAR_deathmatch      = "deathmatch";
const char *GameRules::VAR_noMonsters      = "noMonsters";
const char *GameRules::VAR_respawnMonsters = "respawnMonsters";
const char *GameRules::VAR_randomClasses   = "randomClasses";

DE_PIMPL_NOREF(GameRules)
{
    Record rules {
        Record::withMembers(GameRules::VAR_skill,           2, // medium
                            GameRules::VAR_fast,            false,
                            GameRules::VAR_deathmatch,      0,
                            GameRules::VAR_noMonsters,      false,
                            GameRules::VAR_randomClasses,   false,
                            GameRules::VAR_respawnMonsters, false) };

    Impl() {}

    Impl(const Impl &other)
        : rules(other.rules)
    {}
};

GameRules::GameRules()
    : d(new Impl)
{
    update();
}

GameRules::GameRules(const GameRules &other)
    : d(new Impl(*other.d))
{
    update();
}

//GameRules *GameRules::fromReader(reader_s *reader) // static
//{
//    GameRules *rules = new GameRules;
//    rules->read(reader);
//    return rules;
//}

GameRules *GameRules::fromRecord(const Record &record, const GameRules *defaults) // static
{
    GameRules *gr = new GameRules;
    if (defaults)
    {
        gr->d->rules.copyMembersFrom(defaults->asRecord(), Record::IgnoreDoubleUnderscoreMembers);
    }
    gr->d->rules.copyMembersFrom(record, Record::IgnoreDoubleUnderscoreMembers);
    return gr;
}

Record &GameRules::asRecord()
{
    return d->rules;
}

const Record &GameRules::asRecord() const
{
    return d->rules;
}

GameRules &GameRules::operator = (const GameRules &other)
{
    d->rules = other.d->rules;
    update();
    return *this;
}

String GameRules::description() const
{
    /// @todo Separate co-op behavior to new rules, avoiding netgame test.
    if (IS_NETGAME)
    {
        if(values.deathmatch == 2) return "Deathmatch2";
        if(values.deathmatch)      return "Deathmatch";
        return "Co-op";
    }
    return "Singleplayer";
}

#if 0
void GameRules::write(writer_s *writer) const
{
    DE_ASSERT(writer != 0);

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

void GameRules::read(reader_s *reader)
{
    DE_ASSERT(reader != 0);

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
#endif

String GameRules::asText() const
{
    std::ostringstream os;
    os << "skillmode: " << int(values.skill);
    //os << " jumping: "  << (cfg.common.jumpEnabled ? "yes" : "no");
#if defined(__JHEXEN__)
    os << " random player classes: " << (values.randomClasses ? "yes" : "no");
#endif
    os << " monsters: " << (!values.noMonsters     ? "yes" : "no");
#if !defined(__JHEXEN__)
    os << " (fast: "    << (values.fast            ? "yes" : "no");
    os << " respawn: "  << (values.respawnMonsters ? "yes" : "no") << ")";
#endif
    return os.str();
}

void GameRules::update()
{
    Values *vals = const_cast<Values *>(&values);

    vals->skill           = d->rules.geti(VAR_skill);
    vals->fast            = d->rules.getb(VAR_fast);
    vals->deathmatch      = byte(d->rules.geti(VAR_deathmatch));
    vals->noMonsters      = d->rules.getb(VAR_noMonsters);
    vals->respawnMonsters = d->rules.getb(VAR_respawnMonsters);
#if defined(__JHEXEN__)
    vals->randomClasses = d->rules.getb(VAR_randomClasses);
#endif
}

DE_EXTERN_C int gfw_SessionRule(gfw_gamerule_t rule)
{
    switch (rule)
    {
    case GFW_RULE_skill:            return gfw_Rule(skill);
    case GFW_RULE_fast:             return gfw_Rule(fast);
    case GFW_RULE_deathmatch:       return gfw_Rule(deathmatch);
    case GFW_RULE_noMonsters:       return gfw_Rule(noMonsters);
    case GFW_RULE_respawnMonsters:  return gfw_Rule(respawnMonsters);
#if defined(__JHEXEN__)
    case GFW_RULE_randomClasses:    return gfw_Rule(randomClasses);
#endif
    default: return 0;
    }
}
