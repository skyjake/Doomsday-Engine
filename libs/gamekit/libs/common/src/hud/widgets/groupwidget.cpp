/** @file groupwidget.cpp  GUI widget for grouping widgets.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "hud/widgets/groupwidget.h"

#include <de/list.h>
#include "common.h"

using namespace de;

static void GroupWidget_UpdateGeometry(GroupWidget *group)
{
    DE_ASSERT(group);
    group->updateGeometry();
}

DE_PIMPL_NOREF(GroupWidget)
{
    order_t order = ORDER_NONE;  ///< Order of child objects.
    dint flags = 0;              ///< @ref groupWidgetFlags
    dint padding = 0;
    List<uiwidgetid_t> children;
};

GroupWidget::GroupWidget(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(GroupWidget_UpdateGeometry), nullptr, player)
    , d(new Impl)
{
    setPlayer(player);
}

GroupWidget::~GroupWidget()
{}

int GroupWidget::childCount() const
{
    return d->children.count();
}

void GroupWidget::addChild(HudWidget *other)
{
    if(!other || other == this) return;

    // Ensure widget is not already in this group.
    if(!d->children.contains(other->id()))
    {
        d->children << other->id();
    }
}

/// @todo optimize Do not peform the id => widget lookup constantly!
LoopResult GroupWidget::forAllChildren(const std::function<LoopResult (HudWidget &)>& func) const
{
    for(const uiwidgetid_t &childId : d->children)
    {
        if(auto result = func(GUI_FindWidgetById(childId)))
            return result;
    }
    return LoopContinue;
}

void GroupWidget::clearAllChildren()
{
    d->children.clear();
}

dint GroupWidget::flags() const
{
    return d->flags;
}

void GroupWidget::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

order_t GroupWidget::order() const
{
    return d->order;
}

void GroupWidget::setOrder(order_t newOrder)
{
    d->order = newOrder;
}

dint GroupWidget::padding() const
{
    return d->flags;
}

void GroupWidget::setPadding(dint newPadding)
{
    d->padding = newPadding;
}

void GroupWidget::tick(timespan_t elapsed)
{
    for(const uiwidgetid_t &childId : d->children)
    {
        GUI_FindWidgetById(childId).tick(elapsed);
    }
}

static void applyAlignmentOffset(HudWidget *wi, dint *x, dint *y)
{
    DE_ASSERT(wi);

    if(x)
    {
        if(wi->alignment() & ALIGN_RIGHT)
            *x += wi->maximumWidth();
        else if(!(wi->alignment() & ALIGN_LEFT))
            *x += wi->maximumWidth() / 2;
    }
    if(y)
    {
        if(wi->alignment() & ALIGN_BOTTOM)
            *y += wi->maximumHeight();
        else if(!(wi->alignment() & ALIGN_TOP))
            *y += wi->maximumHeight() / 2;
    }
}

void GroupWidget::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!childCount()) return;

    dint x = 0, y = 0;
    applyAlignmentOffset(this, &x, &y);

    for(const uiwidgetid_t &childId : d->children)
    {
        HudWidget &child = GUI_FindWidgetById(childId);

        if(child.maximumWidth() > 0 && child.maximumHeight() > 0 && child.opacity() > 0)
        {
            GUI_UpdateWidgetGeometry(&child);

            Rect_SetX(&child.geometry(), Rect_X(&child.geometry()) + x);
            Rect_SetY(&child.geometry(), Rect_Y(&child.geometry()) + y);

            const Rect *childGeometry = &child.geometry();
            if(Rect_Width(childGeometry) > 0 && Rect_Height(childGeometry) > 0)
            {
                if(d->order == ORDER_RIGHTTOLEFT)
                {
                    if(!(d->flags & UWGF_VERTICAL))
                        x -= Rect_Width(childGeometry)  + d->padding;
                    else
                        y -= Rect_Height(childGeometry) + d->padding;
                }
                else if(d->order == ORDER_LEFTTORIGHT)
                {
                    if(!(d->flags & UWGF_VERTICAL))
                        x += Rect_Width(childGeometry)  + d->padding;
                    else
                        y += Rect_Height(childGeometry) + d->padding;
                }

                Rect_Unite(&geometry(), childGeometry);
            }
        }
    }
}
