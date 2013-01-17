/**\file rend_bias.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Light/Shadow Bias
 */

#ifndef LIBDENG_RENDER_SHADOWBIAS_H
#define LIBDENG_RENDER_SHADOWBIAS_H

#include <de/vector1.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rendpoly_s;
struct rvertex_s;
struct ColorRawf_s;

#define MAX_BIAS_LIGHTS     (8 * 32) // Hard limit due to change tracking.
#define MAX_BIAS_TRACKED    (MAX_BIAS_LIGHTS / 8)
#define MAX_BIAS_AFFECTED   (6)

typedef struct vilight_s {
    short           source;
    float           color[3];     // Light from an affecting source.
} vilight_t;

// Vertex illumination flags.
#define VIF_LERP         0x1      // Interpolation is in progress.
#define VIF_STILL_UNSEEN 0x2      // The color of the vertex is still unknown.

typedef struct vertexillum_s {
    float           color[3];     // Current color of the vertex.
    float           dest[3];      // Destination color of the vertex.
    uint            updatetime;   // When the value was calculated.
    short           flags;
    vilight_t       casted[MAX_BIAS_AFFECTED];
} vertexillum_t;

typedef struct biasaffection_s {
    short           source;       // Index of light source.
} biasaffection_t;

// Bias light source macros.
#define BLF_COLOR_OVERRIDE   0x00000001
#define BLF_LOCKED           0x40000000
#define BLF_CHANGED          0x80000000

typedef struct source_s {
    int             flags;
    coord_t         origin[3];
    float           color[3];
    float           intensity;
    float           primaryIntensity;
    float           sectorLevel[2];
    uint            lastUpdateTime;
} source_t;

typedef struct biastracker_s {
    uint            changes[MAX_BIAS_TRACKED];
} biastracker_t;

/**
 * Stores the data pertaining to vertex lighting for a worldmap, surface.
 */
typedef struct biassurface_s {
    uint            updated;
    uint            size;
    vertexillum_t*  illum; // [size]
    biastracker_t   tracker;
    biasaffection_t affected[MAX_BIAS_AFFECTED];

    struct biassurface_s* next;
} biassurface_t;

extern int      useBias; // Bias lighting enabled.
extern uint     currentTimeSB;

void            SB_Register(void);
void            SB_InitForMap(const char* uniqueId);
void            SB_InitVertexIllum(vertexillum_t* villum);

struct biassurface_s* SB_CreateSurface(void);
void            SB_DestroySurface(struct biassurface_s* bsuf);
void            SB_SurfaceMoved(struct biassurface_s* bsuf);

void            SB_BeginFrame(void);
void            SB_RendPoly(struct ColorRawf_s* rcolors,
                            struct biassurface_s* bsuf,
                            const struct rvertex_s* rvertices,
                            size_t numVertices, const vectorcompf_t* normal,
                            float sectorLightLevel,
                            de::MapElement const *mapElement, uint elmIdx);
void            SB_EndFrame(void);

int             SB_NewSourceAt(coord_t x, coord_t y, coord_t z, float size, float minLight,
                               float maxLight, float* rgb);
void            SB_UpdateSource(int which, coord_t x, coord_t y, coord_t z, float size,
                                float minLight, float maxLight, float* rgb);
void            SB_Delete(int which);
void            SB_Clear(void);

source_t*       SB_GetSource(int which);
int             SB_ToIndex(source_t* source);

void            SB_SetColor(float* dest, float* src);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_RENDER_SHADOWBIAS_H
