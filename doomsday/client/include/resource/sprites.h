/** @file sprites.h Logical Sprite Management.
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

class Material;
#ifdef __CLIENT__
class Lumobj;
#endif

/**
 * Sprites are patches with a special naming convention so they can be
 * recognized by R_InitSprites.  The sprite and frame specified by a
 * mobj is range checked at run time.
 *
 * A sprite is a patch_t that is assumed to represent a three dimensional
 * object and may have multiple rotations pre drawn.  Horizontal flipping
 * is used to save space. Some sprites will only have one picture used
 * for all views.
 */

#define SPRITEFRAME_MAX_ANGLES 8

struct spriteframe_t
{
    byte rotate; // 0= no rotations, 1= only front, 2= more...
    Material *mats[8]; // Material to use for view angles 0-7
    byte flip[8]; // Flip (1 = flip) to use for view angles 0-7
};

/**
 * Select an appropriate material for visualizing the sprite given a mobj's
 * angle and relative angle with the viewer (the 'eye').
 *
 * @param sprFrame    spriteframe_t instance.
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
Material *SpriteFrame_Material(spriteframe_t &sprFrame, angle_t mobjAngle,
    angle_t angleToEye, bool noRotation = false, bool *flipX = 0, bool *flipY = 0);

/**
 * Returns the material attributed to the specified rotation.
 *
 * @param sprFrame    spriteframe_t instance.
 * @param rotation    Rotation index/identifier to lookup the material for. The
 *                    valid range is [0...SPRITEFRAME_MAX_ANGLES).
 *
 * Return values:
 * @param flipX       @c true= chosen material should be flipped on the X axis.
 * @param flipY       @c true= chosen material should be flipped on the Y axis.
 *
 * @return  The attributed material otherwise @c 0.
 */
Material *SpriteFrame_Material(spriteframe_t &sprFrame, int rotation = 0,
    bool *flipX = 0, bool *flipY = 0);

struct spritedef_t
{
    char name[5];
    int numFrames;
    spriteframe_t *spriteFrames;
};

/**
 * Lookup a sprite frame by unique @a frame index.
 */
spriteframe_t *SpriteDef_Frame(spritedef_t const &sprDef, int frame);

#ifdef __CLIENT__

/**
 * Produce a luminous object from the sprite configuration. The properties of
 * any resultant lumobj are configured in "sprite-local" space. This means that
 * it will positioned relative to the center of the sprite and must be further
 * configured before adding to the map (i.e., translated to the origin in map
 * space.
 *
 * @param sprDef  SpriteDef instance.
 * @param frame   Animation frame to produce a lumobj for.
 *
 * @return  Newly generated lumobj otherwise @c 0.
 */
Lumobj *SpriteDef_GenerateLumobj(spritedef_t const &sprDef, int frame);

#endif

DENG_EXTERN_C spritedef_t *sprites;
DENG_EXTERN_C int numSprites;

void R_InitSprites();

void R_ShutdownSprites();

spritedef_t *R_SpriteDef(int sprite);

Material *R_MaterialForSprite(int sprite, int frame);

#endif // DENG_RESOURCE_SPRITES_H
