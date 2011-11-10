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
#include "m_vector.h"

typedef struct {
    float rgb[3];
    short set, use; /// Is this set? Should it be used?
    float limit; /// .3 by default.
} fadeout_t;

// Sky layer flags.
#define SLF_ENABLED         0x1 // Layer enabled.
#define SLF_MASKED          0x2 // Mask the layer texture.

#define MAXSKYLAYERS        2

#define VALID_SKY_LAYERID(val) ((val) > 0 && (val) <= MAXSKYLAYERS)

typedef struct {
    int flags;
    material_t* material;
    float offset;
    fadeout_t fadeout;

    // Following is configured during layer preparation.
    DGLuint tex;
    int texWidth, texHeight;
    int texMagMode;
} skylayer_t;

typedef struct skymodel_s {
    ded_skymodel_t* def;
    modeldef_t* model;
    int frame;
    int timer;
    int maxTimer;
    float yaw;
} skymodel_t;

typedef struct {
    DGLuint tex;
    int texWidth, texHeight;
    int texMagMode;
    float offset;
} rendskysphereparams_t;

extern boolean alwaysDrawSphere;
extern int firstSkyLayer, activeSkyLayers;

extern boolean skyModelsInited;
extern skymodel_t skyModels[NUM_SKY_MODELS];
extern float skyLightBalance;
extern int skyDetail, skySimple;
extern int skyColumns, skyRows;
extern float skyDist;

/**
 * Register cvars and ccmds for the sky renderer.
 */
void R_SkyRegister(void);

/**
 * Initialize the sky sphere renderer.
 */
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
 * Configure the sky based on the specified definition if not @c NULL, otherwise,
 * setup using suitable defaults.
 */
void R_SetupSky(ded_sky_t* sky);

/**
 * Mark all skies as requiring a full update. Called during engine/renderer reset.
 */
void Rend_SkyReleaseTextures(void);

void R_SetupSkyModels(ded_sky_t* sky);
void R_SetupSkySphereParamsForSkyLayer(rendskysphereparams_t* params, int layer);

/**
 * @return  The current blended ambient sky color.
 */
const float* R_SkyAmbientColor(void);

/**
 * @return  The current sky fadeout (taken from the first active sky layer).
 */
const fadeout_t* R_SkyFadeout(void);

void R_SkyLayerEnable(int layer, boolean yes);
void R_SkyLayerMasked(int layer, boolean yes);
boolean R_SkyLayerIsEnabled(int layer);
boolean R_SkyLayerIsMasked(int layer);
void R_SkyLayerSetMaterial(int layer, struct material_s* material);
void R_SkyLayerSetFadeoutLimit(int layer, float limit);
void R_SkyLayerSetOffset(int layer, float offset);

#endif /* LIBDENG_REFRESH_SKY_H */
