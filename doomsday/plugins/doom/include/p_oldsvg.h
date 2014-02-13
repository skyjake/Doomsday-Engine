/** @file p_oldsvg.h  Doom ver 1.9 saved game state reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBDOOM_DOOMV9_GAMESTATEREADER
#define LIBDOOM_DOOMV9_GAMESTATEREADER

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "saveinfo.h"

/**
 * Doom ver 1.9 saved game state reader.
 *
 * @ingroup libdoom
 */
class DoomV9GameStateReader
{
public:
    /// An error occurred attempting to open the input file. @ingroup errors
    DENG2_ERROR(FileAccessError);

    /// Base class for read errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    /**
     * Determines whether the resource file on @a path is interpretable as a game state which can
     * be loaded with a DoomV9GameStateReader.
     *
     * @param info  SaveInfo to attempt to read game session header into.
     * @param path  Path to the resource file to be recognized.
     */
    static bool recognize(SaveInfo &info, Str const *path);

    void read(SaveInfo &info, Str const *path);
};

#endif // LIBDOOM_DOOMV9_GAMESTATEREADER
