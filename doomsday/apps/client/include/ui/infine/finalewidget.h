/** @file finalewidget.h  InFine animation system, FinaleWidget.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_UI_INFINE_FINALEWIDGET_H
#define DENG_UI_INFINE_FINALEWIDGET_H

#include <de/animator.h>
#include <de/Id>
#include <de/Observers>
#include <de/String>
#include <de/Vector>

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
    DENG2_DEFINE_AUDIENCE(Deletion, void finaleWidgetBeingDeleted(FinaleWidget const &widget))

public:
    explicit FinaleWidget(de::String const &name = "");
    virtual ~FinaleWidget();

    DENG2_AS_IS_METHODS()

#ifdef __CLIENT__
    virtual void draw(de::Vector3f const &offset) = 0;
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
    FinaleWidget &setName(de::String const &newName);

    animatorvector3_t const &origin() const;
    FinaleWidget &setOrigin(de::Vector3f const &newOrigin, int steps = 0);
    FinaleWidget &setOriginX(float newX, int steps = 0);
    FinaleWidget &setOriginY(float newY, int steps = 0);
    FinaleWidget &setOriginZ(float newZ, int steps = 0);

    animator_t const &angle() const;
    FinaleWidget &setAngle(float newAngle, int steps = 0);

    animatorvector3_t const &scale() const;
    FinaleWidget &setScale(de::Vector3f const &newScale, int steps = 0);
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
    DENG2_PRIVATE(d)
};

#endif // DENG_UI_INFINE_FINALEWIDGET_H
