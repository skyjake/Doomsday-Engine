/**\file hu_automap.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2012 Daniel Swanson <danij@dengine.net>
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
 * UIAutomap widget.
 */

#ifndef LIBCOMMON_GUI_LIBRARY_UIAUTOMAP_H
#define LIBCOMMON_GUI_LIBRARY_UIAUTOMAP_H

#include "am_map.h"
#include "hu_lib.h"

#define MAX_MAP_POINTS          (10)

/// To be called to register the console commands and variables of this module.
void UIAutomap_Register(void);

extern boolean freezeMapRLs;

/**
 * UIAutomap. UIWidget for displaying a simplified, dynamic interpretation
 * of the current map with navigational interface.
 */

#define UIAUTOMAP_BORDER        4 ///< In fixed 320x200 pixels.

/**
 * @defgroup uiautomapFlags  UIAutomap Flags
 */
///@{
#define AMF_REND_THINGS         0x01
#define AMF_REND_KEYS           0x02
#define AMF_REND_ALLLINES       0x04
#define AMF_REND_SPECIALLINES   0x08
#define AMF_REND_VERTEXES       0x10
#define AMF_REND_LINE_NORMALS   0x20
///@}

// Mapped point of interest.
typedef struct {
    coord_t pos[3];
} guidata_automap_point_t;

typedef struct {
    automapcfg_t* mcfg;
    int scissorState;
    RectRaw scissorRegion;

// DGL display lists:
    DGLuint lists[NUM_MAP_OBJECTLISTS]; // Each list contains one or more of given type of automap obj.
    boolean constructMap; // @c true = force a rebuild of all lists.

// State:
    int flags;
    boolean active;
    boolean reveal;
    boolean pan; // If the map viewer location is currently in free pan mode.
    boolean rotate;

    boolean forceMaxScale; // If the map is currently in forced max zoom mode.
    float priorToMaxScale; // Viewer scale before entering maxScale mode.

    uint followPlayer; // Console player being followed.

    // Used by MTOF to scale from map-to-frame-buffer coords.
    float scaleMTOF;
    // Used by FTOM to scale from frame-buffer-to-map coords (=1/scaleMTOF).
    float scaleFTOM;

// Map bounds:
    float minScale;
    coord_t bounds[4];

// Paramaters for render:
    float alpha, targetAlpha, oldAlpha;
    float alphaTimer;

// Viewer location on the map:
    float viewTimer;
    coord_t viewX, viewY; // Current.
    coord_t targetViewX, targetViewY; // Should be at.
    coord_t oldViewX, oldViewY; // Previous.
    // For the parallax layer.
    coord_t viewPLX, viewPLY; // Current.

// View frame scale:
    float viewScaleTimer;
    float viewScale; // Current.
    float targetViewScale; // Should be at.
    float oldViewScale; // Previous.

    float minScaleMTOF; // Viewer frame scale limits.
    float maxScaleMTOF;

// View frame rotation:
    float angleTimer;
    float angle; // Current.
    float targetAngle; // Should be at.
    float oldAngle; // Previous.

    // Axis-aligned bounding box of the potentially visible area
    // (rotation-aware) in map coordinates.
    coord_t viewAABB[4];

    // Bounding box of the actual visible area in map coordinates.
    coord_t topLeft[2], bottomRight[2], topRight[2], bottomLeft[2];

// Misc:
    coord_t maxViewPositionDelta;
    boolean updateViewScale;

// Mapped points of interest:
    guidata_automap_point_t points[MAX_MAP_POINTS];
    boolean pointsUsed[MAX_MAP_POINTS];
    int pointCount;
} guidata_automap_t;

#ifdef __cplusplus
extern "C" {
#endif

void UIAutomap_LoadResources(void);
void UIAutomap_ReleaseResources(void);

automapcfg_t* UIAutomap_Config(uiwidget_t* obj);
void UIAutomap_Rebuild(uiwidget_t* obj);

void UIAutomap_ClearLists(uiwidget_t* obj);
void UIAutomap_Reset(uiwidget_t* obj);

void UIAutomap_Drawer(uiwidget_t* obj, const Point2Raw* offset);

boolean UIAutomap_Open(uiwidget_t* obj, boolean yes, boolean fast);
void UIAutomap_Ticker(uiwidget_t* obj, timespan_t ticLength);

void UIAutomap_UpdateGeometry(uiwidget_t* obj);

boolean UIAutomap_Active(uiwidget_t* obj);
boolean UIAutomap_Reveal(uiwidget_t* obj);
boolean UIAutomap_SetReveal(uiwidget_t* obj, boolean on);

/**
 * Add a point of interest at this location.
 */
int UIAutomap_AddPoint(uiwidget_t* obj, coord_t x, coord_t y, coord_t z);
boolean UIAutomap_PointOrigin(const uiwidget_t* obj, int pointIdx, coord_t* x, coord_t* y, coord_t* z);
int UIAutomap_PointCount(const uiwidget_t* obj);
void UIAutomap_ClearPoints(uiwidget_t* obj);

int UIAutomap_Flags(const uiwidget_t* obj);

/**
 * @param flags  @see AutomapFlags.
 */
void UIAutomap_SetFlags(uiwidget_t* obj, int flags);

void UIAutomap_SetWorldBounds(uiwidget_t* obj, coord_t lowX, coord_t hiX, coord_t lowY, coord_t hiY);
void UIAutomap_SetMinScale(uiwidget_t* obj, const float scale);

void UIAutomap_CameraOrigin(uiwidget_t* obj, coord_t* x, coord_t* y);
boolean UIAutomap_SetCameraOrigin(uiwidget_t* obj, coord_t x, coord_t y /*, boolean forceInstantly=false*/);
boolean UIAutomap_SetCameraOrigin2(uiwidget_t* obj, coord_t x, coord_t y, boolean forceInstantly);
boolean UIAutomap_TranslateCameraOrigin(uiwidget_t* obj, coord_t x, coord_t y /*, boolean forceInstantly=false*/);
boolean UIAutomap_TranslateCameraOrigin2(uiwidget_t* obj, coord_t x, coord_t y, boolean forceInstantly);

/**
 * @param max  Maximum view position delta in world units.
 */
void UIAutomap_SetCameraOriginFollowMoveDelta(uiwidget_t* obj, coord_t max);

float UIAutomap_CameraAngle(uiwidget_t* obj);
boolean UIAutomap_SetCameraAngle(uiwidget_t* obj, float angle);

boolean UIAutomap_SetScale(uiwidget_t* obj, float scale);

float UIAutomap_Opacity(const uiwidget_t* obj);
boolean UIAutomap_SetOpacity(uiwidget_t* obj, float alpha);

/**
 * Conversion helpers:
 */

/// Scale from automap window to map coordinates.
float UIAutomap_FrameToMap(uiwidget_t* obj, float val);

/// Scale from map to automap window coordinates.
float UIAutomap_MapToFrame(uiwidget_t* obj, float val);

void UIAutomap_VisibleBounds(const uiwidget_t* obj, coord_t topLeft[2], coord_t bottomRight[2], coord_t topRight[2], coord_t bottomLeft[2]);
void UIAutomap_PVisibleAABounds(const uiwidget_t* obj, coord_t* lowX, coord_t* hiX, coord_t* lowY, coord_t* hiY);

boolean UIAutomap_CameraRotation(uiwidget_t* obj);
boolean UIAutomap_SetCameraRotation(uiwidget_t* obj, boolean on);

boolean UIAutomap_PanMode(uiwidget_t* obj);
boolean UIAutomap_SetPanMode(uiwidget_t* obj, boolean on);

mobj_t* UIAutomap_FollowMobj(uiwidget_t* obj);

boolean UIAutomap_ZoomMax(uiwidget_t* obj);
boolean UIAutomap_SetZoomMax(uiwidget_t* obj, boolean on);

void UIAutomap_ParallaxLayerOrigin(uiwidget_t* obj, coord_t* x, coord_t* y);

#ifdef __cplusplus
}
#endif
#endif /* LIBCOMMON_GUI_LIBRARY_UIAUTOMAP_H */
