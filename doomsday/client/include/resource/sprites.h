/** @file sprites.h  Sprite resource management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef DENG_RESOURCE_SPRITES_H
#define DENG_RESOURCE_SPRITES_H

#include <de/libdeng1.h>
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
    static int const max_angles = 8;

public:
    byte _rotate;       ///< 0= no rotations, 1= only front, 2= more...
    Material *_mats[8]; ///< Material to use for view angles 0-7
    byte _flip[8];      ///< Flip (1 = flip) to use for view angles 0-7

public:
    Sprite();

    /**
     * Select an appropriate material for visualizing the sprite given a mobj's
     * angle and relative angle with the viewer (the 'eye').
     *
     * @param mobjAngle   Angle of the mobj in the map coordinate space.
     * @param angleToEye  Relative angle of the mobj from the view position.
     * @param noRotation  @c true= Ignore rotations and always use the "front".
     *
     * Return values:
     * @param flipX       @c true= chosen material should be flipped on the X axis.
     * @param flipY       @c true= chosen material should be flipped on the Y axis.
     *
     * @return  The chosen material otherwise @c 0.
     */
    Material *material(angle_t mobjAngle, angle_t angleToEye, bool noRotation = false,
                       bool *flipX = 0, bool *flipY = 0) const;

    /**
     * Returns the material attributed to the specified rotation.
     *
     * @param rotation  Rotation index/identifier to lookup the material for. The
     *                  valid range is [0...SPRITEFRAME_MAX_ANGLES)
     *
     * Return values:
     * @param flipX     @c true= chosen material should be flipped on the X axis.
     * @param flipY     @c true= chosen material should be flipped on the Y axis.
     *
     * @return  The attributed material otherwise @c 0.
     */
    Material *material(int rotation = 0, bool *flipX = 0, bool *flipY = 0) const;

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
};

typedef QList<Sprite *> SpriteSet;

void R_InitSprites();

void R_ShutdownSprites();

int R_SpriteCount();

Sprite *R_SpritePtr(int spriteId, int frame);

Sprite &R_Sprite(int spriteId, int frame);

SpriteSet &R_SpriteSet(int spriteId);

#endif // DENG_RESOURCE_SPRITES_H
