/** @file popupbuttonwidget.h  Button for opening a popup.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_POPUPBUTTONWIDGET_H
#define LIBAPPFW_POPUPBUTTONWIDGET_H

#include "de/buttonwidget.h"
#include "de/popupwidget.h"

#include <functional>

namespace de {

/**
 * Button for opening a popup.
 *
 * Unlike a regular button, ensures that if the popup is open when the button is
 * clicked, the popup will just close and not be immediately opened again.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC PopupButtonWidget : public ButtonWidget
{
public:
    typedef std::function<PopupWidget * (const PopupButtonWidget &)> Constructor;
    typedef std::function<void (PopupWidget *)> Opener;

public:
    PopupButtonWidget(const String &name = String());

    /**
     * Sets the popup that this button controls. The popup is automatically anchored
     * to the button.
     *
     * @param popup             Popup to open and close.
     * @param openingDirection  Opening direction for the popup.
     */
    void setPopup(PopupWidget &popup,
                  ui::Direction openingDirection = ui::Up);

    /**
     * Sets the opening callback.
     *
     * @param opener  Callback that performs the opening. Should call open()
     *                on the popup.
     */
    void setOpener(Opener opener);

    /**
     * Sets a constructor function to call to create the popup when it should be
     * opened. The popup is automatically anchored to the button. The popup will be
     * destroyed once closed.
     *
     * @param makePopup  Popup constructor. Reference to this button is given as
     *                   an argument, however the newly created popup does not need
     *                   to use this information.
     * @param openingDirection  Opening direction for the popup.
     */
    void setPopup(Constructor makePopup,
                  ui::Direction openingDirection = ui::Up);

    /**
     * Sets the opening direction of the popup.
     *
     * @param direction  Direction.
     */
    void setOpeningDirection(ui::Direction direction);

    /**
     * Returns the popup, if one exists.
     */
    PopupWidget *popup() const;

    /**
     * Determines if the popup is currently open.
     */
    bool isOpen() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_POPUPBUTTONWIDGET_H

