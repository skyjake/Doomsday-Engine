/** @file videosettingsdialog.h  Dialog for video settings.
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

#ifndef DENG_CLIENT_VIDEOSETTINGSDIALOG_H
#define DENG_CLIENT_VIDEOSETTINGSDIALOG_H

#include "ui/widgets/dialogwidget.h"

/**
 * Dialog for modifying video settings.
 */
class VideoSettingsDialog : public DialogWidget
{
    Q_OBJECT

public:
    VideoSettingsDialog(de::String const &name = "videosettings");

protected slots:
    void toggleAntialias();
    void toggleVerticalSync();
    void changeMode(uint selected);
    void changeColorDepth(uint selected);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_VIDEOSETTINGSDIALOG_H
