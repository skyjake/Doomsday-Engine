/** @file hudwidget.h  Specialized UI widget for HUD elements.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_UI_HUDWIDGET_H
#define LIBCOMMON_UI_HUDWIDGET_H

#include <de/Vector>
#include "doomsday.h"

typedef int uiwidgetid_t;

/**
 * Base class for specialized UI widgets that implement HUD elements.
 *
 * @ingroup ui
 */
class HudWidget
{
public:
    void (*updateGeometry) (HudWidget *wi);
    void (*drawer) (HudWidget *wi, Point2Raw const *offset);

public:
    explicit HudWidget(void (*updateGeometry) (HudWidget *wi),
                       void (*drawer) (HudWidget *wi, Point2Raw const *offset),
                       de::dint player = 0, uiwidgetid_t id = 0);
    virtual ~HudWidget();

    DENG2_AS_IS_METHODS()

    uiwidgetid_t id() const;
    void setId(uiwidgetid_t newId);

    /// @return  Local player number of the owner of this widget.
    de::dint player() const;
    void setPlayer(de::dint newPlayer);

    Rect &geometry() const;

    Size2Raw &maximumSize() const;
    void setMaximumSize(Size2Raw const &newMaxSize);

    inline de::dint maximumHeight() const { return maximumSize().height; }
    inline de::dint maximumWidth () const { return maximumSize().width;  }

    void setMaximumHeight(de::dint newMaxHeight);
    void setMaximumWidth (de::dint newMaxWidth);

    /// @return  @ref alignmentFlags
    de::dint alignment() const;
    HudWidget &setAlignment(de::dint alignFlags);

    de::dfloat opacity() const;
    HudWidget &setOpacity(de::dfloat newOpacity);

    fontid_t font() const;
    HudWidget &setFont(fontid_t newFont);

    virtual void tick(timespan_t /*elapsed*/) {}

private:
    DENG2_PRIVATE(d)
};

typedef void (*UpdateGeometryFunc)(HudWidget *);
typedef void (*DrawFunc)(HudWidget *, Point2Raw const *);

#endif  // LIBCOMMON_UI_HUDWIDGET_H
