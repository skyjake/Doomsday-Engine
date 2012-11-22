/**\file r_sky.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Sky Management.
 */

#ifndef LIBDENG_REFRESH_SKY_H
#define LIBDENG_REFRESH_SKY_H

#include "resource/models.h"

#define MAX_SKY_LAYERS                   ( 2 )
#define MAX_SKY_MODELS                   ( 32 )

#define DEFAULT_SKY_HEIGHT               ( .666667f )
#define DEFAULT_SKY_HORIZON_OFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_XOFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_MATERIAL      ( MS_TEXTURES_NAME":SKY1" )
#define DEFAULT_SKY_SPHERE_FADEOUT_LIMIT ( .3f )

typedef struct skymodel_s {
    ded_skymodel_t* def;
    modeldef_t* model;
    int frame;
    int timer;
    int maxTimer;
    float yaw;
} skymodel_t;

DENG_EXTERN_C boolean alwaysDrawSphere;
DENG_EXTERN_C boolean skyModelsInited;
DENG_EXTERN_C skymodel_t skyModels[MAX_SKY_MODELS];

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize this module.
void R_SkyInit(void);

/**
 * Precache all resources necessary for rendering the current sky.
 */
void R_SkyPrecache(void);

/**
 * Animate the sky.
 */
void R_SkyTicker(void);

/**
 * Configure the sky based on the specified definition if not @c NULL,
 * otherwise, setup using suitable defaults.
 */
void R_SetupSky(ded_sky_t* sky);

/// @return  Unique identifier of the current Sky's first active layer.
int R_SkyFirstActiveLayer(void);

/// @return  Current ambient sky color.
const ColorRawf* R_SkyAmbientColor(void);

/// @return  Horizon offset for the current Sky.
float R_SkyHorizonOffset(void);

/// @return  Height of the current Sky as a factor [0...1] where @c 1 covers the entire view.
float R_SkyHeight(void);

/// @return  @c true if the identified @a layerId of the current Sky is active.
boolean R_SkyLayerActive(int layerId);

/// @return  Fadeout limit for the identified @a layerId of the current Sky.
float R_SkyLayerFadeoutLimit(int layerId);

/// @return  @c true if the identified @a layerId for the current Sky is masked.
boolean R_SkyLayerMasked(int layerId);

/// @return  Material assigned to the identified @a layerId of the current Sky.
material_t* R_SkyLayerMaterial(int layerId);

/// @return  Horizontal offset for the identified @a layerId of the current Sky.
float R_SkyLayerOffset(int layerId);

/**
 * Change the 'active' state for the identified @a layerId of the current Sky.
 * \post Sky light color is marked for update (deferred).
 */
void R_SkyLayerSetActive(int layerId, boolean yes);

/**
 * Change the 'masked' state for the identified @a layerId of the current Sky.
 * \post Sky light color and layer Material are marked for update (deferred).
 */
void R_SkyLayerSetMasked(int layerId, boolean yes);

/**
 * Change the fadeout limit for the identified @a layerId of the current Sky.
 * \post Sky light color is marked for update (deferred).
 */
void R_SkyLayerSetFadeoutLimit(int layerId, float limit);

/**
 * Change the Material assigned to the identified @a layerId of the current Sky.
 * \post Sky light color and layer Material are marked for update (deferred).
 */
void R_SkyLayerSetMaterial(int layerId, material_t* material);

/**
 * Change the horizontal offset for the identified @a layerId of the current Sky.
 */
void R_SkyLayerSetOffset(int layerId, float offset);

/**
 * Alternative interface for manipulating Sky (layer) properties by name/id.
 */
void R_SkyParams(int layer, int param, void* data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REFRESH_SKY_H */
