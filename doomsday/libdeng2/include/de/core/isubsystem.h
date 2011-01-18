/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ISUBSYSTEM_H
#define LIBDENG2_ISUBSYSTEM_H

#include "../Time"

namespace de
{
    /**
     * The interface for all subsystems. A subsystem is  semi-autonomous entity (such as 
     * audio playback) whose presence can be optional. Subsystems can be implemented in plugins.
     *
     * @ingroup core
     */
    class LIBDENG2_API ISubsystem
    {
    public:
        virtual ~ISubsystem() {}
        
        /**
         * Update the subsystems state. This is automatically called once for each 
         * subsystem during the main loop iteration. 
         *
         * @param elapsed  The amount of time elapsed since the previous update.
         */
        virtual void update(const Time::Delta& elapsed) = 0;
        
    private:        
    };
}

#endif /* LIBDENG2_ISUBSYSTEM_H */
