/** @file glatlasbuffer.h  GLBuffer from where GLSubBuffers are allocated.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLATLASBUFFER_H
#define LIBGUI_GLATLASBUFFER_H

#include "de/glbuffer.h"

namespace de {

class GLSubBuffer;

/**
 * GLBuffer from where GLSubBuffers are allocated.
 */
class LIBGUI_PUBLIC GLAtlasBuffer
{
public:
    GLAtlasBuffer(const internal::AttribSpecs &vertexFormat);

    void setMaxElementCount(dsize maxElementCount);
    void setUsage(gfx::Usage usage);

    //void initialize();
    //void deinitialize();

    void clear();

    GLSubBuffer *alloc(dsize elementCount);
    void release(GLSubBuffer &buf);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLATLASBUFFER_H
