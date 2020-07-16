/** @file localserverdialog.h  Dialog for starting a local server.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LOCALSERVERGUIDIALOG_H
#define LOCALSERVERGUIDIALOG_H

#include <de/dialogwidget.h>
#include <de/nativepath.h>

class LocalServerDialog : public de::DialogWidget
{
public:
    explicit LocalServerDialog();

    uint16_t port() const;
    de::String name() const;
    de::String gameMode() const;
    de::StringList additionalOptions() const;
    de::NativePath runtimeFolder() const;

protected:
    void portChanged();
    void configureGameOptions();
    void saveState();
    void validate();

private:
    DE_PRIVATE(d)
};

#endif // LOCALSERVERGUIDIALOG_H
