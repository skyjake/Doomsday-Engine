/**
 * @file processcheckdialog.cpp
 * Dialog for checking running processes on Windows. @ingroup updater
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "processcheckdialog.h"
#include <QMessageBox>
#include <QProcess>
#include <QDebug>

#ifdef WIN32

static bool isProcessRunning(const char* name)
{
    QProcess wmic;
    wmic.start("wmic.exe", QStringList() << "PROCESS" << "get" << "Caption");
    if(!wmic.waitForStarted()) return false;
    if(!wmic.waitForFinished()) return false;

    QByteArray result = wmic.readAll();
    foreach(QString p, QString(result).split("\n", QString::SkipEmptyParts))
    {
        if(!p.trimmed().compare(QLatin1String(name), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

boolean Updater_AskToStopProcess(const char* processName, const char* message)
{
    while(isProcessRunning(processName))
    {
        // Show a notification dialog.
        QMessageBox msg(QMessageBox::Warning, "Files In Use", message,
                        QMessageBox::Retry | QMessageBox::Ignore);
        msg.setInformativeText(QString("(There is a running process called <b>%1</b>.)").arg(processName));
        if(msg.exec() == QMessageBox::Ignore)
        {
            return !isProcessRunning(processName);
        }
    }
    return true;
}

#endif // WIN32
