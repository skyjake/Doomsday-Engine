/** @file alertdialog.h  Dialog for listing recent alerts.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_ALERTDIALOG_H
#define DE_CLIENT_ALERTDIALOG_H

#include <de/dialogwidget.h>

/**
 * Dialog for listing recent alerts.
 *
 * Only one instance of each message is kept in the list.
 *
 * Use the static utility method ClientApp::alert() to conveniently add new alerts when
 * needed.
 *
 * @par Thread-safety
 *
 * Even though widgets in general should only be manipulated from the main thread,
 * adding new alerts is thread-safe.
 */
class AlertDialog : public de::DialogWidget
{
public:
    enum Level { Minor = -1, Normal = 0, Major = 1 };

public:
    AlertDialog(const de::String &name = "alerts");

    /**
     * Adds a new alert. If the same alert is already in the list, the new one is ignored.
     *
     * Can be called from any thread. Alerts are put in a queue until the next time the
     * dialog's update() method is called.
     *
     * @param message  Alert message.
     * @param level    Severity level.
     */
    void newAlert(const de::String &message, Level level = Normal);

    void update();

public:
    void showListOfAlerts();
    void hideNotification();
    void autohideTimeChanged();

protected:
    void finish(int result);
    void panelDismissed();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_ALERTDIALOG_H
