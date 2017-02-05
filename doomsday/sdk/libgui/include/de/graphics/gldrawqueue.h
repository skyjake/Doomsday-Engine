/** @file gldrawqueue.h  Utility for managing and drawing semi-static GL buffers.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLDRAWQUEUE_H
#define LIBGUI_GLDRAWQUEUE_H

#include <de/Drawable>

namespace de {

/**
 * Utility for managing and drawing semi-static GL buffers.
 *
 * The main purpose of GLDrawQueue is to minimize the number of draw calls needed for
 * drawing the view.
 *
 * GLDrawQueue owns a large GLBuffer from which smaller segments can be assigned out to
 * users. The segments may be changed at any time without affecting the rest of the
 * buffer's contents. If the GLBuffer runs out of space, a larger one will be allocated.
 *
 * GLDrawQueue also owns a GLProgram whose shaders must support collecting uniform values
 * from each queued draw into arrays (element indices are stored as vertex attributes).
 * If the arrays get full, the queue is automatically flushed.
 *
 * GLDrawQueue also has a GLState that is used for drawing the queued geometry. If the
 * state is changed, all previously queued data is first flushed. For example, changing
 * the clip rectangle always causes a flush.
 *
 * Internally, GLDrawQueue uses Drawable to manage its GL objects.
 */
class GLDrawQueue
{
public:
    GLDrawQueue();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLDRAWQUEUE_H
