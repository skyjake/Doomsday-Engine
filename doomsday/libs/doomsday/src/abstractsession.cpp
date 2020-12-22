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

#include "doomsday/abstractsession.h"
#include "doomsday/gamestatefolder.h"

#include <de/app.h>
#include <de/filesystem.h>
#include <de/log.h>
#include <de/writer.h>
#include <doomsday/uri.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/gameprofiles.h>

using namespace de;

//static AbstractSession::Profile currentProfile;

DE_PIMPL_NOREF(AbstractSession)
{
    bool inProgress = false;  ///< @c true: session is in progress / internal.save exists.
    res::Uri mapUri;
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

bool AbstractSession::hasBegun() const
{
    return d->inProgress;
}

res::Uri AbstractSession::mapUri() const
{
    return hasBegun()? d->mapUri : res::makeUri("Maps:");
}

const world::IThinkerMapping *AbstractSession::thinkerMapping() const
{
    return d->thinkerMapping;
}

void AbstractSession::setThinkerMapping(world::IThinkerMapping *mapping)
{
    d->thinkerMapping = mapping;
}

//String AbstractSession::gameId() const
//{
//    const auto *gameProfile = DoomsdayApp::currentGameProfile();
//    DENG_ASSERT(gameProfile != nullptr);
//    return gameProfile->gameId();
//}

void AbstractSession::setMapUri(const res::Uri &uri)
{
    d->mapUri = uri;
}

void AbstractSession::removeSaved(const String &path) // static
{
    if (App::rootFolder().has(path))
    {
        App::rootFolder().destroyFile(path);
    }
}

void AbstractSession::copySaved(const String &destPath, const String &sourcePath) // static
{
    if (!destPath.compareWithoutCase(sourcePath)) return;

    LOG_AS("AbstractSession::copySaved");

    removeSaved(destPath);

    const GameStateFolder &original = App::rootFolder().locate<GameStateFolder>(sourcePath);
    GameStateFolder &copied = FS::copySerialized(sourcePath, destPath).as<GameStateFolder>();
    copied.cacheMetadata(original.metadata()); // Avoid immediately opening the .save package.
}
