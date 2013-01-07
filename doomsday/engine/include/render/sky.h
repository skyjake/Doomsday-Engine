/**
 * @file sky.h Sky Sphere and 3D Models
 *
 * This version supports only two sky layers. (More would be a waste of resources?)
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_SKY_H
#define LIBDENG_RENDER_SKY_H

#include "color.h"
#include "resource/models.h"

#define MAX_SKY_LAYERS                   ( 2 )
#define MAX_SKY_MODELS                   ( 32 )

#define DEFAULT_SKY_HEIGHT               ( .666667f )
#define DEFAULT_SKY_HORIZON_OFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_XOFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_MATERIAL      ( "Textures:SKY1" )
#define DEFAULT_SKY_SPHERE_FADEOUT_LIMIT ( .3f )

#ifdef __cplusplus
extern "C" {
#endif

/// Register the console commands, variables, etc..., of this module.
void Sky_Register(void);

/// Initialize this module.
void Sky_Init(void);

/// Shutdown this module.
void Sky_Shutdown(void);

/**
 * Cache all resources necessary for rendering the current sky.
 */
void Sky_Cache(void);

/**
 * Animate the sky.
 */
void Sky_Ticker(void);

/**
 * Configure the sky based on the specified definition if not @c NULL,
 * otherwise, setup using suitable defaults.
 */
void Sky_Configure(ded_sky_t *sky);

/// @return  Unique identifier of the current Sky's first active layer.
int Sky_FirstActiveLayer(void);

/// @return  Current ambient sky color.
ColorRawf const* Sky_AmbientColor(void);

/// @return  Horizon offset for the current Sky.
float Sky_HorizonOffset(void);

/// @return  Height of the current Sky as a factor [0...1] where @c 1 covers the entire view.
float Sky_Height(void);

/// @return  @c true if the identified @a layerId of the current Sky is active.
boolean Sky_LayerActive(int layerId);

/// @return  Fadeout limit for the identified @a layerId of the current Sky.
float Sky_LayerFadeoutLimit(int layerId);

/// @return  @c true if the identified @a layerId for the current Sky is masked.
boolean Sky_LayerMasked(int layerId);

/// @return  Material assigned to the identified @a layerId of the current Sky.
material_t *Sky_LayerMaterial(int layerId);

/// @return  Horizontal offset for the identified @a layerId of the current Sky.
float Sky_LayerOffset(int layerId);

/**
 * Change the 'active' state for the identified @a layerId of the current Sky.
 * @post Sky light color is marked for update (deferred).
 */
void Sky_LayerSetActive(int layerId, boolean yes);

/**
 * Change the 'masked' state for the identified @a layerId of the current Sky.
 * @post Sky light color and layer Material are marked for update (deferred).
 */
void Sky_LayerSetMasked(int layerId, boolean yes);

/**
 * Change the fadeout limit for the identified @a layerId of the current Sky.
 * @post Sky light color is marked for update (deferred).
 */
void Sky_LayerSetFadeoutLimit(int layerId, float limit);

/**
 * Change the Material assigned to the identified @a layerId of the current Sky.
 * @post Sky light color and layer Material are marked for update (deferred).
 */
void Sky_LayerSetMaterial(int layerId, material_t *material);

/**
 * Change the horizontal offset for the identified @a layerId of the current Sky.
 */
void Sky_LayerSetOffset(int layerId, float offset);

/// Render the current sky.
void Sky_Render(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_SKY_H */
