/** @file decoration.h World surface decoration.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_CLIENT_RENDER_DECORATION_H
#define DENG_CLIENT_RENDER_DECORATION_H

#include <de/Error>
#include <de/Vector>

#include "MaterialSnapshot"

class Lumobj;
namespace de
{
class Map;
}
class Surface;

/// No decorations are visible beyond this.
#define MAX_DECOR_DISTANCE      (2048)

/**
 * @ingroup render
 */
class Decoration
{
public:
    /// Required source is missing. @ingroup errors
    //DENG2_ERROR(MissingSourceError);

    /// Required surface is missing. @ingroup errors
    DENG2_ERROR(MissingSurfaceError);

public: /// @todo remove me.
    //Decoration *next;

public:
    /**
     * Construct a new decoration.
     *
     * @param source  Source of the decoration (a material).
     * @param origin  Origin of the decoration in map space.
     */
    Decoration(/*SurfaceDecorSource *source = 0*/
               de::MaterialSnapshotDecoration &source,
               de::Vector3d const &origin = de::Vector3d());

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Returns the current angle fade factor (user configurable).
     */
    static float angleFadeFactor();

    /**
     * Returns the current brightness scale factor (user configurable).
     */
    static float brightFactor();

    /**
     * Returns @c true iff a source is defined for the decoration.
     *
     * @see source(), setSource()
     */
    //bool hasSource() const;

    /**
     * Returns the source of the decoration.
     *
     * @see hasSource(), setSource()
     */
    de::MaterialSnapshotDecoration &source();

    /// @copydoc source()
    de::MaterialSnapshotDecoration const &source() const;

    /**
     * Change the attributed source of the decoration.
     *
     * @param newSource
     */
    //void setSource(SurfaceDecorSource *newSource);

    /**
     * Returns @c true iff a surface is attributed for the decoration.
     *
     * @see surface(), setSurface()
     */
    bool hasSurface() const;

    /**
     * Convenient method which returns the surface owner of the decoration.
     */
    Surface &surface();

    /// @copydoc surface()
    Surface const &surface() const;

    /**
     * Change the attributed surface of the decoration.
     *
     * @param newSurface  Map surface to attribute. Use @c 0 to clear.
     */
    void setSurface(Surface *newSurface);

    /*de::Map &map();

    de::Map const &map() const;*/

    de::Vector3d const &origin() const;

    BspLeaf &bspLeafAtOrigin() const;

    /**
     * Generates a lumobj for the decoration.
     */
    void generateLumobj();

    /**
     * Generates a VSPR_FLARE vissprite for the decoration.
     */
    void generateFlare();

private:
    float lightLevelAtOrigin() const;

    //de::MaterialSnapshotDecoration const &materialDecoration() const;

    Lumobj *lumobj() const;

    //SurfaceDecorSource *_source; ///< Attributed source (if any, not owned).
    de::MaterialSnapshotDecoration *_source;
    de::Vector3d _origin;
    mutable BspLeaf *_bspLeaf;   ///< BSP leaf at @ref origin in the map (not owned).
    Surface *_surface;
    int _lumIdx;                 ///< Generated lumobj index (or Lumobj::NoIndex).
    float _fadeMul;              ///< Intensity multiplier (lumobj and flare).
};

#endif // DENG_CLIENT_RENDER_DECORATION_H
