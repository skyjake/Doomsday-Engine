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

#include "MaterialSnapshot" // remove me
#include "Surface"

class Lumobj;
namespace de
{
class Map;
}

/// No decorations are visible beyond this.
#define MAX_DECOR_DISTANCE      (2048)

/**
 * @ingroup render
 */
class Decoration
{
public:
    /// Required source is missing. @ingroup errors
    DENG2_ERROR(MissingSourceError);

public: /// @todo remove me.
    Decoration *next;

public:
    /**
     * Construct a new decoration.
     *
     * @param source  Source of the decoration (can be set later).
     */
    Decoration(SurfaceDecorSource *source = 0);

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
    bool hasSource() const;

    /**
     * Returns the source of the decoration.
     *
     * @see hasSource(), setSource()
     */
    SurfaceDecorSource &source();

    /// @copydoc source()
    SurfaceDecorSource const &source() const;

    /**
     * Change the attributed source of the decoration.
     *
     * @param newSource
     */
    void setSource(SurfaceDecorSource *newSource);

    /**
     * Convenient method which returns the surface owner of the source of the
     * decoration. Naturally a source must currently be attributed.
     *
     * @see source()
     */
    Surface &surface();

    /// @copydoc surface()
    Surface const &surface() const;

    de::Map &map();

    de::Map const &map() const;

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

    de::MaterialSnapshot::Decoration const &materialDecoration() const;

    Lumobj *lumobj() const;

    SurfaceDecorSource *_source; ///< Attributed source (if any, not owned).
    int _lumIdx;                 ///< Generated lumobj index (or Lumobj::NoIndex).
    float _fadeMul;              ///< Intensity multiplier (lumobj and flare).
};

#endif // DENG_CLIENT_RENDER_DECORATION_H
