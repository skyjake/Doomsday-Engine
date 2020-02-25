/** @file render.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOM_RENDER_H
#define GLOOM_RENDER_H

#include "gloom/render/context.h"
#include <de/Time>

namespace gloom {

/**
 * Renderer component.
 */
class Render
{
public:
    Render();
    virtual ~Render();

    bool isInitialized() const;

    const Context &context() const;
    Context &context();

    virtual void glInit(Context &context);
    virtual void glDeinit();

    virtual void advanceTime(de::TimeSpan elapsed);
    virtual void render() = 0;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_RENDER_H
