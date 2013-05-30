/** @file taskbarwidget.cpp
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

#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/labelwidget.h"
#include "ui/widgets/buttonwidget.h"

#include <de/Drawable>
#include <de/GLBuffer>

using namespace de;

DENG2_PIMPL(TaskBarWidget)
{
    typedef GLBufferT<Vertex2Rgba> VertexBuf;

    ConsoleCommandWidget *cmdLine;

    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    Matrix4f projMatrix;

    Instance(Public *i)
        : Base(i),
          cmdLine(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        uColor = Vector4f(1, 1, 1, 1);
    }

    void glInit()
    {
        VertexBuf *buf = new VertexBuf;
        drawable.addBuffer(buf);

        Vector4f bgColor = self.style().colors().colorf("background");

        VertexBuf::Type verts[] = {
            { Vector2f(0, 0), bgColor },
            { Vector2f(1, 0), bgColor },
            { Vector2f(0, 1), bgColor },
            { Vector2f(1, 1), bgColor }
        };
        buf->setVertices(gl::TriangleStrip, verts, 4, gl::Static);

        self.root().shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix
                << uColor;

        updateProjection();
    }

    void glDeinit()
    {

    }

    void updateProjection()
    {
        projMatrix = self.root().projMatrix2D();
    }
};

TaskBarWidget::TaskBarWidget() : GuiWidget("TaskBar"), d(new Instance(this))
{
    Rule const &gap = style().rules().rule("gap");

    LabelWidget *logo = new LabelWidget;
    logo->setImage(style().images().image("logo.px128"));
    logo->setImageScale(.75f);
    logo->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Width,  rule().height())
            .setInput(Rule::Right,  rule().right() - gap)
            .setInput(Rule::Bottom, rule().bottom());
    add(logo);

    // Taskbar height depends on the font size.
    rule().setInput(Rule::Height, style().fonts().font("default").height() + gap * 2);
}

void TaskBarWidget::glInit()
{
    LOG_AS("TaskBarWidget");
    d->glInit();
}

void TaskBarWidget::glDeinit()
{
    d->glDeinit();
}

void TaskBarWidget::viewResized()
{
    d->updateProjection();
}

void TaskBarWidget::draw()
{
    Rectanglei pos = rule().recti();

    d->uMvpMatrix = d->projMatrix *
            Matrix4f::scaleThenTranslate(Vector2f(pos.width(), pos.height()),
                                         Vector3f(pos.left(), pos.top()));

    d->drawable.draw();
}
