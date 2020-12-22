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

#ifndef LIBCOMMON_GAMERULES_H
#define LIBCOMMON_GAMERULES_H

typedef enum gfw_gamerule_e {
    GFW_RULE_skill,
    GFW_RULE_fast,
    GFW_RULE_deathmatch,
    GFW_RULE_noMonsters,
    GFW_RULE_respawnMonsters,
    GFW_RULE_randomClasses
} gfw_gamerule_t;

#ifdef __cplusplus

#include "gamesession.h"
#include <de/record.h>
#include <de/string.h>

/**
 * @todo Separate behaviors so that each rule is singular.
 */
class GameRules
{
public:
    // Cached values (read-only):
    struct Values {
        int skill;
        bool fast;
        byte deathmatch;
        bool noMonsters;
        bool respawnMonsters;
#if __JHEXEN__
        bool randomClasses;
#endif
    };
    Values const values{};

    static const char *VAR_skill;
    static const char *VAR_fast;
    static const char *VAR_deathmatch;
    static const char *VAR_noMonsters;
    static const char *VAR_respawnMonsters;
    static const char *VAR_randomClasses;

public:
    GameRules();
    GameRules(const GameRules &other);

    //static GameRules *fromReader(Reader1 *reader);
    static GameRules *fromRecord(const de::Record &rec, const GameRules *defaults = nullptr);

    GameRules &operator = (const GameRules &other);

    de::String description() const;

    de::Record &       asRecord();
    const de::Record & asRecord() const;

    template <typename T>
    void set(const de::String &key, const T &value) {
        asRecord().set(key, value);
    }

    //    void write(Writer1 *writer) const;
    //    void read(Reader1 *reader);

    de::String asText() const;

    void update();

private:
    DE_PRIVATE(d)
};

#define gfw_Rule(name) (gfw_Session()->rules().values.name)
#define GameRules_Set(d, name, value) {    \
    (d).set(GameRules::VAR_##name, value); \
    (d).update();                          \
}
#define gfw_SetRule(name, value) \
    GameRules_Set(gfw_Session()->rules(), name, value)

#else

// C API

typedef void *GameRules;
int gfw_SessionRule(gfw_gamerule_t rule);
#define gfw_Rule(name) gfw_SessionRule(GFW_RULE_##name)

#endif // __cplusplus

#endif // LIBCOMMON_GAMERULES_H
