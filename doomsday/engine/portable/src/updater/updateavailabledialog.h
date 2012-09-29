/**
 * @file updateavailabledialog.h
 * Dialog for notifying the user about available updates. @ingroup updater
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_UPDATEAVAILABLEDIALOG_H
#define LIBDENG_UPDATEAVAILABLEDIALOG_H

#include "updaterdialog.h"
#include "versioninfo.h"

class UpdateAvailableDialog : public UpdaterDialog
{
    Q_OBJECT
    
public:
    /// The dialog is initialized with the "Checking" page visible.
    explicit UpdateAvailableDialog(QWidget *parent = 0);

    /// The dialog is initialized with the result page visible.
    explicit UpdateAvailableDialog(const VersionInfo& latestVersion,
                                   de::String changeLogUri, QWidget *parent = 0);
    ~UpdateAvailableDialog();
    
public slots:
    void showResult(const VersionInfo& latestVersion, de::String changeLogUri);
    void neverCheckToggled(bool);
    void showWhatsNew();
    void editSettings();
    void recenterDialog();

signals:
    void checkAgain();

private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_UPDATEAVAILABLEDIALOG_H
