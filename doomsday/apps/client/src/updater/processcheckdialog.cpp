/** @file processcheckdialog.cpp Dialog for checking running processes on Windows.
 * @ingroup updater
 *
 * @authors Copyright © 2012-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "updater/processcheckdialog.h"
#include "ui/clientwindow.h"

//#include <QProcess>
#include <de/commandline.h>
#include <de/messagedialog.h>
#include <SDL_messagebox.h>

using namespace de;

#if defined (DE_WINDOWS)

static bool isProcessRunning(const char *name)
{
    String result;
    CommandLine wmic;
    wmic << "wmic.exe" << "PROCESS" << "get" << "Caption";
    if (wmic.executeAndWait(&result))
    {
        for (String p : result.split("\n"))
        {
            if (!p.strip().compare(name, CaseInsensitive))
                return true;
        }
    }
    return false;
}

dd_bool Updater_AskToStopProcess(const char *processName, const char *message)
{
    while (isProcessRunning(processName))
    {
        MessageDialog *msg = new MessageDialog;
        msg->setDeleteAfterDismissed(true);
        msg->title().setText("Files In Use");
        msg->message().setText(String(message) + "\n\n" _E(2) +
                               Stringf("There is a running process called " _E(b)"%s." _E(.),
                                    processName));

        msg->buttons()
                << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, "Retry")
                << new DialogButtonItem(DialogWidget::Reject, "Ignore");

        // Show a notification dialog.
        if (!msg->exec(ClientWindow::main().root()))
        {
            return !isProcessRunning(processName);
        }
    }
    return true;
}

#endif // WIN32
