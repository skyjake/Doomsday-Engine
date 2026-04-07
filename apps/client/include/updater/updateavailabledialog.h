/**
 * @file updateavailabledialog.h
 * Dialog for notifying the user about available updates. @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UPDATEAVAILABLEDIALOG_H
#define DE_CLIENT_UPDATEAVAILABLEDIALOG_H

#include <de/messagedialog.h>
#include <de/version.h>

class UpdateAvailableDialog : public de::MessageDialog
{
public:
    DE_AUDIENCE(Recheck, void userRequestedSoftwareUpdateCheck())

public:
    /// The dialog is initialized with the "Checking" page visible.
    UpdateAvailableDialog();

    /// The dialog is initialized with the result page visible.
    UpdateAvailableDialog(de::Version const& latestVersion, de::String changeLogUri);

public:
    void showResult(const de::Version &latestVersion, de::String changeLogUri);
    void showWhatsNew();
    void editSettings();

private:
    DE_PRIVATE(d)
};

#endif // DE_UPDATEAVAILABLEDIALOG_H
