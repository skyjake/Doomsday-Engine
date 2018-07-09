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
#include <QApplication>

using namespace de;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    app.setApplicationName   ("GloomEd");
    app.setApplicationVersion("1.0");
    app.setOrganizationName  ("Deng Team");
    app.setOrganizationDomain("dengine.net");

    EditorWindow win;
    win.showNormal();
    return app.exec();
}
