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

/**
 * Game configuration profiles.
 */
class LIBDOOMSDAY_PUBLIC GameProfiles : public de::Profiles
{
public:
    /**
     * Game profile.
     */
    class LIBDOOMSDAY_PUBLIC Profile : public AbstractProfile
    {
    public:
        Profile();

        void setGame(de::String const &id);
        void setPackages(de::StringList const &packagesInOrder);

        de::String game() const;
        de::StringList packages() const;

        virtual bool resetToDefaults();
        virtual de::String toInfoSource() const;

    private:
        DENG2_PRIVATE(d)
    };

public:
    GameProfiles();

protected:
    AbstractProfile *profileFromInfoBlock(de::Info::BlockElement const &block);
};

#endif // LIBDOOMSDAY_GAMEPROFILES_H
