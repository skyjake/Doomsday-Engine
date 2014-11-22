/** @file surfacedecorator.h  World surface decorator.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include <de/libcore.h>

class Surface;

#define MAX_DECOR_LIGHTS        (16384)

/**
 * The decorator assumes responsibility for decorating surfaces according to
 * the defined material when the surface is assigned. When a material changes
 * (e.g., animation) the decorator automatically schedules these surfaces for
 * redecoration.
 *
 * Note that it is the responsibility of the user to inform the decorator when
 * a surface moves or a new material is assigned. Otherwise decorations may not
 * be updated or done so using an out of date material.
 *
 * @ingroup render
 */
class SurfaceDecorator
{
public:
    /**
     * Construct a new surface decorator.
     */
    SurfaceDecorator();

    /**
     * Perform one time decoration of a single @a surface. The surface will not
     * be remembered and any resulting decorations will not be updated later.
     */
    void decorate(Surface &surface);

    /**
     * Perform scheduled (re)decoration work.
     */
    void redecorate();

    /**
     * Forget all known surfaces and previous decorations.
     */
    void reset();

    /**
     * Add the specified @a surface to the decorator's job list. If the surface
     * is already on this list then nothing will happen. If the surface lacks a
     * material or no decorations are defined for this material, then nothing
     * will happen.
     *
     * Any existing decorations are retained until the material next changes.
     *
     * @param surface  Surface to add to the job list.
     */
    void add(Surface *surface);

    /**
     * Remove the specified @a surface from the decorator's job list. If the
     * surface is not on this list then nothing will happen.
     *
     * Any existing decorations are retained and will no longer be updated.
     *
     * @param surface  Surface to remove from the job list.
     */
    void remove(Surface *surface);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_SURFACEDECORATOR_H
