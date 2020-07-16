/** @file finalewidget.h  InFine animation system, FinaleWidget.
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

#ifndef DE_UI_INFINE_FINALEWIDGET_H
#define DE_UI_INFINE_FINALEWIDGET_H

#include <de/legacy/animator.h>
#include <de/id.h>
#include <de/observers.h>
#include <de/string.h>
#include <de/vector.h>

class FinalePageWidget;

/**
 * Base class for Finale widgets.
 *
 * @ingroup infine
 */
class FinaleWidget
{
public:
    /// Notified when the InFine object is about to be deleted.
    DE_DEFINE_AUDIENCE(Deletion, void finaleWidgetBeingDeleted(const FinaleWidget &widget))

public:
    explicit FinaleWidget(const de::String &name = de::String());
    virtual ~FinaleWidget();

    DE_CAST_METHODS()

#ifdef __CLIENT__
    virtual void draw(const de::Vec3f &offset) = 0;
#endif
    virtual void runTicks(/*timespan_t timeDelta*/);

    /**
     * Returns the unique identifier of the widget.
     */
    de::Id id() const;

    /**
     * Returns the symbolic name of the widget.
     */
    de::String name() const;
    FinaleWidget &setName(const de::String &newName);

    const animatorvector3_t &origin() const;
    FinaleWidget &setOrigin(const de::Vec3f &newOrigin, int steps = 0);
    FinaleWidget &setOriginX(float newX, int steps = 0);
    FinaleWidget &setOriginY(float newY, int steps = 0);
    FinaleWidget &setOriginZ(float newZ, int steps = 0);

    const animator_t &angle() const;
    FinaleWidget &setAngle(float newAngle, int steps = 0);

    const animatorvector3_t &scale() const;
    FinaleWidget &setScale(const de::Vec3f &newScale, int steps = 0);
    FinaleWidget &setScaleX(float newScaleX, int steps = 0);
    FinaleWidget &setScaleY(float newScaleY, int steps = 0);
    FinaleWidget &setScaleZ(float newScaleZ, int steps = 0);

    /**
     * Returns the FinalePageWidget to which the widget is attributed (if any).
     */
    FinalePageWidget *page() const;

    /**
     * Change/setup a reverse link between this object and it's owning page.
     * @note Changing this relationship here does not complete the task of
     * linking an object with a page (not enough information). It is therefore
     * the page's responsibility to call this when adding/removing objects.
     */
    FinaleWidget &setPage(FinalePageWidget *newPage);

private:
    DE_PRIVATE(d)
};

#endif // DE_UI_INFINE_FINALEWIDGET_H
