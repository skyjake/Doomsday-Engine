/** @file gameprofiles.cpp  Game profiles.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/gameprofiles.h"

#include <de/Record>

#include <QTextStream>

using namespace de;

static String const VAR_GAME    ("game");
static String const VAR_PACKAGES("packages");

GameProfiles::GameProfiles()
{
    setPersistentName("game");
}

Profiles::AbstractProfile *GameProfiles::profileFromInfoBlock(Info::BlockElement const &block)
{
    std::unique_ptr<Profile> prof(new Profile);

    prof->setGame(block.keyValue(VAR_GAME).text);

    if(Info::ListElement const *pkgs = block.findAs<Info::ListElement>(VAR_PACKAGES))
    {
        StringList ids;
        for(auto const &val : pkgs->values()) ids << val.text;
        prof->setPackages(ids);
    }

    return prof.release();
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(GameProfiles::Profile)
{
    String gameId;
    StringList packages;
};

GameProfiles::Profile::Profile() : d(new Instance)
{}

void GameProfiles::Profile::setGame(String const &id)
{
    d->gameId = id;
}

void GameProfiles::Profile::setPackages(StringList const &packagesInOrder)
{
    d->packages = packagesInOrder;
}

String GameProfiles::Profile::game() const
{
    return d->gameId;
}

StringList GameProfiles::Profile::packages() const
{
    return d->packages;
}

bool GameProfiles::Profile::resetToDefaults()
{
    if(isReadOnly()) return false;

    d->packages.clear();
    return true;
}

String GameProfiles::Profile::toInfoSource() const
{
    String info;
    QTextStream os(&info);
    os.setCodec("UTF-8");

    os << VAR_GAME << ": " << d->gameId << "\n"
       << VAR_PACKAGES << " <" << String::join(d->packages, ", ") << ">";

    return info;
}
