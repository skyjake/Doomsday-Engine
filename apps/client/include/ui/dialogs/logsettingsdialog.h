/** @file logsettingsdialog.h  Dialog for Log settings.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_LOGSETTINGSDIALOG_H
#define DE_CLIENT_LOGSETTINGSDIALOG_H

#include <de/dialogwidget.h>

/**
 * Dialog for modifying log filter and alert settings.
 */
class LogSettingsDialog : public de::DialogWidget
{
public:
    LogSettingsDialog(const de::String &name = "logsettings");

public:
    void resetToDefaults();
    void updateLogFilter();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_LOGSETTINGSDIALOG_H
