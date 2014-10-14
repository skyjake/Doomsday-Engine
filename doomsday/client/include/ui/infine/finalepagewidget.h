/** @file finalepagewidget.h  InFine animation system, FinalePageWidget.
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

#ifndef DENG_UI_INFINE_FINALEPAGEWIDGET_H
#define DENG_UI_INFINE_FINALEPAGEWIDGET_H

#include <QList>
#include <de/Error>
#include "finalewidget.h"

class Material;

/**
 * Finale page widget (layer).
 *
 * @ingroup infine
 */
class FinalePageWidget
{
public:
    /// An invalid color index was specified. @ingroup errors
    DENG2_ERROR(InvalidColorError);

    /// An invalid font index was specified. @ingroup errors
    DENG2_ERROR(InvalidFontError);

    /// @note Unlike de::Visual the children are not owned by the page.
    typedef QList<FinaleWidget *> Widgets;

public:
    FinalePageWidget();
    virtual ~FinalePageWidget();

#ifdef __CLIENT__
    virtual void draw() const;
#endif
    virtual void runTicks(timespan_t timeDelta);

    void makeVisible(bool yes = true);
    void pause(bool yes = true);

    /**
     * Returns @c true if @a widget is present on the page.
     */
    bool hasWidget(FinaleWidget *widget);

    /**
     * Adds a widget to the page if not already present.
     *
     * @param widgetToAdd  Widget to be added.
     *
     * @return  Same as @a widgetToAdd, for convenience.
     */
    FinaleWidget *addWidget(FinaleWidget *widgetToAdd);

    /**
     * Removes a widget from the page if present.
     *
     * @param widgetToRemove  Widget to be removed.
     *
     * @return  Same as @a widgetToRemove, for convenience.
     */
    FinaleWidget *removeWidget(FinaleWidget *widgetToRemove);

    FinalePageWidget &setOffset(de::Vector3f const &newOffset, int steps = 0);
    FinalePageWidget &setOffsetX(float newOffsetX, int steps = 0);
    FinalePageWidget &setOffsetY(float newOffsetY, int steps = 0);
    FinalePageWidget &setOffsetZ(float newOffsetZ, int steps = 0);

    /// Current background Material.
    Material *backgroundMaterial() const;

    /// Sets the background Material.
    FinalePageWidget &setBackgroundMaterial(Material *newMaterial);

    /// Sets the background top color.
    FinalePageWidget &setBackgroundTopColor(de::Vector3f const &newColor, int steps = 0);

    /// Sets the background top color and alpha.
    FinalePageWidget &setBackgroundTopColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);

    /// Sets the background bottom color.
    FinalePageWidget &setBackgroundBottomColor(de::Vector3f const &newColor, int steps = 0);

    /// Sets the background bottom color and alpha.
    FinalePageWidget &setBackgroundBottomColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);

    /// Sets the filter color and alpha.
    FinalePageWidget &setFilterColorAndAlpha(de::Vector4f const &newColorAndAlpha, int steps = 0);

    /// @return  Animator which represents the identified predefined color.
    animatorvector3_t const *predefinedColor(uint idx);

    /// Sets a predefined color.
    FinalePageWidget &setPredefinedColor(uint idx, de::Vector3f const &newColor, int steps = 0);

    /// @return  Unique identifier of the predefined font.
    fontid_t predefinedFont(uint idx);

    /// Sets a predefined font.
    FinalePageWidget &setPredefinedFont(uint idx, fontid_t font);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_UI_INFINE_FINALEPAGEWIDGET_H
