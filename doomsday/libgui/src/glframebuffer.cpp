/** @file glframebuffer.cpp  GL frame buffer.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLFramebuffer"

#include <de/Canvas>
#include <de/Drawable>

namespace de {

DENG2_PIMPL(GLFramebuffer)
{
    Image::Format colorFormat;
    Size size;
    GLTarget target;
    GLTexture color;
    GLTexture depthStencil;

    Drawable bufSwap;
    GLUniform uMvpMatrix;
    GLUniform uBufTex;
    typedef GLBufferT<Vertex2Tex> VBuf;

    Instance(Public *i)
        : Base(i)
        , colorFormat(Image::RGB_888)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uBufTex   ("uTex",       GLUniform::Sampler2D)
    {}

    void alloc()
    {
        VBuf *buf = new VBuf;
        bufSwap.addBuffer(buf);
        bufSwap.program().build(// Vertex shader:
                                Block("uniform highp mat4 uMvpMatrix; "
                                      "attribute highp vec4 aVertex; "
                                      "attribute highp vec2 aUV; "
                                      "varying highp vec2 vUV; "
                                      "void main(void) {"
                                          "gl_Position = uMvpMatrix * aVertex; "
                                          "vUV = aUV; }"),
                                // Fragment shader:
                                Block("uniform sampler2D uTex; "
                                      "varying highp vec2 vUV; "
                                      "void main(void) { "
                                          "gl_FragColor = texture2D(uTex, vUV); }"))
                << uMvpMatrix
                << uBufTex;

        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1), Rectanglef(0, 1, 1, -1)),
                         gl::Static);

        uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        uBufTex = color;
    }

    void release()
    {
        bufSwap.clear();
        color.clear();
        depthStencil.clear();
        target.configure();
    }

    void reconfigure()
    {
        if(!self.isReady() || size == Size()) return;

        color.setUndefinedImage(size, colorFormat);
        color.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        color.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

        depthStencil.setDepthStencilContent(size);
        depthStencil.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        depthStencil.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

        target.configure(&color, &depthStencil);
    }

    void resize(Size const &newSize)
    {
        if(size != newSize)
        {
            size = newSize;
            reconfigure();
        }
    }

    void swapBuffers(Canvas &canvas)
    {
        GLTarget defaultTarget;
        GLState::push()
                .setTarget(defaultTarget)
                .setViewport(Rectangleui::fromSize(size));

        // Draw the back buffer texture to the main framebuffer.
        bufSwap.draw();

        canvas.QGLWidget::swapBuffers();

        GLState::pop().apply();
    }
};

GLFramebuffer::GLFramebuffer(Image::Format const &colorFormat, Size const &initialSize)
    : d(new Instance(this))
{
    d->colorFormat = colorFormat;
    d->size = initialSize;
}

void GLFramebuffer::glInit()
{
    if(isReady()) return;

    d->alloc();
    setState(Ready);

    d->reconfigure();
}

void GLFramebuffer::glDeinit()
{
    setState(NotReady);
    d->release();
}

void GLFramebuffer::setColorFormat(Image::Format const &colorFormat)
{
    if(d->colorFormat != colorFormat)
    {
        d->colorFormat = colorFormat;
        d->reconfigure();
    }
}

void GLFramebuffer::resize(Size const &newSize)
{
    d->resize(newSize);
}

GLFramebuffer::Size GLFramebuffer::size() const
{
    return d->size;
}

GLTarget &GLFramebuffer::target() const
{
    return d->target;
}

GLTexture &GLFramebuffer::colorTexture() const
{
    return d->color;
}

GLTexture &GLFramebuffer::depthStencilTexture() const
{
    return d->depthStencil;
}

void GLFramebuffer::swapBuffers(Canvas &canvas)
{
    d->swapBuffers(canvas);
}

} // namespace de
