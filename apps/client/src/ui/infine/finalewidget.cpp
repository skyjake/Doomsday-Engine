/** @file finalewidget.cpp  InFine animation system, FinaleWidget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "ui/infine/finalewidget.h"
#include "ui/infine/finalepagewidget.h"

using namespace de;

DE_PIMPL_NOREF(FinaleWidget)
{
    Id id;
    String name;
    animatorvector3_t pos;
    animator_t angle;
    animatorvector3_t scale;
    FinalePageWidget *page = nullptr;

    Impl()
    {
        AnimatorVector3_Init(pos, 0, 0, 0);
        Animator_Init(&angle, 0);
        AnimatorVector3_Init(scale, 1, 1, 1);
    }
};

FinaleWidget::FinaleWidget(const de::String &name) : d(new Impl)
{
    setName(name);
}

FinaleWidget::~FinaleWidget()
{
    DE_NOTIFY_VAR(Deletion, i) i->finaleWidgetBeingDeleted(*this);
}

Id FinaleWidget::id() const
{
    return d->id;
}

String FinaleWidget::name() const
{
    return d->name;
}

FinaleWidget &FinaleWidget::setName(const String &newName)
{
    d->name = newName;
    return *this;
}

const animatorvector3_t &FinaleWidget::origin() const
{
    return d->pos;
}

FinaleWidget &FinaleWidget::setOrigin(const Vec3f &newPos, int steps)
{
    AnimatorVector3_Set(d->pos, newPos.x, newPos.y, newPos.z, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setOriginX(float newX, int steps)
{
    Animator_Set(&d->pos[0], newX, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setOriginY(float newY, int steps)
{
    Animator_Set(&d->pos[1], newY, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setOriginZ(float newZ, int steps)
{
    Animator_Set(&d->pos[2], newZ, steps);
    return *this;
}

const animator_t &FinaleWidget::angle() const
{
    return d->angle;
}

FinaleWidget &FinaleWidget::setAngle(float newAngle, int steps)
{
    Animator_Set(&d->angle, newAngle, steps);
    return *this;
}

const animatorvector3_t &FinaleWidget::scale() const
{
    return d->scale;
}

FinaleWidget &FinaleWidget::setScale(const Vec3f &newScale, int steps)
{
    AnimatorVector3_Set(d->scale, newScale.x, newScale.y, newScale.z, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setScaleX(float newXScale, int steps)
{
    Animator_Set(&d->scale[0], newXScale, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setScaleY(float newYScale, int steps)
{
    Animator_Set(&d->scale[1], newYScale, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setScaleZ(float newZScale, int steps)
{
    Animator_Set(&d->scale[2], newZScale, steps);
    return *this;
}

FinalePageWidget *FinaleWidget::page() const
{
    return d->page;
}

FinaleWidget &FinaleWidget::setPage(FinalePageWidget *newPage)
{
    d->page = newPage;
    return *this;
}

void FinaleWidget::runTicks(/*timespan_t timeDelta*/)
{
    AnimatorVector3_Think(d->pos);
    AnimatorVector3_Think(d->scale);
    Animator_Think(&d->angle);
}
