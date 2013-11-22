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

#include "GuiWidget"
#include "GuiRootWidget"
#include "ui/widgets/blurwidget.h"
#include "clientapp.h"
#include <de/garbage.h>
#include <de/MouseEvent>
#include <de/Drawable>
#include <de/GLTexture>
#include <de/GLTarget>

#include <QList>

using namespace de;

DENG2_PIMPL(GuiWidget),
DENG2_OBSERVES(Widget, ChildAddition),
DENG2_OBSERVES(ui::Margins, Change)
#ifdef DENG2_DEBUG
, DENG2_OBSERVES(Widget, ParentChange)
#endif
{
    RuleRectangle rule;     ///< Visual rule, used when drawing.
    RuleRectangle hitRule;  ///< Used only for hit testing. By default matches the visual rule.
    ui::Margins margins;
    Rectanglei savedPos;
    bool inited;
    bool needGeometry;
    bool styleChanged;
    Background background;
    Animation opacity;
    QList<IEventHandler *> eventHandlers;

    // Style.
    DotPath fontId;
    DotPath textColorId;

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
        : Base(i)
        , margins("gap")
        , inited(false)
        , needGeometry(true)
        , styleChanged(false)
        , opacity(1.f, Animation::Linear)
        , fontId("default")
        , textColorId("text")
        , blurInited(false)
        , uBlurMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uBlurColor    ("uColor",     GLUniform::Vec4)
        , uBlurTex      ("uTex",       GLUniform::Sampler2D)
        , uBlurStep     ("uBlurStep",  GLUniform::Vec2)
        , uBlurWindow   ("uWindow",    GLUniform::Vec4)
    {
        self.audienceForChildAddition += this;
        margins.audienceForChange += this;

#ifdef DENG2_DEBUG
        self.audienceForParentChange += this;
        rule.setDebugName(self.path());
#endif

        // By default use the visual rule as the hit test rule.
        hitRule.setRect(rule);
    }

    ~Instance()
    {        
        qDeleteAll(eventHandlers);

        // The base class will delete all children, but we need to deinitialize
        // them first.
        self.notifyTree(&Widget::deinitialize);

        deinitBlur();

        /*
         * Deinitialization must occur before destruction so that GL resources
         * are not leaked. Derived classes are responsible for deinitializing
         * first before beginning destruction.
         */
#ifdef DENG2_DEBUG
        if(inited) qDebug() << "GuiWidget" << &self << self.name() << "is still inited!";
        DENG2_ASSERT(!inited);
#endif
    }

    void marginsChanged()
    {
        styleChanged = true;
    }

#ifdef DENG2_DEBUG
    void widgetParentChanged(Widget &, Widget *, Widget *)
    {
        rule.setDebugName(self.path());
    }
#endif

    void widgetChildAdded(Widget &child)
    {
        if(self.hasRoot())
        {
            // Make sure newly added children know the view size.
            child.viewResized();
            child.notifyTree(&Widget::viewResized);
        }
    }

    void initBlur()
    {
        if(blurInited) return;

        // The blurred version of the view is downsampled.
        blurSize = (self.root().viewSize() / 4).max(Vector2ui(1, 1));

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

        uBlurStep = Vector2f(1.f / float(blurSize.x), 1.f / float(blurSize.y));

        self.root().shaders().build(blurring.program(), "fx.blur.horizontal")
                << uBlurMvpMatrix
                << uBlurTex
                << uBlurStep << uBlurWindow;

        blurring.addProgram("vert");
        self.root().shaders().build(blurring.program("vert"), "fx.blur.vertical")
                << uBlurMvpMatrix
                << uBlurTex
                << uBlurColor << uBlurStep << uBlurWindow;

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
        if(background.type == Background::SharedBlur)
        {
            // Use another widget's blur.
            DENG2_ASSERT(background.blur != 0);
            background.blur->drawBlurredRect(self.rule().recti(), background.solidFill);
            return;
        }

        if(background.type != Background::Blurred &&
           background.type != Background::BlurredWithBorderGlow)
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
        if(background.solidFill.w > 0)
        {
            self.drawBlurredRect(self.rule().recti(), background.solidFill);
        }
    }
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Instance(this))
{
    d->rule.setDebugName(name);
}

void GuiWidget::destroy(GuiWidget *widget)
{
    widget->deinitialize();
    delete widget;
}

GuiRootWidget &GuiWidget::root()
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

GuiRootWidget &GuiWidget::root() const
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

Widget::Children GuiWidget::childWidgets() const
{
    return Widget::children();
}

Widget *GuiWidget::parentWidget() const
{
    return Widget::parent();
}

Style const &GuiWidget::style() const
{
    return ClientApp::windowSystem().style();
}

Font const &GuiWidget::font() const
{
    return style().fonts().font(d->fontId);
}

DotPath const &GuiWidget::textColorId() const
{
    return d->textColorId;
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

void GuiWidget::setTextColor(DotPath const &id)
{
    d->textColorId = id;
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

ui::Margins &GuiWidget::margins()
{
    return d->margins;
}

ui::Margins const &GuiWidget::margins() const
{
    return d->margins;
}

Rectanglef GuiWidget::normalizedRect() const
{
    Rectanglef const rect = rule().rect();
    GuiRootWidget::Size const &viewSize = root().viewSize();
    return Rectanglef(Vector2f(rect.left()   / float(viewSize.x),
                               rect.top()    / float(viewSize.y)),
                      Vector2f(rect.right()  / float(viewSize.x),
                               rect.bottom() / float(viewSize.y)));
}

Rectanglef GuiWidget::normalizedRect(Rectanglei const &viewSpaceRect) const
{
    GuiRootWidget::Size const &viewSize = root().viewSize();
    return Rectanglef(Vector2f(float(viewSpaceRect.left())   / float(viewSize.x),
                               float(viewSpaceRect.top())    / float(viewSize.y)),
                      Vector2f(float(viewSpaceRect.right())  / float(viewSize.x),
                               float(viewSpaceRect.bottom()) / float(viewSize.y)));
}

Rectanglef GuiWidget::normalizedContentRect() const
{
    Rectanglef const rect = rule().rect().adjusted( Vector2f(margins().left().value(),
                                                             margins().top().value()),
                                                   -Vector2f(margins().right().value(),
                                                             margins().bottom().value()));
    GuiRootWidget::Size const &viewSize = root().viewSize();
    return Rectanglef(Vector2f(float(rect.left())   / float(viewSize.x),
                               float(rect.top())    / float(viewSize.y)),
                      Vector2f(float(rect.right())  / float(viewSize.x),
                               float(rect.bottom()) / float(viewSize.y)));
}

static void deleteGuiWidget(void *ptr)
{
    GuiWidget::destroy(reinterpret_cast<GuiWidget *>(ptr));
}

void GuiWidget::guiDeleteLater()
{
    Garbage_TrashInstance(this, deleteGuiWidget);
}

void GuiWidget::set(Background const &bg)
{
    d->background = bg;
    requestGeometry();
}

bool GuiWidget::clipped() const
{
    return behavior().testFlag(ContentClipping);
}

GuiWidget::Background const &GuiWidget::background() const
{
    return d->background;
}

void GuiWidget::setOpacity(float opacity, TimeDelta span, TimeDelta startDelay)
{
    d->opacity.setValue(opacity, span, startDelay);
}

Animation GuiWidget::opacity() const
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

    // Disabled widgets are automatically made translucent.
    if(isDisabled()) opacity *= .3f;

    return opacity;
}

void GuiWidget::addEventHandler(IEventHandler *handler)
{
    d->eventHandlers.append(handler);
}

void GuiWidget::removeEventHandler(IEventHandler *handler)
{
    d->eventHandlers.removeOne(handler);
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
#ifdef DENG2_DEBUG
        // Detect mistakes in GLState stack usage.
        dsize const depthBeforeDrawingWidget = GLState::stackDepth();
#endif

        d->drawBlurredBackground();

        if(clipped())
        {
            GLState::push().setNormalizedScissor(normalizedRect());
        }

        drawContent();

        if(clipped())
        {
            GLState::pop();
        }

        DENG2_ASSERT(GLState::stackDepth() == depthBeforeDrawingWidget);
    }
}

bool GuiWidget::handleEvent(Event const &event)
{
    foreach(IEventHandler *handler, d->eventHandlers)
    {
        if(handler->handleEvent(*this, event))
        {
            return true;
        }
    }
    return Widget::handleEvent(event);
}

bool GuiWidget::hitTest(Vector2i const &pos) const
{
    if(behavior().testFlag(Unhittable))
    {
        // Can never be hit by anything.
        return false;
    }

    Widget const *w = Widget::parent();
    while(w)
    {
        GuiWidget const *gui = dynamic_cast<GuiWidget const *>(w);
        if(gui)
        {
            if(gui->behavior().testFlag(ChildHitClipping) &&
               !gui->d->hitRule.recti().contains(pos))
            {
                // Must hit clipped parent widgets as well.
                return false;
            }
        }
        w = w->Widget::parent();
    }

    return d->hitRule.recti().contains(pos);
}

bool GuiWidget::hitTest(Event const &event) const
{
    return event.isMouse() && hitTest(event.as<MouseEvent>().pos());
}

RuleRectangle &GuiWidget::hitRule()
{
    return d->hitRule;
}

GuiWidget::MouseClickStatus GuiWidget::handleMouseClick(Event const &event, MouseEvent::Button button)
{
    if(isDisabled()) return MouseClickUnrelated;

    if(event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.button() != button)
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

void GuiWidget::drawBlurredRect(Rectanglei const &rect, Vector4f const &color)
{
    Vector2ui const viewSize = root().viewSize();

    d->uBlurTex = d->blur[1];
    d->uBlurColor = Vector4f((1 - color.w) + color.x * color.w,
                             (1 - color.w) + color.y * color.w,
                             (1 - color.w) + color.z * color.w,
                             1.f);
    d->uBlurWindow = Vector4f(rect.left()   / float(viewSize.x),
                              rect.top()    / float(viewSize.y),
                              rect.width()  / float(viewSize.x),
                              rect.height() / float(viewSize.y));
    d->uBlurMvpMatrix = root().projMatrix2D() *
            Matrix4f::scaleThenTranslate(rect.size(), rect.topLeft);
    d->blurring.setProgram("vert");

    d->blurring.draw();
}

void GuiWidget::requestGeometry(bool yes)
{
    d->needGeometry = yes;
}

bool GuiWidget::geometryRequested() const
{
    return d->needGeometry;
}

bool GuiWidget::isInitialized() const
{
    return d->inited;
}

void GuiWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    if(d->background.type != Background::Blurred &&
       d->background.type != Background::BlurredWithBorderGlow &&
       d->background.type != Background::SharedBlur)
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
        verts.makeFlexibleFrame(rule().recti().shrunk(1),
                                d->background.thickness,
                                d->background.color,
                                root().atlas().imageRectf(root().gradientFrame()));
        break;

    case Background::BorderGlow:
    case Background::BlurredWithBorderGlow:
        verts.makeFlexibleFrame(rule().recti().expanded(d->background.thickness),
                                d->background.thickness,
                                d->background.color,
                                root().atlas().imageRectf(root().borderGlow()));
        break;

    case Background::Blurred: // blurs drawn separately in GuiWidget::draw()
    case Background::SharedBlur:
        break;

    case Background::None:
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
