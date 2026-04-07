/** @file animgroups.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/animgroups.h"
#include "doomsday/res/resources.h"

#include <de/log.h>

namespace res {

using namespace de;

DE_PIMPL_NOREF(AnimGroups)
{
    List<res::AnimGroup *> animGroups;

    ~Impl()
    {
        clearAllAnimGroups();
    }

    void clearAllAnimGroups()
    {
        deleteAll(animGroups);
        animGroups.clear();
    }
};

AnimGroups &AnimGroups::get()
{
    return Resources::get().animGroups();
}

AnimGroups::AnimGroups() : d(new Impl)
{}

dint AnimGroups::animGroupCount()
{
    return d->animGroups.count();
}

void AnimGroups::clearAllAnimGroups()
{
    d->clearAllAnimGroups();
}

res::AnimGroup &AnimGroups::newAnimGroup(dint flags)
{
    LOG_AS("AnimGroups");
    const dint uniqueId = d->animGroups.count() + 1; // 1-based.
    // Allocating one by one is inefficient but it doesn't really matter.
    d->animGroups.append(new res::AnimGroup(uniqueId, flags));
    return *d->animGroups.last();
}

res::AnimGroup *AnimGroups::animGroup(dint uniqueId)
{
    LOG_AS("AnimGroups::animGroup");
    if (uniqueId > 0 && uniqueId <= d->animGroups.count())
    {
        return d->animGroups.at(uniqueId - 1);
    }
    LOGDEV_RES_WARNING("Invalid group #%i, returning NULL") << uniqueId;
    return nullptr;
}

res::AnimGroup *AnimGroups::animGroupForTexture(const res::TextureManifest &textureManifest)
{
    // Group ids are 1-based.
    // Search backwards to allow patching.
    for (dint i = animGroupCount(); i > 0; i--)
    {
        res::AnimGroup *group = animGroup(i);
        if (group->hasFrameFor(textureManifest))
        {
            return group;
        }
    }
    return nullptr;  // Not found.
}

} // namespace res
