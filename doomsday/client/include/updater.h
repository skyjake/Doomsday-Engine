/**
 * @file updater.h
 * Automatic updater that works with dengine.net. @ingroup base
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

/**
 * @defgroup updater Automatic Updater
 */

#ifndef LIBDENG_UPDATER_H
#define LIBDENG_UPDATER_H

#include "dd_types.h"

#include <de/libdeng2.h>
#include <de/App>
#include <QObject>
#include <QNetworkReply>

class ProgressWidget;

/**
 * Automatic updater. Communicates with dengine.net and coordinates the
 * download and reinstall procedure.
 */
class Updater : public QObject
{
    Q_OBJECT

public:
    enum CheckMode
    {
        AlwaysShowResult,
        OnlyShowResultIfUpdateAvailable
    };

public:
    /**
     * Initializes the automatic updater. If it is time to check for an update,
     * queries the latest version from http://dengine.net/ and determines the
     * need to update.
     */
    Updater();

    void setupUI();

    ProgressWidget &progress();

public slots:
    void gotReply(QNetworkReply *);
    void downloadCompleted(int result);
    void settingsDialogClosed(int result);

    void recheck();

    /**
     * Shows the Updater Settings dialog.
     */
    void showSettings();

    /**
     * Tells the updater to check for updates now. This is called when a manual
     * check is requested.
     *
     * @param notify  Show the update notification dialog even though
     *                the current version is up to date.
     */
    void checkNow(CheckMode mode = AlwaysShowResult);

    void checkNowShowingProgress();

    /**
     * Print in the console when the latest update check was made.
     */
    void printLastUpdated();

private:
    DENG2_PRIVATE(d)
};

/**
 * Initializes the automatic updater. If it is time to check for an update,
 * queries the latest version from http://dengine.net/ and determines the need
 * to update.
 */
//void Updater_Init(void);

/**
 * Shuts down the automatic updater. Must be called at engine shutdown.
 */
//void Updater_Shutdown(void);

/**
 * Tells the updater to check for updates now. This is called when a manual
 * check is requested.
 *
 * @param notify  Show the notification dialog even when up to date.
 */
//void Updater_CheckNow(boolean notify);

//void Updater_ShowSettings(void);

//void Updater_PrintLastUpdated(void);

#endif // LIBDENG_UPDATER_H
