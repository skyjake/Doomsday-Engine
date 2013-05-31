/** @file guirootwidget.cpp  Graphical root widget.
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

#include "ui/widgets/guirootwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <de/AtlasTexture>
#include <de/GLTexture>
#include <de/GLUniform>

#include <QImage>
#include <QPainter>

using namespace de;

DENG2_PIMPL(GuiRootWidget)
{
    ClientWindow *window;
    QScopedPointer<AtlasTexture> atlas; ///< Shared atlas for most UI graphics/text.
    GLUniform uTexAtlas;
    Id solidWhiteTex;
    Id roundCorners;
    Id gradientFrame;

    Instance(Public *i, ClientWindow *win)
        : Base(i),
          window(win),
          atlas(0),
          uTexAtlas("uTex", GLUniform::Sampler2D)
    {}

    ~Instance()
    {
        // Tell all widgets to release their resource allocations. The base
        // class destructor will destroy all widgets, but this class governs
        // shared GL resources, so we'll ask the widgets to do this now.
        self.notifyTree(&Widget::deinitialize);
    }

    void initAtlas()
    {
        if(atlas.isNull())
        {
            atlas.reset(AtlasTexture::newWithRowAllocator(
                            Atlas::BackingStore | Atlas::AllowDefragment,
                            GLTexture::maximumSize().min(GLTexture::Size(4096, 4096))));
            uTexAtlas = *atlas;

            // A set of general purpose textures:

            // One solid white pixel.
            Image const solidWhitePixel = Image::solidColor(Image::Color(255, 255, 255, 255),
                                                            Image::Size(1, 1));
            solidWhiteTex = atlas->alloc(solidWhitePixel);

            // Rounded corners.
            {
                QImage corners(QSize(20, 20), QImage::Format_ARGB32);
                corners.fill(QColor(255, 255, 255, 0).rgba());
                QPainter painter(&corners);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(Qt::white, 1));
                painter.drawEllipse(QPoint(11, 11), 8, 8);
                painter.setPen(QPen(Qt::black, 1));
                painter.drawEllipse(QPoint(10, 10), 8, 8);
                roundCorners = atlas->alloc(corners);
            }

            // Gradient frame.
            {
                QImage frame(QSize(12, 12), QImage::Format_ARGB32);
                frame.fill(QColor(255, 255, 255, 0).rgba());
                QPainter painter(&frame);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QColor(0, 0, 0, 160));
                painter.drawRect(QRect(0, 0, 11, 11));
                painter.setPen(QColor(255, 255, 255, 255));
                painter.drawRect(QRect(1, 1, 9, 9));
                painter.setPen(QColor(255, 255, 255, 192));
                painter.drawRect(QRect(2, 2, 7, 7));
                painter.setPen(QColor(255, 255, 255, 128));
                painter.drawRect(QRect(3, 3, 5, 5));
                painter.setPen(QColor(255, 255, 255, 64));
                painter.drawRect(QRect(4, 4, 3, 3));
                gradientFrame = atlas->alloc(frame);
            }
        }
    }
};

GuiRootWidget::GuiRootWidget(ClientWindow *window)
    : d(new Instance(this, window))
{}

void GuiRootWidget::setWindow(ClientWindow *window)
{
    d->window = window;
}

ClientWindow &GuiRootWidget::window()
{
    DENG2_ASSERT(d->window != 0);
    return *d->window;
}

AtlasTexture &GuiRootWidget::atlas()
{
    d->initAtlas();
    return *d->atlas;
}

GLUniform &GuiRootWidget::uAtlas()
{
    return d->uTexAtlas;
}

Id GuiRootWidget::solidWhitePixel() const
{
    d->initAtlas();
    return d->solidWhiteTex;
}

Id GuiRootWidget::roundCorners() const
{
    d->initAtlas();
    return d->roundCorners;
}

Id GuiRootWidget::gradientFrame() const
{
    d->initAtlas();
    return d->gradientFrame;
}

GLShaderBank &GuiRootWidget::shaders()
{
    return ClientApp::glShaderBank();
}

Matrix4f GuiRootWidget::projMatrix2D() const
{
    RootWidget::Size const size = viewSize();
    return Matrix4f::ortho(0, size.x, 0, size.y);
}

void GuiRootWidget::update()
{
    // Allow GL operations.
    window().canvas().makeCurrent();

    RootWidget::update();
}
