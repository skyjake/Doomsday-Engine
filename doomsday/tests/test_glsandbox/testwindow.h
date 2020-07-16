/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GLSANDBOX_TESTWINDOW_H
#define GLSANDBOX_TESTWINDOW_H

#include <de/glwindow.h>

class TestWindow : public de::GLWindow
{
public:
    TestWindow();

    void draw() override;
    void rootUpdate() override;
//    void keyPressEvent(QKeyEvent *ev) override;

    void testRenderToTexture();
    void testDynamicAtlas();
    void testModel();
    void loadMD2Model();
    void loadMD5Model();

private:
    DE_PRIVATE(d)
};

#endif // GLSANDBOX_TESTWINDOW_H
