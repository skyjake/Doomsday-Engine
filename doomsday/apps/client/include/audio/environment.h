/** @file environment.h  Sound "environment" config for spacial audio effects.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_ENVIRONMENT_H
#define CLIENT_AUDIO_ENVIRONMENT_H

namespace audio {

/**
 * POD structure for representing effective environmental audio characteristics.
 * @ingroup audio
 */
struct Environment
{
    de::dfloat volume  = 0;
    de::dfloat space   = 0;
    de::dfloat decay   = 0;
    de::dfloat damping = 0;
};

}  // namespace audio

#endif  // CLIENT_AUDIO_ENVIRONMENT_H
