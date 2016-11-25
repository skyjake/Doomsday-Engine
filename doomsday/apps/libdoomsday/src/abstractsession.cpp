/** @file abstractsession.cpp  Logical game session base class.
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

#include "doomsday/AbstractSession"
#include "doomsday/GameStateFolder"

#include <de/App>
#include <de/FileSystem>
#include <de/Log>
#include <de/Writer>

using namespace de;

static AbstractSession::Profile currentProfile;

AbstractSession::Profile &AbstractSession::profile() //static
{
    /// @todo None current profiles should be stored persistently when the game changes.
    return currentProfile;
}

void AbstractSession::removeSaved(String const &path) //static
{
    if (App::rootFolder().has(path))
    {
        App::rootFolder().removeFile(path);
    }
}

void AbstractSession::copySaved(String const &destPath, String const &sourcePath) //static
{
    if (!destPath.compareWithoutCase(sourcePath)) return;

    LOG_AS("AbstractSession::copySaved");

    removeSaved(destPath);

    GameStateFolder const &original = App::rootFolder().locate<GameStateFolder>(sourcePath);
    GameStateFolder &copied = App::fileSystem().copySerialized(sourcePath, destPath).as<GameStateFolder>();
    copied.cacheMetadata(original.metadata()); // Avoid immediately opening the .save package.
}
