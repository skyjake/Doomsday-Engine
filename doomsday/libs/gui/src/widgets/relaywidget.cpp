/** @file relaywidget.cpp  Relays drawing and events.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/relaywidget.h"

namespace de {

DE_PIMPL(RelayWidget)
, DE_OBSERVES(Widget, Deletion)
{
    GuiWidget *target = nullptr;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        setTarget(nullptr);
    }

    void setTarget(GuiWidget *w)
    {
        if (target) target->audienceForDeletion() -= this;
        target = w;
        if (target) target->audienceForDeletion() += this;
    }

    void widgetBeingDeleted(Widget &w)
    {
        if (target == &w)
        {
            DE_NOTIFY_PUBLIC(Target, i)
            {
                i->relayTargetBeingDeleted(self());
            }
            target = nullptr;
        }
    }

    DE_PIMPL_AUDIENCE(Target)
};

DE_AUDIENCE_METHOD(RelayWidget, Target)

RelayWidget::RelayWidget(GuiWidget *target, const String &name)
    : GuiWidget(name), d(new Impl(this))
{
    d->setTarget(target);
}

void RelayWidget::setTarget(GuiWidget *target)
{
    d->setTarget(target);
}

GuiWidget *RelayWidget::target() const
{
    return d->target;
}

void RelayWidget::initialize()
{
    GuiWidget::initialize();
    if (d->target)
    {
        d->target->setRoot(&root());
        d->target->notifySelfAndTree(&Widget::initialize);
    }
}

void RelayWidget::deinitialize()
{
    GuiWidget::deinitialize();
    if (d->target)
    {
        d->target->notifySelfAndTree(&Widget::deinitialize);
    }
}

void RelayWidget::viewResized()
{
    GuiWidget::viewResized();
    if (d->target)
    {
        d->target->notifySelfAndTree(&Widget::viewResized);
    }
}

void RelayWidget::update()
{
    GuiWidget::update();
    if (d->target)
    {
        d->target->setRoot(&root());
        d->target->notifySelfAndTree(&Widget::update);
    }
}

bool RelayWidget::handleEvent(const Event &event)
{
    if (d->target)
    {
        return d->target->dispatchEvent(event, &Widget::handleEvent);
    }
    return GuiWidget::handleEvent(event);
}

bool RelayWidget::hitTest(const Vec2i &pos) const
{
    if (d->target)
    {
        return d->target->hitTest(pos);
    }
    return false;
}

void RelayWidget::drawContent()
{
    if (d->target)
    {
        NotifyArgs args(&Widget::draw);
        args.conditionFunc  = &Widget::isVisible;
        args.preNotifyFunc  = &Widget::preDrawChildren;
        args.postNotifyFunc = &Widget::postDrawChildren;
        d->target->notifySelfAndTree(args);
    }
}

} // namespace de
