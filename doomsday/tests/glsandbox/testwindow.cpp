/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "testwindow.h"

#include <de/GLState>

using namespace de;

DENG2_PIMPL(TestWindow),
DENG2_OBSERVES(Canvas, GLInit),
DENG2_OBSERVES(Canvas, GLResize),
DENG2_OBSERVES(Canvas, GLDraw)
{
    Instance(Public *i) : Base(i)
    {
        // Use this as the main window.
        setMain(i);

        self.canvas().audienceForGLInit += this;
        self.canvas().audienceForGLResize += this;
    }

    void canvasGLInit(Canvas &cv)
    {
        LOG_DEBUG("GLInit");

        cv.renderTarget().setClearColor(Vector4f(.2f, .2f, .2f, 0));
    }

    void canvasGLResized(Canvas &cv)
    {
        LOG_DEBUG("GLResized: %i x %i") << cv.width() << cv.height();

        GLState &st = GLState::top();
        st.setViewport(Rectangleui::fromSize(cv.size()));
    }

    void canvasGLDraw(Canvas &cv)
    {
        LOG_DEBUG("GLDraw");

        cv.renderTarget().clear(GLTarget::Color | GLTarget::Depth);

    }
};

TestWindow::TestWindow() : d(new Instance(this))
{
    setWindowTitle("libgui GL Sandbox");
    setMinimumSize(640, 480);
}

void TestWindow::canvasGLDraw(Canvas &canvas)
{
    d->canvasGLDraw(canvas);
    canvas.swapBuffers();

    CanvasWindow::canvasGLDraw(canvas);
}
