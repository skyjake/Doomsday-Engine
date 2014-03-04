/** @file guirootwidget.cpp  Graphical root widget.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GuiRootWidget"
#include "de/GuiWidget"
#include "de/BaseGuiApp"
#include "de/Style"

#include <de/CanvasWindow>
#include <de/AtlasTexture>
#include <de/GLTexture>
#include <de/GLUniform>
#include <de/GLTarget>
#include <de/GLState>

#include <QImage>
#include <QPainter>

namespace de {

DENG2_PIMPL(GuiRootWidget)
, DENG2_OBSERVES(Widget, ChildAddition)
{
    CanvasWindow *window;
    QScopedPointer<AtlasTexture> atlas; ///< Shared atlas for most UI graphics/text.
    GLUniform uTexAtlas;
    Id solidWhiteTex;
    Id roundCorners;
    Id gradientFrame;
    Id borderGlow;
    Id toggleOnOff;
    Id tinyDot;
    Id fold;
    bool noFramesDrawnYet;

    Instance(Public *i, CanvasWindow *win)
        : Base(i)
        , window(win)
        , atlas(0)
        , uTexAtlas("uTex", GLUniform::Sampler2D)
        , noFramesDrawnYet(true)
    {
        self.audienceForChildAddition() += this;
    }

    ~Instance()
    {
        // Tell all widgets to release their resource allocations. The base
        // class destructor will destroy all widgets, but this class governs
        // shared GL resources, so we'll ask the widgets to do this now.
        self.notifyTree(&Widget::deinitialize);

        // Destroy GUI widgets while the shared resources are still available.
        self.clearTree();
    }

    void initAtlas()
    {
        if(atlas.isNull())
        {
            Style const &st = Style::appStyle();

            atlas.reset(AtlasTexture::newWithKdTreeAllocator(
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
                QImage corners(QSize(15, 15), QImage::Format_ARGB32);
                corners.fill(QColor(255, 255, 255, 0).rgba());
                QPainter painter(&corners);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(Qt::white, 1));
                painter.drawEllipse(QPoint(8, 8), 6, 6);
                //painter.setPen(QPen(Qt::black, 1));
                //painter.drawEllipse(QPoint(10, 10), 8, 8);
                roundCorners = atlas->alloc(corners);
            }

            // Gradient frame.
            {
                QImage frame(QSize(12, 12), QImage::Format_ARGB32);
                frame.fill(QColor(255, 255, 255, 0).rgba());
                QPainter painter(&frame);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(QPen(QColor(255, 255, 255, 255), 2));
                painter.setBrush(Qt::NoBrush);//QColor(255, 255, 255, 255));
                painter.drawEllipse(QPoint(6, 6), 4, 4);
                /*
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QColor(255, 255, 255, 32));
                painter.drawRect(QRect(1, 1, 9, 9));
                painter.setPen(QColor(255, 255, 255, 64));
                painter.drawRect(QRect(2, 2, 7, 7));
                painter.setPen(QColor(255, 255, 255, 128));
                painter.drawRect(QRect(3, 3, 5, 5));
                painter.setPen(QColor(255, 255, 255, 255));
                painter.drawRect(QRect(4, 4, 3, 3));
                {
                    painter.setPen(QColor(255, 255, 255, 192));
                    QPointF const points[4] = {
                        QPointF(4, 4), QPointF(4, 7),
                        QPointF(7, 4), QPointF(7, 7)
                    };
                    painter.drawPoints(points, 4);
                }
                {
                    painter.setPen(QColor(255, 255, 255, 96));
                    QPointF const points[4] = {
                        QPointF(3, 3), QPointF(3, 8),
                        QPointF(8, 3), QPointF(8, 8)
                    };
                    painter.drawPoints(points, 4);
                }
                {
                    painter.setPen(QColor(255, 255, 255, 48));
                    QPointF const points[4] = {
                        QPointF(2, 2), QPointF(2, 9),
                        QPointF(9, 2), QPointF(9, 9)
                    };
                    painter.drawPoints(points, 4);
                }
                {
                    painter.setPen(QColor(255, 255, 255, 16));
                    QPointF const points[4] = {
                        QPointF(1, 1), QPointF(1, 10),
                        QPointF(10, 1), QPointF(10, 10)
                    };
                    painter.drawPoints(points, 4);
                }
                */
                gradientFrame = atlas->alloc(frame);
            }

            // Border glow.
            borderGlow = atlas->alloc(st.images().image("window.borderglow"));

            // On/Off toggle.
            toggleOnOff = atlas->alloc(st.images().image("toggle.onoff"));

            // Fold indicator.
            fold = atlas->alloc(st.images().image("fold"));

            // Tiny dot.
            {
                QImage dot(QSize(5, 5), QImage::Format_ARGB32);
                dot.fill(QColor(255, 255, 255, 0).rgba());
                QPainter painter(&dot);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(Qt::NoPen);
                painter.setBrush(Qt::white);
                painter.drawEllipse(QPointF(2.5f, 2.5f), 2, 2);
                tinyDot = atlas->alloc(dot);
            }
        }
    }

    void widgetChildAdded(Widget &child)
    {
        // Make sure newly added children know the view size.
        child.viewResized();
        child.notifyTree(&Widget::viewResized);
    }
};

GuiRootWidget::GuiRootWidget(CanvasWindow *window)
    : d(new Instance(this, window))
{}

void GuiRootWidget::setWindow(CanvasWindow *window)
{
    d->window = window;
}

CanvasWindow &GuiRootWidget::window()
{
    DENG2_ASSERT(d->window != 0);
    return *d->window;
}

void GuiRootWidget::addOnTop(GuiWidget *widget)
{
    add(widget);
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

Id GuiRootWidget::borderGlow() const
{
    d->initAtlas();
    return d->borderGlow;
}

Id GuiRootWidget::toggleOnOff() const
{
    d->initAtlas();
    return d->toggleOnOff;
}

Id GuiRootWidget::tinyDot() const
{
    d->initAtlas();
    return d->tinyDot;
}

Id de::GuiRootWidget::fold() const
{
    d->initAtlas();
    return d->fold;
}

GLShaderBank &GuiRootWidget::shaders()
{
    return BaseGuiApp::shaders();
}

Matrix4f GuiRootWidget::projMatrix2D() const
{
    RootWidget::Size const size = viewSize();
    return Matrix4f::ortho(0, size.x, 0, size.y);
}

void GuiRootWidget::routeMouse(Widget *routeTo)
{
    setEventRouting(QList<int>()
                    << Event::MouseButton
                    << Event::MouseMotion
                    << Event::MousePosition
                    << Event::MouseWheel, routeTo);
}

void GuiRootWidget::dispatchLatestMousePosition()
{}

bool GuiRootWidget::processEvent(Event const &event)
{
    if(!RootWidget::processEvent(event))
    {
        if(event.type() == Event::MouseButton)
        {
            // Button events that no one handles will relinquish input focus.
            setFocus(0);
        }
        return false;
    }
    return true;
}

void GuiRootWidget::handleEventAsFallback(Event const &)
{}

GuiWidget const *GuiRootWidget::globalHitTest(Vector2i const &pos) const
{
    Widget::Children const childs = children();
    for(int i = childs.size() - 1; i >= 0; --i)
    {
        if(GuiWidget const *w = childs.at(i)->maybeAs<GuiWidget>())
        {
            if(GuiWidget const *hit = w->treeHitTest(pos))
            {
                return hit;
            }
        }
    }
    return 0;
}

void GuiRootWidget::update()
{
    if(window().canvas().isGLReady())
    {
        // Allow GL operations.
        window().canvas().makeCurrent();

        RootWidget::update();
    }
}

void GuiRootWidget::draw()
{
    if(d->noFramesDrawnYet)
    {
        // Widgets may not yet be ready on the first frame; make sure
        // we don't show garbage.
        window().canvas().renderTarget().clear(GLTarget::Color);

        d->noFramesDrawnYet = false;
    }

#ifdef DENG2_DEBUG
    // Detect mistakes in GLState stack usage.
    dsize const depthBeforeDrawing = GLState::stackDepth();
#endif

    RootWidget::draw();

    DENG2_ASSERT(GLState::stackDepth() == depthBeforeDrawing);
}

void GuiRootWidget::drawUntil(Widget &until)
{
    NotifyArgs args(&Widget::draw);
    args.conditionFunc  = &Widget::isVisible;
    args.preNotifyFunc  = &Widget::preDrawChildren;
    args.postNotifyFunc = &Widget::postDrawChildren;
    args.until          = &until;
    notifyTree(args);
}

} // namespace de
