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

#include "doomsday/GameProfiles"
#include "doomsday/Games"
#include "doomsday/DoomsdayApp"

#include <de/Record>

#include <QTextStream>

using namespace de;

static String const VAR_GAME        ("game");
static String const VAR_PACKAGES    ("packages");
static String const VAR_USER_CREATED("userCreated");

DENG2_PIMPL(GameProfiles)
, DENG2_OBSERVES(Games, Addition)
{
    Instance(Public *i) : Base(i) {}

    void gameAdded(Game &game)
    {
        /*
         * Make sure there is a profile matching this game's title. The session
         * configuration for each game is persistently stored using these profiles.
         * (User-created profiles must use different names.)
         */
        if(!self.tryFind(game.title()))
        {
            auto *prof = new Profile(game.title());
            prof->setGame(game.id());
            self.add(prof);
        }
    }
};

GameProfiles::GameProfiles()
    : d(new Instance(this))
{
    setPersistentName("game");
}

void GameProfiles::setGames(Games &games)
{
    games.audienceForAddition() += d;
}

LoopResult GameProfiles::forAll(std::function<LoopResult (Profile &)> func)
{
    return Profiles::forAll([&func] (AbstractProfile &prof) -> LoopResult
    {
        if(auto result = func(prof.as<Profile>()))
        {
            return result;
        }
        return LoopContinue;
    });
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

    prof->setUserCreated(!block.keyValue(VAR_USER_CREATED).text.compareWithoutCase("True"));

    return prof.release();
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(GameProfiles::Profile)
{
    String gameId;
    StringList packages;
    bool userCreated = false;

    Instance() {}

    Instance(Instance const &other)
        : gameId     (other.gameId)
        , packages   (other.packages)
        , userCreated(other.userCreated)
    {}
};

GameProfiles::Profile::Profile(String const &name)
    : d(new Instance)
{
    setName(name);
}

GameProfiles::Profile::Profile(Profile const &other)
    : AbstractProfile(other)
    , d(new Instance(*other.d))
{}

void GameProfiles::Profile::setGame(String const &id)
{
    d->gameId = id;
}

void GameProfiles::Profile::setPackages(StringList const &packagesInOrder)
{
    d->packages = packagesInOrder;
}

void GameProfiles::Profile::setUserCreated(bool userCreated)
{
    d->userCreated = userCreated;
}

String GameProfiles::Profile::game() const
{
    return d->gameId;
}

StringList GameProfiles::Profile::packages() const
{
    return d->packages;
}

bool GameProfiles::Profile::isUserCreated() const
{
    return d->userCreated;
}

StringList GameProfiles::Profile::allRequiredPackages() const
{
    return DoomsdayApp::games()[d->gameId].requiredPackages() +
           d->packages;
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
       << VAR_PACKAGES << " <" << String::join(d->packages, ", ") << ">\n"
       << VAR_USER_CREATED << ": " << (d->userCreated? "True" : "False");

    return info;
}
