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

#include <de/libdeng2.h>

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
     * Decorate @a surface.
     */
    void decorate(Surface &surface);

    /// Note that existing decorations are retained.
    void add(Surface *surface);

    /// Note that existing decorations are retained.
    void remove(Surface *surface);

    void reset();
    void redecorate();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_SURFACEDECORATOR_H
