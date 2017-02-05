/** @file gameprofiles.cpp  Game profiles.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "doomsday/GameStateFolder"

#include <de/App>
#include <de/PackageLoader>
#include <de/Record>

#include <QTextStream>

using namespace de;

static String const VAR_GAME        ("game");
static String const VAR_PACKAGES    ("packages");
static String const VAR_USER_CREATED("userCreated");
static String const VAR_USE_GAME_REQUIREMENTS("useGameRequirements");

static GameProfile nullGameProfile;

DENG2_PIMPL(GameProfiles)
, DENG2_OBSERVES(Games, Addition)
{
    Impl(Public *i) : Base(i) {}

    void gameAdded(Game &game)
    {
        /*
         * Make sure there is a profile matching this game's title. The session
         * configuration for each game is persistently stored using these profiles.
         * (User-created profiles must use different names.)
         */
        if (!self().tryFind(game.title()))
        {
            auto *prof = new Profile(game.title());
            prof->setGame(game.id());
            self().add(prof);
        }
    }
};

GameProfiles::GameProfiles()
    : d(new Impl(this))
{
    setPersistentName("game");
}

void GameProfiles::setGames(Games &games)
{
    games.audienceForAddition() += d;
}

GameProfiles::Profile const &GameProfiles::null()
{
    return nullGameProfile;
}

GameProfile const &GameProfiles::builtInProfile(String const &gameId) const
{
    return find(DoomsdayApp::games()[gameId].title()).as<GameProfile>();
}

LoopResult GameProfiles::forAll(std::function<LoopResult (Profile &)> func)
{
    return Profiles::forAll([&func] (AbstractProfile &prof) -> LoopResult
    {
        if (auto result = func(prof.as<Profile>()))
        {
            return result;
        }
        return LoopContinue;
    });
}

LoopResult GameProfiles::forAll(std::function<LoopResult (Profile const &)> func) const
{
    return Profiles::forAll([&func] (AbstractProfile const &prof) -> LoopResult
    {
        if (auto result = func(prof.as<Profile>()))
        {
            return result;
        }
        return LoopContinue;
    });
}

QList<GameProfile *> GameProfiles::profilesSortedByFamily()
{
    QList<GameProfile *> profs;
    forAll([&profs] (GameProfile &profile)
    {
        profs << &profile;
        return LoopContinue;
    });
    qSort(profs.begin(), profs.end(), [] (GameProfile const *a, GameProfile const *b)
    {
        String family1 = a->game().family();
        String family2 = b->game().family();
        if (family1.isEmpty()) family1 = "other";
        if (family2.isEmpty()) family2 = "other";
        if (family1 == family2)
        {
            return a->name().compareWithoutCase(b->name()) < 0;
        }
        return family1.compareWithoutCase(family2) < 0;
    });
    return profs;
}

QList<GameProfile const *> GameProfiles::allPlayableProfiles() const
{
    QList<GameProfile const *> playable;
    forAll([&playable] (Profile const &prof)
    {
        if (prof.isPlayable()) playable << &prof;
        return LoopContinue;
    });
    return playable;
}

Profiles::AbstractProfile *GameProfiles::profileFromInfoBlock(Info::BlockElement const &block)
{
    std::unique_ptr<Profile> prof(new Profile);

    prof->setGame(block.keyValue(VAR_GAME).text);

    if (Info::ListElement const *pkgs = block.findAs<Info::ListElement>(VAR_PACKAGES))
    {
        StringList ids;
        for (auto const &val : pkgs->values()) ids << val.text;
        prof->setPackages(ids);
    }

    prof->setUserCreated(!block.keyValue(VAR_USER_CREATED).text.compareWithoutCase("True"));
    if (block.contains(VAR_USE_GAME_REQUIREMENTS))
    {
        prof->setUseGameRequirements(!block.keyValue(VAR_USE_GAME_REQUIREMENTS)
                                     .text.compareWithoutCase("True"));
    }

    return prof.release();
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(GameProfiles::Profile)
{
    String gameId;
    StringList packages;
    bool userCreated = false;
    bool useGameRequirements = true;

    Impl() {}

    Impl(Impl const &other)
        : gameId             (other.gameId)
        , packages           (other.packages)
        , userCreated        (other.userCreated)
        , useGameRequirements(other.useGameRequirements)
    {}
};

GameProfiles::Profile::Profile(String const &name)
    : d(new Impl)
{
    setName(name);
}

GameProfiles::Profile::Profile(Profile const &other)
    : AbstractProfile(other)
    , d(new Impl(*other.d))
{}

GameProfiles::Profile &GameProfiles::Profile::operator = (Profile const &other)
{
    AbstractProfile::operator = (other);
    d->gameId = other.d->gameId;
    d->packages = other.d->packages;
    d->userCreated = other.d->userCreated;
    d->useGameRequirements = other.d->useGameRequirements;
    return *this;
}

void GameProfiles::Profile::setGame(String const &id)
{
    if (d->gameId != id)
    {
        d->gameId = id;
        notifyChange();
    }
}

void GameProfiles::Profile::setPackages(StringList const &packagesInOrder)
{
    if (d->packages != packagesInOrder)
    {
        d->packages = packagesInOrder;
        notifyChange();
    }
}

void GameProfiles::Profile::setUserCreated(bool userCreated)
{
    if (d->userCreated != userCreated)
    {
        d->userCreated = userCreated;
        notifyChange();
    }
}

void GameProfiles::Profile::setUseGameRequirements(bool useGameRequirements)
{
    if (d->useGameRequirements != useGameRequirements)
    {
        d->useGameRequirements = useGameRequirements;
        notifyChange();
    }
}

bool GameProfiles::Profile::appendPackage(String const &id)
{
    if (!d->packages.contains(id))
    {
        d->packages << id;
        notifyChange();
        return true;
    }
    return false;
}

Game &GameProfiles::Profile::game() const
{
    return DoomsdayApp::games()[d->gameId];
}

String GameProfiles::Profile::gameId() const
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

bool GameProfiles::Profile::isUsingGameRequirements() const
{
    return d->useGameRequirements;
}

StringList GameProfiles::Profile::allRequiredPackages() const
{
    StringList list;
    if (d->useGameRequirements)
    {
        list = DoomsdayApp::games()[d->gameId].requiredPackages();
    }
    return list + d->packages;
}

StringList GameProfiles::Profile::packagesAffectingGameplay() const
{
    StringList ids = PackageLoader::get().expandDependencies(allRequiredPackages());
    QMutableListIterator<String> iter(ids);
    while (iter.hasNext())
    {
        if (!GameStateFolder::isPackageAffectingGameplay(iter.next()))
        {
            iter.remove();
        }
    }
    return ids;
}

StringList GameProfiles::Profile::unavailablePackages() const
{
    StringList ids;
    for (String const &pkg : allRequiredPackages())
    {
        if (!App::packageLoader().isAvailable(pkg))
        {
            ids << pkg;
        }
    }
    return ids;
}

bool GameProfiles::Profile::isCompatibleWithPackages(StringList const &ids) const
{
    return GameProfiles::arePackageListsCompatible(packagesAffectingGameplay(), ids);
}

bool GameProfiles::Profile::isPlayable() const
{
    for (String const &pkg : allRequiredPackages())
    {
        if (!App::packageLoader().isAvailable(pkg))
            return false;
    }
    return true;
}

void GameProfiles::Profile::loadPackages() const
{
    for (String const &id : allRequiredPackages())
    {
        PackageLoader::get().load(id);
    }
}

void GameProfiles::Profile::unloadPackages() const
{
    StringList const allPackages = allRequiredPackages();
    for (int i = allPackages.size() - 1; i >= 0; --i)
    {
        PackageLoader::get().unload(allPackages.at(i));
    }
}

bool GameProfiles::Profile::resetToDefaults()
{
    if (isReadOnly()) return false;

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
       << VAR_USER_CREATED << ": " << (d->userCreated? "True" : "False") << "\n"
       << VAR_USE_GAME_REQUIREMENTS << ": " << (d->useGameRequirements? "True" : "False");

    return info;
}

bool GameProfiles::arePackageListsCompatible(StringList const &list1, StringList const &list2) // static
{
    if (list1.size() != list2.size()) return false;

    // The package lists must match order and IDs, but currently we ignore the
    // versions.
    for (int i = 0; i < list1.size(); ++i)
    {
        if (!Package::equals(list1.at(i), list2.at(i)))
        {
            return false;
        }
    }
    return true;
}
