/**
 * @file updatersettingsdialog.h
 * Dialog for configuring automatic updates. @ingroup updater
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

#ifndef DE_CLIENT_UPDATERSETTINGSDIALOG_H
#define DE_CLIENT_UPDATERSETTINGSDIALOG_H

#include <de/dialogwidget.h>

/**
 * Dialog for configuring the settings of the automatic updater.
 */
class UpdaterSettingsDialog : public de::DialogWidget
{
public:
    enum Mode { Normal = 0, WithApplyAndCheckButton = 1 };

    UpdaterSettingsDialog(Mode mode = Normal, const de::String &name = "updatersettings");

    /**
     * Determines whether settings have changed.
     */
    bool settingsHaveChanged() const;

    void apply();
    void applyAndCheckNow();

protected:
    void finish(int result);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UPDATERSETTINGSDIALOG_H
