/** @file gameprofiles.h  Game profiles.
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

#ifndef LIBDOOMSDAY_GAMEPROFILES_H
#define LIBDOOMSDAY_GAMEPROFILES_H

#include "libdoomsday.h"

#include <de/Profiles>

class Games;

/**
 * Game configuration profiles.
 */
class LIBDOOMSDAY_PUBLIC GameProfiles : public de::Profiles
{
public:
    /**
     * Game profile. Identifies a specific Game and a set of packages to be loaded.
     * Profiles are serialized as plain text in "/home/configs/game.dei".
     */
    class LIBDOOMSDAY_PUBLIC Profile : public AbstractProfile
    {
    public:
        Profile(de::String const &name = de::String());
        Profile(Profile const &other);

        Profile &operator = (Profile const &other);

        void setGame(de::String const &id);
        void setPackages(de::StringList const &packagesInOrder);
        void setUserCreated(bool userCreated);
        void setUseGameRequirements(bool useGameRequirements);

        de::String game() const;
        de::StringList packages() const;
        bool isUserCreated() const;
        bool isUsingGameRequirements() const;

        /**
         * Returns a list of the game's packages in addition to the profile's
         * configured packages.
         */
        de::StringList allRequiredPackages() const;

        de::StringList packagesAffectingGameplay() const;

        de::StringList unavailablePackages() const;

        bool isCompatibleWithPackages(de::StringList const &ids) const;

        bool isPlayable() const;

        void loadPackages() const;

        void unloadPackages() const;

        virtual bool resetToDefaults() override;
        virtual de::String toInfoSource() const override;

    private:
        DENG2_PRIVATE(d)
    };

public:
    GameProfiles();

    /**
     * Sets the games collection associated with these profiles. Each of the games
     * will get their own matching profile.
     *
     * @param games  Games.
     */
    void setGames(Games &games);

    /**
     * Finds the built-in profile for a particular game.
     * @param gameId  Game identifier.
     * @return Profile.
     */
    Profile const &builtInProfile(de::String const &gameId) const;

    de::LoopResult forAll(std::function<de::LoopResult (Profile &)> func);
    de::LoopResult forAll(std::function<de::LoopResult (Profile const &)> func) const;

    QList<Profile const *> allPlayableProfiles() const;

    static Profile const &null();

    static bool arePackageListsCompatible(de::StringList const &list1,
                                          de::StringList const &list2);

protected:
    AbstractProfile *profileFromInfoBlock(de::Info::BlockElement const &block);

private:
    DENG2_PRIVATE(d)
};

typedef GameProfiles::Profile GameProfile;

#endif // LIBDOOMSDAY_GAMEPROFILES_H
