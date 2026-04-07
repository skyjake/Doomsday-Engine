/** @file viewports.h  Player viewports and related low-level rendering.
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_VIEWPORTS_H
#define DE_CLIENT_VIEWPORTS_H

#ifdef __SERVER__
#  error "viewports.h is for the client only"
#endif

#include <de/rectangle.h>
#include <de/matrix.h>
#include <de/legacy/rect.h>

namespace world { class ConvexSubspace; }
class Lumobj;
struct Generator;

struct viewport_t
{
    int console;
    de::Rectanglei geometry;
};

struct viewer_t
{
    de::Vec3d origin;
    float pitch;

    viewer_t(const de::Vec3d &origin = de::Vec3d(),
             angle_t angle              = 0,
             float pitch                = 0)
        : origin(origin)
        , pitch(pitch)
        , _angle(angle)
    {}
    viewer_t(const viewer_t &other)
        : origin(other.origin)
        , pitch(other.pitch)
        , _angle(other._angle)
    {}

    viewer_t lerp(const viewer_t &end, float pos) const {
        return viewer_t(de::lerp(origin, end.origin, pos),
                        _angle + int(pos * (int(end._angle) - int(_angle))),
                        de::lerp(pitch,  end.pitch,  pos));
    }
    angle_t angle() const;
    angle_t angleWithoutHeadTracking() const { return _angle; }
    void setAngle(angle_t a) { _angle = a; }

private:
    angle_t _angle;
};

struct viewdata_t
{
    viewer_t current;
    viewer_t lastSharp[2]; ///< For smoothing.
    viewer_t latest; ///< "Sharp" values taken from here.

    /*
     * These vectors are in the DGL coordinate system, which is a left-handed
     * one (same as in the game, but Y and Z have been swapped). Anyone who uses
     * these must note that it might be necessary to fix the aspect ratio of the
     * Y axis by dividing the Y coordinate by 1.2.
     */
    de::Vec3f frontVec, upVec, sideVec; /* to the left */

    float viewCos = 0.f;
    float viewSin = 0.f;

    de::Rectanglei window, windowTarget, windowOld;
    float windowInter = 0.f;
};

enum ViewPortLayer {
    Player3DViewLayer,
    ViewBorderLayer,
    HUDLayer
};

DE_EXTERN_C int      rendInfoTris;
DE_EXTERN_C dd_bool  firstFrameAfterLoad;

/**
 * Register console variables.
 */
void Viewports_Register();

int R_FrameCount();

void R_ResetFrameCount();

/**
 * Render all view ports in the viewport grid.
 */
//void R_RenderViewPorts(ViewPortLayer layer);

void R_RenderViewPort(int playerNum);

/**
 * Render a blank view for the specified player.
 */
void R_RenderBlankView();

/**
 * Draw the border around the view window.
 */
void R_RenderPlayerViewBorder();

/// @return  Current viewport; otherwise @c 0.
const viewport_t *R_CurrentViewPort();

/**
 * Set the current GL viewport.
 */
void R_UseViewPort(const viewport_t *vp);

void R_UseViewPort(int consoleNum);

/**
 * Determines the location of the game view of a player. This is the area where
 * the game view, border and game HUD will be drawn.
 * @param console  Player number.
 * @return Console rectangle in UI coordinates.
 */
de::Rectanglei R_ConsoleRect(int console);

/**
 * Determines the location of the 3D viewport of a player.
 * @param console  Player number.
 * @return Player's 3D world view rectangle in UI coordinates.
 */
de::Rectanglei R_Console3DViewRect(int console);

void R_UpdateViewer(int consoleNum);

void R_ResetViewer();

int R_IsViewerResetPending();

void R_ClearViewData();

/**
 * To be called at the beginning of a render frame to perform necessary initialization.
 */
void R_BeginFrame();

/**
 * Update the sharp world data by rotating the stored values of plane
 * heights and sharp camera positions.
 */
void R_NewSharpWorld();

/**
 * Returns @c true iff the subspace is marked as visible for the current frame.
 *
 * @see R_ViewerSubspaceMarkVisible()
 */
bool R_ViewerSubspaceIsVisible(const world::ConvexSubspace &subspace);

/**
 * Mark the subspace as visible for the current frame.
 *
 * @see R_ViewerSubspaceIsVisible()
 */
void R_ViewerSubspaceMarkVisible(const world::ConvexSubspace &subspace, bool yes = true);

/**
 * Returns @c true iff the (particle) generator is marked as visible for the current frame.
 *
 * @see R_ViewerGeneratorMarkVisible()
 */
bool R_ViewerGeneratorIsVisible(const Generator &generator);

/**
 * Mark the (particle) generator as visible for the current frame.
 *
 * @see R_ViewerGeneratorIsVisible()
 */
void R_ViewerGeneratorMarkVisible(const Generator &generator, bool yes = true);

/// @return  Distance in map space units between the lumobj and viewer.
double R_ViewerLumobjDistance(int idx);

/// @return  @c true if the lumobj is clipped for the viewer.
bool R_ViewerLumobjIsClipped(int idx);

/// @return  @c true if the lumobj is hidden for the viewer.
bool R_ViewerLumobjIsHidden(int idx);

/**
 * Clipping strategy:
 *
 * If culling world surfaces with the angle clipper and the viewer is not in the
 * void; use the angle clipper. Otherwise, use the BSP-based LOS algorithm.
 */
void R_ViewerClipLumobj(Lumobj *lum);

void R_ViewerClipLumobjBySight(Lumobj *lum, world::ConvexSubspace *subspace);

/**
 * Attempt to set up a view grid and calculate the viewports. Set 'numCols' and
 * 'numRows' to zero to just update the viewport coordinates.
 */
bool R_SetViewGrid(int numCols, int numRows);

void R_SetupDefaultViewWindow(int consoleNum);

/**
 * Animates the view window towards the target values.
 */
void R_ViewWindowTicker(int consoleNum, timespan_t ticLength);

/**
 * Returns the model-view-projection matrix for the camera position and orientation
 * in the current frame. Remains the same thoughtout rendering of the frame.
 *
 * @return MVP matrix.
 */
const de::Mat4f &Viewer_Matrix();

#endif // DE_CLIENT_VIEWPORTS_H
