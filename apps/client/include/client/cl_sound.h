/** @file cl_sound.h  Clientside sounds.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_SOUND_H
#define DE_CLIENT_SOUND_H

#include <doomsday/network/protocol.h>

/**
 * Read a sound delta from the message buffer and play it.
 * Only used with PSV_FRAME2 packets.
 */
void Cl_ReadSoundDelta(deltatype_t type);

/**
 * Called when a PSV_FRAME sound packet is received.
 */
void Cl_Sound();

#endif // DE_CLIENT_SOUND_H
