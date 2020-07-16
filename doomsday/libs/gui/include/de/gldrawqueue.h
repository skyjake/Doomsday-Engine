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

#include <de/gluniform.h>

namespace de {

class GLBuffer;
class GLProgram;
class GLSubBuffer;
class GLUniform;

/**
 * Utility for drawing GLSubBuffers.
 *
 * The main purpose of GLDrawQueue is to minimize the number of draw calls needed for
 * drawing the view.
 *
 * GLDrawQueue also owns a GLProgram whose shaders must support collecting uniform values
 * from each queued draw into arrays (element indices are stored as vertex attributes).
 * If the arrays get full, the queue is automatically flushed.
 *
 * GLDrawQueue also has a GLState that is used for drawing the queued geometry. If the
 * state is changed, all previously queued data is first flushed. For example, changing
 * the clip rectangle always causes a flush.
 */
class LIBGUI_PUBLIC GLDrawQueue
{
public:
    GLDrawQueue();

    void beginFrame();

    void setProgram(GLProgram &program,
                    const Block &batchUniformName = Block(),
                    GLUniform::Type batchUniformType = GLUniform::Float);

    int batchIndex() const;

    void setBatchColor(const Vec4f &color);

    void setBatchSaturation(float saturation);

    void setBatchScissorRect(const Vec4f &scissor);

    void setBuffer(const GLBuffer &buffer);

    /**
     * Enqueues a sub-buffer for drawing. If the previously enqueued buffers are not
     * from the same GLBuffer, the queue is flushed first.
     *
     * @param sub  Sub-buffer to draw.
     */
    void enqueueDraw(const GLSubBuffer &buffer);

    /**
     * Draws everything in the queue.
     */
    void flush();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLDRAWQUEUE_H
