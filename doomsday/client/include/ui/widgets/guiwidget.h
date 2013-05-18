/** @file guiwidget.h  Base class for graphical widgets.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_GUIWIDGET_H
#define DENG_CLIENT_GUIWIDGET_H

#include <de/Widget>
#include <de/RuleRectangle>
#include "ui/style.h"

class GuiRootWidget;

/**
 * Base class for graphical widgets.
 * @ingroup gui
 */
class GuiWidget : public de::Widget
{
public:
    GuiWidget(de::String const &name = "");

    GuiRootWidget &root();
    Style const &style();

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    de::RuleRectangle &rule();

    /**
     * Returns the rule rectangle that defines the placement of the widget on
     * the target canvas.
     */
    de::RuleRectangle const &rule() const;

    void deleteLater();

    // Events.
    void initialize();
    void deinitialize();
    void update();

protected:
    virtual void glInit();
    virtual void glDeinit();

    /**
     * Checks if the widget's rectangle has changed.
     *
     * @param currentPlace  The widget's current placement is returned here.
     *
     * @return @c true, if the place of the widget has changed since the
     * last call to checkPlace(); otherwise, @c false.
     */
    bool checkPlace(de::Rectanglei &currentPlace);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GUIWIDGET_H
