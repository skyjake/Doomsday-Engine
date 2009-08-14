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

#include "../Thinker"
#include "../Animator"

namespace de
{
    class Thing;
    class User;
    class Record;
    
    /**
     * Movable entity within in a map, represented by a Thing (sprite, 3D model, 
     * or wall segments). Objects by themselves cannot be collided with, as 
     * collision detection is the Thing's responsibility.
     *
     * @ingroup world
     */
    class LIBDENG2_API Object : public Thinker
    {
    public:
        Object();
        
        virtual ~Object();

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        /// Position of the object's origin.
        AnimatorVector3 pos_;

        /// Current speed.
        Vector3f momentum_;
        
        /// Rotation angles for the object (yaw, pitch, roll).
        AnimatorVector3 angles_;

        /// Optional physical representation of the object (modified state).
        /// E.g., a user that is only a spectator doesn't have a Thing.
        Thing* thing_;

        /// Another object this one is resting on.
        Object* onObject_;

        /// This is set only if this object is the representation of a user.
        User* user_;
    };
}

#endif /* LIBDENG2_OBJECT_H */
