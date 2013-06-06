/** @file buttonwidget.h  Clickable button widget.
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

#ifndef DENG_CLIENT_BUTTONWIDGET_H
#define DENG_CLIENT_BUTTONWIDGET_H

#include <de/Action>

#include "labelwidget.h"

/**
 * Clickable button widget.
 *
 * @ingroup gui
 */
class ButtonWidget : public LabelWidget
{
public:
    ButtonWidget(de::String const &name = "");

    /**
     * Sets the action of the button. It gets triggered when the button is
     * pressed.
     *
     * @param action  Action instance. Widget takes ownership.
     */
    void setAction(de::Action *action);

    // Events.
    void update();
    bool handleEvent(de::Event const &event);

protected:
    void updateModelViewProjection(de::GLUniform &uMvp);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_BUTTONWIDGET_H
