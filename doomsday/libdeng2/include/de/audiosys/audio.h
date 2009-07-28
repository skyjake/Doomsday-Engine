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

#ifndef LIBDENG2_AUDIO_H
#define LIBDENG2_AUDIO_H

#include "../ISubsystem"

/**
 * @defgroup audio Audio Subsystem
 *
 * Classes that provide common generic functionality for all audio subsystems.
 * These also define the interface of the audio subsystem.
 */

namespace de
{
    /**
     * The abstract base class for an audio subsystem.
     *
     * @ingroup audio
     */
    class Audio : public ISubsystem
    {
    public:
        Audio();
        
        virtual ~Audio();
        
    private:
    };      
};

#endif /* LIBDENG2_AUDIO_H */
