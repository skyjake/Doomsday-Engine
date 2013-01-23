/** @file rootwidget.cpp Widget for managing the root of the UI.
 * @ingroup widget
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RootWidget"
#include "de/ConstantRule"
#include "de/math.h"

namespace de {

struct RootWidget::Instance
{
    ConstantRule *viewWidth;
    ConstantRule *viewHeight;

    Instance()
    {
        viewWidth  = new ConstantRule(0);
        viewHeight = new ConstantRule(0);
    }

    ~Instance()
    {
        de::releaseRef(viewWidth);
        de::releaseRef(viewHeight);
    }

    Vector2i viewSize() const
    {
        return Vector2i(de::floor(viewWidth->value()), de::floor(viewHeight->value()));
    }
};

RootWidget::RootWidget() : Widget(), d(new Instance)
{}

RootWidget::~RootWidget()
{
    delete d;
}

Vector2i RootWidget::viewSize() const
{
    return d->viewSize();
}

void RootWidget::setViewSize(Vector2i const &size)
{
    d->viewWidth->set(size.x);
    d->viewHeight->set(size.y);

    notifyTree(&Widget::viewResized);
}

Rule const *RootWidget::viewWidth() const
{
    return d->viewWidth;
}

Rule const *RootWidget::viewHeight() const
{
    return d->viewHeight;
}

void RootWidget::initialize()
{
    notifyTree(&Widget::initialize);
}

void RootWidget::draw()
{
    notifyTree(&Widget::draw);
}

} // namespace de
