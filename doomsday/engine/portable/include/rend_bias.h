/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * rend_bias.h: Light/Shadow Bias
 */

#ifndef __DOOMSDAY_RENDER_SHADOW_BIAS_H__
#define __DOOMSDAY_RENDER_SHADOW_BIAS_H__

#define MAX_BIAS_LIGHTS   (8 * 32) // Hard limit due to change tracking.
#define MAX_BIAS_AFFECTED 6

typedef struct vilight_s {
    short           source;
    byte            rgb[4];       // Light from an affecting source.
} vilight_t;

// Vertex illumination flags.
#define VIF_LERP         0x1      // Interpolation is in progress.
#define VIF_STILL_UNSEEN 0x2      // The color of the vertex is still unknown.

typedef struct vertexillum_s {
    gl_rgba_t       color;        // Current color of the vertex.
    gl_rgba_t       dest;         // Destination color of the vertex.
    unsigned int    updatetime;   // When the value was calculated.
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
    float           pos[3];
    float           color[3];
    float           intensity;
    float           primaryIntensity;
    float           sectorLevel[2];
    unsigned int    lastUpdateTime;
} source_t;

typedef struct biastracker_s {
    unsigned int changes[MAX_BIAS_LIGHTS / 32];
} biastracker_t;

struct rendpoly_s;

extern int      useBias; // Bias lighting enabled.
extern unsigned int currentTimeSB;

void            SB_Register(void);
void            SB_InitForMap(const char *uniqueId);
void            SB_SegHasMoved(struct seg_s *seg);
void            SB_PlaneHasMoved(struct subsector_s *subsector, uint plane);
void            SB_BeginFrame(void);
void            SB_RendPoly(struct rendpoly_s *poly,
                            float sectorLightLevel,
                            vertexillum_t *illumination,
                            biastracker_t *tracker,
                            biasaffection_t *affected,
                            uint mapElementIndex);
void            SB_EndFrame(void);

uint            SB_NewSourceAt(float x, float y, float z, float size, float minLight,
                               float maxLight, float *rgb);
void            SB_UpdateSource(uint which, float x, float y, float z, float size,
                                float minLight, float maxLight, float *rgb);
void            SB_Delete(uint which);
void            SB_Clear(void);

source_t*       SB_GetSource(int which);
uint            SB_ToIndex(source_t* source);

void            SB_SetColor(float *dest, float *src);
void            HSVtoRGB(float *rgb, float h, float s, float v);

#endif
