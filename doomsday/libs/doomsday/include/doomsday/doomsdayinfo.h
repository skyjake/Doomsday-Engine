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

#pragma once

#include "libdoomsday.h"
#include <de/string.h>
#include <de/nativepath.h>

/**
 * Information about Doomsday Engine and its plugins.
 *
 * @ingroup shell
 */
class LIBDOOMSDAY_PUBLIC DoomsdayInfo
{
public:
    using String = de::String;

    struct Game {
        String title;
        String option; ///< Mode identifier.
    };

    enum OptionType { Toggle, Choice, Text };

    struct LIBDOOMSDAY_PUBLIC GameOption {
        struct LIBDOOMSDAY_PUBLIC Value {
            String value;
            String label;
            String ruleSemantic; // for determining if the option is set

            Value(const String &value = String(), const String &label = String(),
                  const String &ruleSemantic = String())
                : value(value)
                , label(label)
                , ruleSemantic(ruleSemantic)
            {}
        };

        OptionType      type;
        String          title;
        String          command; // e.g., "setmap %s"
        Value           defaultValue;
        de::List<Value> allowedValues;

        GameOption(OptionType type, String title, String command, Value defaultValue = Value(),
                   const de::List<Value> &allowedValues = {});
    };

    /**
     * Returns a list containing all the supported games with the
     * human-presentable titles plus game mode identifiers (for the @c -game
     * option).
     */
    static de::List<Game> allGames();

    static String titleForGame(const String &gameId);

    static de::List<GameOption> gameOptions(const String &gameId);

    static de::NativePath defaultServerRuntimeFolder();
};
