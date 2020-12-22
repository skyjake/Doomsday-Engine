/** @file animgroup.h  Material animation group.
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

#ifndef LIBDOOMSDAY_RESOURCE_ANIMATIONGROUP_H
#define LIBDOOMSDAY_RESOURCE_ANIMATIONGROUP_H

#include "texturemanifest.h"
#include <de/list.h>

namespace res {

using namespace de;

/**
 * Material Animation group.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC AnimGroup
{
public:
    /**
     * A single frame in the animation.
     */
    struct LIBDOOMSDAY_PUBLIC Frame
    {
    public:
        /**
         * Returns the texture manifest for the frame.
         */
        TextureManifest &textureManifest() const;

        /**
         * Returns the duration of the frame in tics.
         */
        uint16_t tics() const;

        /**
         * Returns the additional duration of the frame in tics.
         */
        uint16_t randomTics() const;

        friend class AnimGroup;

    private:
        Frame(TextureManifest &textureManifest, uint16_t tics, uint16_t randomTics);

        TextureManifest *_textureManifest;
        uint16_t _tics;
        uint16_t _randomTics;
    };

    typedef List<Frame *> Frames;

public:
    /**
     * Construct a new animation group.
     *
     * @param uniqueId  Unique identifier to associate with the group.
     * @param flags     @ref animationGroupFlags
     */
    AnimGroup(int uniqueId, int flags = 0);

    /**
     * Returns the unique identifier associated with the animation.
     */
    int id() const;

    /**
     * @return @ref animationGroupFlags
     */
    int flags() const;

    /**
     * Returns @c true iff at least one frame in the animation uses the specified
     * @a textureManifest
     *
     * @see frames()
     */
    bool hasFrameFor(const TextureManifest &textureManifest) const;

    /**
     * Append a new frame to the animation.
     *
     * @param texture     Manifest for the texture to use during the frame.
     * @param tics        Duration of the frame in tics.
     * @param randomTics  Additional random duration of the frame in tics.
     *
     * @return  The new frame.
     */
    Frame &newFrame(TextureManifest &textureManifest, uint16_t tics,
                    uint16_t randomTics = 0);

    /**
     * Clear all frames in the animation.
     */
    void clearAllFrames();

    /**
     * Returns the total number of frames in the animation.
     */
    inline int frameCount() const { return allFrames().count(); }

    /**
     * Convenient method of returning a frame in the animation by @a index.
     * It is assumed that the index is within valid [0..frameCount) range.
     *
     * @see frameCount()
     */
    inline Frame &frame(int index) const { return *allFrames().at(index); }

    /**
     * Provides access to the frame list for efficient traversal.
     *
     * @see frame()
     */
    const Frames &allFrames() const;

private:
    DE_PRIVATE(d)
};

typedef AnimGroup::Frame AnimGroupFrame;

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_ANIMATIONGROUP_H
