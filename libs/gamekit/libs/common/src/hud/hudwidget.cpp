/** @file hudwidget.cpp  Specialized UI widget for HUD elements.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "hud/hudwidget.h"

#include <de/legacy/memory.h>
#include "hud/widgets/groupwidget.h"  ///< @todo remove me

using namespace de;
using namespace common;

DE_PIMPL_NOREF(HudWidget)
{
    uiwidgetid_t id = 0;              ///< Unique identifier associated with this widget.
    dint alignFlags = ALIGN_TOPLEFT;  ///< @ref alignmentFlags
    Size2Raw maxSize{};               ///< Maximum size of this widget in pixels.
    Rect *geometry = Rect_New();      ///< Geometry of this widget in pixels.
    dint player = 0;                  ///< Local player number associated with this widget.
    fontid_t font = 0;                ///< Current font used for text child objects of this widget.
    dfloat opacity = 1;               ///< Current opacity value for this widget.

    ~Impl() { Rect_Delete(geometry); }
};

HudWidget::HudWidget(void (*updateGeometry) (HudWidget *wi),
                     void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                     dint playerNum, uiwidgetid_t id)
    : updateGeometry(updateGeometry)
    , drawer(drawer)
    , d(new Impl)
{
    setId(id);
    setPlayer(playerNum);
}

HudWidget::~HudWidget()
{}

uiwidgetid_t HudWidget::id() const
{
    return d->id;
}

void HudWidget::setId(uiwidgetid_t newId)
{
    d->id = newId;
}

int HudWidget::player() const
{
    return d->player;
}

void HudWidget::setPlayer(int newPlayer)
{
    d->player = newPlayer;
}

Rect &HudWidget::geometry() const
{
    DE_ASSERT(d->geometry);
    return *d->geometry;
}

Size2Raw &HudWidget::maximumSize() const
{
    return d->maxSize;
}

void HudWidget::setMaximumSize(const Size2Raw &newSize)
{
    if(d->maxSize.width == newSize.width &&
       d->maxSize.height == newSize.height) return;
    d->maxSize.width  = newSize.width;
    d->maxSize.height = newSize.height;

    if(auto *group = maybeAs<GroupWidget>(this))
    {
        group->forAllChildren([&newSize] (HudWidget &child)
        {
            child.setMaximumSize(newSize);
            return LoopContinue;
        });
    }
}

void HudWidget::setMaximumHeight(int newMaxHeight)
{
    if(d->maxSize.height == newMaxHeight) return;
    d->maxSize.height = newMaxHeight;

    if(auto *group = maybeAs<GroupWidget>(this))
    {
        group->forAllChildren([&newMaxHeight] (HudWidget &child)
        {
            child.setMaximumHeight(newMaxHeight);
            return LoopContinue;
        });
    }
}

void HudWidget::setMaximumWidth(int newMaxWidth)
{
    if(d->maxSize.width == newMaxWidth) return;
    d->maxSize.width = newMaxWidth;

    if(auto *group = maybeAs<GroupWidget>(this))
    {
        group->forAllChildren([&newMaxWidth] (HudWidget &child)
        {
            child.setMaximumWidth(newMaxWidth);
            return LoopContinue;
        });
    }
}

int HudWidget::alignment() const
{
    return d->alignFlags;
}

HudWidget &HudWidget::setAlignment(int alignFlags)
{
    d->alignFlags = alignFlags;
    return *this;
}

float HudWidget::opacity() const
{
    return d->opacity;
}

HudWidget &HudWidget::setOpacity(float newOpacity)
{
    d->opacity = newOpacity;
    if(auto *group = maybeAs<GroupWidget>(this))
    {
        group->forAllChildren([&newOpacity] (HudWidget &child)
        {
            child.setOpacity(newOpacity);
            return LoopContinue;
        });
    }
    return *this;
}

fontid_t HudWidget::font() const
{
    return d->font;
}

HudWidget &HudWidget::setFont(fontid_t newFont)
{
    d->font = newFont;
    return *this;
}
