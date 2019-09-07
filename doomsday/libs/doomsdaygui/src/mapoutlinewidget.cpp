/** @file mapoutlinewidget.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/gui/mapoutlinewidget.h"

#include <de/ProgressWidget>
#include <de/Drawable>

using namespace de;

DE_GUI_PIMPL(MapOutlineWidget)
{
    ProgressWidget *progress; // shown initially, before outline received
    DotPath oneSidedColorId{"inverted.altaccent"};
    DotPath twoSidedColorId{"altaccent"};

    // Outline.
    Rectanglei mapBounds;

    // Drawing.
    DefaultVertexBuf *vbuf = nullptr;
    Drawable drawable;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    GLUniform uColor     { "uColor",     GLUniform::Vec4 };
    Animation mapOpacity { 0, Animation::Linear };

    Impl(Public *i) : Base(i)
    {
        self().add(progress = new ProgressWidget);
        progress->setMode(ProgressWidget::Indefinite);
        progress->setColor("progress.dark.wheel");
        progress->setShadowColor("progress.dark.shadow");
        progress->rule().setRect(self().rule());
    }

    void glInit()
    {
        vbuf = new DefaultVertexBuf;
        drawable.addBuffer(vbuf);

        shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix << uColor;
    }

    void glDeinit()
    {
        drawable.clear();
        vbuf = nullptr;
    }

    void makeOutline(const network::MapOutlinePacket &mapOutline)
    {
        if (!vbuf) return;

        // This is likely called wherever incoming network packets are being processed,
        // and thus there should currently be no active OpenGL context.
        root().window().glActivate();

        progress->setOpacity(0, 0.5);
        mapOpacity.setValue(1, 0.5);

        mapBounds = Rectanglei();

        const Vec4f oneSidedColor = style().colors().colorf(oneSidedColorId);
        const Vec4f twoSidedColor = style().colors().colorf(twoSidedColorId);

        DefaultVertexBuf::Builder verts;
        DefaultVertexBuf::Type vtx;

        for (int i = 0; i < mapOutline.lineCount(); ++i)
        {
            const auto &line = mapOutline.line(i);

            vtx.rgba = (line.type == network::MapOutlinePacket::OneSidedLine? oneSidedColor : twoSidedColor);

            // Two vertices per line.
            vtx.pos = line.start; verts << vtx;
            vtx.pos = line.end;   verts << vtx;

            if (i > 0)
            {
                mapBounds.include(line.start);
            }
            else
            {
                mapBounds = {line.start, line.start}; // initialize
            }
            mapBounds.include(line.end);
        }

//        vtx.rgba = Vec4f(1, 0, 1, 1);
//        vtx.pos = mapBounds.topLeft; verts << vtx;
//        vtx.pos = mapBounds.bottomRight; verts << vtx;

        vbuf->setVertices(gfx::Lines, verts, gfx::Static);

        root().window().glDone();
    }

    Mat4f modelMatrix() const
    {
        DE_ASSERT(vbuf);

        if (mapBounds.isNull()) return Mat4f();

        const Rectanglef rect = self().contentRect();
        const float scale = de::min(rect.width()  / mapBounds.width(),
                                    rect.height() / mapBounds.height());
        return Mat4f::translate(rect.middle()) *
               Mat4f::scale    (Vec3f(scale, -scale, 1)) *
               Mat4f::translate(Vec2f(-mapBounds.middle()));
    }
};

MapOutlineWidget::MapOutlineWidget(const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{}

void MapOutlineWidget::setColors(const DotPath &oneSidedLine, const DotPath &twoSidedLine)
{
    d->oneSidedColorId = oneSidedLine;
    d->twoSidedColorId = twoSidedLine;
}

void MapOutlineWidget::setOutline(const network::MapOutlinePacket &mapOutline)
{
    d->makeOutline(mapOutline);
}

void MapOutlineWidget::drawContent()
{
    GuiWidget::drawContent();
    if (d->vbuf && d->vbuf->count())
    {
        auto &painter = root().painter();
        painter.flush();
        GLState::push().setNormalizedScissor(painter.normalizedScissor());
        d->uMvpMatrix = root().projMatrix2D() * d->modelMatrix();
        d->uColor = Vec4f(1, 1, 1, d->mapOpacity * visibleOpacity());
        d->drawable.draw();
        GLState::pop();
    }
}

void MapOutlineWidget::glInit()
{
    GuiWidget::glInit();
    d->glInit();
}

void MapOutlineWidget::glDeinit()
{
    GuiWidget::glDeinit();
    d->glDeinit();
}
