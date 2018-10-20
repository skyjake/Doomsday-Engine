/** @file audioenvironent.h  Audio environment.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_WORLD_AUDIOENVIRONMENT_H_
#define DE_CLIENT_WORLD_AUDIOENVIRONMENT_H_

#include <de/legacy/types.h>

namespace world {

/**
 * Environmental audio characteristics.
 */
class AudioEnvironment
{
public:
    uint volume  = 0;
    uint space   = 0;
    uint decay   = 0;
    uint damping = 0;

    void reset() { volume = space = decay = damping = 0; }
};

}  // namespace world

#endif  // DE_CLIENT_WORLD_AUDIOENVIRONMENT_H_
