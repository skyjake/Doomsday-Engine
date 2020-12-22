/**
 * @file downloaddialog.h
 * Dialog for downloads. @ingroup ui
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_DOWNLOADDIALOG_H
#define DE_CLIENT_DOWNLOADDIALOG_H

#include <de/dialogwidget.h>
#include <de/progresswidget.h>

/**
 * Dialog for downloading content.
 */
class DownloadDialog : public de::DialogWidget
{
public:
    DownloadDialog(const de::String &name = de::String());

    ~DownloadDialog();

    de::ProgressWidget &progressIndicator();

    virtual void cancel() = 0;

protected:
    void updateStyle() override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_DOWNLOADDIALOG_H
