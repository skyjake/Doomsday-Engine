/** @file scrollareawidget.cpp  Scrollable area.
 *
 * @todo The scroll indicator is currently only implemented for the vertical
 * direction.
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

#include "ui/widgets/scrollareawidget.h"

#include <de/GLState>
#include <de/KeyEvent>
#include <de/MouseEvent>
#include <de/Lockable>

using namespace de;

DENG2_PIMPL(ScrollAreaWidget), public Lockable
{
    /**
     * Rectangle for all the content shown in the widget. The widget's
     * rectangle is the viewport into this content rectangle.
     */
    RuleRectangle contentRule;

    ScalarRule *x;
    ScalarRule *y;
    Rule *maxX;
    Rule *maxY;

    Origin origin;
    bool pageKeysEnabled;
    Animation scrollOpacity;
    int scrollBarWidth;
    Rectanglef indicatorUv;
    ColorBank::Colorf accent;

    Rule const *margin;
    Rule const *vertMargin;

    Instance(Public *i)
        : Base(i),
          origin(Top),
          pageKeysEnabled(true),
          scrollOpacity(0),
          scrollBarWidth(0),
          margin(0),
          vertMargin(0)
    {
        contentRule.setDebugName("ScrollArea-contentRule");

        updateStyle();

        x = new ScalarRule(0);
        y = new ScalarRule(0);

        maxX = new OperatorRule(OperatorRule::Maximum, Const(0),
                                contentRule.width() - self.rule().width() + *margin * 2);

        maxY = new OperatorRule(OperatorRule::Maximum, Const(0),
                                contentRule.height() - self.rule().height() + *vertMargin * 2);
    }

    ~Instance()
    {
        releaseRef(x);
        releaseRef(y);
        releaseRef(maxX);
        releaseRef(maxY);
    }

    void updateStyle()
    {
        Style const &st = self.style();

        margin         = &st.rules().rule("gap");
        vertMargin     = &st.rules().rule("gap");
        scrollBarWidth = st.rules().rule("scrollarea.bar").valuei();
        accent         = st.colors().colorf("accent");
    }

    void restartScrollOpacityFade()
    {
        if(origin == Bottom && self.isAtBottom())
        {
            scrollOpacity.setValue(0, .7f, .2f);
        }
        else
        {
            scrollOpacity.setValueFrom(.8f, .333f, 5, 2);
        }
    }
};

ScrollAreaWidget::ScrollAreaWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    setBehavior(ChildHitClipping);

    // Link the content rule into the widget's rectangle.
    d->contentRule.setInput(Rule::Left, rule().left() + *d->margin -
                            OperatorRule::minimum(*d->x, *d->maxX));

    setOrigin(Top);
    setContentWidth(0);
    setContentHeight(0);
}

void ScrollAreaWidget::setOrigin(Origin origin)
{
    DENG2_GUARD(d);

    d->origin = origin;

    if(origin == Top)
    {
        // Anchor content to the top of the widget.
        d->contentRule.setInput(Rule::Top, rule().top() + *d->vertMargin -
                                OperatorRule::minimum(*d->y, *d->maxY));

        d->contentRule.clearInput(Rule::Bottom);
    }
    else
    {
        // Anchor content to the bottom of the widget.
        d->contentRule.setInput(Rule::Bottom, rule().bottom() - *d->vertMargin +
                                OperatorRule::minimum(*d->y, *d->maxY));

        d->contentRule.clearInput(Rule::Top);
    }
}

ScrollAreaWidget::Origin ScrollAreaWidget::origin() const
{
    return d->origin;
}

void ScrollAreaWidget::setIndicatorUv(Rectanglef const &uv)
{
    d->indicatorUv = uv;
}

void ScrollAreaWidget::setIndicatorUv(Vector2f const &uvPoint)
{
    d->indicatorUv = Rectanglef::fromSize(uvPoint, Vector2f(0, 0));
}

void ScrollAreaWidget::setContentWidth(int width)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Width, Const(width));
}

void ScrollAreaWidget::setContentWidth(Rule const &width)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Width, width);
}

void ScrollAreaWidget::setContentHeight(int height)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Height, Const(height));
}

void ScrollAreaWidget::setContentHeight(Rule const &height)
{
    DENG2_GUARD(d);
    d->contentRule.setInput(Rule::Height, height);
}

void ScrollAreaWidget::setContentSize(Vector2i const &size)
{
    DENG2_GUARD(d);
    setContentWidth(size.x);
    setContentHeight(size.y);
}

void ScrollAreaWidget::modifyContentWidth(int delta)
{
    DENG2_GUARD(d);
    setContentWidth(de::max(0, d->contentRule.width().valuei() + delta));
}

void ScrollAreaWidget::modifyContentHeight(int delta)
{
    DENG2_GUARD(d);
    setContentHeight(de::max(0, d->contentRule.height().valuei() + delta));
}

int ScrollAreaWidget::contentWidth() const
{
    DENG2_GUARD(d);
    return d->contentRule.width().valuei();
}

int ScrollAreaWidget::contentHeight() const
{
    DENG2_GUARD(d);
    return d->contentRule.height().valuei();
}

RuleRectangle const &ScrollAreaWidget::contentRule() const
{
    return d->contentRule;
}

ScalarRule &ScrollAreaWidget::scrollPositionX() const
{
    return *d->x;
}

ScalarRule &ScrollAreaWidget::scrollPositionY() const
{
    return *d->y;
}

Rule const &ScrollAreaWidget::maximumScrollX() const
{
    return *d->maxX;
}

Rule const &ScrollAreaWidget::maximumScrollY() const
{
    return *d->maxY;
}

Rectanglei ScrollAreaWidget::viewport() const
{
    duint const margin = d->margin->valuei();
    duint const vertMargin = d->vertMargin->valuei();

    Rectanglei vp = rule().recti().moved(Vector2i(margin, vertMargin));
    if(vp.width() <= 2 * margin)
    {
        vp.setWidth(0);
    }
    else
    {
        vp.bottomRight.x -= 2 * margin;
    }
    if(vp.height() <= 2 * vertMargin)
    {
        vp.setHeight(0);
    }
    else
    {
        vp.bottomRight.y -= 2 * vertMargin;
    }
    return vp;
}

Vector2i ScrollAreaWidget::viewportSize() const
{
    return Vector2i(rule().width().valuei()  - 2 * d->margin->valuei(),
                    rule().height().valuei() - 2 * d->vertMargin->valuei())
            .max(Vector2i(0, 0));
}

int ScrollAreaWidget::topMargin() const
{
    return d->vertMargin->valuei();
}

int ScrollAreaWidget::rightMargin() const
{
    return d->margin->valuei();
}

Vector2i ScrollAreaWidget::scrollPosition() const
{
    DENG2_GUARD(d);
    return Vector2i(scrollPositionX().valuei(), scrollPositionY().valuei());
}

Vector2i ScrollAreaWidget::scrollPageSize() const
{
    return viewportSize();
}

Vector2i ScrollAreaWidget::maximumScroll() const
{
    DENG2_GUARD(d);
    return Vector2i(maximumScrollX().valuei(), maximumScrollY().valuei());
}

void ScrollAreaWidget::scroll(Vector2i const &to, TimeDelta span)
{
    scrollX(to.x, span);
    scrollY(to.y, span);
}

void ScrollAreaWidget::scrollX(int to, TimeDelta span)
{
    d->x->set(de::clamp(0, to, maximumScrollX().valuei()), span);
}

void ScrollAreaWidget::scrollY(int to, TimeDelta span)
{
    d->y->set(de::clamp(0, to, maximumScrollY().valuei()), span);
    d->restartScrollOpacityFade();
}

bool ScrollAreaWidget::isAtBottom() const
{
    return d->origin == Bottom && d->y->animation().target() == 0;
}

void ScrollAreaWidget::enablePageKeys(bool enabled)
{
    d->pageKeysEnabled = enabled;
}

bool ScrollAreaWidget::handleEvent(Event const &event)
{
    // Mouse wheel scrolling.
    if(event.type() == Event::MouseWheel && hitTest(event))
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.wheelMotion() == MouseEvent::FineAngle)
        {
            d->y->set(de::clamp(0, int(d->y->animation().target()) +
                                mouse.wheel().y / 2 * (d->origin == Top? -1 : 1),
                                d->maxY->valuei()), .05f);
            d->restartScrollOpacityFade();
        }
        return true;
    }

    // Page key scrolling.
    if(event.isKeyDown())
    {
        KeyEvent const &ev = event.as<KeyEvent>();

        float pageSize = scrollPageSize().y;
        if(d->origin == Bottom) pageSize = -pageSize;

        switch(ev.ddKey())
        {
        case DDKEY_PGUP:
            if(!d->pageKeysEnabled) return false;
            if(ev.modifiers().testFlag(KeyEvent::Shift))
            {
                scrollToTop();
            }
            else
            {
                scrollY(d->y->animation().target() - pageSize, .3f);
            }
            return true;

        case DDKEY_PGDN:
            if(!d->pageKeysEnabled) return false;
            if(ev.modifiers().testFlag(KeyEvent::Shift))
            {
                scrollToBottom();
            }
            else
            {
                scrollY(d->y->animation().target() + pageSize, .3f);
            }
            return true;

        default:
            break;
        }
    }

    return GuiWidget::handleEvent(event);
}

void ScrollAreaWidget::scrollToTop(TimeDelta span)
{
    if(d->origin == Top)
    {
        scrollY(0, span);
    }
    else
    {
        scrollY(maximumScrollY().valuei(), span);
    }
}

void ScrollAreaWidget::scrollToBottom(TimeDelta span)
{
    if(d->origin == Top)
    {
        scrollY(maximumScrollY().valuei(), span);
    }
    else
    {
        scrollY(0, span);
    }
}

void ScrollAreaWidget::scrollToLeft(TimeDelta span)
{
    scrollX(0, span);
}

void ScrollAreaWidget::scrollToRight(TimeDelta span)
{
    scrollX(maximumScrollX().valuei(), span);
}

void ScrollAreaWidget::glMakeScrollIndicatorGeometry(DefaultVertexBuf::Builder &verts)
{
    // Draw the scroll indicator.
    if(d->scrollOpacity <= 0) return;

    Vector2i const viewSize = viewportSize();
    if(viewSize == Vector2i(0, 0)) return;

    int const indHeight = de::clamp(
                d->vertMargin->valuei() * 2,
                int(float(viewSize.y * viewSize.y) / float(d->contentRule.height().value())),
                viewSize.y / 2);
    float const indPos = scrollPositionY().value() / maximumScrollY().value();
    float const avail = viewSize.y - indHeight;

    verts.makeQuad(Rectanglef(Vector2f(viewSize.x + d->margin->value() - 2 * d->scrollBarWidth,
                                       avail - indPos * avail + indHeight),
                              Vector2f(viewSize.x + d->margin->value() - d->scrollBarWidth,
                                       avail - indPos * avail)),
                   Vector4f(1, 1, 1, d->scrollOpacity) * d->accent,
                   d->indicatorUv);
}

void ScrollAreaWidget::update()
{
    GuiWidget::update();

    // Clamp the scroll position.
    if(d->x->value() > d->maxX->value())
    {
        d->x->set(d->maxX->value());
    }
    if(d->y->value() > d->maxY->value())
    {
        d->y->set(d->maxY->value());
    }
}

void ScrollAreaWidget::preDrawChildren()
{
    GLState::push().setNormalizedScissor(normalizedRect());
}

void ScrollAreaWidget::postDrawChildren()
{
    GLState::pop();
}
