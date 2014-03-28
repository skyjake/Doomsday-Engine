/** @file gamesessionwriter.h  Serializing game state to a saved session.
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

#ifndef LIBCOMMON_GAMESESSIONWRITER_H
#define LIBCOMMON_GAMESESSIONWRITER_H

#include <de/game/SavedSession>
#include <de/String>

namespace common {

/**
 * Native game state saved session writer.
 *
 * @param saveFolder    Folder in which the new .save package will be written.
 * @param saveFileName  Name of the session .save package being written to.
 * @param metadata      Session metadata to be written. A copy is made.
 *
 * @ingroup libcommon
 */
void writeGameSession(de::Folder &saveFolder, de::String const &saveFileName,
                      de::game::SessionMetadata const &metadata);

}

#endif // LIBCOMMON_GAMESESSIONWRITER_H
