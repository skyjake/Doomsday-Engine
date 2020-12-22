/** @file s_environ.h Audio environment management.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#pragma once

#include "../libdoomsday.h"
#include <doomsday/uri.h>

enum AudioEnvironmentId
{
    AE_NONE = -1,
    AE_FIRST = 0,
    AE_METAL = AE_FIRST,
    AE_ROCK,
    AE_WOOD,
    AE_CLOTH,
    NUM_AUDIO_ENVIRONMENTS
};

/**
 * Defines the properties of an audio environment.
 */
struct AudioEnvironment
{
    char name[9]; ///< Environment type name.
    int volume;
    int decay;
    int damping;
};

/**
 * Lookup the symbolic name of the identified audio environment.
 */
LIBDOOMSDAY_PUBLIC const char *S_AudioEnvironmentName(AudioEnvironmentId id);

/**
 * Lookup the identified audio environment.
 */
LIBDOOMSDAY_PUBLIC const AudioEnvironment &S_AudioEnvironment(AudioEnvironmentId id);

/**
 * Lookup the audio environment associated with material @a uri. If no environment
 * is defined then @c AE_NONE is returned.
 */
LIBDOOMSDAY_PUBLIC AudioEnvironmentId S_AudioEnvironmentId(const res::Uri *uri);
