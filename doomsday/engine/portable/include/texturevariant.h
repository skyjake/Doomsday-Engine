/**\file texture.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_GL_TEXTUREVARIANT_H
#define LIBDENG_GL_TEXTUREVARIANT_H

#include "texture.h"

/**
 * @defGroup textureFlags  Texture Flags
 */
/*@{*/
#define TF_ZEROMASK                 0x1 // Zero the alpha of loaded textures.
#define TF_NO_COMPRESSION           0x2 // Do not compress the loaded textures.
#define TF_UPSCALE_AND_SHARPEN      0x4
#define TF_MONOCHROME               0x8
/*@}*/

typedef struct pointlight_analysis_s {
    float originX, originY, brightMul;
    float color[3];
} pointlight_analysis_t;

typedef struct ambientlight_analysis_s {
    float color[3]; // Average color.
    float colorAmplified[3]; // Average color amplified.
} ambientlight_analysis_t;

typedef struct averagecolor_analysis_s {
    float color[3];
} averagecolor_analysis_t;

typedef struct texturevariantspecification_s {
    byte flags; // GLTF_* flags.
    byte border; // In pixels, added to all four edges of the texture.
    short loadFlags; // MLF_* flags.
    union {
        struct {
            boolean pSprite; // @c true, iff this is for use as a psprite.
            int tclass, tmap; // Color translation.
        } sprite;
        struct {
            float contrast;
        } detail;
    } type; // type-specific data.
} texturevariantspecification_t;

typedef enum {
    TEXTUREANALYSIS_FIRST = 0,
    TA_SPRITE_AUTOLIGHT = TEXTUREANALYSIS_FIRST,
    TA_WORLD_AMBIENTLIGHT,
    TA_SKY_TOPCOLOR,
    TEXTURE_ANALYSIS_COUNT
} textureanalysisid_t;

typedef struct texturevariant_s {
    void* analyses[TEXTURE_ANALYSIS_COUNT];
    const texture_t* generalCase;
    boolean isMasked;
    DGLuint glName; // Name of the associated DGL texture.
    float coords[2]; // Prepared texture coordinates.
    texturevariantspecification_t spec;
} texturevariant_t;

#endif /* LIBDENG_GL_TEXTUREVARIANT_H */
