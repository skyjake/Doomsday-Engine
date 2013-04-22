/** @file rend_bias.h Light/Shadow Bias.
 *
 * Calculating macro-scale lighting on the fly.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_SHADOWBIAS
#define DENG_RENDER_SHADOWBIAS

#include <de/vector1.h> /// @todo Remove me
#include <de/Vector>

struct rendpoly_s;
struct rvertex_s;
struct ColorRawf_s;

#define MAX_BIAS_LIGHTS     (8 * 32) // Hard limit due to change tracking.
#define MAX_BIAS_TRACKED    (MAX_BIAS_LIGHTS / 8)
#define MAX_BIAS_AFFECTED   (6)

typedef struct vilight_s {
    short source;
    float color[3]; // Light from an affecting source.
} vilight_t;

// Vertex illumination flags.
#define VIF_LERP            0x1 ///< Interpolation is in progress.
#define VIF_STILL_UNSEEN    0x2 ///< The color of the vertex is still unknown.

typedef struct vertexillum_s {
    float color[3]; // Current color of the vertex.
    float dest[3]; // Destination color of the vertex.
    uint updatetime; // When the value was calculated.
    short flags;
    vilight_t casted[MAX_BIAS_AFFECTED];
} vertexillum_t;

typedef struct biasaffection_s {
    short source; // Index of light source.
} biasaffection_t;

// Bias light source macros.
#define BLF_COLOR_OVERRIDE   0x00000001
#define BLF_LOCKED           0x40000000
#define BLF_CHANGED          0x80000000

typedef struct source_s {
    int flags;
    coord_t origin[3];
    float color[3];
    float intensity;
    float primaryIntensity;
    float sectorLevel[2];
    uint lastUpdateTime;
} source_t;

typedef struct biastracker_s {
    uint changes[MAX_BIAS_TRACKED];
} biastracker_t;

/**
 * Stores the data pertaining to vertex lighting for a worldmap, surface.
 */
typedef struct biassurface_s {
    uint updated;
    uint size;
    vertexillum_t *illum; // [size]
    biastracker_t tracker;
    biasaffection_t affected[MAX_BIAS_AFFECTED];

    struct biassurface_s *next;
} biassurface_t;

extern int useBias; // Bias lighting enabled.
extern uint currentTimeSB;

/**
 * Register console variables for Shadow Bias.
 */
void SB_Register(void);

/**
 * Initializes the bias lights according to the loaded Light
 * definitions.
 */
void SB_InitForMap(char const *uniqueId);

void SB_InitVertexIllum(vertexillum_t *villum);

struct biassurface_s *SB_CreateSurface(void);

void SB_DestroySurface(struct biassurface_s *bsuf);

void SB_SurfaceMoved(struct biassurface_s *bsuf);

/**
 * Do initial processing that needs to be done before rendering a
 * frame.  Changed lights cause the tracker bits to the set for all
 * hedges and planes.
 */
void SB_BeginFrame(void);

void SB_EndFrame(void);

/**
 * Surface can be a either a wall or a plane (ceiling or a floor).
 *
 * @param rcolors       Array of colors to be written to.
 * @param bsuf          Bias data for this surface.
 * @param rvertices     Array of vertices to be lit.
 * @param numVertices   Number of vertices (in the array) to be lit.
 * @param normal        Surface normal.
 * @param sectorLightLevel Sector light level.
 * @param mapElement    Ptr to either a HEdge or BspLeaf.
 * @param elmIdx        Used with BspLeafs to select a specific plane.
 */
void SB_RendPoly(struct ColorRawf_s *rcolors, struct biassurface_s *bsuf,
    struct rvertex_s const *rvertices, size_t numVertices, de::Vector3f const &normal,
    float sectorLightLevel, de::MapElement const *mapElement, int elmIdx);

/**
 * Creates a new bias light source and sets the appropriate properties to
 * the values of the passed parameters. The id of the new light source is
 * returned unless there are no free sources available.
 *
 * @param   x           X coordinate of the new light.
 * @param   y           Y coordinate of the new light.
 * @param   z           Z coordinate of the new light.
 * @param   size        Size (strength) of the new light.
 * @param   minLight    Lower sector light level limit.
 * @param   maxLight    Upper sector light level limit.
 * @param   rgb         Ptr to float[3], the color for the new light.
 *
 * @return              The id of the newly created bias light source else -1.
 */
int SB_NewSourceAt(coord_t x, coord_t y, coord_t z, float size, float minLight,
    float maxLight, float *rgb);

/**
 * Same as above but for updating an existing source.
 */
void SB_UpdateSource(int which, coord_t x, coord_t y, coord_t z, float size,
    float minLight, float maxLight, float *rgb);

/**
 * Removes the specified bias light source from the map.
 *
 * @param which         The id of the bias light source to be deleted.
 */
void SB_Delete(int which);

/**
 * Removes ALL bias light sources on the map.
 */
void SB_Clear(void);

/**
 * @return  Ptr to the bias light source for the given id.
 */
source_t *SB_GetSource(int which);

/**
 * Convert bias light source ptr to index.
 */
int SB_ToIndex(source_t *source);

void SB_SetColor(float *dest, float *src);

#endif // DENG_RENDER_SHADOWBIAS
