/** @file painter.h  GUI painter.
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

#ifndef LIBAPPFW_PAINTER_H
#define LIBAPPFW_PAINTER_H

#include <de/glatlasbuffer.h>
#include <de/glsubbuffer.h>
#include <de/gldrawqueue.h>
#include <de/vertexbuilder.h>
#include <de/list.h>

namespace de {

struct LIBGUI_PUBLIC GuiVertex
{
    Vec2f pos;
    Vec2f texCoord;
    Vec4f rgba;
    float batchIndex;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

typedef VertexBuilder<GuiVertex>::Vertices GuiVertexBuilder;

/**
 * GUI painter.
 */
class LIBGUI_PUBLIC Painter
{
public:
    Painter();

    /**
     * Initializes the Painter for drawing. This must be called on every frame, before
     * drawing anything.
     */
    void init();

    void deinit();

    void setProgram(GLProgram &program);

    void useDefaultProgram();

    void setTexture(GLUniform &uTex);

    void setModelViewProjection(const Mat4f &mvp);

    void setNormalizedScissor(const Rectanglef &normScissorRect = Rectanglef(0, 0, 1, 1));

    Rectanglef normalizedScissor() const;

    void setColor(const Vec4f &color);

    void setSaturation(float saturation);

    /**
     * Draw a triangle strip.
     *
     * @param vertices  Vertices to draw. Batch indices in the array are updated by
     *                  the draw queue.
     */
    void drawTriangleStrip(List<GuiVertex> &vertices);

    void flush();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_PAINTER_H
