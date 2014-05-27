/** @file vissprite.h  Vissprite pool.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_VISSPRITE_H
#define DENG_CLIENT_RENDER_VISSPRITE_H

#include "render/billboard.h"
#include "render/ivissprite.h"
#include "render/rend_model.h"
#include <QList>

/**
 * Vissprite pool.
 *
 * @ingroup render
 */
class VisspritePool
{
public:
    // Sorted list of vissprites.
    typedef QList<IVissprite *> SortedVissprites;

public:
    VisspritePool();

    /**
     * To be called at the start of a render frame to return all vissprites
     * currently in use to the unused list.
     */
    void reset();

    /**
     * Retrieve an unused flare from the vissprite pool (ownership is retained).
     */
    visflare_t *newFlare();

    /**
     * Retrieve an unused masked wall from the vissprite pool (ownership is retained).
     */
    vismaskedwall_t *newMaskedWall();

    /**
     * Retrieve an unused model from the vissprite pool (ownership is retained).
     */
    vismodel_t *newModel();

    /**
     * Retrieve an unused sprite from the vissprite pool (ownership is retained).
     */
    vissprite_t *newSprite();

    /**
     * Provides access to the sorted list of @em all vissprites currently in use.
     */
    SortedVissprites const &sorted() const;

private:
    DENG2_PRIVATE(d)
};

// -----------------------------------------------------------------------------

#include "BspLeaf"

struct ModelDef;

typedef enum {
    VPSPR_SPRITE,
    VPSPR_MODEL
} vispspritetype_t;

typedef struct vispsprite_s {
    vispspritetype_t type;
    ddpsprite_t *psp;
    coord_t origin[3];

    union vispsprite_data_u {
        struct vispsprite_sprite_s {
            BspLeaf *bspLeaf;
            float alpha;
            dd_bool isFullBright;
        } sprite;
        struct vispsprite_model_s {
            BspLeaf *bspLeaf;
            coord_t gzt; // global top for silhouette clipping
            int flags; // for color translation and shadow draw
            uint id;
            int selector;
            int pClass; // player class (used in translation)
            coord_t floorClip;
            dd_bool stateFullBright;
            dd_bool viewAligned;    // Align to view plane.
            coord_t secFloor, secCeil;
            float alpha;
            coord_t visOff[3]; // Last-minute offset to coords.
            dd_bool floorAdjust; // Allow moving sprite to match visible floor.

            ModelDef *mf, *nextMF;
            float yaw, pitch;
            float pitchAngleOffset;
            float yawAngleOffset;
            float inter; // Frame interpolation, 0..1
        } model;
    } data;
} vispsprite_t;

DENG_EXTERN_C vispsprite_t visPSprites[DDMAXPSPRITES];

#endif // DENG_CLIENT_RENDER_VISSPRITE_H
