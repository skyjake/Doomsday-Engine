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

#include "ui/widgets/mapoutlinewidget.h"

#include <de/ProgressWidget>
#include <de/Drawable>

using namespace de;

DENG_GUI_PIMPL(MapOutlineWidget)
{
    ProgressWidget *progress; // shown initially, before outline received

    // Outline.
    Rectanglei mapBounds;

    // Drawing.
    DefaultVertexBuf vbuf;
    Drawable drawable;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    GLUniform uColor     { "uColor",     GLUniform::Vec4 };

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

    }

    void glDeinit()
    {

    }

    void makeOutline(shell::MapOutlinePacket const &mapOutline)
    {
        progress->hide();

        // This is likely called wherever incoming network packets are being processed,
        // and thus there should be no active OpenGL context.
        root().window().glActivate();

        vbuf.clear();


        root().window().glDone();
    }
};

MapOutlineWidget::MapOutlineWidget(String const &name)
    : GuiWidget(name)
    , d(new Impl(this))
{}

void MapOutlineWidget::setOutline(shell::MapOutlinePacket const &mapOutline)
{
    d->makeOutline(mapOutline);
}

void MapOutlineWidget::viewResized()
{
    GuiWidget::viewResized();
    d->uMvpMatrix = root().projMatrix2D();
}

void MapOutlineWidget::drawContent()
{
    GuiWidget::drawContent();
    d->uColor = Vector4f(1, 1, 1, visibleOpacity());
    d->drawable.draw();
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
