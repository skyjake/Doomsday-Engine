/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * rend_bias.h: Light/Shadow Bias
 */

#ifndef __DOOMSDAY_RENDER_SHADOW_BIAS_H__
#define __DOOMSDAY_RENDER_SHADOW_BIAS_H__

#include "p_mapdata.h"

#define MAX_BIAS_LIGHTS   (8 * 32) // Hard limit due to change tracking.
#define MAX_BIAS_AFFECTED 6

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
    int             sectorLevel[2];
    unsigned int    lastUpdateTime;
} source_t;

typedef struct biastracker_s {
    unsigned int changes[MAX_BIAS_LIGHTS / 32];
} biastracker_t;

struct vertexillum_s;
struct rendpoly_s;

extern int      useBias; // Bias lighting enabled.
extern unsigned int currentTimeSB;

void            SB_Register(void);
void            SB_InitForLevel(const char *uniqueId);
void            SB_SegHasMoved(seg_t *seg);
void            SB_PlaneHasMoved(subsector_t *subsector, int plane);
void            SB_BeginFrame(void);
void            SB_RendPoly(struct rendpoly_s *poly,
                            surface_t *surface, sector_t *sector,
                            struct vertexillum_s *illumination,
                            biastracker_t *tracker,
                            struct biasaffection_s *affected,
                            int mapElementIndex);
void            SB_EndFrame(void);

int             SB_NewSourceAt(float x, float y, float z, float size, int minLight,
                               int maxLight, float *rgb);
void            SB_UpdateSource(int which, float x, float y, float z, float size,
                                int minLight, int maxLight, float *rgb);
void            SB_Delete(int which);
void            SB_Clear(void);

source_t*       SB_GetSource(int which);
int             SB_ToIndex(source_t* source);

void            SB_SetColor(float *dest, float *src);
void            HSVtoRGB(float *rgb, float h, float s, float v);

#endif
