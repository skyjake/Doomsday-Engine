/** @file doomsdayinfo.h  Information about Doomsday Engine and its plugins.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_DOOMSDAYINFO_H
#define LIBSHELL_DOOMSDAYINFO_H

#include "libshell.h"
#include <de/String>
#include <de/NativePath>
#include <QList>

namespace de { namespace shell {

/**
 * Information about Doomsday Engine and its plugins.
 *
 * @ingroup shell
 */
class LIBSHELL_PUBLIC DoomsdayInfo
{
public:
    struct Game {
        String title;
        String option; ///< Mode identifier.
    };

    enum OptionType { Toggle, Choice, Text };

    struct LIBSHELL_PUBLIC GameOption {
        struct LIBSHELL_PUBLIC Value {
            String value;
            String label;
            String ruleSemantic; // for determining if the option is set

            Value(String const &value = String(), String const &label = String(),
                  String const &ruleSemantic = String())
                : value(value)
                , label(label)
                , ruleSemantic(ruleSemantic)
            {}
        };

        OptionType   type;
        String       title;
        String       command; // e.g., "setmap %1"
        Value        defaultValue;
        QList<Value> allowedValues;

        GameOption(OptionType type, String title, String command, Value defaultValue = Value(),
                   QList<Value> allowedValues = QList<Value>());
    };

    /**
     * Returns a list containing all the supported games with the
     * human-presentable titles plus game mode identifiers (for the @c -game
     * option).
     */
    static QList<Game> allGames();

    static String titleForGame(String const &gameId);

    static QList<GameOption> gameOptions(String const &gameId);

    static NativePath defaultServerRuntimeFolder();
};

}} // namespace de::shell

#endif // LIBSHELL_DOOMSDAYINFO_H
