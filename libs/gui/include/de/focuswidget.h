/** @file focuswidget.h  Input focus indicator.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBAPPFW_FOCUSWIDGET_H
#define LIBAPPFW_FOCUSWIDGET_H

#include <de/labelwidget.h>

namespace de {

/**
 * Input focus indicator.
 *
 * GuiRootWidget owns an instance of FocusWidget to show where the input focus
 * is currently.
 */
class LIBGUI_PUBLIC FocusWidget : public LabelWidget
{
public:
    FocusWidget(const de::String &name = "focus");

    void startFlashing(const GuiWidget *reference = nullptr);
    void stopFlashing();

    void fadeIn();
    void fadeOut();

    /**
     * Determines whether the focus widget is active and flashing.
     */
    bool isKeyboardFocusActive() const;

    // Events.
    void update() override;

protected:
    void updateFlash();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FOCUSWIDGET_H
