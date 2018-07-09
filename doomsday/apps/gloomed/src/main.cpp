/** @file main.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "editorwindow.h"
#include "utils.h"
#include <de/CommandLine>
#include <QApplication>
#include <QMessageBox>

using namespace de;

static iProcess *gloomProc = nullptr;

static bool launchGloom()
{
    if (gloomProc)
    {
        // Is it still running?
        if (isRunning_Process(gloomProc))
        {
            return true;
        }
        iRelease(gloomProc);
        gloomProc = nullptr;
    }

    CommandLine cmd;
#if defined (MACOSX)
    cmd << convert(qApp->applicationDirPath() + "/../../../Gloom.app/Contents/MacOS/Gloom");
#endif
    gloomProc = cmd.executeProcess();
    return gloomProc != nullptr;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    app.setApplicationName   ("GloomEd");
    app.setApplicationVersion("1.0");
    app.setOrganizationName  ("Deng Team");
    app.setOrganizationDomain("dengine.net");

    EditorWindow win;

    QObject::connect(&win.editor(), &Editor::buildMapRequested, [&app, &win]() {
        try
        {
            // Export/update the map package.

            // Launch the Gloom app.
            if (!launchGloom())
            {
                QMessageBox::critical(&win, app.applicationName(), "Failed to launch Gloom.");
                return;
            }

            // Wait for the process to start listening and tell it to load the map.

        }
        catch (const Error &er)
        {
            warning("Map build error: %s", er.asPlainText().c_str());
        }
    });

    win.showNormal();
    return app.exec();
}
