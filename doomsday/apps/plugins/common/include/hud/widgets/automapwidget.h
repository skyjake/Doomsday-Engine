/** @file automapwidget.h  GUI widget for the automap.
 *
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_UI_AUTOMAPWIDGET_H
#define LIBCOMMON_UI_AUTOMAPWIDGET_H

#include <functional>
#include <de/Error>
#include "hud/hudwidget.h"

/**
 * Base class for a world space point of interest (POI).
 *
 * @ingroup world
 *
 * @todo Base on de::MapObject
 */
class MapPoint
{
public:
    MapPoint(de::Vector3d const &origin = de::Vector3d()) {
        setOrigin(origin);
    }
    virtual ~MapPoint() {}

    /**
     * Returns the origin of the point in world space.
     */
    de::Vector3d origin() const { return _origin; }
    void setOrigin(de::Vector3d const &newOrigin = de::Vector3d()) {
        _origin = newOrigin;
    }

private:
    de::Vector3d _origin;
};

class AutomapStyle;

/**
 * @defgroup automapWidgetFlags  Automap Widget Flags
 */
///@{
#define AWF_SHOW_THINGS             ( 0x01 )
#define AWF_SHOW_KEYS               ( 0x02 )
#define AWF_SHOW_ALLLINES           ( 0x04 )
#define AWF_SHOW_SPECIALLINES       ( 0x08 )
#define AWF_SHOW_VERTEXES           ( 0x10 )
#define AWF_SHOW_LINE_NORMALS       ( 0x20 )
///@}

#define AUTOMAPWIDGET_OPEN_SECONDS  ( 0.3f )

/**
 * Specialized HudWidget displaying a simplified, 2D interpretation of the current
 * world map (much like a satellite navigation system).
 *
 * @ingroup ui
 */
class AutomapWidget : public HudWidget
{
public:
    /// Required/referenced point of interest is missing. @ingroup errors
    DENG2_ERROR(MissingPointError);

    /**
     * Models a marked point of interest (POI).
     */
    typedef MapPoint MarkedPoint;

public:
    AutomapWidget(de::dint player);
    virtual ~AutomapWidget();

    /**
     * Returns @c true if the automap is @em open (i.e., active).
     */
    bool isOpen() const;

    /**
     * Returns @c true if the automap is @em revealed (i.e., we're showing all map
     * subspaces irrespective of whether they've been mapped/seen yet).
     */
    bool isRevealed() const;

    /**
     * Change the @em open (i.e., active) state of the automap.
     */
    void open(bool yes = true, bool instantly = false);

    /**
     * Change the @em revealed (i.e., we're showing all map subspaces irrespective
     * of whether they've been mapped/seen yet).
     */
    void reveal(bool yes = true);

    de::dint flags() const;
    void setFlags(de::dint newFlags);

    /**
     * Returns the current map opacity factor.
     */
    de::dfloat opacityEX() const;
    void setOpacityEX(de::dfloat newOpacity);

    /**
     * Returns the current view space scaling factor.
     */
    dfloat scale() const;
    void setScale(de::dfloat newScale);

    /**
     * Returns the @em active style config.
     */
    AutomapStyle *style() const;

public:  // Coordinate space conversion utilities: --------------------------------

    /// Scale automap window coordinates to map space coordinates.
    de::dfloat frameToMap(de::dfloat coord) const;

    /// Scale map space coordinates to automap window coordinates.
    de::dfloat mapToFrame(de::dfloat coord) const;

public:  // Map space camera: -----------------------------------------------------

    /**
     * Returns @c true if camera follow mode is enabled (i.e., track the map origin
     * of the currently followed player (if any).
     */
    bool cameraFollowMode() const;
    void setCameraFollowMode(bool yes = true);

    /**
     * Returns @c true if camera rotation mode is enabled (i.e., track the view
     * direction of the currently followed player (if any)).
     */
    bool cameraRotationMode() const;
    void setCameraRotationMode(bool yes = true);

    /**
     * Returns @c true if camera zoom mode is enabled (i.e., force the view scale
     * factor to @c 1).
     */
    bool cameraZoomMode() const;
    void setCameraZoomMode(bool yes = true);

    /**
     * Returns the player number the camera is following; otherwise @c -1 if no
     * player is currently being followed.
     */
    de::dint cameraFollowPlayer() const;
    void setCameraFollowPlayer(de::dint newPlayer);

    /**
     * Returns the angle of the camera in world space.
     */
    de::dfloat cameraAngle() const;

    /**
     * Change the world space angle of the camera to @a newAngle.
     *
     * @param newOrigin  New world space coordinates to apply.
     */
    void setCameraAngle(de::dfloat newAngle);

    /**
     * Returns the world space origin of the camera.
     *
     * @see setCameraOrigin()
     */
    de::Vector2d cameraOrigin() const;

    /**
     * Change the world space origin of the camera to @a newOrigin.
     *
     * @param newOrigin  New world space coordinates to apply.
     * @param instantly  @c true= apply @a newOrigin immediately, foregoing animation.
     *
     * @see moveCameraOrigin()
     */
    void setCameraOrigin(de::Vector2d const &newOrigin, bool instantly = false);

    /**
     * Translate the world space origin of the camera given the relative @a delta.
     *
     * @param delta      Movement delta in world space.
     * @param instantly  @c true= apply @a delta immediately, foregoing animation.
     *
     * @see AutomapWidget_SetCameraOrigin()
     */
    inline void moveCameraOrigin(de::Vector2d const &delta, bool instantly = false) {
        setCameraOrigin(cameraOrigin() + delta, instantly);
    }

public:  // Marked map space points of interest: ----------------------------------

    /**
     * Returns the total number of marked points of interest.
     */
    de::dint pointCount() const;

    /**
     * Mark a new point of interest at the given map space @a origin.
     *
     * @param origin  Map space point of interest.
     *
     * @return  Index number of the newly added point (base 0).
     */
    de::dint addPoint(de::Vector3d const &origin);

    /**
     * Returns @c true if @a index is a known point of interest.
     *
     * @param index  Index number attributed to the point of interest.
     */
    bool hasPoint(de::dint index) const;

    /**
     * Lookup a marked point of interest by its unique @a index.
     *
     * @param index  Index number attributed to the point of interest.
     *
     * @return  AutomapWidget_Point associated with @a index.
     */
    MarkedPoint &point(de::dint index) const;

    /**
     * Iterate through all marked points of interest.
     *
     * @param func  Callback to make for each MarkedPoint.
     */
    de::LoopResult forAllPoints(std::function<de::LoopResult (MarkedPoint &)> func) const;

    /**
     * Clear all marked points of interest.
     *
     * @param silent  @c true= do not announce this action (no sounds or log messages).
     */
    void clearAllPoints(bool silent = false);

public:  /// @todo make private:

    void reset();
    void lineAutomapVisibilityChanged(Line const &line);

    void setMapBounds(coord_t lowX, coord_t hiX, coord_t lowY, coord_t hiY);

// ---

    void tick(timespan_t elapsed);
    void updateGeometry();
    void draw(de::Vector2i const &offset = de::Vector2i()) const;

    void pvisibleBounds(coord_t *lowX, coord_t *hiX, coord_t *lowY, coord_t *hiY) const;

    struct mobj_s *followMobj() const;

public:
    static void prepareAssets();
    static void releaseAssets();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif  // LIBCOMMON_UI_AUTOMAPWIDGET_H
