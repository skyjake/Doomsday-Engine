/** @file main.cpp
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

#include "testapp.h"
#include "mainwindow.h"
#include <de/EscapeParser>
#include <QMessageBox>
#include <QDebug>

using namespace de;

int main(int argc, char **argv)
{
    TestApp::setDefaultOpenGLFormat();
    TestApp app(argc, argv);
    try
    {
        app.initialize();
        return app.execLoop();
    }
    catch (Error const &er)
    {
        EscapeParser esc;
        esc.parse(er.asText());
        qWarning() << "App init failed:\n" << esc.plainText();
        QMessageBox::critical(0, "test_appfw", "App init failed:\n" + esc.plainText());
        return -1;
    }

#ifdef DE_DEBUG
    // Check that all reference-counted objects have been deleted.
    DE_ASSERT(Counted::totalCount == 0);
#endif
    return 0;
}
