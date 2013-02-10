/** @file doomsdayinfo.h  Information about Doomsday Engine and its plugins.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBSHELL_DOOMSDAYINFO_H
#define LIBSHELL_DOOMSDAYINFO_H

#include <de/String>
#include <de/NativePath>
#include <QList>

namespace de {
namespace shell {

/**
 * Information about Doomsday Engine and its plugins.
 */
class DoomsdayInfo
{
public:
    struct GameMode
    {
        String title;
        String option; ///< Mode identifier.
    };

    /**
     * Returns a list containing all the supported games with the
     * human-presentable titles plus game mode identifiers (for the @c -game
     * option).
     */
    static QList<GameMode> allGameModes();

    static String titleForGameMode(String const &mode);

    static NativePath defaultServerRuntimeFolder();
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_DOOMSDAYINFO_H
