/** @file savegameconverter.h  Utility for converting legacy savegames.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_RESOURCE_SAVEGAMECONVERTER_H
#define DENG_RESOURCE_SAVEGAMECONVERTER_H

#include <de/game/SavedSession>
#include <de/Path>

namespace de {

/**
 * Utility for converting legacy savegame file formats (via plugins).
 *
 * @param inputFilePath  Path to the savegame file to be converted.
 * @param session        SavedSession to update if conversion is successful.
 *
 * @return  @c true if conversion completed successfully. Note that this is not a
 * guarantee that the given @a session is now loadable, however.
 *
 * @ingroup resource
 */
bool convertSavegame(de::Path inputFilePath, de::game::SavedSession &session);

}

#endif // DENG_RESOURCE_SAVEGAMECONVERTER_H
