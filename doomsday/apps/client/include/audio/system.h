/** @file audio/system.h  Audio subsystem.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_SYSTEM_H
#define CLIENT_AUDIO_SYSTEM_H

#include <de/System>

namespace audio {

/**
 * Client audio subsystem.
 *
 * Singleton: there can only be one instance of the audio system at a time.
 *
 * @ingroup audio
 */
class System : public de::System
{
public:
    static System &get();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

public:
    System();

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

    /**
     * Stop all channels and music, delete the entire sample cache.
     */
    void reset();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SYSTEM_H
