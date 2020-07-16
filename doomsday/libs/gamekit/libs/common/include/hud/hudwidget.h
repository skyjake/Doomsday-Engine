/** @file hudwidget.h  Specialized UI widget for HUD elements.
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

#ifndef LIBCOMMON_UI_HUDWIDGET_H
#define LIBCOMMON_UI_HUDWIDGET_H

#include <de/vector.h>
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
    void (*drawer) (HudWidget *wi, const Point2Raw *offset);

public:
    explicit HudWidget(void (*updateGeometry) (HudWidget *wi),
                       void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                       int player = 0, uiwidgetid_t id = 0);
    virtual ~HudWidget();

    DE_CAST_METHODS()

    uiwidgetid_t id() const;
    void setId(uiwidgetid_t newId);

    /// @return  Local player number of the owner of this widget.
    int player() const;
    void setPlayer(int newPlayer);

    Rect &geometry() const;

    Size2Raw &maximumSize() const;
    void setMaximumSize(const Size2Raw &newMaxSize);

    inline int maximumHeight() const { return maximumSize().height; }
    inline int maximumWidth () const { return maximumSize().width;  }

    void setMaximumHeight(int newMaxHeight);
    void setMaximumWidth (int newMaxWidth);

    /// @return  @ref alignmentFlags
    int alignment() const;
    HudWidget &setAlignment(int alignFlags);

    float opacity() const;
    HudWidget &setOpacity(float newOpacity);

    fontid_t font() const;
    HudWidget &setFont(fontid_t newFont);

    virtual void tick(timespan_t /*elapsed*/) {}

private:
    DE_PRIVATE(d)
};

typedef void (*UpdateGeometryFunc)(HudWidget *);
typedef void (*DrawFunc)(HudWidget *, const Point2Raw *);

#endif  // LIBCOMMON_UI_HUDWIDGET_H
