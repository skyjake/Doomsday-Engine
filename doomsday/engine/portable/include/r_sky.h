/**\file r_sky.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#include "r_model.h"

#define MAX_SKY_LAYERS                   ( 2 )
#define MAX_SKY_MODELS                   ( 32 )

#define DEFAULT_SKY_HEIGHT               ( .666667f )
#define DEFAULT_SKY_HORIZON_OFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_XOFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_MATERIAL      ( MN_TEXTURES_NAME":SKY1" )
#define DEFAULT_SKY_SPHERE_FADEOUT_LIMIT ( .3f )

typedef struct skymodel_s {
    ded_skymodel_t* def;
    modeldef_t* model;
    int frame;
    int timer;
    int maxTimer;
    float yaw;
} skymodel_t;

extern boolean alwaysDrawSphere;
extern boolean skyModelsInited;
extern skymodel_t skyModels[MAX_SKY_MODELS];

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

/// @return  Unique identifier of the first active sky layer.
int R_SkyFirstActiveLayer(void);

/// @return  Current ambient sky color.
const float* R_SkyAmbientColor(void);

float R_SkyHorizonOffset(void);

float R_SkyHeight(void);

boolean R_SkyLayerActive(int layerId);

float R_SkyLayerFadeoutLimit(int layerId);

boolean R_SkyLayerMasked(int layerId);

material_t* R_SkyLayerMaterial(int layerId);

float R_SkyLayerOffset(int layerId);

void R_SkyLayerSetActive(int layerId, boolean yes);

void R_SkyLayerSetMasked(int layerId, boolean yes);

void R_SkyLayerSetFadeoutLimit(int layerId, float limit);

void R_SkyLayerSetMaterial(int layerId, material_t* material);

void R_SkyLayerSetOffset(int layerId, float offset);

#endif /* LIBDENG_REFRESH_SKY_H */
