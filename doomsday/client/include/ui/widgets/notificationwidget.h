/** @file notificationwidget.h  Notifiction area.
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

#ifndef DENG_CLIENT_NOTIFICATIONWIDGET_H
#define DENG_CLIENT_NOTIFICATIONWIDGET_H

#include "guiwidget.h"

/**
 * Notification area.
 *
 * Children the widget are expected to size themselves and allow unrestricted,
 * automatical positioning inside the area. Children can be added and removed
 * dynamically. The notification area is dismissed if there are no visible
 * notifications.
 *
 * The client window owns an instance of NotificationWidget. Other widgets and
 * subsystems are expected to give ownership of their notifications to the
 * window's NotificationWidget.
 */
class NotificationWidget : public GuiWidget
{
    Q_OBJECT

public:
    NotificationWidget(de::String const &name = "");

    de::Rule const &shift();

    /**
     * Adds a notification to the notification area. The NotificationWidget
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

#endif // DENG_CLIENT_NOTIFICATIONWIDGET_H
