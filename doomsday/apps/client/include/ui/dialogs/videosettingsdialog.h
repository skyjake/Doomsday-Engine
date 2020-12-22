/** @file videosettingsdialog.h  Dialog for video settings.
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

#ifndef DE_CLIENT_VIDEOSETTINGSDIALOG_H
#define DE_CLIENT_VIDEOSETTINGSDIALOG_H

#include <de/dialogwidget.h>

/**
 * Dialog for modifying video settings.
 */
class VideoSettingsDialog : public de::DialogWidget
{
public:
    VideoSettingsDialog(const de::String &name = "videosettings");

protected:
    void resetToDefaults();
#if !defined (DE_MOBILE)
    void changeMode(de::ui::DataPos selected);
    void changeColorDepth(de::ui::DataPos selected);
    void changeRefreshRate(de::ui::DataPos selected);
    void showColorAdjustments();
    void showWindowMenu();
    void applyModeToWindow();
#endif

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_VIDEOSETTINGSDIALOG_H
