/** @file animgroup.cpp  Material animation group.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "resource/animgroup.h"

#include <de/Log>
#include <QtAlgorithms>

using namespace de;

AnimGroup::Frame::Frame(TextureManifest &textureManifest, ushort tics, ushort randomTics)
    : _textureManifest(&textureManifest)
    , _tics(tics)
    , _randomTics(randomTics)
{}

TextureManifest &AnimGroup::Frame::textureManifest() const
{
    return *_textureManifest;
}

ushort AnimGroup::Frame::tics() const
{
    return _tics;
}

ushort AnimGroup::Frame::randomTics() const
{
    return _randomTics;
}

DENG2_PIMPL(AnimGroup)
{
    Frames frames;
    int uniqueId;
    int flags; ///< @ref animationGroupFlags

    Instance(Public *i)
        : Base(i)
        , uniqueId(0)
        , flags(0)
    {}

    ~Instance()
    {
        self.clearAllFrames();
    }
};

AnimGroup::AnimGroup(int uniqueId, int flags) : d(new Instance(this))
{
    d->uniqueId = uniqueId;
    d->flags    = flags;
}

void AnimGroup::clearAllFrames()
{
    qDeleteAll(d->frames);
    d->frames.clear();
}

int AnimGroup::id() const
{
    return d->uniqueId;
}

int AnimGroup::flags() const
{
    return d->flags;
}

bool AnimGroup::hasFrameFor(TextureManifest const &textureManifest) const
{
    foreach(Frame *frame, d->frames)
    {
        if(&frame->textureManifest() == &textureManifest)
            return true;
    }
    return false;
}

AnimGroup::Frame &AnimGroup::newFrame(TextureManifest &textureManifest,
    ushort tics, ushort randomTics)
{
    d->frames.append(new Frame(textureManifest, tics, randomTics));
    return *d->frames.last();
}

AnimGroup::Frames const &AnimGroup::allFrames() const
{
    return d->frames;
}
