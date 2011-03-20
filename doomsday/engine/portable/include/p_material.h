/**\file p_material.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MATERIAL_H
#define LIBDENG_MATERIAL_H

#include "p_dmu.h"

struct texturevariant_s;

// Material texture unit idents:
enum {
    MTU_PRIMARY,
    MTU_DETAIL,
    MTU_REFLECTION,
    MTU_REFLECTION_MASK,
    NUM_MATERIAL_TEXTURE_UNITS
};

typedef struct material_textureunit_s {
    const struct texturevariant_s* tex;
    int             magMode;
    blendmode_t     blendMode; // Currently used only with reflection.
    float           alpha;
    float           scale[2]; // For use with the texture matrix.
    float           offset[2]; /// Texture origin offset in material-space.
} material_textureunit_t;

typedef struct material_snapshot_s {
    short           width, height; // In world units.
    boolean         isOpaque;
    float           glowing;
    boolean         decorated;
    float           color[3]; // Average color (for lighting).
    float           colorAmplified[3]; // Average color amplified (for lighting).
    float           topColor[3]; // Averaged top line color, used for sky fadeouts.
    material_textureunit_t units[NUM_MATERIAL_TEXTURE_UNITS];

/**
 * \todo: the following should be removed once incorporated into the layers (above).
 */
    struct shinydata_s {
        float           minColor[3];
    } shiny;
} material_snapshot_t;

#define MSU(ms, u) ((ms)->units[u])

boolean         Material_GetProperty(const material_t* mat, setargs_t* args);
boolean         Material_SetProperty(material_t* mat, const setargs_t* args);

material_env_class_t Material_GetEnvClass(const material_t* mat);
void            Material_SetEnvClass(material_t* mat, material_env_class_t envClass);

void            Material_SetTranslation(material_t* mat, material_t* current, material_t* next, float inter);

void            Material_Ticker(material_t* mat, timespan_t time);
void            Material_DeleteTextures(material_t* mat);

#endif /* LIBDENG_MATERIAL_H */
