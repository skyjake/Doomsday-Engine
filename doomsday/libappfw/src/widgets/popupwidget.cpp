/** @file popupwidget.cpp
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

#include "de/PopupWidget"
#include "de/ButtonWidget"
#include "de/GuiRootWidget"
#include "de/Style"
#include "de/BaseGuiApp"

#include <de/Drawable>
#include <de/MouseEvent>
#include <de/ScalarRule>
#include <de/Garbage>
#include <de/math.h>
#include <QTimer>

namespace de {

static TimeDelta const OPENING_ANIM_SPAN = 0.4;
static TimeDelta const CLOSING_ANIM_SPAN = 0.3;

DENG_GUI_PIMPL(PopupWidget)
, DENG2_OBSERVES(Widget, Deletion)
{
    bool useInfoStyle;
    bool deleteAfterDismiss;
    bool clickToClose;
    bool outsideClickOngoing;
    Widget *realParent;
    Rule const *anchorX;
    Rule const *anchorY;
    Rule const *marker;

    Instance(Public *i)
        : Base(i)
        , useInfoStyle(false)
        , deleteAfterDismiss(false)
        , clickToClose(true)
        , outsideClickOngoing(false)
        , realParent(0)
        , anchorX(0)
        , anchorY(0)
    {
        // Style.
        marker = &style().rules().rule("gap");
    }

    ~Instance()
    {
        if(realParent) realParent->audienceForDeletion() -= this;

        releaseRef(anchorX);
        releaseRef(anchorY);
    }

    void updateLayout()
    {
        self.rule()
                .clearInput(Rule::AnchorX)
                .clearInput(Rule::AnchorY);

        switch(self.openingDirection())
        {
        case ui::Up:
            self.rule()
                    .setInput(Rule::Bottom, OperatorRule::maximum(
                                  *anchorY - *marker, self.rule().height()))
                    .setInput(Rule::Left, OperatorRule::clamped(
                                  *anchorX - self.rule().width() / 2,
                                  self.margins().left(),
                                  self.root().viewWidth() - self.rule().width() - self.margins().right()));
            break;

        case ui::Down:
            self.rule()
                    .setInput(Rule::Top,  *anchorY + *marker)
                    .setInput(Rule::Left, OperatorRule::clamped(
                                  *anchorX - self.rule().width() / 2,
                                  self.margins().left(),
                                  self.root().viewWidth() - self.rule().width() - self.margins().right()));
            break;

        case ui::Left:
            self.rule()
                    .setInput(Rule::Right, OperatorRule::maximum(
                                  *anchorX - *marker, self.rule().width()))
                    .setInput(Rule::Top, OperatorRule::clamped(
                                  *anchorY - self.rule().height() / 2,
                                  self.margins().top(),
                                  self.root().viewHeight() - self.rule().height() - self.margins().bottom()));
            break;

        case ui::Right:
            self.rule()
                    .setInput(Rule::Left, OperatorRule::minimum(
                                  *anchorX + *marker,
                                  self.root().viewWidth() - self.rule().width() - self.margins().right()))
                    .setInput(Rule::Top,  OperatorRule::clamped(
                                  *anchorY - self.rule().height() / 2,
                                  self.margins().top(),
                                  self.root().viewHeight() - self.rule().height() - self.margins().bottom()));
            break;

        case ui::NoDirection:
            self.rule()
                    .setInput(Rule::AnchorX, *anchorX)
                    .setInput(Rule::AnchorY, *anchorY)
                    .setAnchorPoint(Vector2f(.5f, .5f));
            break;
        }
    }

    void updateStyle()
    {
        Style const &st = style();

        if(useInfoStyle)
        {
            self.set(self.infoStyleBackground());
        }
        else
        {
            self.set(Background(st.colors().colorf("background"),
                                Background::BorderGlow,
                                st.colors().colorf("glow"),
                                st.rules().rule("glow").valuei()));
        }

        if(self.levelOfNesting() > 0)
        {
            // If nested, use an opaque background.
            self.set(self.background().withSolidFillOpacity(1));
        }
    }

    void widgetBeingDeleted(Widget &widget)
    {
        if(&widget == realParent)
        {
            // We don't know who the real parent is any more.
            realParent = 0;
        }
    }
};

PopupWidget::PopupWidget(String const &name) : PanelWidget(name), d(new Instance(this))
{
    setOpeningDirection(ui::Up);
    d->updateStyle();
}

int PopupWidget::levelOfNesting() const
{
    int nesting = 0;
    for(Widget const *p = d->realParent? d->realParent : parentWidget(); p; p = p->parent())
    {
        if(p->is<PopupWidget>())
        {
            ++nesting;
        }
    }
    return nesting;
}

void PopupWidget::setAnchorAndOpeningDirection(RuleRectangle const &rule, ui::Direction dir)
{
    if(dir == ui::NoDirection)
    {
        // Anchored to the middle by default.
        setAnchor(rule.left() + rule.width() / 2,
                  rule.top() + rule.height() / 2);
    }
    else if(dir == ui::Left || dir == ui::Right)
    {
        setAnchorY(rule.top() + rule.height() / 2);
        setAnchorX(dir == ui::Left? rule.left() : rule.right());
    }
    else if(dir == ui::Up || dir == ui::Down)
    {
        setAnchorX(rule.left() + rule.width() / 2);
        setAnchorY(dir == ui::Up? rule.top() : rule.bottom());
    }

    setOpeningDirection(dir);
}

void PopupWidget::setAnchor(Vector2i const &pos)
{
    setAnchor(Const(pos.x), Const(pos.y));
}

void PopupWidget::setAnchorX(int xPos)
{
    setAnchorX(Const(xPos));
}

void PopupWidget::setAnchorY(int yPos)
{
    setAnchorY(Const(yPos));
}

void PopupWidget::setAnchor(Rule const &x, Rule const &y)
{
    setAnchorX(x);
    setAnchorY(y);
}

void PopupWidget::setAnchorX(Rule const &x)
{
    releaseRef(d->anchorX);
    d->anchorX = holdRef(x);
}

void PopupWidget::setAnchorY(Rule const &y)
{
    releaseRef(d->anchorY);
    d->anchorY = holdRef(y);
}

Rule const &PopupWidget::anchorX() const
{
    return *d->anchorX;
}

Rule const &PopupWidget::anchorY() const
{
    return *d->anchorY;
}

void PopupWidget::detachAnchor()
{
    setAnchorX(Constf(anchorX().value()));
    setAnchorY(Constf(anchorY().value()));
    d->updateLayout();
}

void PopupWidget::setDeleteAfterDismissed(bool deleteAfterDismiss)
{
    d->deleteAfterDismiss = deleteAfterDismiss;
}

void PopupWidget::setClickToClose(bool clickCloses)
{
    d->clickToClose = clickCloses;
}

void PopupWidget::useInfoStyle()
{
    d->useInfoStyle = true;
    d->updateStyle();
}

bool PopupWidget::isUsingInfoStyle()
{
    return d->useInfoStyle;
}

GuiWidget::Background PopupWidget::infoStyleBackground() const
{
    return Background(style().colors().colorf("popup.info.background"),
                      Background::BorderGlow,
                      style().colors().colorf("popup.info.glow"),
                      style().rules().rule("glow").valuei());
}

bool PopupWidget::handleEvent(Event const &event)
{
    if(!isOpen()) return false;

    // Popups eat all mouse button events.
    if(event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        bool const inside = hitTest(event);

        // Clicking outside the popup will close it.
        if(d->clickToClose)
        {
            switch(mouse.state())
            {
            case MouseEvent::Pressed:
                if(!inside)
                {
                    d->outsideClickOngoing = true;
                }
                break;

            case MouseEvent::Released:
                if(!inside && d->outsideClickOngoing)
                {
                    /* This needs to satisfy the following conditions:
                     *
                     * - Since a mouse click comprises a Press followed by a Release, we shouldn't
                     *   let the potential clicked widget that lies outside the popup know of the
                     *   click before it has successfully completed.
                     *
                     * - Buttons perform their action when clicked, so the clicked widget must
                     *   be notified before the popup is closed. This way buttons that open
                     *   popup menus can either be used to toggle the menu open/closed, but also
                     *   immediately open their menu if it currently is closed.
                     */
                    if(GuiWidget const *hit = root().globalHitTest(mouse.pos()))
                    {
                        DENG2_ASSERT(hit != this); // !inside

                        if(hit->isEnabled() && hit->isVisible())
                        {
                            // Replay the click on this widget.
                            const_cast<GuiWidget *>(hit)->handleEvent(
                                    MouseEvent(mouse.button(), MouseEvent::Pressed, mouse.pos()));
                            const_cast<GuiWidget *>(hit)->handleEvent(mouse);
                        }
                    }

                    close(0); // immediately
                }
                d->outsideClickOngoing = false;
                break;
            }
        }
        return true;
    }    

    if(event.type() == Event::KeyPress  ||
       event.type() == Event::KeyRepeat ||
       event.type() == Event::KeyRelease)
    {
        if(event.isKeyDown() && event.as<KeyEvent>().ddKey() == DDKEY_ESCAPE)
        {
            close();
            return true;
        }

        // Popups should still allow global key bindings to be activated.
        root().handleEventAsFallback(event);

        // Don't pass it further, though.
        return true;
    }

    return PanelWidget::handleEvent(event);
}

void PopupWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    PanelWidget::glMakeGeometry(verts);

    ui::Direction const dir = openingDirection();
    if(dir == ui::NoDirection) return;

    // Anchor triangle.
    DefaultVertexBuf::Builder tri;
    DefaultVertexBuf::Type v;

    v.rgba = background().solidFill;
    v.texCoord = root().atlas().imageRectf(root().solidWhitePixel()).middle();

    int marker = d->marker->valuei();

    Vector2i anchorPos(d->anchorX->valuei(), d->anchorY->valuei());

    if(dir == ui::Up)
    {
        // Can't put the anchor too close to the edges.
        anchorPos.x = clamp(2 * marker, anchorPos.x, int(root().viewSize().x) - 2*marker);

        v.pos = anchorPos; tri << v;
        v.pos = anchorPos + Vector2i(-marker, -marker); tri << v;
        v.pos = anchorPos + Vector2i(marker, -marker); tri << v;
    }
    else if(dir == ui::Left)
    {
        // The anchor may still get clamped out of sight.
        if(anchorPos.x > rule().right().valuei())
        {
            v.pos = anchorPos; tri << v;
            v.pos = anchorPos + Vector2i(-marker, marker); tri << v;
            v.pos = anchorPos + Vector2i(-marker, -marker); tri << v;
        }
    }
    else if(dir == ui::Right)
    {
        v.pos = anchorPos; tri << v;
        v.pos = anchorPos + Vector2i(marker, -marker); tri << v;
        v.pos = anchorPos + Vector2i(marker, marker); tri << v;
    }
    else
    {
        v.pos = anchorPos; tri << v;
        v.pos = anchorPos + Vector2i(marker, marker); tri << v;
        v.pos = anchorPos + Vector2i(-marker, marker); tri << v;
    }

    verts += tri;
}

void PopupWidget::updateStyle()
{
    PanelWidget::updateStyle();

    d->updateStyle();
}

void PopupWidget::preparePanelForOpening()
{
    d->updateStyle();

    PanelWidget::preparePanelForOpening();

    // Reparent the popup into the root widget, on top of everything else.
    d->realParent = Widget::parent();
    DENG2_ASSERT(d->realParent != 0);
    d->realParent->audienceForDeletion() += d;
    d->realParent->remove(*this);
    d->realParent->root().as<GuiRootWidget>().addOnTop(this);

    d->updateLayout();
}

void PopupWidget::panelDismissed()
{
    PanelWidget::panelDismissed();

    // Move back to the original parent widget.
    if(d->realParent)
    {
        d->realParent->audienceForDeletion() -= d;
    }
    else
    {
        // The real parent has been deleted.
        d->realParent = &root();
    }
    parentWidget()->remove(*this);

    if(d->deleteAfterDismiss)
    {
        // Don't bother putting it back in the original parent.
        guiDeleteLater();
    }
    else
    {
        d->realParent->add(this);
    }

    d->realParent = 0;
}

} // namespace de
