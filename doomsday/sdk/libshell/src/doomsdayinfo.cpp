/** @file doomsdayinfo.cpp  Information about Doomsday Engine and its plugins.
 *
 * @todo This information should not be hardcoded. Instead, it should be read
 * from Info files, and some functionality could be determined using Doomsday
 * Script or a shared Game Rules library.
 *
 * @todo Option to order games by release date, name, or some other criteria.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/shell/DoomsdayInfo"
#include <QDir>

namespace de {
namespace shell {

static struct
{
    char const *name;
    char const *mode;
}
const gameTable[] =
{
    { "Shareware DOOM",                         "doom1-share" },
    { "DOOM",                                   "doom1" },
    { "Ultimate DOOM",                          "doom1-ultimate" },
    { "DOOM II",                                "doom2" },
    { "Final DOOM: Plutonia Experiment",        "doom2-plut" },
    { "Final DOOM: TNT Evilution",              "doom2-tnt" },
    { "Chex Quest",                             "chex" },
    { "HacX",                                   "hacx" },

    { "Shareware Heretic",                      "heretic-share" },
    { "Heretic",                                "heretic" },
    { "Heretic: Shadow of the Serpent Riders",  "heretic-ext" },

    { "Hexen v1.1",                             "hexen" },
    { "Hexen v1.0",                             "hexen-v10" },
    { "Hexen: Death Kings of Dark Citadel",     "hexen-dk" },
    { "Hexen Demo",                             "hexen-demo" },

    { nullptr, nullptr }
};

QList<DoomsdayInfo::Game> DoomsdayInfo::allGames()
{
    QList<Game> games;
    for (int i = 0; gameTable[i].name; ++i)
    {
        Game game;
        game.title  = gameTable[i].name;
        game.option = gameTable[i].mode;
        games.append(game);
    }
    return games;
}

String DoomsdayInfo::titleForGame(String const &mode)
{
    for (int i = 0; gameTable[i].name; ++i)
    {
        if (gameTable[i].mode == mode)
            return gameTable[i].name;
    }
    return mode;
}

QList<DoomsdayInfo::GameOption> DoomsdayInfo::gameOptions(String const &gameId)
{
    using GOValue = GameOption::Value;

    QList<GameOption> opts;

    // Common options.
    opts << GameOption(Choice, "Game type", "server-game-deathmatch %1",
                       GOValue(),
                       QList<GOValue>({ GOValue("0", "Co-op", "coop"),
                                        GOValue("1", "Deathmatch", "dm"),
                                        GOValue("2", "Deathmatch II", "dm2") }));

    opts << GameOption(Choice, "Skill level", "server-game-skill %1",
                       GOValue(),
                       QList<GOValue>({ GOValue("0", "Novice", "skill1"),
                                        GOValue("1", "Easy", "skill2"),
                                        GOValue("2", "Normal", "skill3"),
                                        GOValue("3", "Hard", "skill4"),
                                        GOValue("4", "Nightmare", "skill5") }));

    opts << GameOption(Toggle, "Players can jump", "server-game-jump %1",
                       GOValue(),
                       QList<GOValue>({ GOValue("0"),
                                        GOValue("1", "", "jump") }));

    opts << GameOption(Toggle, "Monsters disabled", "server-game-nomonsters %1",
                       GOValue(),
                       QList<GOValue>({ GOValue("0"),
                                        GOValue("1", "", "nomonst") }));

    if (!gameId.startsWith("hexen"))
    {
        opts << GameOption(Toggle, "Respawn monsters", "server-game-respawn %1",
                           GOValue(),
                           QList<GOValue>({ GOValue("0"),
                                            GOValue("1", "", "respawn") }));
    }

    if (gameId.startsWith("doom1"))
    {
        opts << GameOption(Text, "Map", "setmap %1", GOValue("E1M1", "", "mapId"));
    }
    else if (gameId.startsWith("doom2"))
    {
        opts << GameOption(Text, "Map", "setmap %1", GOValue("MAP01", "", "mapId"));
    }
    else if (gameId.startsWith("heretic"))
    {
        opts << GameOption(Text, "Map", "setmap %1", GOValue("E1M1", "", "mapId"));
    }
    else if (gameId.startsWith("hexen"))
    {
        opts << GameOption(Text, "Map", "setmap %1", GOValue("MAP01", "", "mapId"));
    }

    return opts;
}

NativePath DoomsdayInfo::defaultServerRuntimeFolder()
{
#ifdef MACOSX
    return QDir::home().filePath("Library/Application Support/Doomsday Engine/server-runtime");
#elif WIN32
    return QDir::home().filePath("AppData/Local/Deng Team/Doomsday Engine/server-runtime");
#else
    return NativePath(QDir::home().filePath(".doomsday")) / "server-runtime";
#endif
}

DoomsdayInfo::GameOption::GameOption(OptionType type, String title,
                                     String command, Value defaultValue,
                                     QList<Value> allowedValues)
    : type(type)
    , title(title)
    , command(command)
    , defaultValue(defaultValue)
    , allowedValues(allowedValues)
{}

} // namespace shell
} // namespace de
