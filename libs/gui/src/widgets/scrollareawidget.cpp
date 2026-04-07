/** @file scrollareawidget.cpp  Scrollable area.
 *
 * @todo The scroll indicator is currently only implemented for the vertical
 * direction.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scrollareawidget.h"

#include <de/keyevent.h>
#include <de/mouseevent.h>
#include <de/lockable.h>
#include <de/drawable.h>

#include <utility>

#if defined (DE_WINDOWS)
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif

namespace de {

DE_GUI_PIMPL(ScrollAreaWidget), public Lockable
{
    /**
     * Rectangle for all the content shown in the widget. The widget's
     * rectangle is the viewport into this content rectangle.
     */
    RuleRectangle contentRule;

    AnimationRule *x;
    AnimationRule *y;
    Rule *maxX;
    Rule *maxY;

    String scrollBarColorId { "accent" };
    ColorBank::Colorf scrollBarColor;
    ColorBank::Colorf scrollBarHoverColor;
    Animation scrollOpacity { 0 };
    Origin origin = Top;
    int scrollBarWidth { 0 };
    int grabOffset;

    bool scrollBarGrabbed { false };
    bool indicatorAnimating { false };
    bool pageKeysEnabled { true };
    bool scrollingEnabled { true };

    // GL objects.
    bool indicatorDrawEnabled { false };
    bool scrollBarHover { false };
    Rectanglef indicatorUv;
    Rectanglef scrollBarVisRect;
    Rectanglef scrollBarLaneRect;
    GuiVertexBuilder verts;

    Impl(Public *i) : Base(i)
    {
        contentRule.setDebugName("ScrollArea-contentRule");

        updateStyle();

        x = new AnimationRule(0);
        y = new AnimationRule(0);

        maxX = new OperatorRule(OperatorRule::Maximum, Const(0),
                                contentRule.width() - self().rule().width() + self().margins().width());

        maxY = new OperatorRule(OperatorRule::Maximum, Const(0),
                                contentRule.height() - self().rule().height() + self().margins().height());
    }

    ~Impl()
    {
        releaseRef(x);
        releaseRef(y);
        releaseRef(maxX);
        releaseRef(maxY);
    }

    void glInit()
    {
        if (indicatorDrawEnabled)
        {
            /*DefaultVertexBuf *buf = new DefaultVertexBuf;
            drawable.addBuffer(buf);

            shaders().build(drawable.program(), "generic.textured.color_ucolor")
                    << uMvpMatrix << uAtlas() << uColor;*/
        }
    }

    void glDeinit()
    {
        verts.clear();
        //drawable.clear();
    }

    void updateStyle()
    {
        const Style &st = style();

        scrollBarWidth      = st.rules().rule("scrollarea.bar").valuei();
        scrollBarColor      = st.colors().colorf(scrollBarColorId);
        scrollBarHoverColor = st.colors().colorf("background");
    }

    void restartScrollOpacityFade()
    {
        indicatorAnimating = true;
        if (origin == Bottom && self().isAtBottom())
        {
            scrollOpacity.setValue(0, 700_ms, 200_ms);
        }
        else
        {
            scrollOpacity.setValueFrom(.8f, .333f, 5.0_s, 2.0_s);
        }
    }

    void setScrollBarHovering(bool hover)
    {
        if (hover) restartScrollOpacityFade();
        if (hover != scrollBarHover)
        {
            scrollBarHover = hover;
            self().requestGeometry();
        }
    }

    typedef std::pair<Rectanglef, Rectanglef> RectanglefPair;

    RectanglefPair scrollIndicatorRects(const Vec2f &originPos) const
    {
        const Vec2i viewSize = self().viewportSize();
        if (viewSize == Vec2i() || self().maximumScrollY().valuei() == 0)
        {
            return RectanglefPair();
        }

        const auto &margins = self().margins();
        const float contentHeight = float(contentRule.height().value());
        int indHeight = 0;
        
        if (contentHeight > 0)
        {
            indHeight = de::clamp(
                    margins.height().valuei(),
                    int(float(viewSize.y * viewSize.y) / contentHeight),
                    viewSize.y / 2);
        }

        float indPos = self().scrollPositionY().value() / self().maximumScrollY().value();
        if (origin == Top) indPos = 1 - indPos;

        const float avail = viewSize.y - indHeight;
        Rectanglef rect { originPos + Vec2f(viewSize.x + margins.left().value() - 2 * scrollBarWidth,
                                               avail - indPos * avail),
                          originPos + Vec2f(viewSize.x + margins.left().value() - scrollBarWidth,
                                               avail - indPos * avail + indHeight) };

        Rectanglef laneRect(rect.left(), originPos.y, scrollBarWidth, viewSize.y);

        return RectanglefPair(rect, laneRect);
    }
};

ScrollAreaWidget::ScrollAreaWidget(const String &name)
    : GuiWidget(name), d(new Impl(this))
{
    setBehavior(ChildHitClipping);
    setBehavior(ChildVisibilityClipping); // enable clip-culling

    // Link the content rule into the widget's rectangle.
    d->contentRule.setInput(Rule::Left, rule().left() + margins().left() -
                            OperatorRule::minimum(*d->x, *d->maxX));

    setOrigin(Top);
    setContentWidth(0);
    setContentHeight(0);
}

void ScrollAreaWidget::setScrollBarColor(const DotPath &colorId)
{
    d->scrollBarColorId = colorId;
    d->updateStyle();
}

void ScrollAreaWidget::setOrigin(Origin origin)
{
    DE_GUARD(d);

    d->origin = origin;

    if (origin == Top)
    {
        // Anchor content to the top of the widget.
        d->contentRule.setInput(Rule::Top, rule().top() + margins().top() -
                                OperatorRule::minimum(*d->y, *d->maxY));

        d->contentRule.clearInput(Rule::Bottom);
    }
    else
    {
        // Anchor content to the bottom of the widget.
        d->contentRule.setInput(Rule::Bottom, rule().bottom() - margins().bottom() +
                                OperatorRule::minimum(*d->y, *d->maxY));

        d->contentRule.clearInput(Rule::Top);
    }
}

ScrollAreaWidget::Origin ScrollAreaWidget::origin() const
{
    return d->origin;
}

void ScrollAreaWidget::setIndicatorUv(const Rectanglef &uv)
{
    d->indicatorUv = uv;
}

void ScrollAreaWidget::setIndicatorUv(const Vec2f &uvPoint)
{
    d->indicatorUv = Rectanglef::fromSize(uvPoint, Vec2f(0, 0));
}

void ScrollAreaWidget::setContentWidth(int width)
{
    DE_GUARD(d);
    d->contentRule.setInput(Rule::Width, Const(width));
}

void ScrollAreaWidget::setContentWidth(const Rule &width)
{
    DE_GUARD(d);
    d->contentRule.setInput(Rule::Width, width);
}

void ScrollAreaWidget::setContentHeight(int height)
{
    DE_GUARD(d);
    d->contentRule.setInput(Rule::Height, Const(height));
}

void ScrollAreaWidget::setContentHeight(const Rule &height)
{
    DE_GUARD(d);
    d->contentRule.setInput(Rule::Height, height);
}

void ScrollAreaWidget::setContentSize(const Rule &width, const Rule &height)
{
    DE_GUARD(d);
    setContentWidth(width);
    setContentHeight(height);
}

void ScrollAreaWidget::setContentSize(const ISizeRule &dimensions)
{
    DE_GUARD(d);
    setContentWidth(dimensions.width());
    setContentHeight(dimensions.height());
}

void ScrollAreaWidget::setContentSize(const Vec2i &size)
{
    DE_GUARD(d);
    setContentWidth(size.x);
    setContentHeight(size.y);
}

void ScrollAreaWidget::setContentSize(const Vec2ui &size)
{
    setContentSize(Vec2i(size.x, size.y));
}

void ScrollAreaWidget::modifyContentWidth(int delta)
{
    DE_GUARD(d);
    setContentWidth(de::max(0, d->contentRule.width().valuei() + delta));
}

void ScrollAreaWidget::modifyContentHeight(int delta)
{
    DE_GUARD(d);
    setContentHeight(de::max(0, d->contentRule.height().valuei() + delta));
}

int ScrollAreaWidget::contentWidth() const
{
    DE_GUARD(d);
    return d->contentRule.width().valuei();
}

int ScrollAreaWidget::contentHeight() const
{
    DE_GUARD(d);
    return d->contentRule.height().valuei();
}

const RuleRectangle &ScrollAreaWidget::contentRule() const
{
    return d->contentRule;
}

AnimationRule &ScrollAreaWidget::scrollPositionX() const
{
    return *d->x;
}

AnimationRule &ScrollAreaWidget::scrollPositionY() const
{
    return *d->y;
}

const Rule &ScrollAreaWidget::maximumScrollX() const
{
    return *d->maxX;
}

const Rule &ScrollAreaWidget::maximumScrollY() const
{
    return *d->maxY;
}

bool ScrollAreaWidget::isScrolling() const
{
    return !d->x->animation().done() || !d->y->animation().done();
}

Rectanglei ScrollAreaWidget::viewport() const
{
    const Vec4i margin = margins().toVector();

    Rectanglei vp = rule().recti().moved(margin.xy());
    if (int(vp.width()) <= margin.x + margin.z)
    {
        vp.setWidth(0);
    }
    else
    {
        vp.bottomRight.x -= margin.x + margin.z;
    }
    if (int(vp.height()) <= margin.y + margin.w)
    {
        vp.setHeight(0);
    }
    else
    {
        vp.bottomRight.y -= margin.y + margin.w;
    }
    return vp;
}

Vec2i ScrollAreaWidget::viewportSize() const
{
    return Vec2i(rule().width().valuei()  - margins().width().valuei(),
                    rule().height().valuei() - margins().height().valuei())
            .max(Vec2i(0, 0));
}

Vec2i ScrollAreaWidget::scrollPosition() const
{
    DE_GUARD(d);
    return Vec2i(scrollPositionX().valuei(), scrollPositionY().valuei());
}

Vec2i ScrollAreaWidget::scrollPageSize() const
{
    return viewportSize();
}

Vec2i ScrollAreaWidget::maximumScroll() const
{
    DE_GUARD(d);
    return Vec2i(maximumScrollX().valuei(), maximumScrollY().valuei());
}

void ScrollAreaWidget::scroll(const Vec2i &to, const TimeSpan& span)
{
    scrollX(to.x, span);
    scrollY(to.y, span);
}

void ScrollAreaWidget::scrollX(int to, TimeSpan span)
{
    d->x->set(de::clamp(0, to, maximumScrollX().valuei()), std::move(span));
}

void ScrollAreaWidget::scrollY(int to, TimeSpan span)
{
    d->y->set(de::clamp(0, to, maximumScrollY().valuei()), std::move(span));
    d->restartScrollOpacityFade();
}

void ScrollAreaWidget::scrollY(const Rule &to, TimeSpan span)
{
    d->y->set(OperatorRule::clamped(to, Const(0), maximumScrollY()), std::move(span));
    d->restartScrollOpacityFade();
}

bool ScrollAreaWidget::isScrollable() const
{
    return d->scrollingEnabled && maximumScrollY().valuei() > 0;
}

bool ScrollAreaWidget::isAtBottom() const
{
    return d->origin == Bottom && fequal(d->y->animation().target(), 0);
}

void ScrollAreaWidget::enableScrolling(bool enabled)
{
    d->scrollingEnabled = enabled;
}

void ScrollAreaWidget::enablePageKeys(bool enabled)
{
    d->pageKeysEnabled = enabled;
}

void ScrollAreaWidget::enableIndicatorDraw(bool enabled)
{
    d->indicatorDrawEnabled = enabled;
    d->restartScrollOpacityFade();
}

bool ScrollAreaWidget::handleEvent(const Event &event)
{
    if (d->scrollBarGrabbed && event.isMouse())
    {
        // Update position.
        if (event.type() == Event::MousePosition)
        {
            const int mouseY = event.as<MouseEvent>().pos().y;
            int topEdge = mouseY - d->scrollBarLaneRect.top() - d->grabOffset;
            float relativePos = topEdge / (d->scrollBarLaneRect.height() - d->scrollBarVisRect.height());
            if (d->origin == Bottom)
            {
                relativePos = 1.f - relativePos;
            }
            scrollY(relativePos * (contentHeight() - viewportSize().y));
        }

        // Click status.
        switch (handleMouseClick(event))
        {
        case MouseClickFinished:
        case MouseClickAborted:
            d->scrollBarGrabbed = false;
            break;

        default:
            break;
        }
        return true;
    }

    // Mouse wheel scrolling.
    if (d->scrollingEnabled && event.isMouse() && hitTest(event))
    {
        if (event.type() == Event::MouseWheel)
        {
            const MouseEvent &mouse = event.as<MouseEvent>();
#ifdef MACOSX
            if (mouse.wheelMotion() == MouseEvent::Pixels)
            {
                d->y->set(de::clamp(0, int(d->y->animation().target()) +
                                    pointsToPixels(mouse.wheel().y * (d->origin == Top? -1 : 1)),
                                    d->maxY->valuei()), .05f);
                d->restartScrollOpacityFade();
            }
#else
            if (mouse.wheelMotion() == MouseEvent::Steps)
            {
                unsigned int lineCount = 1;
#ifdef DE_WINDOWS
                // Use the number of lines to scroll from system preferences.
                SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lineCount, 0);
                if (lineCount == WHEEL_PAGESCROLL)
                {
                    lineCount = contentRect().height() / style().fonts().font("default").height().valuei();
                }
#endif
                d->y->set(de::clamp(0, int(d->y->animation().target()) +
                                    int(mouse.wheel().y * lineCount * style().fonts().font("default").height().valuei() *
                                        (d->origin == Top? -1 : 1)), d->maxY->valuei()), .15f);
                d->restartScrollOpacityFade();
            }
#endif
            return true;
        }
        else if (d->indicatorDrawEnabled)
        {
            if (event.type() == Event::MousePosition)
            {
                const bool hovering = (d->scrollBarVisRect.expanded(pointsToPixels(1))
                                       .contains(event.as<MouseEvent>().pos()));
                d->setScrollBarHovering(hovering);
            }

            if (d->scrollBarHover && !d->scrollBarGrabbed)
            {
                if(handleMouseClick(event) == MouseClickStarted)
                {
                    d->scrollBarGrabbed = true;
                    d->grabOffset = event.as<MouseEvent>().pos().y - d->scrollBarVisRect.top();
                    return true;
                }
            }
        }
    }

    if (d->scrollBarHover && event.type() == Event::MousePosition && !hitTest(event))
    {
        d->setScrollBarHovering(false);
    }

    // Page key scrolling.
    if (d->scrollingEnabled && event.isKeyDown())
    {
        const KeyEvent &ev = event.as<KeyEvent>();

        float pageSize = scrollPageSize().y;
        if (d->origin == Bottom) pageSize = -pageSize;

        switch (ev.ddKey())
        {
        case DDKEY_PGUP:
            if (!d->pageKeysEnabled) return false;
            if (ev.modifiers().testFlag(KeyEvent::Shift))
            {
                scrollToTop();
            }
            else
            {
                scrollY(d->y->animation().target() - pageSize/2, .3f);
            }
            return true;

        case DDKEY_PGDN:
            if (!d->pageKeysEnabled) return false;
            if (ev.modifiers().testFlag(KeyEvent::Shift))
            {
                scrollToBottom();
            }
            else
            {
                scrollY(d->y->animation().target() + pageSize/2, .3f);
            }
            return true;

        default:
            break;
        }
    }

    return GuiWidget::handleEvent(event);
}

void ScrollAreaWidget::scrollToTop(const TimeSpan& span)
{
    if (d->origin == Top)
    {
        scrollY(0, span);
    }
    else
    {
        scrollY(maximumScrollY().valuei(), span);
    }
}

void ScrollAreaWidget::scrollToBottom(const TimeSpan& span)
{
    if (d->origin == Top)
    {
        scrollY(maximumScrollY().valuei(), span);
    }
    else
    {
        scrollY(0, span);
    }
}

void ScrollAreaWidget::scrollToLeft(TimeSpan span)
{
    scrollX(0, span);
}

void ScrollAreaWidget::scrollToRight(TimeSpan span)
{
    scrollX(maximumScrollX().valuei(), span);
}

void ScrollAreaWidget::scrollToWidget(const GuiWidget &widget, TimeSpan span)
{
    int off = widget.rule().midY().valuei() - contentRule().top().valuei() -
              rule().height().valuei()/2;
    scrollY(off, span);
}

ScrollAreaWidget &ScrollAreaWidget::findTopmostScrollable()
{
    for (GuiWidget *parent = parentGuiWidget(); parent; parent = parent->parentGuiWidget())
    {
        if (ScrollAreaWidget *scroll = maybeAs<ScrollAreaWidget>(parent))
        {
            if (scroll->isScrollable())
            {
                return *scroll;
            }
        }
    }
    return *this;
}

void ScrollAreaWidget::glInit()
{
    d->glInit();
}

void ScrollAreaWidget::glDeinit()
{
    d->glDeinit();
}

void ScrollAreaWidget::glMakeScrollIndicatorGeometry(GuiVertexBuilder &verts,
                                                     const Vec2f &origin)
{
    // Draw the scroll indicator.
    if (d->scrollOpacity <= 0) return;

    auto rects = d->scrollIndicatorRects(origin);
    d->scrollBarVisRect  = rects.first;
    d->scrollBarLaneRect = rects.second;
    if (d->scrollBarVisRect.isNull()) return;

    Vec4f const barOpacity { 1, 1, 1, d->scrollOpacity };

    verts.makeQuad(d->scrollBarVisRect.expanded((d->scrollBarHover ? pointsToPixels(1) : 0)),
                   barOpacity * d->scrollBarColor,
                   d->indicatorUv);

    if (d->scrollBarHover)
    {
        verts.makeQuad(d->scrollBarVisRect, barOpacity * d->scrollBarHoverColor, d->indicatorUv);
    }
}

/*void ScrollAreaWidget::viewResized()
{
    GuiWidget::viewResized();
    //d->uMvpMatrix = root().projMatrix2D();
}*/

void ScrollAreaWidget::update()
{
    GuiWidget::update();

    if (d->indicatorAnimating)
    {
        requestGeometry();
    }
    if (d->scrollOpacity.done())
    {
        d->indicatorAnimating = false;
    }

    // Clamp the scroll position.
    if (d->x->value() > d->maxX->value())
    {
        d->x->set(d->maxX->value());
    }
    if (d->y->value() > d->maxY->value())
    {
        d->y->set(d->maxY->value());
    }
}

void ScrollAreaWidget::drawContent()
{
    if (d->indicatorDrawEnabled)
    {
        auto &painter = root().painter();

        // The indicator is quite simple, so just keep it dynamic. This will
        // also avoid the need to detect when the indicator is moving and
        // whether the atlas has been repositioned.

        setIndicatorUv(root().atlas().imageRectf(root().solidWhitePixel()).middle());

        d->verts.clear(); //DefaultVertexBuf::Builder verts;
        glMakeScrollIndicatorGeometry(d->verts, rule().recti().topLeft + margins().toVector().xy());
        //d->drawable.buffer<DefaultVertexBuf>().setVertices(gfx::TriangleStrip, verts, gfx::Dynamic);

        if (d->verts)
        {
        //d->drawable.draw();
            painter.setColor(Vec4f(1, 1, 1, visibleOpacity()));
            painter.drawTriangleStrip(d->verts);
        }
    }
}

} // namespace de
