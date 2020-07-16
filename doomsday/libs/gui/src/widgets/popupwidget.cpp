/** @file popupwidget.cpp
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

#include "de/popupwidget.h"
#include "de/buttonwidget.h"
#include "de/guirootwidget.h"
#include "de/ui/style.h"
#include "de/baseguiapp.h"

#include <de/drawable.h>
#include <de/mouseevent.h>
#include <de/animationrule.h>
#include <de/garbage.h>
#include <de/math.h>

namespace de {

DE_GUI_PIMPL(PopupWidget)
{
    ColorTheme colorTheme          = Normal;
    bool       flexibleDir         = true;
    bool       deleteAfterDismiss  = false;
    bool       clickToClose        = true;
    bool       outsideClickOngoing = false;

    DotPath               outlineColorId;
    ColorBank::Colorf     outlineColor;
    SafeWidgetPtr<Widget> realParent;
    RuleRectangle         anchor;
    const Rule *          marker;
    ButtonWidget *        close = nullptr;

    Impl(Public *i) : Base(i)
    {
        // Style.
        marker = &rule("gap");
    }

    void flipOpeningDirectionIfNeeded()
    {
        ui::Direction openDir = self().openingDirection();

        // Opening direction depends on the anchor position: popup will open to
        // direction that has more space available.
        switch (openDir)
        {
        case ui::Up:
            if (anchor.midY().value() < self().root().viewHeight().value()/2)
            {
                openDir = ui::Down;
            }
            break;

        case ui::Down:
            if (anchor.midY().value() > self().root().viewHeight().value()/2)
            {
                openDir = ui::Up;
            }
            break;

        case ui::Left:
            if (anchor.midX().value() < self().root().viewWidth().value()/2)
            {
                openDir = ui::Right;
            }
            break;

        case ui::Right:
            if (anchor.midX().value() > self().root().viewWidth().value()/2)
            {
                openDir = ui::Left;
            }
            break;

        default:
            break;
        }

        self().setOpeningDirection(openDir);
    }

    using Vector2R = Vector2<const Rule *>;

    Vector2R anchorRule() const
    {
        switch (self().openingDirection())
        {
        case ui::Up:
            return Vector2R(&anchor.midX(), &anchor.top());

        case ui::Down:
            return Vector2R(&anchor.midX(), &anchor.bottom());

        case ui::Left:
            return Vector2R(&anchor.left(), &anchor.midY());

        case ui::Right:
            return Vector2R(&anchor.right(), &anchor.midY());

        default:
            break;
        }

        return Vector2R(&anchor.midX(), &anchor.midY());
    }

    Vec2i anchorPos() const
    {
        auto rule = anchorRule();
        return Vec2i(rule.x->valuei(), rule.y->valuei());
    }

    void updateLayout()
    {
        self().rule()
                .clearInput(Rule::Left)
                .clearInput(Rule::Right)
                .clearInput(Rule::Top)
                .clearInput(Rule::Bottom)
                .clearInput(Rule::AnchorX)
                .clearInput(Rule::AnchorY);

        auto anchorPos = anchorRule();

        switch (self().openingDirection())
        {
        case ui::Up:
            self().rule()
                    .setInput(Rule::Bottom, OperatorRule::maximum(
                                  *anchorPos.y - *marker,
                                  self().rule().height()))
                    .setInput(Rule::Left, OperatorRule::clamped(
                                  *anchorPos.x - self().rule().width() / 2,
                                  self().margins().left(),
                                  self().root().viewWidth() - self().rule().width() - self().margins().right()));
            break;

        case ui::Down:
            self().rule()
                    .setInput(Rule::Top,  OperatorRule::minimum(
                                  *anchorPos.y + *marker,
                                  self().root().viewHeight() - self().rule().height() - self().margins().bottom()))
                    .setInput(Rule::Left, OperatorRule::clamped(
                                  *anchorPos.x - self().rule().width() / 2,
                                  self().margins().left(),
                                  self().root().viewWidth() - self().rule().width() - self().margins().right()));
            break;

        case ui::Left:
            self().rule()
                    .setInput(Rule::Right, OperatorRule::maximum(
                                  *anchorPos.x - *marker,
                                  self().rule().width()))
                    .setInput(Rule::Top, OperatorRule::clamped(
                                  *anchorPos.y - self().rule().height() / 2,
                                  self().margins().top(),
                                  self().root().viewHeight() - self().rule().height() -
                                  self().margins().bottom() + self().margins().top()));
            break;

        case ui::Right:
            self().rule()
                    .setInput(Rule::Left, OperatorRule::minimum(
                                  *anchorPos.x + *marker,
                                  self().root().viewWidth() - self().rule().width() - self().margins().right()))
                    .setInput(Rule::Top,  OperatorRule::clamped(
                                  *anchorPos.y - self().rule().height() / 2,
                                  self().margins().top(),
                                  self().root().viewHeight() - self().rule().height() - self().margins().bottom()));
            break;

        case ui::NoDirection:
            self().rule().setMidAnchorX(*anchorPos.x)
                       .setMidAnchorY(*anchorPos.y);
            break;
        }
    }

    void updateStyle()
    {
        const Style &st = style();
        const bool opaqueBackground = (self().levelOfNesting() > 0);

        outlineColor = st.colors().colorf(outlineColorId);

        if (colorTheme == Inverted)
        {
            self().set(self().infoStyleBackground());
        }
        else
        {
            Background bg(st.colors().colorf("background"),
                          !opaqueBackground && st.isBlurringAllowed()
                              ? (style().sharedBlurWidget() ? Background::SharedBlurWithBorderGlow
                                                            : Background::BlurredWithBorderGlow)
                              : Background::BorderGlow,
                          st.colors().colorf("glow"),
                          st.rules().rule("glow").valuei());
            bg.blur = style().sharedBlurWidget();
            self().set(bg);
        }

        if (opaqueBackground)
        {
            // If nested, use an opaque background.
            self().set(self().background().withSolidFillOpacity(1));
        }
    }
};

PopupWidget::PopupWidget(const String &name) : PanelWidget(name), d(new Impl(this))
{
    setOpeningDirection(ui::Up);
    d->updateStyle();
}

int PopupWidget::levelOfNesting() const
{
    int nesting = 0;
    // GuiRootWidget is not a GuiWidget; root widget never has a parent.
    for (const GuiWidget *p = d->realParent && d->realParent->parent()?
                static_cast<const GuiWidget *>(d->realParent.get()) : parentGuiWidget();
         p; p = p->parentGuiWidget())
    {
        if (is<PopupWidget>(p))
        {
            ++nesting;
        }
    }
    return nesting;
}

void PopupWidget::setAnchorAndOpeningDirection(const RuleRectangle &rule, ui::Direction dir)
{
    d->anchor.setRect(rule);
    setOpeningDirection(dir);
}

void PopupWidget::setAllowDirectionFlip(bool flex)
{
    d->flexibleDir = flex;
}

void PopupWidget::setAnchor(const Vec2i &pos)
{
    d->anchor.setLeftTop(Const(pos.x), Const(pos.y));
    d->anchor.setRightBottom(d->anchor.left(), d->anchor.top());
}

void PopupWidget::setAnchorX(int xPos)
{
    d->anchor.setInput(Rule::Left,  Const(xPos))
             .setInput(Rule::Right, Const(xPos));
}

void PopupWidget::setAnchorY(int yPos)
{
    d->anchor.setInput(Rule::Top,    Const(yPos))
             .setInput(Rule::Bottom, Const(yPos));
}

void PopupWidget::setAnchor(const Rule &x, const Rule &y)
{
    setAnchorX(x);
    setAnchorY(y);
}

void PopupWidget::setAnchorX(const Rule &x)
{
    d->anchor.setInput(Rule::Left,   x)
             .setInput(Rule::Right,  x);
}

void PopupWidget::setAnchorY(const Rule &y)
{
    d->anchor.setInput(Rule::Top,    y)
             .setInput(Rule::Bottom, y);
}

const RuleRectangle &PopupWidget::anchor() const
{
    return d->anchor;
}

void PopupWidget::detachAnchor()
{
    setAnchor(d->anchorPos());
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

void PopupWidget::useInfoStyle(bool yes)
{
    setColorTheme(yes? Inverted : Normal);
}

bool PopupWidget::isUsingInfoStyle()
{
    return d->colorTheme == Inverted;
}

void PopupWidget::setColorTheme(ColorTheme theme)
{
    d->colorTheme = theme;
    if (d->close) d->close->setColorTheme(theme);
    d->updateStyle();
}

GuiWidget::ColorTheme PopupWidget::colorTheme() const
{
    return d->colorTheme;
}

void PopupWidget::setOutlineColor(const DotPath &outlineColor)
{
    d->outlineColorId = outlineColor;
    d->updateStyle();
}

void PopupWidget::setCloseButtonVisible(bool enable)
{
    if (enable && !d->close)
    {
        d->close = new ButtonWidget;
        d->close->setColorTheme(d->colorTheme);
        d->close->setStyleImage("close.ringless", "small");
        d->close->margins().set("dialog.gap").setTopBottom(RuleBank::UNIT);
        d->close->setImageColor(d->close->textColorf());
        d->close->setSizePolicy(ui::Expand, ui::Expand);
        d->close->setActionFn([this] () { close(); });
        d->close->rule()
                .setInput(Rule::Top,   rule().top()   + margins().top())
                .setInput(Rule::Right, rule().right() - margins().right());
        add(d->close);
    }
    else if (!enable && d->close)
    {
        delete d->close;
        d->close = nullptr;
    }
}

ButtonWidget &PopupWidget::closeButton()
{
    setCloseButtonVisible(true);
    return *d->close;
}

void PopupWidget::offerFocus()
{
    if (d->close)
    {
        root().setFocus(d->close);
    }
}

GuiWidget::Background PopupWidget::infoStyleBackground() const
{
    return Background(style().colors().colorf("popup.info.background"),
                      Background::BorderGlow,
                      style().colors().colorf("popup.info.glow"),
                      rule("glow").valuei());
}

bool PopupWidget::handleEvent(const Event &event)
{
    if (!isOpen()) return false;

    // Popups eat all mouse button events.
    if (event.type() == Event::MouseButton)
    {
        //const MouseEvent &mouse = event.as<MouseEvent>();
        const bool inside = hitTest(event);

        if (!inside && d->clickToClose)
        {
            close(0.1);
        }
    }

    if (event.type() == Event::KeyPress  ||
        event.type() == Event::KeyRepeat ||
        event.type() == Event::KeyRelease)
    {
        const KeyEvent &key = event.as<KeyEvent>();
        if (event.isKeyDown() &&
            (key.ddKey() == DDKEY_ESCAPE ||
             key.ddKey() == DDKEY_ENTER  ||
             key.ddKey() == DDKEY_RETURN ||
             key.ddKey() == ' '))
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

void PopupWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    if (rule().recti().isNull()) return; // Still closed.

    PanelWidget::glMakeGeometry(verts);

    const ui::Direction dir = openingDirection();
    if (dir == ui::NoDirection) return;

    // Anchor triangle.
    GuiVertexBuilder tri;
    GuiVertex v;

    v.rgba = background().solidFill;
    v.texCoord = root().atlas().imageRectf(root().solidWhitePixel()).middle();

    const int marker = d->marker->valuei();
    Vec2i anchorPos = d->anchorPos();
    bool markerVisible = false;

    if (dir == ui::Up)
    {
        // Can't put the anchor too close to the edges.
        anchorPos.x = clamp(2 * marker, anchorPos.x, int(root().viewSize().x) - 2*marker);

        if (anchorPos.y > rule().bottom().valuei())
        {
            v.pos = anchorPos; tri << v;
            v.pos = anchorPos + Vec2i(-marker, -marker); tri << v;
            v.pos = anchorPos + Vec2i(marker, -marker); tri << v;
            markerVisible = true;
        }
    }
    else if (dir == ui::Left)
    {
        // The anchor may still get clamped out of sight.
        if (anchorPos.x > rule().right().valuei())
        {
            v.pos = anchorPos; tri << v;
            v.pos = anchorPos + Vec2i(-marker, marker); tri << v;
            v.pos = anchorPos + Vec2i(-marker, -marker); tri << v;
            markerVisible = true;
        }
    }
    else if (dir == ui::Right)
    {
        if (anchorPos.x < rule().left().valuei())
        {
            v.pos = anchorPos; tri << v;
            v.pos = anchorPos + Vec2i(marker, -marker); tri << v;
            v.pos = anchorPos + Vec2i(marker, marker); tri << v;
            markerVisible = true;
        }
    }
    else
    {
        if (anchorPos.y < rule().top().valuei())
        {
            v.pos = anchorPos; tri << v;
            v.pos = anchorPos + Vec2i(marker, marker); tri << v;
            v.pos = anchorPos + Vec2i(-marker, marker); tri << v;
            markerVisible = true;
        }
    }

    // Outline.
    if (d->outlineColor.w > 0.f)
    {
        tri << v; // discontinued

        const Rectanglei rect = rule().recti();
        const int ow = GuiWidget::pointsToPixels(2);
        const int halfOw = ow/2;
        const int midOw = ow + halfOw;

        v.rgba = d->outlineColor;

        // Top edge.
        v.pos = rect.topLeft + Vec2i(-ow, -ow); tri << v << v;
        v.pos = rect.topLeft; tri << v;

        if (markerVisible && dir == ui::Down)
        {
            v.pos = Vec2i(anchorPos.x - marker - halfOw, rect.top() - ow); tri << v;
            v.pos += Vec2i(halfOw, ow); tri << v;

            v.pos = anchorPos + Vec2i(0, -midOw); tri << v;
            v.pos.y += midOw; tri << v;

            v.pos = Vec2i(anchorPos.x + marker + halfOw, rect.top() - ow); tri << v;
            v.pos += Vec2i(-halfOw, ow); tri << v;
        }

        // Right edge.
        v.pos = rect.topRight() + Vec2i(ow, -ow); tri << v;
        v.pos = rect.topRight(); tri << v;

        if (markerVisible && dir == ui::Left)
        {
            v.pos = Vec2i(rect.right() + ow, anchorPos.y - marker - halfOw); tri << v;
            v.pos += Vec2i(-ow, halfOw); tri << v;

            v.pos = anchorPos + Vec2i(midOw, 0); tri << v;
            v.pos.x += -midOw; tri << v;

            v.pos = Vec2i(rect.right() + ow, anchorPos.y + marker + halfOw); tri << v;
            v.pos += Vec2i(-ow, -halfOw); tri << v;
        }

        // Bottom edge.
        v.pos = rect.bottomRight + Vec2i(ow, ow); tri << v;
        v.pos = rect.bottomRight; tri << v;

        if (markerVisible && dir == ui::Up)
        {
            v.pos = Vec2i(anchorPos.x + marker + halfOw, rect.bottom() + ow); tri << v;
            v.pos += Vec2i(-halfOw, -ow); tri << v;

            v.pos = anchorPos + Vec2i(0, midOw); tri << v;
            v.pos.y += -midOw; tri << v;

            v.pos = Vec2i(anchorPos.x - marker - halfOw, rect.bottom() + ow); tri << v;
            v.pos += Vec2i(halfOw, -ow); tri << v;
        }

        // Left edge.
        v.pos = rect.bottomLeft() + Vec2i(-ow, ow); tri << v;
        v.pos = rect.bottomLeft(); tri << v;

        if (markerVisible && dir == ui::Right)
        {
            v.pos = Vec2i(rect.left() - ow, anchorPos.y + marker + halfOw); tri << v;
            v.pos += Vec2i(ow, -halfOw); tri << v;

            v.pos = anchorPos + Vec2i(-midOw, 0); tri << v;
            v.pos.x += midOw; tri << v;

            v.pos = Vec2i(rect.left() - ow, anchorPos.y - marker - halfOw); tri << v;
            v.pos += Vec2i(ow, halfOw); tri << v;
        }

        // Back to top.
        v.pos = rect.topLeft + Vec2i(-ow, -ow); tri << v;
        v.pos = rect.topLeft; tri << v;
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

    if (d->flexibleDir)
    {
        d->flipOpeningDirectionIfNeeded();
    }

    // Reparent the popup into the root widget, on top of everything else.
    d->realParent.reset(Widget::parent());
    DE_ASSERT(d->realParent);
    d->realParent->remove(*this);
    d->realParent->root().as<GuiRootWidget>().addOnTop(this);

    d->updateLayout();

    root().pushFocus();
    offerFocus();
}

void PopupWidget::panelClosing()
{
    PanelWidget::panelClosing();

    root().popFocus();
}

void PopupWidget::panelDismissed()
{
    PanelWidget::panelDismissed();

    // Move back to the original parent widget.
    if (!d->realParent)
    {
        // The real parent has been deleted.
        d->realParent.reset(&root());
        DE_ASSERT(d->realParent);
    }
    parentWidget()->remove(*this);

    if (d->deleteAfterDismiss)
    {
        // Don't bother putting it back in the original parent.
        guiDeleteLater();
    }
    else
    {
        d->realParent->add(this);
    }

    d->realParent.reset();
}

} // namespace de
