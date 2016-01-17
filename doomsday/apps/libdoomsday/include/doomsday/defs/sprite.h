/** @file sprite.h  Sprite definition accessor.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_DEFN_SPRITE_H
#define LIBDOOMSDAY_DEFN_SPRITE_H

#include "definition.h"
#include <de/Error>
#include <de/RecordAccessor>

namespace defn {

typedef de::duint32 angle_t;

/**
 * Utility for handling sprite definitions.
 *
 * A sprite is a map entity visualization which approximates a 3D object using a set of
 * 2D images. Each image represents a view of the entity, from a specific view(-angle).
 * The illusion of 3D is successfully achieved through matching the relative angle to the
 * viewer with an image which depicts the entity from this angle.
 *
 * @note Sprite animation sequences are defined elsewhere.
 *
 * @todo Reimplement view(-angle) addressing (spherical coords?).
 */
class LIBDOOMSDAY_PUBLIC Sprite : public Definition
{
public:
    /// Required view is missing. @ingroup errors
    DENG2_ERROR(MissingViewError);

public:
    Sprite()                    : Definition() {}
    Sprite(Sprite const &other) : Definition(other) {}
    Sprite(de::Record &d)       : Definition(d) {}
    Sprite(de::Record const &d) : Definition(d) {}

    void resetToDefaults();

    /**
     * @param angle  @c 0= front, @c 1= one angle turn clockwise, etc...
     */
    de::Record &addView(de::String material, de::dint angle, bool mirrorX = false);

    /**
     * Returns the total number of Views defined for the sprite.
     */
    de::dint viewCount() const;

    /**
     * Returns @c true if a View is defined for the specified @a angle.
     *
     * @param angle  View angle/rotation index/identifier to lookup.
     */
    bool hasView(de::dint angle) const;

    /**
     * Returns the View associated with the specified @a angle.
     *
     * @param angle  View angle/rotation index/identifier to lookup.
     */
    de::Record &view(de::dint angle);

    /**
     * Select an appropriate View for visualizing the entity, given a mobj angle and the
     * relative angle with the viewer (the 'eye').
     *
     * @param mobjAngle   Angle of the mobj in the map coordinate space.
     * @param angleToEye  Relative angle of the mobj from the view position.
     * @param noRotation  @c true= Ignore rotations and always use the "front".
     *
     * @return  View associated with the closest angle.
     */
    de::Record &nearestView(angle_t mobjAngle, angle_t angleToEye, bool noRotation = false);
};

}  // namespace defn

#endif  // LIBDOOMSDAY_DEFN_SPRITE_H
