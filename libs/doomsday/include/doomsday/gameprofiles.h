/** @file gameprofiles.h  Game profiles.
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

#ifndef LIBDOOMSDAY_GAMEPROFILES_H
#define LIBDOOMSDAY_GAMEPROFILES_H

#include "libdoomsday.h"

#include <de/scripting/iobject.h>
#include <de/profiles.h>

class Game;
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
     *
     * When a custom data file is set, any normally required packages with the "gamedata"
     * tag are ignored. The assumption is that the custom data file provides everything
     * that is provided by those default gamedata packages.
     */
    class LIBDOOMSDAY_PUBLIC Profile : public AbstractProfile, public de::IObject
    {
    public:
        Profile(const de::String &name = de::String());
        Profile(const Profile &other);

        Profile &operator = (const Profile &other);

        void setGame(const de::String &id);
        void setCustomDataFile(const de::String &id);
        void setPackages(const de::StringList& packagesInOrder);
        void setUserCreated(bool userCreated);
        void setUseGameRequirements(bool useGameRequirements);
        void setAutoStartMap(const de::String &map);
        void setAutoStartSkill(int level);
        void setLastPlayedAt(const de::Time &at = de::Time());
        void setSaveLocationId(de::duint32 saveLocationId);
        void setOptionValue(const de::String &option, const de::Value &value);

        bool appendPackage(const de::String &id);

        de::String gameId() const;
        Game &game() const;
        de::String customDataFile() const;
        de::StringList packages() const;
        bool isUserCreated() const;
        bool isUsingGameRequirements() const;
        de::String autoStartMap() const;
        int autoStartSkill() const;
        de::Time lastPlayedAt() const;
        de::duint32 saveLocationId() const;
        de::String savePath() const;
        const de::Value &optionValue(const de::String &option) const;

        void createSaveLocation();
        void destroySaveLocation();
        void checkSaveLocation() const;
        bool isSaveLocationEmpty() const;

        /**
         * Returns a list of the game's packages in addition to the profile's
         * configured packages.
         */
        de::StringList allRequiredPackages() const;

        de::StringList packagesAffectingGameplay() const;

        de::StringList unavailablePackages() const;

        bool isCompatibleWithPackages(const de::StringList &ids) const;

        bool isPlayable() const;

        /**
         * Checks for auto-versioned packages where the specified version is not available,
         * but a newer auto-versioned package is available. If so, the newer package
         * replaces the old one.
         */
        void upgradePackages();

        void loadPackages() const;

        void unloadPackages() const;

        virtual bool       resetToDefaults() override;
        virtual de::String toInfoSource() const override;

        // Implements IObject.
        de::Record &      objectNamespace() override;
        const de::Record &objectNamespace() const override;

    private:
        DE_PRIVATE(d)
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
    const Profile &builtInProfile(const de::String &gameId) const;

    de::LoopResult forAll(std::function<de::LoopResult (Profile &)> func);
    de::LoopResult forAll(std::function<de::LoopResult (const Profile &)> func) const;

    de::List<const Profile *> allPlayableProfiles() const;
    de::List<Profile *> profilesInFamily(const de::String &family);
    de::List<Profile *> profilesSortedByFamily();

    static const Profile &null();

    static bool arePackageListsCompatible(const de::StringList &list1,
                                          const de::StringList &list2);

protected:
    AbstractProfile *profileFromInfoBlock(const de::Info::BlockElement &block);

private:
    DE_PRIVATE(d)
};

typedef GameProfiles::Profile GameProfile;

#endif // LIBDOOMSDAY_GAMEPROFILES_H
