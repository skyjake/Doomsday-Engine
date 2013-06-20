/** @file popupwidget.cpp
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

#include "ui/widgets/popupwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/style.h"
#include "ui/clientwindow.h"

#include <de/Drawable>
#include <de/MouseEvent>
#include <de/ScalarRule>
#include <QTimer>

using namespace de;

static TimeDelta const OPENING_ANIM_SPAN = 0.5;
static TimeDelta const CLOSING_ANIM_SPAN = 0.5;

DENG2_PIMPL(PopupWidget)
{
    typedef DefaultVertexBuf VertexBuf;

    bool opened;
    Widget *realParent;
    GuiWidget *content;
    ScalarRule *openingRule;
    Rule const *anchorX;
    Rule const *anchorY;
    ui::Direction dir;
    QTimer dismissTimer;

    Rule const *marker;

    // GL objects.
    Drawable drawable;
    GLUniform uMvpMatrix;
    //GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          opened(false),
          realParent(0),
          content(0),
          anchorX(0),
          anchorY(0),
          dir(ui::Up),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {
        openingRule = new ScalarRule(0);

        dismissTimer.setSingleShot(true);
        dismissTimer.setInterval(CLOSING_ANIM_SPAN.asMilliSeconds());
        QObject::connect(&dismissTimer, SIGNAL(timeout()), thisPublic, SLOT(dismiss()));

        // Style.
        marker = &self.style().rules().rule("gap");
    }

    ~Instance()
    {
        releaseRef(openingRule);
        releaseRef(anchorX);
        releaseRef(anchorY);
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);
        self.root().shaders().build(drawable.program(), "generic.textured.color")
                << uMvpMatrix << self.root().uAtlas();
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void layoutAbove()
    {
        self.rule()
                .setInput(Rule::Bottom, *anchorY - *marker)
                .setInput(Rule::Left, OperatorRule::clamped(
                              *anchorX - self.rule().width() / 2,
                              self.margin(),
                              self.root().viewWidth() - self.rule().width() - self.margin()));
    }

    void layoutBelow()
    {
        self.rule()
                .setInput(Rule::Top,  *anchorY + *marker)
                .setInput(Rule::Left, *anchorX - self.rule().width() / 2);
    }

    void layoutLeft()
    {
        self.rule()
                .setInput(Rule::Right, *anchorX - *marker)
                .setInput(Rule::Top,   *anchorY - self.rule().height() / 2);
    }

    void layoutRight()
    {
        self.rule()
                .setInput(Rule::Left, *anchorX + *marker)
                .setInput(Rule::Top,  *anchorY - self.rule().height() / 2);
    }

    void updateLayout()
    {
        //Rectanglei const view = Rectanglei::fromSize(self.root().viewSize());
        //Vector2i const pos(anchorX->valuei(), anchorY->valuei());
        //Vector2i const size(self.rule().width().valuei(), self.rule().height().valuei());

        // Let's first try the requested direction.
        if(dir == ui::Up)
        {
            //if(pos.y - marker->valuei() >= size.y)
            {
                layoutAbove();
            }
            /*
            else
            {
                layoutBelow();
            }*/
            return;
        }

        if(dir == ui::Down)
        {
            //if(int(view.height()) - pos.y - marker->valuei() >= size.y)
            {
                layoutBelow();
            }
            /*else
            {
                layoutAbove();
            }*/
            return;
        }

        if(dir == ui::Left)
        {
            //if(pos.x - marker->valuei() >= size.x)
            {
                layoutLeft();
            }
            /*else
            {
                layoutRight();
            }*/
            return;
        }

        if(dir == ui::Right)
        {
            //if(int(view.width()) - pos.x - marker->valuei() >= size.x)
            {
                layoutRight();
            }
            /*else
            {
                layoutLeft();
            }*/
            return;
        }
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || self.geometryRequested())
        {
            self.requestGeometry(false);

            VertexBuf::Builder verts;
            self.glMakeGeometry(verts);
            drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
        }
    }

    void close(TimeDelta delay)
    {
        if(!opened) return;

        opened = false;

        // Begin the closing animation.
        openingRule->setStyle(Animation::EaseIn);
        openingRule->set(0, CLOSING_ANIM_SPAN, delay);

        emit self.closed();

        dismissTimer.start();
    }
};

PopupWidget::PopupWidget(String const &name) : d(new Instance(this))
{
    setBehavior(ChildHitClipping);

    // Initially the popup is hidden.
    hide();

    set(Background(style().colors().colorf("background")));
}

void PopupWidget::setContent(GuiWidget *content)
{
    if(d->content)
    {
        d->content->rule().clearInput(Rule::Left);
        d->content->rule().clearInput(Rule::Top);

        rule().clearInput(Rule::Width);
        rule().clearInput(Rule::Height);

        delete remove(*d->content);
    }

    d->content = content;
    add(content); // ownership taken

    // The given widget determines the content size of the popup.
    rule().setInput(Rule::Width,  content->rule().width())
          .setInput(Rule::Height, *d->openingRule);
    //d->openingRule->set(content->rule().height());

    content->rule()
            .setInput(Rule::Left, rule().left())
            .setInput(Rule::Top,  rule().top());
}

GuiWidget &PopupWidget::content() const
{
    DENG2_ASSERT(d->content != 0);
    return *d->content;
}

void PopupWidget::setAnchor(Vector2i const &pos)
{
    setAnchor(Const(pos.x), Const(pos.y));
}

void PopupWidget::setAnchor(Rule const &x, Rule const &y)
{
    releaseRef(d->anchorX);
    releaseRef(d->anchorY);

    d->anchorX = holdRef(x);
    d->anchorY = holdRef(y);
}

void PopupWidget::setOpeningDirection(ui::Direction dir)
{
    d->dir = dir;
}

void PopupWidget::viewResized()
{
    GuiWidget::viewResized();

    d->uMvpMatrix = root().projMatrix2D();

    update();
}

void PopupWidget::update()
{
    GuiWidget::update();

    if(!isHidden())
    {
        d->updateGeometry();
    }
}

void PopupWidget::preDrawChildren()
{
    GLState::push().setNormalizedScissor(normalizedRect());
}

void PopupWidget::postDrawChildren()
{
    GLState::pop();
}

bool PopupWidget::handleEvent(Event const &event)
{
    if(!d->opened) return false;

    // Popups eat all mouse button events.
    if(event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();

        // Clicking outside the popup will close it.
        if(!hitTest(event) && mouse.state() == MouseEvent::Released)
        {
            d->close(0); // immediately
        }
        return true;
    }

    if(event.type() == Event::KeyPress ||
       event.type() == Event::KeyRepeat ||
       event.type() == Event::KeyRelease)
    {
        return true;
    }

    return GuiWidget::handleEvent(event);
}

void PopupWidget::open()
{
    if(d->opened) return;

    d->dismissTimer.stop();

    // Reparent the popup into the root widget, on top of everything else.
    d->realParent = Widget::parent();
    d->realParent->remove(*this);
    d->realParent->root().add(this);

    show();

    preparePopupForOpening();

    d->openingRule->setStyle(Animation::Bounce, 6);
    d->openingRule->set(d->content->rule().height(), OPENING_ANIM_SPAN);

    d->opened = true;

    emit opened();
}

void PopupWidget::close()
{
    d->close(0.2);
}

void PopupWidget::dismiss()
{
    if(isHidden()) return;

    hide();
    d->opened = false;
    d->dismissTimer.stop();

    // Move back to the original parent widget.
    root().remove(*this);
    d->realParent->add(this);
}

void PopupWidget::drawContent()
{
    d->drawable.draw();
}

void PopupWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    GuiWidget::glMakeGeometry(verts);

    // Anchor triangle.
    DefaultVertexBuf::Builder tri;
    DefaultVertexBuf::Type v;

    v.rgba = background().solidFill;
    v.texCoord = root().atlas().imageRectf(root().solidWhitePixel()).middle();

    int marker = d->marker->valuei();

    Vector2i const anchorPos(d->anchorX->valuei(), d->anchorY->valuei());
    v.pos = anchorPos; tri << v;
    v.pos = anchorPos + Vector2i(-marker, -marker); tri << v;
    v.pos = anchorPos + Vector2i(marker, -marker); tri << v;

    verts += tri;
}

void PopupWidget::glInit()
{
    d->glInit();
}

void PopupWidget::glDeinit()
{
    d->glDeinit();
}

void PopupWidget::preparePopupForOpening()
{
    d->updateLayout();
}
