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

#include "testwindow.h"

#include <de/atlastexture.h>
#include <de/drawable.h>
#include <de/escapeparser.h>
#include <de/filesystem.h>
#include <de/glbuffer.h>
#include <de/glinfo.h>
#include <de/glshader.h>
#include <de/glstate.h>
#include <de/glframebuffer.h>
#include <de/gltexture.h>
#include <de/guiapp.h>
#include <de/imagebank.h>
#include <de/modeldrawable.h>

#include <SDL_messagebox.h>

using namespace de;

DE_PIMPL(TestWindow)
, DE_OBSERVES(GLWindow, Init)
, DE_OBSERVES(GLWindow, Resize)
, DE_OBSERVES(KeyEventSource, KeyEvent)
, DE_OBSERVES(Clock, TimeChange)
, DE_OBSERVES(Bank, Load)
{
    enum Mode { TestRenderToTexture, TestDynamicAtlas, TestModel };

    Mode      mode;
    ImageBank imageBank;
    Drawable  ob;
    Drawable  atlasOb;
    Mat4f     modelMatrix;
    Mat4f     projMatrix;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uTime;
    GLUniform uTex;
    GLTexture frameTex;
    GLTexture testpic;

    ModelDrawable model;
    ModelDrawable::Animator modelAnim;
    std::unique_ptr<AtlasTexture> modelAtlas;
    GLUniform uModelTex;
    GLProgram modelProgram;

    std::unique_ptr<AtlasTexture> atlas;
    std::unique_ptr<GLFramebuffer> frameTarget;
    Time startedAt;
    Time lastAtlasAdditionAt;
    bool eraseAtlas;

    typedef GLBufferT<Vertex3TexRgba> VertexBuf;
    typedef GLBufferT<Vertex2Tex> Vertex2Buf;

    Impl(Public *i)
        : Base(i)
        , mode(TestRenderToTexture)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uColor("uColor", GLUniform::Vec4)
        , uTime("uTime", GLUniform::Float)
        , uTex("uTex", GLUniform::Sampler2D)
        , modelAnim(model)
        , uModelTex("uTex", GLUniform::Sampler2D)
        , atlas(AtlasTexture::newWithRowAllocator(Atlas::AllowDefragment | Atlas::BackingStore |
                                                  Atlas::WrapBordersInBackingStore))
    {
        // Use this as the main window.
        setMain(i);

        self().audienceForInit() += this;
        self().audienceForResize() += this;
        self().eventHandler().audienceForKeyEvent() += this;
        Clock::get().audienceForTimeChange() += this;

        uColor = Vec4f(.5f, .75f, .5f, 1);
        atlas->setTotalSize(Vec2ui(256, 256));
        atlas->setBorderSize(2);
        atlas->setMagFilter(gfx::Nearest);

        imageBank.add("rtt.cube", "/packs/net.dengine.test.glsandbox/testpic.png");
        //imageBank.loadAll();
        imageBank.audienceForLoad() += this;

        //model.load(App::rootFolder().locate<File>("/data/models/marine.md2")); //boblampclean.md5mesh"));

        modelAtlas.reset(AtlasTexture::newWithKdTreeAllocator(Atlas::DefaultFlags, Atlas::Size(2048, 2048)));
        model.setAtlas(*modelAtlas);
        uModelTex = *modelAtlas;
    }

    ~Impl()
    {
        self().glActivate();
        model.glDeinit();
        ob.clear();
        atlasOb.clear();
        modelProgram.clear();
    }

    void windowInit(GLWindow &)
    {
        try
        {
            LOG_DEBUG("GLInit");
            glInit();
        }
        catch (const Error &er)
        {
            er.warnPlainText();

            EscapeParser esc;
            esc.parse(er.asText());
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR, "GL Init Error", esc.plainText().c_str(), nullptr);

            exit(1);
        }
    }

    void glInit()
    {
        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
        
        // Set up the default state.
        GLState &st = GLState::current();
        st.setBlend(true);
        st.setBlendFunc(gfx::SrcAlpha, gfx::OneMinusSrcAlpha);
        //st.setCull(gfx::Back);
        st.setDepthTest(true);

        // Textures.
        testpic.setAutoGenMips(true);
        imageBank.load("rtt.cube");
        //testpic.setImage(imageBank.image("rtt/cube"));
        //testpic.setImage(QImage(":/images/testpic.png"));
        testpic.setWrapT(gfx::RepeatMirrored);
        testpic.setMinFilter(gfx::Linear, gfx::MipLinear);
        uTex = testpic;

        // Prepare the custom target.
        frameTex.setUndefinedImage(Vec2ui(512, 256), Image::RGBA_8888);
        frameTarget.reset(new GLFramebuffer(frameTex));

        // 3D cube.
        VertexBuf *buf = new VertexBuf;
        ob.addBuffer(buf);

        VertexBuf::Type verts[8] = {
            { Vec3f(-1, -1, -1), Vec2f(0, 0), Vec4f(1) },
            { Vec3f( 1, -1, -1), Vec2f(1, 0), Vec4f(1, 1, 0, 1) },
            { Vec3f( 1,  1, -1), Vec2f(1, 1), Vec4f(1, 0, 0, 1) },
            { Vec3f(-1,  1, -1), Vec2f(0, 1), Vec4f(0, 0, 1, 1) },
            { Vec3f(-1, -1,  1), Vec2f(1, 1), Vec4f(1) },
            { Vec3f( 1, -1,  1), Vec2f(0, 1), Vec4f(1, 1, 0, 1) },
            { Vec3f( 1,  1,  1), Vec2f(0, 0), Vec4f(1, 0, 0, 1) },
            { Vec3f(-1,  1,  1), Vec2f(1, 0), Vec4f(0, 0, 1, 1) }
        };

        buf->setVertices(verts, 8, gfx::Static);

        GLBuffer::Indices idx;
        idx << 0 << 4 << 3 << 7 << 2 << 6 << 1 << 5 << 0 << 4
            << 4 << 0
            << 0 << 3 << 1 << 2
            << 2 << 7
            << 7 << 4 << 6 << 5;
        buf->setIndices(gfx::TriangleStrip, idx, gfx::Static);

        ob.program().build(
                    ByteRefArray::fromCStr(
                        "uniform highp mat4 uMvpMatrix;\n"
                        "uniform highp vec4 uColor;\n"
                        "uniform highp float uTime;\n"

                        "in highp vec4 aVertex;\n"
                        "in highp vec2 aUV;\n"
                        "in highp vec4 aColor;\n"

                        "out highp vec2 vUV;\n"
                        "out highp vec4 vColor;\n"

                        "void main(void) {\n"
                        "  gl_Position = uMvpMatrix * aVertex;\n"
                        "  vUV = aUV + vec2(uTime/10.0, 0.0);\n"
                        "  vColor = aColor + vec4(sin(uTime), cos(uTime), "
                            "sin(uTime), cos(uTime)*0.5) * uColor;\n"
                        "}\n"),
                    ByteRefArray::fromCStr(
                        "uniform sampler2D uTex;\n"

                        "in highp vec2 vUV;\n"
                        "in highp vec4 vColor;\n"

                        "void main(void) {\n"
                        "  highp vec4 color = texture(uTex, vUV);\n"
                        "  if (color.a < 0.05) discard;\n"
                        "  out_FragColor = color * vColor;\n"
                        "}"))
                << uMvpMatrix
                << uColor << uTime
                << uTex;

        // Require testpic to be ready before rendering the cube.
        ob += testpic;

        // The atlas objects.
        Vertex2Buf *buf2 = new Vertex2Buf;
        Vertex2Buf::Type verts2[4] = {
            { Vec2f(0,   0),   Vec2f(0, 0) },
            { Vec2f(100, 0),   Vec2f(1, 0) },
            { Vec2f(100, 100), Vec2f(1, 1) },
            { Vec2f(0,   100), Vec2f(0, 1) }
        };
        buf2->setVertices(gfx::TriangleFan, verts2, 4, gfx::Static);
        atlasOb.addBuffer(buf2);

        atlasOb.program().build(
                    ByteRefArray::fromCStr(
                        "uniform highp mat4 uMvpMatrix;\n"
                        "in highp vec4 aVertex;\n"
                        "in highp vec2 aUV;\n"
                        "out highp vec2 vUV;\n"
                        "void main(void) {\n"
                        "  gl_Position = uMvpMatrix * aVertex;\n"
                        "  vUV = aUV;\n"
                        "}\n"),
                    ByteRefArray::fromCStr(
                        "uniform sampler2D uTex;\n"
                        "in highp vec2 vUV;\n"
                        "void main(void) {\n"
                        "  out_FragColor = texture(uTex, vUV);\n"
                        "}\n"))
                << uMvpMatrix // note: uniforms shared between programs
                << uTex;

        self().framebuffer().setClearColor(Vec4f(.2f, .2f, .2f, 0));

        modelProgram.build(
                    ByteRefArray::fromCStr(
                        "uniform highp mat4 uMvpMatrix;\n"
                        "uniform highp vec4 uColor;\n"
                        "uniform highp mat4 uBoneMatrices[64];\n"

                        "in highp vec4 aVertex;\n"
                        "in highp vec3 aNormal;\n"
                        "in highp vec2 aUV;\n"
                        "in highp vec4 aBounds0;\n"
                        "in highp vec4 aColor;\n"
                        "in highp vec4 aBoneIDs;\n"
                        "in highp vec4 aBoneWeights;\n"

                        "out highp vec2 vUV;\n"
                        "out highp vec4 vColor;\n"
                        "out highp vec3 vNormal;\n"

                        "void main(void) {\n"
                        "  highp mat4 bone =\n"
                        "    uBoneMatrices[int(aBoneIDs.x + 0.5)] * aBoneWeights.x + \n"
                        "    uBoneMatrices[int(aBoneIDs.y + 0.5)] * aBoneWeights.y + \n"
                        "    uBoneMatrices[int(aBoneIDs.z + 0.5)] * aBoneWeights.z + \n"
                        "    uBoneMatrices[int(aBoneIDs.w + 0.5)] * aBoneWeights.w;\n"
                        "  highp vec4 modelPos = bone * aVertex;\n"
                        "  gl_Position = uMvpMatrix * modelPos;\n"
                        "  vUV = aBounds0.xy + aUV * aBounds0.zw;\n"
                        "  vColor = aColor;\n"
                        "  vNormal = (bone * vec4(aNormal, 0.0)).xyz;\n"
                        "}\n"),
                    ByteRefArray::fromCStr(
                        "uniform sampler2D uTex;\n"
                        "in highp vec2 vUV;\n"
                        "in highp vec3 vNormal;\n"
                        "void main(void) {\n"
                        "  out_FragColor = texture(uTex, vUV) * "
                            "vec4(vec3((vNormal.x + 1.0) / 2.0), 1.0);"
                        "}\n"))
                << uMvpMatrix
                << uModelTex;
        model.setProgram(&modelProgram);

        LIBGUI_ASSERT_GL_OK();
    }

    void bankLoaded(const DotPath &path)
    {
        LOG_RES_NOTE("Bank item \"%s\" loaded") << path;
        if (path == "rtt.cube")
        {
            DE_ASSERT_IN_MAIN_THREAD();

            self().glActivate();
            testpic.setImage(imageBank.image(path));
            //self().canvas().doneCurrent();

            imageBank.unload(path);
        }
    }

    void windowResized(GLWindow &)
    {
        LOG_GL_VERBOSE("GLResized: %i x %i pixels") << self().pixelWidth() << self().pixelHeight();

        GLState &st = GLState::current();
        //st.setViewport(Rectangleui::fromSize(cv.size()));
        st.setViewport(Rectangleui(0, 0, self().pixelWidth(), self().pixelHeight()));

        updateProjection();

        LIBGUI_ASSERT_GL_OK();
    }

    void updateProjection()
    {
        const auto pointSize = self().pointSizef();
        switch (mode)
        {
        case TestRenderToTexture:
            // 3D projection.
            projMatrix = Mat4f::perspective(75, pointSize.x / pointSize.y) *
                         Mat4f::lookAt(Vec3f(), Vec3f(0, 0, -5), Vec3f(0, -1, 0));
            break;

        case TestDynamicAtlas:
            // 2D projection.
            projMatrix = Mat4f::ortho(-pointSize.x / 2,
                                      +pointSize.x / 2,
                                      -pointSize.y / 2,
                                      +pointSize.y / 2) *
                         Mat4f::scale(pointSize.y / 150.f) *
                         Mat4f::translate(Vec2f(-50, -50));
            break;

        case TestModel:
            // 3D projection.
            projMatrix = Mat4f::perspective(75, pointSize.x / pointSize.y) *
                         Mat4f::lookAt(Vec3f(), Vec3f(0, -3, 0), Vec3f(0, 0, 1));
            break;
        }
    }

    void setMode(Mode newMode)
    {
        mode = newMode;
        updateProjection();

        switch (mode)
        {
        case TestDynamicAtlas:
            lastAtlasAdditionAt = Time();
            uMvpMatrix = projMatrix;
            break;

        case TestModel:
            break;

        default:
            break;
        }
    }

    void draw()
    {
        switch (mode)
        {
        case TestRenderToTexture:
            // First render the frame to the texture.
            GLState::push()
                .setTarget(*frameTarget)
                .setViewport(Rectangleui::fromSize(frameTex.size()));
            drawRttFrame();
            GLState::pop();

            // Render normally.
            drawRttFrame();
            break;

        case TestDynamicAtlas:
            GLState::push().setBlend(false);
            drawAtlasFrame();
            GLState::pop();
            break;

        case TestModel:
            drawModel();
            break;
        }
    }

    void drawRttFrame()
    {
        GLState::current().target().clear(GLFramebuffer::ColorDepth);

        // The left cube.
        uTex = testpic;
        uMvpMatrix = projMatrix *
                     Mat4f::translate(Vec3f(-1.5f, 0, 0)) *
                     modelMatrix;
        ob.draw();

        // The right cube.
        uTex = frameTex;
        uMvpMatrix = projMatrix *
                     Mat4f::translate(Vec3f(+1.5f, 0, 0)) *
                     modelMatrix;
        ob.draw();
    }

    void drawAtlasFrame()
    {
        GLState::current().target().clear(GLFramebuffer::ColorDepth);
        uTex = *atlas;
        uMvpMatrix = projMatrix;
        atlasOb.draw();
    }

    void initModelAnimation()
    {
        modelAnim.clear();
        modelAnim.start(0);
    }

    void drawModel()
    {
        GLState::current().target().clear(GLFramebuffer::ColorDepth);

        uMvpMatrix = projMatrix * modelMatrix;

        if (!modelAnim.isEmpty())
        {
            modelAnim.at(0).time = startedAt.since();
        }

        model.draw(&modelAnim);
    }

    void timeChanged(const Clock &clock)
    {
        self().glActivate();
        LIBGUI_ASSERT_GL_OK();

        if (!startedAt.isValid())
        {
            startedAt = clock.time();
        }
        uTime = startedAt.since();

        switch (mode)
        {
        case TestRenderToTexture:
            modelMatrix = Mat4f::rotate(std::cos(uTime.toFloat()/2) * 45, Vec3f(1, 0, 0)) *
                          Mat4f::rotate(std::sin(uTime.toFloat()/3) * 60, Vec3f(0, 1, 0));
            break;

        case TestModel:
            modelMatrix = Mat4f::translate(Vec3f(0, std::cos(uTime.toFloat()/2.5f), 0)) *
                          Mat4f::rotate(std::cos(uTime.toFloat()/2) * 45, Vec3f(1, 0, 0)) *
                          Mat4f::rotate(std::sin(uTime.toFloat()/3) * 60, Vec3f(0, 1, 0)) *
                          Mat4f::scale(3.f / de::max(model.dimensions().x, model.dimensions().y, model.dimensions().z)) *
                          Mat4f::translate(-model.midPoint());
            break;

        case TestDynamicAtlas:
            if (lastAtlasAdditionAt.since() > 0.2)
            {
                lastAtlasAdditionAt = Time();
                nextAtlasAlloc();
            }
            break;
        }

        LIBGUI_ASSERT_GL_OK();
        self().glDone();
    }

    void nextAtlasAlloc()
    {
        if (eraseAtlas)
        {
            atlas->clear();
            eraseAtlas = false;
            return;
        }

#if 1
        if ((rand() % 10) <= 5 && !atlas->isEmpty())
        {
            // Randomly remove one of the allocations.
            List<Id> ids;
            for (const Id &id : atlas->allImages()) ids << id;
            Id chosen = ids[rand() % ids.size()];
            atlas->release(chosen);

            LOG_MSG("Removed ") << chosen;
        }
#endif

        // Generate a random image.
        Image::Size imgSize(10 + rand() % 40, 10 + 10 * (rand() % 2));
        Image img(imgSize, Image::RGBA_8888);
        img.fill(Image::makeColor(rand() % 256, rand() % 256, rand() % 256));
        img.drawRect(img.rect(), Image::Color(255, 255, 255, 255));

        Id id = atlas->alloc(img);

        LOG_MSG("Allocated ") << id;
        if (id.isNone())
        {
            lastAtlasAdditionAt = Time() + 5.0;

            // Erase the entire atlas.
            eraseAtlas = true;
        }
    }

    void keyEvent(const KeyEvent &event)
    {
        debug("sdlkey %x (%s) [%s]",
              event.sdlKey(),
              event.state() == KeyEvent::Pressed
                  ? "down"
                  : event.state() == KeyEvent::Released ? "up" : "repeat",
              event.text().c_str());

        if (event.state() == KeyEvent::Pressed)
        {
            switch (event.ddKey())
            {
            case '1': self().testRenderToTexture(); break;
            case '2': self().testDynamicAtlas(); break;
            case '3': self().testModel(); break;
            case '4': self().loadMD2Model(); break;
            case '5': self().loadMD5Model(); break;
            default: break;
            }
        }
    }
};

TestWindow::TestWindow() : d(new Impl(this))
{
    setTitle("libgui GL Sandbox");
    setMinimumSize(Size(640, 480));

//    QToolBar *tools = new QToolBar(tr("Tests"));
//    tools->addAction("RTT", this, SLOT(testRenderToTexture()));
//    tools->addAction("Atlas", this, SLOT(testDynamicAtlas()));
//    tools->addAction("Model", this, SLOT(testModel()));
//    tools->addSeparator();
//    tools->addAction("MD2", this, SLOT(loadMD2Model()));
//    tools->addAction("MD5", this, SLOT(loadMD5Model()));

//    tools->show();

//    tools->setGeometry(25, 75, tools->width(), tools->height());
}

void TestWindow::draw()
{
    LIBGUI_ASSERT_GL_OK();
    d->draw();
    LIBGUI_ASSERT_GL_OK();
}

void TestWindow::rootUpdate()
{
    // no widgets to update
}

/*
void TestWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->modifiers() & Qt::ControlModifier)
    {
        ev->accept();

        switch (ev->key())
        {
        case Qt::Key_1:
            testRenderToTexture();
            break;

        case Qt::Key_2:
            testDynamicAtlas();
            break;

        case Qt::Key_3:
            testModel();
            break;

        case Qt::Key_4:
            loadMD2Model();
            break;

        case Qt::Key_5:
            loadMD5Model();
            break;
        }
        return;
    }

    GLWindow::keyPressEvent(ev);
}
*/

void TestWindow::testRenderToTexture()
{
    d->setMode(Impl::TestRenderToTexture);
}

void TestWindow::testDynamicAtlas()
{
    d->setMode(Impl::TestDynamicAtlas);
}

void TestWindow::testModel()
{
    d->setMode(Impl::TestModel);
}

void TestWindow::loadMD2Model()
{
    glActivate();
    d->model.load(App::rootFolder().locate<File>("/packs/net.dengine.test.glsandbox/models/marine.md2"));
    d->initModelAnimation();
    glDone();
}

void TestWindow::loadMD5Model()
{
    glActivate();
    d->model.load(App::rootFolder().locate<File>("/packs/net.dengine.test.glsandbox/models/boblampclean.md5mesh"));
    d->initModelAnimation();
    glDone();
}
