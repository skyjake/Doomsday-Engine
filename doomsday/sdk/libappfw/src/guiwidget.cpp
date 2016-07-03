/** @file guiwidget.cpp  Base class for graphical widgets.
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

#include "de/GuiWidget"
#include "de/GuiRootWidget"
#include "de/BlurWidget"
#include "de/Style"
#include "de/BaseGuiApp"
#include "de/IPersistent"

#include <de/Garbage>
#include <de/MouseEvent>
#include <de/Drawable>
#include <de/GLTexture>
#include <de/GLTarget>

#include <QList>

namespace de {

DENG2_PIMPL(GuiWidget)
, DENG2_OBSERVES(Widget, ChildAddition)
, DENG2_OBSERVES(ui::Margins, Change)
#ifdef DENG2_DEBUG
, DENG2_OBSERVES(Widget, ParentChange)
#endif
{
    RuleRectangle rule;     ///< Visual rule, used when drawing.
    std::unique_ptr<RuleRectangle> hitRule; ///< Used only for hit testing. By default matches the visual rule.
    ui::Margins margins;
    Rectanglei savedPos;
    bool inited;
    bool needGeometry;
    bool styleChanged;
    Attributes attribs;
    Background background;
    Animation opacity;
    Animation opacityWhenDisabled;
    bool firstUpdateAfterCreation;
    QList<IEventHandler *> eventHandlers;

    // Style.
    DotPath fontId;
    DotPath textColorId;

    // Background blurring.
    struct BlurState
    {
        Vector2ui size;
        QScopedPointer<GLFramebuffer> fb[2];
        Drawable drawable;
        GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
        GLUniform uColor     { "uColor",     GLUniform::Vec4 };
        GLUniform uTex       { "uTex",       GLUniform::Sampler2D };
        GLUniform uBlurStep  { "uBlurStep",  GLUniform::Vec2 };
        GLUniform uWindow    { "uWindow",    GLUniform::Vec4 };
    };
    std::unique_ptr<BlurState> blur;

    Impl(Public *i)
        : Base(i)
        , margins("gap")
        , inited(false)
        , needGeometry(true)
        , styleChanged(false)
        , attribs(DefaultAttributes)
        , opacity(1.f, Animation::Linear)
        , opacityWhenDisabled(1.f, Animation::Linear)
        , firstUpdateAfterCreation(true)
        , fontId("default")
        , textColorId("text")
    {
        self.audienceForChildAddition() += this;
        margins.audienceForChange() += this;

#ifdef DENG2_DEBUG
        self.audienceForParentChange() += this;
        rule.setDebugName(self.path());
#endif
    }

    ~Impl()
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
        if (inited) qDebug() << "GuiWidget" << &self << self.name() << "is still inited!";
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
        if (self.hasRoot())
        {
            // Make sure newly added children know the view size.
            child.viewResized();
            child.notifyTree(&Widget::viewResized);
        }
    }

    /**
     * Test if a clipped widget is contained within its ancestors' clip rectangles.
     */
    bool isClipCulled() const
    {
        bool wasClipped = false;
        Rectanglei visibleArea = self.root().viewRule().recti();

        for (Widget const *w = self.parentWidget(); w; w = w->parent())
        {
            if (!w->is<GuiWidget>()) continue;

            // Does this ancestor use child clipping?
            if (w->behavior().testFlag(ChildVisibilityClipping))
            {
                wasClipped = true;
                visibleArea &= w->as<GuiWidget>().rule().recti();
            }
        }
        if (!wasClipped) return false;

        if (self.isClipped())
        {
            int const CULL_SAFETY_WIDTH = 100; // avoid pop-in when scrolling

            // Clipped widgets are guaranteed to be within their rectangle.
            return !visibleArea.overlaps(self.rule().recti().expanded(CULL_SAFETY_WIDTH));
        }
        // Otherwise widgets may draw anywhere in the view.
        return visibleArea.isNull();
    }

    void initBlur()
    {
        if (blur) return;

        blur.reset(new BlurState);

        // The blurred version of the view is downsampled.
        blur->size = (self.root().viewSize() / GuiWidget::toDevicePixels(4)).max(Vector2ui(1, 1));

        for (int i = 0; i < 2; ++i)
        {
            // Multisampling is disabled in the blurs for now.
            blur->fb[i].reset(new GLFramebuffer(Image::RGB_888, blur->size, 1));
            blur->fb[i]->glInit();
            blur->fb[i]->colorTexture().setFilter(gl::Linear, gl::Linear, gl::MipNone);
        }

        // Set up the drawble.
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        blur->drawable.addBuffer(buf);
        buf->setVertices(gl::TriangleStrip,
                         DefaultVertexBuf::Builder().makeQuad(
                             Rectanglef(0, 0, 1, 1),
                             Vector4f(1, 1, 1, 1),
                             Rectanglef(0, 0, 1, 1)),
                         gl::Static);

        blur->uBlurStep = Vector2f(1.f / float(blur->size.x),
                                   1.f / float(blur->size.y));

        self.root().shaders().build(blur->drawable.program(), "fx.blur.horizontal")
                << blur->uMvpMatrix
                << blur->uTex
                << blur->uBlurStep
                << blur->uWindow;

        blur->drawable.addProgram("vert");
        self.root().shaders().build(blur->drawable.program("vert"), "fx.blur.vertical")
                << blur->uMvpMatrix
                << blur->uTex
                << blur->uColor
                << blur->uBlurStep
                << blur->uWindow;
    }

    void deinitBlur()
    {
        if (!blur) return;

        for (int i = 0; i < 2; ++i)
        {
            blur->fb[i].reset();
        }
        blur->drawable.clear();

        blur.reset();
    }

    void reinitBlur()
    {
        if (blur)
        {
            deinitBlur();
            initBlur();
        }
    }

    void drawBlurredBackground()
    {
        if (background.type == Background::SharedBlur ||
            background.type == Background::SharedBlurWithBorderGlow)
        {
            // Use another widget's blur.
            DENG2_ASSERT(background.blur != 0);
            if (background.blur)
            {
                background.blur->drawBlurredRect(self.rule().recti(), background.solidFill);
            }
            return;
        }

        if (background.type != Background::Blurred &&
            background.type != Background::BlurredWithBorderGlow &&
            background.type != Background::BlurredWithSolidFill)
        {
            deinitBlur();
            return;
        }

        // Make sure blurring is initialized.
        initBlur();

        DENG2_ASSERT(blur->fb[0]->isReady());

        // Pass 1: render all the widgets behind this one onto the first blur
        // texture, downsampled.
        GLState::push()
                .setTarget(blur->fb[0]->target())
                .setViewport(Rectangleui::fromSize(blur->size));
        blur->fb[0]->target().clear(GLTarget::Depth);
        self.root().drawUntil(self);
        GLState::pop();

        // Pass 2: apply the horizontal blur filter to draw the background
        // contents onto the second blur texture.
        GLState::push()
                .setTarget(blur->fb[1]->target())
                .setViewport(Rectangleui::fromSize(blur->size));
        blur->uTex = blur->fb[0]->colorTexture();
        blur->uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        blur->uWindow = Vector4f(0, 0, 1, 1);
        blur->drawable.setProgram(blur->drawable.program());
        blur->drawable.draw();
        GLState::pop();

        // Pass 3: apply the vertical blur filter, drawing the final result
        // into the original target.
        Vector4f blurColor = background.solidFill;
        float blurOpacity  = self.visibleOpacity();
        if (background.type == Background::BlurredWithSolidFill)
        {
            blurColor.w = 1;
        }
        if (!attribs.testFlag(DontDrawContent) && blurColor.w > 0 && blurOpacity > 0)
        {
            self.drawBlurredRect(self.rule().recti(), blurColor, blurOpacity);
        }
    }

    inline float currentOpacity() const
    {
        return min(opacity.value(), opacityWhenDisabled.value());
    }

    void updateOpacityForDisabledWidgets()
    {
        float const opac = (self.isDisabled()? .3f : 1.f);
        if (opacityWhenDisabled.target() != opac)
        {
            opacityWhenDisabled.setValue(opac, .3f);
        }
        if (firstUpdateAfterCreation ||
           !attribs.testFlag(AnimateOpacityWhenEnabledOrDisabled))
        {
            opacityWhenDisabled.finish();
        }
    }

    void restoreState()
    {
        try
        {
            if (IPersistent *po = self.maybeAs<IPersistent>())
            {
                DENG2_BASE_GUI_APP->persistentUIState() >> *po;
            }
        }
        catch (Error const &er)
        {
            // Benign: widget will use default state.
            LOG_VERBOSE("Failed to restore state of widget '%s': %s")
                    << self.path() << er.asText();
        }
    }

    void saveState()
    {
        try
        {
            if (IPersistent *po = self.maybeAs<IPersistent>())
            {
                DENG2_BASE_GUI_APP->persistentUIState() << *po;
            }
        }
        catch (Error const &er)
        {
            LOG_WARNING("Failed to save state of widget '%s': %s")
                    << self.path() << er.asText();
        }
    }

    static float toDevicePixels(float logicalPixels)
    {
        return logicalPixels * DENG2_BASE_GUI_APP->dpiFactor();
    }
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Impl(this))
{
    d->rule.setDebugName(name);
}

void GuiWidget::destroy(GuiWidget *widget)
{
    if (widget)
    {
        widget->deinitialize();
        delete widget;
    }
}

void GuiWidget::destroyLater(GuiWidget *widget)
{
    if (widget)
    {
        widget->deinitialize();
        widget->guiDeleteLater();
    }
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
    return Style::get();
}

Rule const &GuiWidget::rule(DotPath const &path) const
{
    return style().rules().rule(path);
}

Font const &GuiWidget::font() const
{
    return style().fonts().font(d->fontId);
}

DotPath const &GuiWidget::fontId() const
{
    return d->fontId;
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

Rectanglei GuiWidget::contentRect() const
{
    Vector4i const pad = margins().toVector();
    return rule().recti().adjusted(pad.xy(), -pad.zw());
}

RuleRectangle const &GuiWidget::rule() const
{
    return d->rule;
}

float GuiWidget::estimatedHeight() const
{
    return rule().height().value();
}

ui::Margins &GuiWidget::margins()
{
    return d->margins;
}

ui::Margins const &GuiWidget::margins() const
{
    return d->margins;
}

Rectanglef GuiWidget::normalizedRect(de::Rectanglei const &rect,
                                     de::Rectanglei const &containerRect) // static
{
    Rectanglef const rectf = rect.moved(-containerRect.topLeft);
    Vector2f const contSize = containerRect.size();
    return Rectanglef(Vector2f(rectf.left()   / contSize.x,
                               rectf.top()    / contSize.y),
                      Vector2f(rectf.right()  / contSize.x,
                               rectf.bottom() / contSize.y));
}

float GuiWidget::toDevicePixels(float logicalPixels)
{
    return Impl::toDevicePixels(logicalPixels);
}

Rectanglef GuiWidget::normalizedRect() const
{
    return GuiWidget::normalizedRect(rule().recti(),
                                     Rectanglei::fromSize(root().viewSize()));
}

Rectanglef GuiWidget::normalizedRect(Rectanglei const &viewSpaceRect) const
{
    return GuiWidget::normalizedRect(viewSpaceRect,
                                     Rectanglei::fromSize(root().viewSize()));
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

GuiWidget::ColorTheme GuiWidget::invertColorTheme(ColorTheme theme)
{
    return theme == Inverted? Normal : Inverted;
}

void GuiWidget::recycleTrashedWidgets()
{
    Garbage_RecycleAllWithDestructor(deleteGuiWidget);
}

void GuiWidget::set(Background const &bg)
{
    d->background = bg;
    requestGeometry();
}

bool GuiWidget::isClipped() const
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
    float opacity = d->currentOpacity();
    if (!d->attribs.testFlag(IndependentOpacity))
    {
        for (Widget *i = Widget::parent(); i != 0; i = i->parent())
        {
            if (GuiWidget *w = i->maybeAs<GuiWidget>())
            {
                opacity *= w->d->currentOpacity();
            }
        }
    }
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

void GuiWidget::setAttribute(Attributes const &attr, FlagOp op)
{
    applyFlagOperation(d->attribs, attr, op);
}

GuiWidget::Attributes GuiWidget::attributes() const
{
    return d->attribs;
}

void GuiWidget::saveState()
{
    d->saveState();

    foreach (Widget *child, childWidgets())
    {
        if (GuiWidget *widget = child->maybeAs<GuiWidget>())
        {
            widget->saveState();
        }
    }
}

void GuiWidget::restoreState()
{
    d->restoreState();

    foreach (Widget *child, childWidgets())
    {
        if (GuiWidget *widget = child->maybeAs<GuiWidget>())
        {
            widget->restoreState();
        }
    }
}

void GuiWidget::initialize()
{
    if (d->inited) return;

    try
    {
        d->inited = true;
        glInit();

        if (d->attribs.testFlag(RetainStatePersistently))
        {
            d->restoreState();
        }
    }
    catch (Error const &er)
    {
        LOG_WARNING("Error when initializing widget '%s': %s")
                << name() << er.asText();
    }
}

void GuiWidget::deinitialize()
{
    if (!d->inited) return;

    try
    {
        if (d->attribs.testFlag(RetainStatePersistently))
        {
            d->saveState();
        }

        d->inited = false;
        d->deinitBlur();
        glDeinit();
    }
    catch (Error const &er)
    {
        LOG_WARNING("Error when deinitializing widget '%s': %s")
                << name() << er.asText();
    }
}

void GuiWidget::viewResized()
{
    d->reinitBlur();
}

void GuiWidget::update()
{
    if (!d->inited)
    {
        initialize();
    }
    if (d->styleChanged)
    {
        d->styleChanged = false;
        updateStyle();
    }
    if (!d->attribs.testFlag(ManualOpacity))
    {
        d->updateOpacityForDisabledWidgets();
    }

    d->firstUpdateAfterCreation = false;
}

void GuiWidget::draw()
{
    if (d->inited && !isHidden() && visibleOpacity() > 0 && !d->isClipCulled())
    {
#ifdef DENG2_DEBUG
        // Detect mistakes in GLState stack usage.
        dsize const depthBeforeDrawingWidget = GLState::stackDepth();
#endif

        d->drawBlurredBackground();

        if (!d->attribs.testFlag(DontDrawContent))
        {
            if (isClipped())
            {
                GLState::push().setNormalizedScissor(normalizedRect());
            }

            drawContent();

            if (isClipped())
            {
                GLState::pop();
            }
        }

        DENG2_ASSERT(GLState::stackDepth() == depthBeforeDrawingWidget);
    }
}

bool GuiWidget::handleEvent(Event const &event)
{
    foreach (IEventHandler *handler, d->eventHandlers)
    {
        if (handler->handleEvent(*this, event))
        {
            return true;
        }
    }

    if (Widget::handleEvent(event))
    {
        return true;
    }

    if (d->attribs.testFlag(EatAllMouseEvents))
    {
        if ((event.type() == Event::MouseButton ||
            event.type() == Event::MousePosition ||
            event.type() == Event::MouseWheel) && hitTest(event))
        {
            return true;
        }
    }
    return false;
}

bool GuiWidget::hitTest(Vector2i const &pos) const
{
    if (behavior().testFlag(Unhittable))
    {
        // Can never be hit by anything.
        return false;
    }

    Widget const *w = Widget::parent();
    while (w)
    {
        GuiWidget const *gui = dynamic_cast<GuiWidget const *>(w);
        if (gui)
        {
            if (gui->behavior().testFlag(ChildHitClipping) &&
               !gui->hitRule().recti().contains(pos))
            {
                // Must hit clipped parent widgets as well.
                return false;
            }
        }
        w = w->Widget::parent();
    }

    return hitRule().recti().contains(pos);
}

bool GuiWidget::hitTest(Event const &event) const
{
    return event.isMouse() && hitTest(event.as<MouseEvent>().pos());
}

GuiWidget const *GuiWidget::treeHitTest(Vector2i const &pos) const
{
    Children const childs = childWidgets();
    for (int i = childs.size() - 1; i >= 0; --i)
    {
        if (GuiWidget const *w = childs.at(i)->maybeAs<GuiWidget>())
        {
            // Check children first.
            if (GuiWidget const *hit = w->treeHitTest(pos))
            {
                return hit;
            }
        }
    }
    if (hitTest(pos))
    {
        return this;
    }
    return 0;
}

RuleRectangle &GuiWidget::hitRule()
{
    if (!d->hitRule)
    {
        d->hitRule.reset(new RuleRectangle);
        d->hitRule->setRect(d->rule);
    }
    return *d->hitRule;
}

RuleRectangle const &GuiWidget::hitRule() const
{
    if (d->hitRule) return *d->hitRule;
    return d->rule;
}

GuiWidget::MouseClickStatus GuiWidget::handleMouseClick(Event const &event, MouseEvent::Button button)
{
    if (isDisabled()) return MouseClickUnrelated;

    if (event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if (mouse.button() != button)
        {
            return MouseClickUnrelated;
        }

        if (mouse.state() == MouseEvent::Pressed && hitTest(mouse.pos()))
        {
            root().routeMouse(this);
            return MouseClickStarted;
        }

        if (mouse.state() == MouseEvent::Released && root().isEventRouted(event.type(), this))
        {
            root().routeMouse(nullptr);
            if (hitTest(mouse.pos()))
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

void GuiWidget::drawBlurredRect(Rectanglei const &rect, Vector4f const &color, float opacity)
{
    auto *blur = d->blur.get();
    if (!blur) return;

    DENG2_ASSERT(blur->fb[1]->isReady());

    Vector2ui const viewSize = root().viewSize();

    blur->uTex = blur->fb[1]->colorTexture();
    blur->uColor = Vector4f((1 - color.w) + color.x * color.w,
                            (1 - color.w) + color.y * color.w,
                            (1 - color.w) + color.z * color.w,
                            opacity);
    blur->uWindow = Vector4f(rect.left()   / float(viewSize.x),
                             rect.top()    / float(viewSize.y),
                             rect.width()  / float(viewSize.x),
                             rect.height() / float(viewSize.y));
    blur->uMvpMatrix = root().projMatrix2D() *
                       Matrix4f::scaleThenTranslate(rect.size(), rect.topLeft);
    blur->drawable.setProgram("vert");

    blur->drawable.draw();
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

GuiWidget *GuiWidget::guiFind(String const &name)
{
    return find(name)->maybeAs<GuiWidget>();
}

GuiWidget const *GuiWidget::guiFind(String const &name) const
{
    return find(name)->maybeAs<GuiWidget>();
}

void GuiWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    auto &rootWgt = root();
    float const thick = d->toDevicePixels(d->background.thickness);

    // Is there a solid fill?
    if (d->background.solidFill.w > 0)
    {
        if (d->background.type == Background::GradientFrameWithRoundedFill)
        {
            Rectanglei const recti = rule().recti().shrunk(d->toDevicePixels(2));
            verts.makeQuad(recti.shrunk(thick), d->background.solidFill,
                           rootWgt.atlas().imageRectf(rootWgt.solidRoundCorners()).middle());
            verts.makeFlexibleFrame(recti, thick, d->background.solidFill,
                                    rootWgt.atlas().imageRectf(rootWgt.solidRoundCorners()));
        }
        else if (d->background.type != Background::Blurred &&
                 d->background.type != Background::BlurredWithBorderGlow &&
                 d->background.type != Background::SharedBlur &&
                 d->background.type != Background::SharedBlurWithBorderGlow)
        {
            verts.makeQuad(rule().recti(),
                           d->background.solidFill,
                           rootWgt.atlas().imageRectf(rootWgt.solidWhitePixel()).middle());
        }
    }

    switch (d->background.type)
    {
    case Background::GradientFrame:
    case Background::GradientFrameWithRoundedFill:
        verts.makeFlexibleFrame(rule().recti().shrunk(d->toDevicePixels(1)),
                                thick,
                                d->background.color,
                                rootWgt.atlas().imageRectf(rootWgt.boldRoundCorners()));
        break;

    case Background::Rounded:
        verts.makeFlexibleFrame(rule().recti().shrunk(d->toDevicePixels(d->background.thickness - 4)),
                                thick,
                                d->background.color,
                                rootWgt.atlas().imageRectf(rootWgt.roundCorners()));
        break;

    case Background::BorderGlow:
    case Background::BlurredWithBorderGlow:
    case Background::SharedBlurWithBorderGlow:
        verts.makeFlexibleFrame(rule().recti().expanded(thick),
                                thick,
                                d->background.color,
                                rootWgt.atlas().imageRectf(rootWgt.borderGlow()));
        break;

    case Background::Blurred: // blurs drawn separately in GuiWidget::draw()
    case Background::SharedBlur:
    case Background::BlurredWithSolidFill:
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

Animation &GuiWidget::opacityAnimation()
{
    return d->opacity;
}

void GuiWidget::preDrawChildren()
{
    if (behavior().testFlag(ChildVisibilityClipping))
    {
        GLState::push().setNormalizedScissor(normalizedRect());
    }
}

void GuiWidget::postDrawChildren()
{
    if (behavior().testFlag(ChildVisibilityClipping))
    {
        GLState::pop();
    }
}

} // namespace de
