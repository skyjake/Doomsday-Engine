/** @file notificationareawidget.h  Notification area.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_NOTIFICATIONAREAWIDGET_H
#define LIBAPPFW_NOTIFICATIONAREAWIDGET_H

#include "de/relaywidget.h"

namespace de {

/**
 * Notification area.
 *
 * Notification widgets are expected to size themselves and allow unrestricted,
 * automatical positioning inside the area. Notifications can be added and removed
 * dynamically. The notification area is dismissed if there are no visible notifications.
 *
 * Notification widgets should not be part of the normal widget tree (no add/remove
 * called). Internally, NotificationAreaWidget uses RelayWidget to link the notifications
 * to the widget tree.
 *
 * The client window owns an instance of NotificationAreaWidget. Other widgets and
 * subsystems are expected to retain ownership of their notifications, and delete them
 * when the widget/subsystem is destroyed/shut down.
 *
 * Owners of notifications can use the UniqueWidgePtr template to automatically
 * delete their notification widgets.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC NotificationAreaWidget : public GuiWidget
{
public:
    NotificationAreaWidget(const String &name = {});

    /**
     * Places the notification widget in the top right corner of @a area.
     *
     * @param area              Reference area.
     * @param horizontalOffset  Additional horizontal offset.
     */
    void useDefaultPlacement(const RuleRectangle &area,
                             const Rule &horizontalOffset);

    const Rule &shift();

    /**
     * Adds a notification to the notification area. If the notification widget is
     * destroyed while visible, it will simply disappear from the notification area.
     * Widgets are initialized before showing.
     *
     * @param notif  Notification widget.
     */
    void showChild(GuiWidget &notif);

    /**
     * Hides a notification. The widget is deinitialized when dismissed (could be
     * after a delay if the entire notification area is animated away).
     *
     * @param notif  Notification widget.
     */
    void hideChild(GuiWidget &notif);

    void showOrHide(GuiWidget &notif, bool doShow) {
        if (doShow) showChild(notif); else hideChild(notif);
    }

    bool isChildShown(GuiWidget &notif) const;

    void dismiss();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_NOTIFICATIONAREAWIDGET_H
