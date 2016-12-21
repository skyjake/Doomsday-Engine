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
#include <doomsday/uri.h>

using namespace de;

static AbstractSession::Profile currentProfile;

DENG2_PIMPL_NOREF(AbstractSession)
{
    bool inProgress = false;  ///< @c true: session is in progress / internal.save exists.
    de::Uri mapUri;
    world::IThinkerMapping *thinkerMapping = nullptr;
};

AbstractSession::AbstractSession()
    : d(new Impl)
{}

AbstractSession::~AbstractSession()
{}

void AbstractSession::setInProgress(bool inProgress)
{
    d->inProgress = inProgress;
}

AbstractSession::Profile &AbstractSession::profile() //static
{
    /// @todo None current profiles should be stored persistently when the game changes.
    return currentProfile;
}

bool AbstractSession::hasBegun() const
{
    return d->inProgress;
}

de::Uri AbstractSession::mapUri() const
{
    return hasBegun()? d->mapUri : de::Uri("Maps:", RC_NULL);
}

world::IThinkerMapping const *AbstractSession::thinkerMapping() const
{
    return d->thinkerMapping;
}

void AbstractSession::setThinkerMapping(world::IThinkerMapping *mapping)
{
    d->thinkerMapping = mapping;
}

void AbstractSession::setMapUri(de::Uri const &uri)
{
    d->mapUri = uri;
}

void AbstractSession::removeSaved(String const &path) // static
{
    if (App::rootFolder().has(path))
    {
        App::rootFolder().removeFile(path);
    }
}

void AbstractSession::copySaved(String const &destPath, String const &sourcePath) // static
{
    if (!destPath.compareWithoutCase(sourcePath)) return;

    LOG_AS("AbstractSession::copySaved");

    removeSaved(destPath);

    GameStateFolder const &original = App::rootFolder().locate<GameStateFolder>(sourcePath);
    GameStateFolder &copied = App::fileSystem().copySerialized(sourcePath, destPath).as<GameStateFolder>();
    copied.cacheMetadata(original.metadata()); // Avoid immediately opening the .save package.
}
