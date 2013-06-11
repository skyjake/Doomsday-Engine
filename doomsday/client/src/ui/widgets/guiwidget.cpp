/** @file guiwidget.cpp  Base class for graphical widgets.
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

#include "ui/widgets/guiwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "clientapp.h"
#include <de/garbage.h>
#include <de/MouseEvent>
#include <de/Drawable>
#include <de/GLTexture>
#include <de/GLTarget>

using namespace de;

DENG2_PIMPL(GuiWidget)
{
    RuleRectangle rule;
    Rectanglei savedPos;
    bool inited;
    bool needGeometry;
    bool styleChanged;
    Background background;
    Animation opacity;

    // Style.
    DotPath fontId;
    DotPath textColorId;
    DotPath marginId;

    // Background blurring.
    bool blurInited;
    Vector2ui blurSize;
    GLTexture blur[2];
    QScopedPointer<GLTarget> blurTarget[2];
    Drawable blurring;
    GLUniform uBlurMvpMatrix;
    GLUniform uBlurColor;
    GLUniform uBlurTex;
    GLUniform uBlurStep;
    GLUniform uBlurWindow;

    Instance(Public *i)
        : Base(i),
          inited(false),
          needGeometry(true),
          styleChanged(false),
          opacity(1.f, Animation::Linear),
          fontId("default"),
          textColorId("text"),
          marginId("gap"),
          blurInited(false),
          uBlurMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uBlurColor("uColor", GLUniform::Vec4),
          uBlurTex("uTex", GLUniform::Sampler2D),
          uBlurStep("uBlurStep", GLUniform::Vec2),
          uBlurWindow("uWindow", GLUniform::Vec4)
    {}

    ~Instance()
    {
        // Deinitialize now if it hasn't been done already.
        self.deinitialize();
    }

    void initBlur()
    {
        if(blurInited) return;

        // The blurred version of the view is downsampled.
        blurSize = (self.root().viewSize() / 2).max(Vector2ui(1, 1));

        for(int i = 0; i < 2; ++i)
        {
            blur[i].setUndefinedImage(blurSize, Image::RGBA_8888);

#if 0
            QImage test(QSize(blurSize.x, blurSize.y), QImage::Format_ARGB32);
            QPainter pnt(&test);
            test.fill(0xff00ffff);
            pnt.setPen(Qt::black);
            pnt.setBrush(Qt::white);
            pnt.drawEllipse(QPoint(blurSize.x/2, blurSize.y/2),
                            blurSize.x/2 - 1, blurSize.y/2 - 1);
            blur[i].setImage(test);
#endif

            blur[i].setWrap(gl::ClampToEdge, gl::ClampToEdge);
            blurTarget[i].reset(new GLTarget(blur[i]));
        }

        // Set up the drawble.
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        blurring.addBuffer(buf);
        buf->setVertices(gl::TriangleStrip,
                         DefaultVertexBuf::Builder().makeQuad(
                             Rectanglef(0, 0, 1, 1),
                             Vector4f(1, 1, 1, 1),
                             Rectanglef(0, 0, 1, 1)),
                         gl::Static);

        uBlurStep = Vector2f(1.f / blurSize.x, 1.f / blurSize.y);

        self.root().shaders().build(blurring.program(), "fx.blur.horizontal")
                << uBlurMvpMatrix
                << uBlurTex
                << uBlurStep << uBlurWindow;

        blurring.addProgram("vert");
        self.root().shaders().build(blurring.program("vert"), "fx.blur.vertical")
                << uBlurMvpMatrix
                << uBlurTex
                << uBlurColor << uBlurStep << uBlurWindow;

        // Projection matrix for the blur quad.
        //uBlurMvpMatrix = Matrix4f::ortho(0, blurSize.x, 0, blurSize.y);

        //blurState.setViewport(Rectangleui::fromSize(blurSize));
        //blurring.setState(1, blurState);

        blurInited = true;
    }

    void deinitBlur()
    {
        if(!blurInited) return;

        for(int i = 0; i < 2; ++i)
        {
            blurTarget[i].reset();
            blur[i].clear();
        }
        blurring.clear();

        blurInited = false;
    }

    void reinitBlur()
    {
        if(blurInited)
        {
            deinitBlur();
            initBlur();
        }
    }

    void drawBlurredBackground()
    {
        if(background.type != Background::Blurred)
        {
            deinitBlur();
            return;
        }

        // Make sure blurring is initialized.
        initBlur();

        // Pass 1: render all the widgets behind this one onto the first blur
        // texture, downsampled.
        GLState::push()
                .setTarget(*blurTarget[0])
                .setViewport(Rectangleui::fromSize(blurSize));
        self.root().drawUntil(self);
        GLState::pop();

        // Pass 2: apply the horizontal blur filter to draw the background
        // contents onto the second blur texture.
        GLState::push()
                .setTarget(*blurTarget[1])
                .setViewport(Rectangleui::fromSize(blurSize));
        uBlurTex = blur[0];
        uBlurMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        uBlurWindow = Vector4f(0, 0, 1, 1);
        blurring.setProgram(blurring.program());
        blurring.draw();
        GLState::pop();

        // Pass 3: apply the vertical blur filter, drawing the final result
        // into the original target.
        Rectanglei pos = self.rule().recti();
        Vector2ui const viewSize = self.root().viewSize();
        uBlurTex = blur[1];
        uBlurColor = background.solidFill;
        uBlurWindow = Vector4f(pos.left()   / float(viewSize.x),
                               pos.top()    / float(viewSize.y),
                               pos.width()  / float(viewSize.x),
                               pos.height() / float(viewSize.y));
        uBlurMvpMatrix = self.root().projMatrix2D() *
                Matrix4f::scaleThenTranslate(pos.size(), pos.topLeft);
        blurring.setProgram("vert");
        blurring.draw();
    }
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Instance(this))
{}

GuiRootWidget &GuiWidget::root()
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

Style const &GuiWidget::style() const
{
    return ClientApp::windowSystem().style();
}

Font const &GuiWidget::font() const
{
    return style().fonts().font(d->fontId);
}

void GuiWidget::setFont(DotPath const &id)
{
    d->fontId = id;
    d->styleChanged = true;
}

ColorBank::Color GuiWidget::textColor() const
{
    return style().colors().color(d->textColorId);
}

ColorBank::Colorf GuiWidget::textColorf() const
{
    return style().colors().colorf(d->textColorId);
}

Rule const &GuiWidget::margin() const
{
    return style().rules().rule(d->marginId);
}

void GuiWidget::setTextColor(DotPath const &id)
{
    d->textColorId = id;
    d->styleChanged = true;
}

void GuiWidget::setMargin(DotPath const &id)
{
    d->marginId = id;
    d->styleChanged = true;
}

RuleRectangle &GuiWidget::rule()
{
    return d->rule;
}

RuleRectangle const &GuiWidget::rule() const
{
    return d->rule;
}

static void deleteGuiWidget(void *ptr)
{
    delete reinterpret_cast<GuiWidget *>(ptr);
}

void GuiWidget::deleteLater()
{
    Garbage_TrashInstance(this, deleteGuiWidget);
}

void GuiWidget::set(Background const &bg)
{
    d->background = bg;
}

bool GuiWidget::clipped() const
{
    return behavior().testFlag(ContentClipping);
}

GuiWidget::Background const &GuiWidget::background() const
{
    return d->background;
}

void GuiWidget::setOpacity(float opacity, TimeDelta span)
{
    d->opacity.setValue(opacity, span);
}

float GuiWidget::opacity() const
{
    return d->opacity;
}

float GuiWidget::visibleOpacity() const
{
    float opacity = d->opacity;
    for(Widget *i = Widget::parent(); i != 0; i = i->parent())
    {
        GuiWidget *w = dynamic_cast<GuiWidget *>(i);
        if(w)
        {
            opacity *= w->d->opacity;
        }
    }
    return opacity;
}

void GuiWidget::initialize()
{
    if(d->inited) return;

    try
    {
        d->inited = true;
        glInit();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error when initializing widget '%s':\n")
                << name() << er.asText();
    }
}

void GuiWidget::deinitialize()
{
    if(!d->inited) return;

    try
    {
        d->inited = false;
        d->deinitBlur();
        glDeinit();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error when deinitializing widget '%s':\n")
                << name() << er.asText();
    }
}

void GuiWidget::viewResized()
{
    d->reinitBlur();
}

void GuiWidget::update()
{
    if(!d->inited)
    {
        initialize();
    }
    if(d->styleChanged)
    {
        d->styleChanged = false;
        updateStyle();
    }
}

void GuiWidget::draw()
{
    if(d->inited && !isHidden() && visibleOpacity() > 0)
    {
        d->drawBlurredBackground();

        if(clipped())
        {
            GLState::push().setScissor(rule().recti());
        }

        drawContent();

        if(clipped())
        {
            GLState::pop();
        }
    }
}

bool GuiWidget::hitTest(Vector2i const &pos) const
{
    if(behavior().testFlag(Unhittable))
    {
        // Can never be hit by anything.
        return false;
    }
    return rule().recti().contains(pos);
}

bool GuiWidget::hitTest(Event const &event) const
{
    return event.isMouse() && hitTest(event.as<MouseEvent>().pos());
}

GuiWidget::MouseClickStatus GuiWidget::handleMouseClick(Event const &event)
{
    if(event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.button() != MouseEvent::Left)
        {
            return MouseClickUnrelated;
        }

        if(mouse.state() == MouseEvent::Pressed && hitTest(mouse.pos()))
        {
            root().routeMouse(this);
            return MouseClickStarted;
        }

        if(mouse.state() == MouseEvent::Released && root().isEventRouted(event.type(), this))
        {
            root().routeMouse(0);
            if(hitTest(mouse.pos()))
            {
                return MouseClickFinished;
            }
            return MouseClickAborted;
        }
    }
    return MouseClickUnrelated;
}

void GuiWidget::glInit()
{}

void GuiWidget::glDeinit()
{}

void GuiWidget::drawContent()
{}

void GuiWidget::requestGeometry(bool yes)
{
    d->needGeometry = yes;
}

bool GuiWidget::geometryRequested() const
{
    return d->needGeometry;
}

void GuiWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    if(d->background.type != Background::Blurred)
    {
        // Is there a solid fill?
        if(d->background.solidFill.w > 0)
        {
            verts.makeQuad(rule().recti(),
                           d->background.solidFill,
                           root().atlas().imageRectf(root().solidWhitePixel()).middle());
        }
    }

    switch(d->background.type)
    {
    case Background::GradientFrame:
        verts.makeFlexibleFrame(rule().recti(),
                                d->background.thickness,
                                d->background.color,
                                root().atlas().imageRectf(root().gradientFrame()));
        break;

    case Background::None:
    case Background::Blurred: // drawn separately in GuiWidget::draw()
        break;
    }
}

bool GuiWidget::hasChangedPlace(Rectanglei &currentPlace)
{
    currentPlace = rule().recti();
    bool changed = (d->savedPos != currentPlace);
    d->savedPos = currentPlace;
    return changed;
}

void GuiWidget::updateStyle()
{}
