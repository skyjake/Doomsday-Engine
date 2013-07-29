/** @file documentwidget.cpp  Widget for displaying large amounts of text.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/documentwidget.h"
#include "ui/widgets/guirootwidget.h"

#include <de/Drawable>

using namespace de;

DENG2_PIMPL(DocumentWidget)
{
    typedef DefaultVertexBuf VertexBuf;

    Drawable drawable;
    GLUniform uMvpMatrix;

    Instance(Public *i)
        : Base(i),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {

    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if(!self.hasChangedPlace(pos) && !self.geometryRequested())
        {
            // No need to change anything.
            return;
        }

        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);

        // Geometry is now up to date.
        self.requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();
    }
};

DocumentWidget::DocumentWidget(String const &name) : d(new Instance(this))
{}

void DocumentWidget::viewResized()
{
    d->uMvpMatrix = root().projMatrix2D();
}

void DocumentWidget::update()
{
    ScrollAreaWidget::update();
}

void DocumentWidget::drawContent()
{
    d->draw();
}

bool DocumentWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void DocumentWidget::glInit()
{
    d->glInit();
}

void DocumentWidget::glDeinit()
{
    d->glDeinit();
}

void DocumentWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    ScrollAreaWidget::glMakeGeometry(verts);

    // Geometry from the text composer.
}

