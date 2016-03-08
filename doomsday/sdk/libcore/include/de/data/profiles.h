/** @file profiles.h  Abstract set of persistent profiles.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG2_PROFILES_H
#define DENG2_PROFILES_H

#include <de/String>
#include <de/Info>
#include <functional>

namespace de {

/**
 * Abstract set of persistent profiles.
 *
 * This class is intended to be a base class for more specialized profile
 * collections.
 *
 * Each profile is required to have a unique name.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Profiles
{
public:
    /**
     * Base class for profiles. The derived class implements this with the
     * appropriate contents and serialization. @ingroup data
     */
    class DENG2_PUBLIC AbstractProfile
    {
    public:
        AbstractProfile();

        virtual ~AbstractProfile();

        void setOwner(Profiles *owner);

        Profiles &owner();

        Profiles const &owner() const;

        /**
         * Returns the name of the profile.
         */
        String name() const;

        /**
         * Renames the profile.
         *
         * @param name  New name of the profile.
         *
         * @return @c true, if renamed successfully. The renaming will fail if
         * a profile with the new name already exists.
         */
        bool setName(String const &newName);

        /**
         * Determines if a profile should be considered read-only. The UI
         * should not let the user modify profiles that are read-only.
         *
         * @param name  Profile name.
         *
         * @return @c true, if the profile is read-only.
         */
        bool isReadOnly() const;

        void setReadOnly(bool readOnly);

        virtual bool resetToDefaults() = 0;

        /**
         * Serializes the contents of the profile to a text string using Info
         * source syntax.
         */
        virtual String toInfoSource() const = 0;

        DENG2_AS_IS_METHODS()

    private:
        DENG2_PRIVATE(d)
    };

    DENG2_ERROR(NotFoundError);

public:
    Profiles();

    /**
     * Sets the name this register will use for storing profiles persistently.
     * By default the register has no persistent name and thus will not be
     * stored persistently.
     *
     * In the Config, there will be a record called "Config.(persistentName)"
     * containing relevant information.
     *
     * @param name  Persistent name for the register. Must be file name and
     *              script variable name friendly.
     */
    void setPersistentName(String const &name);

    String persistentName() const;

    bool isPersistent() const;

    /**
     * Lists the names of all the existing profiles.
     */
    StringList profiles() const;

    LoopResult forAll(std::function<LoopResult (AbstractProfile &)> func);

    /**
     * Returns the total number of profiles.
     */
    int count() const;

    /**
     * Finds a profile.
     * @param name  Profile name.
     *
     * @return Profile object, or nullptr if not found.
     */
    AbstractProfile *tryFind(String const &name) const;

    AbstractProfile &find(String const &name) const;

    void clear();

    /**
     * Adds a profile to the set of profiles.
     * @param profile  Profile. Ownership transferred to Profiles.
     */
    void add(AbstractProfile *profile);

    /**
     * Removes a profile. The removed profile is not deleted.
     * @param profile  Profile to remove. Ownership given to caller.
     */
    void remove(AbstractProfile &profile);

    /**
     * Serializes all the profiles to /home/configs/(persistentName).dei. Only
     * non-readonly profiles are written.
     */
    void serialize() const;

    /**
     * Deserializes all the profiles from /profiles/(persistentName).dei and
     * /home/configs/(persistentName).dei.
     *
     * All existing profiles in the collection are deleted beforehand.
     */
    void deserialize();

protected:
    virtual AbstractProfile *profileFromInfoBlock(
            de::Info::BlockElement const &block) = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG2_PROFILES_H
