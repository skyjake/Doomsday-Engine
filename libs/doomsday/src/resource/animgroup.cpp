/** @file animgroup.cpp  Material animation group.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/res/animgroup.h"

#include <de/log.h>

namespace res {

DE_PIMPL_NOREF(AnimGroup)
{
    Frames frames;
    int uniqueId = 0;
    int flags = 0; ///< @ref animationGroupFlags

    ~Impl()
    {
        clearAllFrames();
    }

    void clearAllFrames()
    {
        deleteAll(frames);
        frames.clear();
    }
};

AnimGroup::AnimGroup(int uniqueId, int flags) : d(new Impl)
{
    d->uniqueId = uniqueId;
    d->flags    = flags;
}

void AnimGroup::clearAllFrames()
{
    d->clearAllFrames();
}

int AnimGroup::id() const
{
    return d->uniqueId;
}

int AnimGroup::flags() const
{
    return d->flags;
}

bool AnimGroup::hasFrameFor(const TextureManifest &textureManifest) const
{
    for (Frame *frame : d->frames)
    {
        if (&frame->textureManifest() == &textureManifest)
            return true;
    }
    return false;
}

AnimGroup::Frame &AnimGroup::newFrame(TextureManifest &textureManifest,
                                      uint16_t tics, uint16_t randomTics)
{
    d->frames.append(new Frame(textureManifest, tics, randomTics));
    return *d->frames.last();
}

const AnimGroup::Frames &AnimGroup::allFrames() const
{
    return d->frames;
}

//---------------------------------------------------------------------------------------

AnimGroup::Frame::Frame(TextureManifest &textureManifest, uint16_t tics, uint16_t randomTics)
    : _textureManifest(&textureManifest)
    , _tics(tics)
    , _randomTics(randomTics)
{}

TextureManifest &AnimGroup::Frame::textureManifest() const
{
    return *_textureManifest;
}

uint16_t AnimGroup::Frame::tics() const
{
    return _tics;
}

uint16_t AnimGroup::Frame::randomTics() const
{
    return _randomTics;
}

} // namespace res
