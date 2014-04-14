/** @file notificationareawidget.h  Notification area.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../GuiWidget"

namespace de {

/**
 * Notification area.
 *
 * Children of the widget are expected to size themselves and allow unrestricted,
 * automatical positioning inside the area. Children can be added and removed
 * dynamically. The notification area is dismissed if there are no visible notifications.
 *
 * The client window owns an instance of NotificationAreaWidget. Other widgets and
 * subsystems are expected to give ownership of their notifications to the window's
 * NotificationAreaWidget.
 */
class LIBAPPFW_PUBLIC NotificationAreaWidget : public GuiWidget
{
    Q_OBJECT

public:
    NotificationAreaWidget(String const &name = "");

    /**
     * Places the notification widget in the top right corner of @a area.
     *
     * @param area  Reference area.
     */
    void useDefaultPlacement(RuleRectangle const &area);

    Rule const &shift();

    /**
     * Adds a notification to the notification area. The NotificationAreaWidget
     * takes ownership of @a notif (the latter is made a child).
     *
     * @param notif  Notification widget.
     */
    void showChild(GuiWidget *notif);

    /**
     * Hides a notification. The widget is hidden and gets returned to its
     * original parent, if there was one.
     *
     * @param notif  Notification widget.
     */
    void hideChild(GuiWidget &notif);

    void showOrHide(GuiWidget *notif, bool doShow) {
        if(doShow) showChild(notif); else hideChild(*notif);
    }

    bool isChildShown(GuiWidget &notif) const;

    // Events.
    void viewResized();
    void drawContent();

public slots:
    void dismiss();

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_NOTIFICATIONAREAWIDGET_H
