/**
 * @file nativeui.cpp
 * Native GUI functionality. @ingroup base
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

#include <QMessageBox>
#include <QFile>
#include <QString>
#include <stdarg.h>
#include "dd_share.h"
#include "sys_system.h"
#include <QLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>

#include "nativeui.h"

void Sys_MessageBox(messageboxtype_t type, const char* title, const char* msg, const char* detailedMsg)
{
    Sys_MessageBox2(type, title, msg, 0, detailedMsg);
}

void Sys_MessageBox2(messageboxtype_t type, const char* title, const char* msg, const char* informativeMsg, const char* detailedMsg)
{
    if(novideo)
    {
        // There's no GUI...
        qWarning("%s", msg);
        return;
    }

    QMessageBox box;
    box.setWindowTitle(title);
    box.setText(msg);
    switch(type)
    {
    case MBT_INFORMATION: box.setIcon(QMessageBox::Information); break;
    case MBT_WARNING:     box.setIcon(QMessageBox::Warning); break;
    case MBT_ERROR:       box.setIcon(QMessageBox::Critical); break;
    default:
        break;
    }
    if(detailedMsg)
    {
        /// @todo Making the dialog a bit wider would be nice, but it seems one has to
        /// derive a new message box class for that -- the default one has a fixed size.

        box.setDetailedText(detailedMsg);
    }
    if(informativeMsg)
    {
        box.setInformativeText(informativeMsg);
    }
    box.exec();
}

void Sys_MessageBoxf(messageboxtype_t type, const char* title, const char* format, ...)
{
    char buffer[1024];
    va_list args;

    va_start(args, format);
    dd_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Sys_MessageBox(type, title, buffer, 0);
}

void Sys_MessageBoxWithDetailsFromFile(messageboxtype_t type, const char* title, const char* msg,
                                       const char* informativeMsg, const char* detailsFileName)
{
    QByteArray details;
    QFile file(detailsFileName);
    if(file.open(QFile::ReadOnly | QFile::Text))
    {
        details = file.readAll();
        // This will be used as a null-terminated string.
        details.append(char(0));
        file.close();
    }
    Sys_MessageBox2(type, title, msg, informativeMsg, details.constData());
}
