/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2010 Daniel Swanson <danij@dengine.net>
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
 * p_automap.h : The automap.
 */

#ifndef LIBCOMMON_AUTOMAP_H
#define LIBCOMMON_AUTOMAP_H

#define MAX_MAP_POINTS       (10)

// Automap flags:
#define AMF_REND_THINGS         0x01
#define AMF_REND_KEYS           0x02
#define AMF_REND_ALLLINES       0x04
#define AMF_REND_XGLINES        0x08
#define AMF_REND_VERTEXES       0x10
#define AMF_REND_LINE_NORMALS   0x20

typedef struct automapwindow_s {
    // Where the window currently is on screen, and the dimensions.
    float           x, y, width, height;

    // Where the window should be on screen, and the dimensions.
    float           targetX, targetY, targetWidth, targetHeight;
    float           oldX, oldY, oldWidth, oldHeight;

    float           posTimer;
} automapwindow_t;

typedef struct {
    float           pos[3];
} automappoint_t;

typedef struct automap_s {
// State
    int             flags;
    boolean         active;

    boolean         fullScreenMode; // If the map is currently in fullscreen mode.
    boolean         panMode; // If the map viewer location is currently in free pan mode.
    boolean         rotate;

    boolean         forceMaxScale; // If the map is currently in forced max zoom mode.
    float           priorToMaxScale; // Viewer scale before entering maxScale mode.

    // Used by MTOF to scale from map-to-frame-buffer coords.
    float           scaleMTOF;
    // Used by FTOM to scale from frame-buffer-to-map coords (=1/scaleMTOF).
    float           scaleFTOM;

// Map bounds.
    float           minScale;
    float           bounds[4];

// Paramaters for render.
    float           alpha, targetAlpha, oldAlpha;
    float           alphaTimer;

// Automap window (screen space).
    automapwindow_t window;

// Viewer location on the map.
    float           viewTimer;
    float           viewX, viewY; // Current.
    float           targetViewX, targetViewY; // Should be at.
    float           oldViewX, oldViewY; // Previous.
    // For the parallax layer.
    float           viewPLX, viewPLY; // Current.

// View frame scale.
    float           viewScaleTimer;
    float           viewScale; // Current.
    float           targetViewScale; // Should be at.
    float           oldViewScale; // Previous.

    float           minScaleMTOF; // Viewer frame scale limits.
    float           maxScaleMTOF; //

// View frame rotation.
    float           angleTimer;
    float           angle; // Current.
    float           targetAngle; // Should be at.
    float           oldAngle; // Previous.

// Axis-aligned bounding box of the potentially visible area (extended to be rotation-aware) in map coordinates.
    float           viewAABB[4];

// Bounding box of the actual visible area in map coordinates.
    float           topLeft[2], bottomRight[2], topRight[2], bottomLeft[2];

// Misc
    float           maxViewPositionDelta;
    boolean         updateViewScale;

    // Marked map points.
    automappoint_t  markpoints[MAX_MAP_POINTS];
    boolean         markpointsUsed[MAX_MAP_POINTS];
    unsigned int    markpointnum; // next point to be assigned.
} automap_t;

#ifdef __cplusplus
extern "C" {
#endif

void            Automap_Open(automap_t* map, int yes, int fast);
void            Automap_RunTic(automap_t* map, timespan_t ticLength);
void            Automap_UpdateWindow(automap_t* map, float newX, float newY, float newWidth, float newHeight);

int             Automap_IsActive(const automap_t* map);

int             Automap_AddMark(automap_t* map, float x, float y, float z);
int             Automap_GetMark(const automap_t* map, unsigned int mark, float* x, float* y, float* z);
unsigned int    Automap_GetNumMarks(const automap_t* map);
void            Automap_ClearMarks(automap_t* map);

int             Automap_GetFlags(const automap_t* map);
void            Automap_SetFlags(automap_t* map, int flags);

void            Automap_SetWorldBounds(automap_t* map, float lowX, float hiX, float lowY, float hiY);
void            Automap_SetMinScale(automap_t* map, const float scale);

void            Automap_GetWindow(const automap_t* map, float* x, float* y, float* w, float* h);
void            Automap_SetWindowTarget(automap_t* map, int x, int y, int w, int h);

void            Automap_GetLocation(const automap_t* map, float* x, float* y);
void            Automap_SetMaxLocationTargetDelta(automap_t* map, float max);
void            Automap_SetLocationTarget(automap_t* map, float x, float y);

float           Automap_GetViewAngle(const automap_t* map);
void            Automap_SetViewScaleTarget(automap_t* map, float scale);
void            Automap_SetViewAngleTarget(automap_t* map, float angle);

float           Automap_GetOpacity(const automap_t* map);
void            Automap_SetOpacityTarget(automap_t* map, float alpha);

// Conversion helpers:
float           Automap_FrameToMap(const automap_t* map, float val);
float           Automap_MapToFrame(const automap_t* map, float val);
float           Automap_MapToFrameMultiplier(const automap_t* map);

void            Automap_VisibleBounds(const automap_t* map, float topLeft[2], float bottomRight[2], float topRight[2], float bottomLeft[2]);
void            Automap_PVisibleAABounds(const automap_t* map, float* lowX, float* hiX, float* lowY, float* hiY);

/// \todo
void            Automap_SetWindowFullScreenMode(automap_t* map, int value);
int             Automap_IsMapWindowInFullScreenMode(const automap_t* map);
void            Automap_SetViewRotate(automap_t* map, int offOnToggle);
void            Automap_ToggleFollow(automap_t* map);
void            Automap_ToggleZoomMax(automap_t* map);
void            Automap_GetViewParallaxPosition(const automap_t* map, float* x, float* y);

#ifdef __cplusplus
}
#endif
#endif
