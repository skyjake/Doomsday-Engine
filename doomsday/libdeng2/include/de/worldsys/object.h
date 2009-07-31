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
 
#ifndef LIBDENG2_OBJECT_H
#define LIBDENG2_OBJECT_H

#include "../ISerializable"
#include "../Enumerator"
#include "../Animator"

namespace de
{
    class Thing;
    class User;
    class Record;
    
    /**
     * Movable entity within in a map, represented by a sprite, 3D model, or wall segments.
     *
     * @ingroup world
     */
    class LIBDENG2_API Object : public ISerializable
    {
    public:
        typedef Enumerator::Type Id;
        
    public:
        Object();
        
        virtual ~Object();

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        Id id_;
        
        /// Position of the object's origin.
        AnimatorVector3 pos_;

        /// Location of the origin within the object's local space.
        AnimatorVector3 origin_;

        /// Current speed.
        Vector3f momentum_;
        
        /// Rotation angles for the object (yaw, pitch, roll).
        AnimatorVector3 angles_;

        /// Optional physical representation of the object (modified state).
        /// E.g., a user that is only a spectator doesn't have a Thing.
        Thing* thing_;

        /// Time when the object was created.
        Time bornAt_;

        /// Another object this one is resting on.
        Object* onObject_;

        /// This is set only if this object is the representation of a user.
        User* user_;

        /// Optional extra information specific to this object (assigned 
        /// in a script, for example).
        Record* info_;
    };
}

#endif /* LIBDENG2_OBJECT_H */
