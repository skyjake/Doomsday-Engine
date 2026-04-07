/** @file editorapp.h  GloomEd application.
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

#ifndef GLOOMED_EDITORAPP_H
#define GLOOMED_EDITORAPP_H

#include <doomsday/doomsdayapp.h>
#include <de/EmbeddedApp>
#include <QApplication>

class Editor;

class EditorApp : public QApplication, public de::EmbeddedApp, public DoomsdayApp
{
public:
    EditorApp(int &argc, char **argv);

    void initialize();
    bool launchViewer();
    void loadEditorMapInViewer(Editor &editor);

    void checkPackageCompatibility(const de::StringList &,
                                   const de::String &,
                                   const std::function<void()> &finalizeFunc);

private:
    DE_PRIVATE(d)
};

#endif // GLOOMED_EDITORAPP_H
