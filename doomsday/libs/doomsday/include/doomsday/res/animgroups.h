/** @file animgroups.h
 *
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_ANIMGROUPS_H
#define LIBDOOMSDAY_RESOURCE_ANIMGROUPS_H

#include "../libdoomsday.h"
#include <de/libcore.h>
#include "animgroup.h"

namespace res {

class AnimGroup;
class TextureManifest;

class LIBDOOMSDAY_PUBLIC AnimGroups
{
public:
    static AnimGroups &get();

public:
    AnimGroups();

    /**
     * Returns the total number of animation/precache groups.
     */
    int animGroupCount();

    /**
     * Destroys all the animation groups.
     */
    void clearAllAnimGroups();

    /**
     * Construct a new animation group.
     *
     * @param flags  @ref animationGroupFlags
     */
    AnimGroup &newAnimGroup(int flags);

    /**
     * Returns the AnimGroup associated with @a uniqueId (1-based); otherwise @c 0.
     */
    AnimGroup *animGroup(int uniqueId);

    AnimGroup *animGroupForTexture(const TextureManifest &textureManifest);

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_ANIMGROUPS_H
