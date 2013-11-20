/** @file sprite.h  3D-Sprite resource.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RESOURCE_SPRITE_H
#define DENG_RESOURCE_SPRITE_H

#include "dd_types.h" // angle_t
#include <de/Error>
#include <de/String>
#include <QList>

class Material;
#ifdef __CLIENT__
class Lumobj;
#endif

/**
 * A sprite is a map entity visualization which approximates a 3D object using
 * a set of 2D images. Each image represents a view of the entity, from a specific
 * view-angle. The illusion of 3D is successfully achieved through matching the
 * relative angle to the viewer with an image which depicts the entity from this
 * angle.
 *
 * Sprite animation sequences are defined elsewhere.
 *
 * @ingroup resource
 */
class Sprite
{
public:
    /// Required view angle is missing. @ingroup errors
    DENG2_ERROR(MissingViewAngleError);

    static int const max_angles = 8;

    struct ViewAngle {
        Material *material;
        bool mirrorX;

        ViewAngle() : material(0), mirrorX(false)
        {}
    };

public:
    Sprite();
    Sprite(Sprite const &other);

    Sprite &operator = (Sprite const &other);

    void newViewAngle(Material *material, uint rotation, bool mirrorX);

    /**
     * Returns @c true iff a view angle is defined for the specified @a rotation.
     *
     * @param rotation  Rotation index/identifier to lookup the material for. The
     *                  valid range is [0..max_angles)
     */
    bool hasViewAngle(int rotation) const;

    /**
     * Returns the view angle for the specified @a rotation.
     *
     * @param rotation  Rotation index/identifier to lookup the material for. The
     *                  valid range is [0..max_angles)
     *
     * @return  The viewAngle associated with the specified rotation.
     */
    ViewAngle const &viewAngle(int rotation) const;

    /**
     * Select an appropriate view angle for visualizing the sprite given a mobj's
     * angle and relative angle with the viewer (the 'eye').
     *
     * @param mobjAngle   Angle of the mobj in the map coordinate space.
     * @param angleToEye  Relative angle of the mobj from the view position.
     * @param noRotation  @c true= Ignore rotations and always use the "front".
     *
     * @return  The viewAngle associated with the chosen rotation.
     */
    ViewAngle const &closestViewAngle(angle_t mobjAngle, angle_t angleToEye,
        bool noRotation = false) const;

#ifdef __CLIENT__
    /**
     * Produce a luminous object from the sprite configuration. The properties
     * of any resultant lumobj are configured in "sprite-local" space. This means
     * that it will positioned relative to the center of the sprite and must be
     * further configured before adding to the map (i.e., translated to the origin
     * in map space).
     *
     * @return  Newly generated lumobj otherwise @c 0.
     */
    Lumobj *generateLumobj() const;
#endif

private:
    DENG2_PRIVATE(d)
};

typedef Sprite::ViewAngle SpriteViewAngle;

#endif // DENG_RESOURCE_SPRITE_H
