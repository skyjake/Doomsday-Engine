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

#include <QMessageBox>

#include <de/GLState>
#include <de/Drawable>
#include <de/GLBuffer>
#include <de/GLShader>
#include <de/GLTexture>
#include <de/GuiApp>
#include <de/Clock>

using namespace de;

DENG2_PIMPL(TestWindow),
DENG2_OBSERVES(Canvas, GLInit),
DENG2_OBSERVES(Canvas, GLResize),
DENG2_OBSERVES(Clock, TimeChange)
{
    Drawable ob;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uTime;
    GLUniform uTex;
    GLTexture testpic;
    Time startedAt;

    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    Instance(Public *i)
        : Base(i),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4),
          uTime     ("uTime",      GLUniform::Float),
          uTex      ("uTex",       GLUniform::Sampler2D)
    {
        // Use this as the main window.
        setMain(i);

        self.canvas().audienceForGLInit += this;
        self.canvas().audienceForGLResize += this;
        Clock::appClock().audienceForTimeChange += this;

        uColor = Vector4f(.5f, .75f, .5f, 1);
    }

    void canvasGLInit(Canvas &cv)
    {
        try
        {
            LOG_DEBUG("GLInit");
            glInit(cv);
        }
        catch(Error const &er)
        {
            QMessageBox::critical(thisPublic, "GL Init Error", er.asText());
            exit(1);
        }
    }

    void glInit(Canvas &cv)
    {
        testpic.setImage(QImage(":/images/testpic.png"));
        testpic.generateMipmap();
        uTex = testpic;

        VertexBuf *buf = new VertexBuf;
        ob.addBuffer(1, buf);

        VertexBuf::Type verts[4] = {
            { Vector2f(0,   0),   Vector2f(0, 0), Vector4f(1, 1, 1, 1) },
            { Vector2f(400, 0),   Vector2f(1, 0), Vector4f(1, 1, 0, 1) },
            { Vector2f(400, 400), Vector2f(1, 1), Vector4f(1, 0, 0, 1) },
            { Vector2f(0,   400), Vector2f(0, 1), Vector4f(0, 0, 1, 1) }
        };
#if 1
        buf->setVertices(gl::TriangleFan, verts, 4, gl::Static);
#else
        // Set vertices without primitive type and specify indices instead.
        buf->setVertices(verts, 4, gl::Static);

        GLBuffer::Indices idx;
        idx << 0 << 1 << 2 << 3;
        buf->setIndices(gl::TriangleFan, idx, gl::Static);
#endif

        Block vertShader =
                "uniform highp mat4 uMvpMatrix;\n"
                "uniform highp vec4 uColor;\n"
                "uniform highp float uTime;\n"

                "attribute highp vec4 aVertex;\n"
                "attribute highp vec2 aUV;\n"
                "attribute highp vec4 aColor;\n"

                "varying highp vec2 vUV;\n"
                "varying highp vec4 vColor;\n"

                "void main(void) {\n"
                "   gl_Position = uMvpMatrix * aVertex;\n"
                "   vUV = aUV;\n"
                "   vColor = aColor + sin(uTime) * uColor;\n"
                "}\n";

        Block fragShader =
                "uniform sampler2D uTex;\n"

                "varying highp vec2 vUV;\n"
                "varying highp vec4 vColor;\n"

                "void main(void) {\n"
                "    gl_FragColor = texture2D(uTex, vUV) * vColor;\n"
                //"    gl_FragColor = vec4(texture2D(uTex, vUV).r, 0.0, 0.0, 1.0);\n"
                "}";

        ob.program().build(vertShader, fragShader)
                << uMvpMatrix
                << uColor
                << uTime
                << uTex;

        cv.renderTarget().setClearColor(Vector4f(.2f, .2f, .2f, 0));
    }

    void canvasGLResized(Canvas &cv)
    {
        LOG_DEBUG("GLResized: %i x %i") << cv.width() << cv.height();

        GLState &st = GLState::top();
        st.setViewport(Rectangleui::fromSize(cv.size()));
        st.setBlend(true);
        st.setBlendFunc(gl::SrcAlpha, gl::OneMinusSrcAlpha);

        uMvpMatrix = Matrix4f::ortho(-cv.width()/2, cv.width()/2, -cv.height()/2, cv.height()/2)
                * Matrix4f::scale(cv.height()/400.f)
                * Matrix4f::translate(Vector2f(-200, -200));

        LOG_DEBUG("uMvpMatrix: ") << uMvpMatrix.toMatrix4f().asText();
    }

    void draw(Canvas &cv)
    {
        //LOG_DEBUG("GLDraw");

        cv.renderTarget().clear(GLTarget::Color | GLTarget::Depth);

        ob.draw();
    }

    void timeChanged(Clock const &clock)
    {
        if(!startedAt.isValid())
        {
            startedAt = clock.time();
        }
        uTime = startedAt.since();
        self.update();
    }
};

TestWindow::TestWindow() : d(new Instance(this))
{
    setWindowTitle("libgui GL Sandbox");
    setMinimumSize(640, 480);
}

void TestWindow::canvasGLDraw(Canvas &canvas)
{
    d->draw(canvas);
    canvas.swapBuffers();

    CanvasWindow::canvasGLDraw(canvas);
}
