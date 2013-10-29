/** @file viewports.h  Player viewports and related low-level rendering.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_VIEWPORTS_H
#define DENG_CLIENT_VIEWPORTS_H

#ifdef __SERVER__
#  error "viewports.h is for the client only"
#endif

#include <de/Rectangle>
#include <de/Vector>
#include <de/rect.h>

class BspLeaf;
struct Generator;
class Lumobj;

struct viewport_t
{
    int console;
    de::Rectanglei geometry;
};

struct viewer_t
{
    de::Vector3d origin;
    float pitch;

    viewer_t(de::Vector3d const &origin = de::Vector3d(),
             angle_t angle              = 0,
             float pitch                = 0)
        : origin(origin)
        , pitch(pitch)
        , _angle(angle)
    {}
    viewer_t(viewer_t const &other)
        : origin(other.origin)
        , pitch(other.pitch)
        , _angle(other._angle)
    {}

    viewer_t lerp(viewer_t const &end, float pos) const {
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
    de::Vector3f frontVec, upVec, sideVec; /* to the left */

    float viewCos, viewSin;

    de::Rectanglei window, windowTarget, windowOld;
    float windowInter;
};

enum ViewPortLayer {
    Player3DViewLayer,
    ViewBorderLayer,
    HUDLayer
};

DENG_EXTERN_C int      rendInfoTris;
DENG_EXTERN_C dd_bool  firstFrameAfterLoad;

/**
 * Register console variables.
 */
void Viewports_Register();

int R_FrameCount();

void R_ResetFrameCount();

/**
 * Render all view ports in the viewport grid.
 */
void R_RenderViewPorts(ViewPortLayer layer);

/**
 * Render a blank view for the specified player.
 */
void R_RenderBlankView();

/**
 * Draw the border around the view window.
 */
void R_RenderPlayerViewBorder();

/// @return  Current viewport; otherwise @c 0.
viewport_t const *R_CurrentViewPort();

/**
 * Set the current GL viewport.
 */
void R_UseViewPort(viewport_t const *vp);

viewdata_t const *R_ViewData(int consoleNum);

void R_UpdateViewer(int consoleNum);

void R_ResetViewer();

int R_NextViewer();

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
 * Update the sharp world data by rotating the stored values of sharp camera
 * positions.
 */
void R_NewSharpViewers();

/**
 * Returns @c true iff the BSP leaf is marked as visible for the current frame.
 *
 * @see R_ViewerBspLeafMarkVisible()
 */
bool R_ViewerBspLeafIsVisible(BspLeaf const &bspLeaf);

/**
 * Mark the BSP leaf as visible for the current frame.
 *
 * @see R_ViewerBspLeafIsVisible()
 */
void R_ViewerBspLeafMarkVisible(BspLeaf const &bspLeaf, bool yes = true);

/**
 * Returns @c true iff the (particle) generator is marked as visible for the current frame.
 *
 * @see R_ViewerGeneratorMarkVisible()
 */
bool R_ViewerGeneratorIsVisible(Generator const &generator);

/**
 * Mark the (particle) generator as visible for the current frame.
 *
 * @see R_ViewerGeneratorIsVisible()
 */
void R_ViewerGeneratorMarkVisible(Generator const &generator, bool yes = true);

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

void R_ViewerClipLumobjBySight(Lumobj *lum, BspLeaf *bspLeaf);

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

#endif // DENG_CLIENT_VIEWPORTS_H
