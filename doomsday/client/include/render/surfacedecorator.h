/** @file surfacedecorator.h World surface decorator.
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

#ifndef DENG_CLIENT_RENDER_SURFACEDECORATOR_H
#define DENG_CLIENT_RENDER_SURFACEDECORATOR_H

#include <de/Vector>

#include "MaterialSnapshot" // remove me
#include "Surface"

#define MAX_DECOR_LIGHTS        (16384)

/**
 * @ingroup render
 */
class SurfaceDecorator
{
public:
    /**
     * Construct a new surface decorator.
     */
    //SurfaceDecorator();

    /**
     * Decorate @a surface.
     */
    void decorate(Surface &surface);

private:
    void newDecoration(SurfaceDecorSource &source);

    uint generateDecorations(de::MaterialSnapshot::Decoration const &decor,
        de::Vector2i const &patternOffset, de::Vector2i const &patternSkip, Surface &suf,
        Material &material, de::Vector3d const &topLeft_, de::Vector3d const &/*bottomRight*/,
        de::Vector2d sufDimensions, de::Vector3d const &delta, int axis,
        de::Vector2f const &matOffset, Sector *containingSector);

    void plotSources(Surface &suf, de::Vector2f const &offset,
        de::Vector3d const &topLeft, de::Vector3d const &bottomRight, Sector *sec = 0);
};

/// @todo Refactor away --------------------------------------------------------

/**
 * Re-initialize the decoration pool (might be called during a map load or when
 * beginning a new render frame).
 */
void Rend_DecorReset();

/**
 * Create lumobjs for all decorations who want them.
 */
void Rend_DecorAddLuminous();

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
void Rend_DecorProject();

#endif // DENG_CLIENT_RENDER_SURFACEDECORATOR_H
