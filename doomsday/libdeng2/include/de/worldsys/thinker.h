/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_THINKER_H
#define LIBDENG2_THINKER_H

#include "../ISerializable"
#include "../Time"
#include "../Enumerator"

namespace de
{
    class Record;
    
    /**
     * An independent entity that periodically executes a "thinker" function
     * to update its state and perform actions. Games will derive thinkers
     * specific to their built-in world behaviors.
     *
     * @ingroup world
     */
    class Thinker : public ISerializable
    {
    public:
        typedef Enumerator::Type Id;

    public:
        /**
         * Constructs a new thinker. Note that the ID is not initialized here
         * (an enumerator will generate it, or it will be deserialized).
         */
        Thinker();
        
        virtual ~Thinker();
        
        /**
         * Perform thinking. If there is a function called "thinker" present
         * in the private namespace, it will be automatically called.
         * 
         * @param elapsed  Time elapsed since the last think() call.
         */
        virtual void think(const Time::Delta& elapsed);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);

    private:
        /// Unique identifier for the thinker.
        Id id_;
        
        /// Time when the thinker was initially created.
        Time bornAt_;

        /// Optional thinker-specific namespace.
        Record* info_;
    };
}

#endif /* LIBDENG2_THINKER_H */
