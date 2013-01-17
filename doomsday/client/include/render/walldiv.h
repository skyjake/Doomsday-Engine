/** @file walldiv.h Wall geometry divisions.
 * @ingroup render
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_WALLDIV_H
#define LIBDENG_RENDER_WALLDIV_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct walldivs_s;

typedef struct walldivnode_s {
    struct walldivs_s* divs;
    coord_t height;
} walldivnode_t;

coord_t WallDivNode_Height(walldivnode_t* node);
walldivnode_t* WallDivNode_Next(walldivnode_t* node);
walldivnode_t* WallDivNode_Prev(walldivnode_t* node);

/// Maximum number of walldivnode_ts in a walldivs_t dataset.
#define WALLDIVS_MAX_NODES          64

typedef struct walldivs_s {
    uint num;
    struct walldivnode_s nodes[WALLDIVS_MAX_NODES];
} walldivs_t;

uint WallDivs_Size(const walldivs_t* wallDivs);
walldivnode_t* WallDivs_First(walldivs_t* wallDivs);
walldivnode_t* WallDivs_Last(walldivs_t* wallDivs);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_RENDER_WALLDIV_H
