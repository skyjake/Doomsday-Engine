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

#ifndef LIBDENG2_THING_H
#define LIBDENG2_THING_H

#include "../String"
#include "../Animator"

namespace de
{
    /**
     * Maintains information about the state of an object in the world.
     * Each Thing instance is constructed based on thing definitions that
     * describe how the thing appears and behaves. Each Thing instance is specific
     * to one object.
     */
    class Thing
    {
    public:
        Thing();
        
    private:
        /// Type identifier.
        String id_;

        /// @todo  Appearance: sprite frame, 3D model, etc.
        ///        Behavior: states, scripts, counters, etc.
        
        /// Overall opacity of the thing (1.0 = fully opaque, 0.0 = invisible).
        Animator opacity_;

        /// Radius of the thing.
        dfloat radius_;
        
        /// Height of the thing.
        dfloat height_;
    };
}

#endif /* LIBDENG2_THING_H */
