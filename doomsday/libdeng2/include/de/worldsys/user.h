/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_USER_H
#define LIBDENG2_USER_H

#include "../deng.h"
#include "../Id"
#include "../ISerializable"
#include "../Record"

namespace de
{
    /**
     * A player in a game world. The game plugin is responsible for creating
     * concrete User instances. The game plugin can extend the User class with any
     * extra information it needs.
     *
     * The state of the user can be (de)serialized. This is used for transmitting
     * the state to remote parties and when saving it to a savegame.
     *
     * @ingroup world
     */ 
    class LIBDENG2_API User : public ISerializable
    {
    public:
        User();
        
        virtual ~User();

        const Id& id() const { return _id; }

        /**
         * Sets the id of the user.
         *
         * @param id  New identifier.
         */ 
        void setId(const Id& id) { _id = id; }

        const Record& info() const { return _info; }

        Record& info() { return _info; }

        const Variable& info(const String& member) const;

        Variable& info(const String& member);

        /**
         * Returns the name of the player.
         */
        const String name() const;

        /**
         * Sets the name of the user.
         *
         * @param name  Name of the user.
         */
        void setName(const String& name);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
                
    private:
        /// User's id in the session. Assigned by the server's Session.
        Id _id;
        
        /// Description of the user.
        Record _info;
    };
};

#endif /* LIBDENG2_USER_H */
