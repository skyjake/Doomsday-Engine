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

#include <de/Log>
#include <de/Canvas>
#include <de/Drawable>
#include <de/GLInfo>

namespace de {

struct DefaultSampleCount {
    int samples;

    DENG2_DEFINE_AUDIENCE(Change, void defaultSampleCountChanged())

    DefaultSampleCount() : samples(1) {}
    void set(int value) {
        samples = value;
        DENG2_FOR_AUDIENCE(Change, i) {
            i->defaultSampleCountChanged();
        }
    }
};

static DefaultSampleCount defaultSampleCount;

DENG2_PIMPL(GLFramebuffer)
, DENG2_OBSERVES(DefaultSampleCount, Change)
{
    Image::Format colorFormat;
    Size size;
    int _samples; ///< don't touch directly (0 == default)
    GLTarget target;
    GLTexture color;
    GLTexture depthStencil;

    GLTarget multisampleTarget;

    Drawable bufSwap;
    GLUniform uMvpMatrix;
    GLUniform uBufTex;
    typedef GLBufferT<Vertex2Tex> VBuf;

    Instance(Public *i)
        : Base(i)
        , colorFormat(Image::RGB_888)
        , _samples(0)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uBufTex   ("uTex",       GLUniform::Sampler2D)
    {
        defaultSampleCount.audienceForChange += this;
    }

    ~Instance()
    {
        defaultSampleCount.audienceForChange -= this;
    }

    int sampleCount() const
    {
        if(_samples <= 0) return defaultSampleCount.samples;
        return _samples;
    }

    bool isMultisampled() const
    {
        if(!GLInfo::extensions().EXT_framebuffer_multisample)
        {
            // Not supported.
            return false;
        }
        return sampleCount() > 1;
    }

    void defaultSampleCountChanged()
    {
        reconfigure();
    }

    void alloc()
    {
        // Prepare the fallback blit method.
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
        multisampleTarget.configure();
    }

    void reconfigure()
    {
        if(!self.isReady() || size == Size()) return;

        LOGDEV_GL_VERBOSE("Reconfiguring framebuffer: %s ms:%i")
                << size.asText() << sampleCount();

        // Configure textures for the framebuffer.
        color.setUndefinedImage(size, colorFormat);
        color.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        color.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

        depthStencil.setDepthStencilContent(size);
        depthStencil.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        depthStencil.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

        try
        {
            // We'd like to use texture attachments for both color and depth/stencil.
            target.configure(&color, &depthStencil);
        }
        catch(GLTarget::ConfigError const &er)
        {
            // Alternatively try without depth/stencil texture (some renderer features
            // will not be available!).
            LOG_GL_WARNING("Texture-based framebuffer failed: %s\n"
                           "Trying fallback without depth/stencil texture")
                    << er.asText();

            target.configure(GLTarget::Color, color, GLTarget::DepthStencil);
        }

        target.clear(GLTarget::ColorDepthStencil);

        if(isMultisampled())
        {
            try
            {
                // Set up the multisampled target with suitable renderbuffers.
                multisampleTarget.configure(size, GLTarget::ColorDepthStencil, sampleCount());
                multisampleTarget.clear(GLTarget::ColorDepthStencil);

                // Actual drawing occurs in the multisampled target that is then
                // blitted to the main target.
                target.setProxy(&multisampleTarget);
            }
            catch(GLTarget::ConfigError const &er)
            {
                LOG_GL_WARNING("Multisampling not supported: %s") << er.asText();
                _samples = 1;
                goto noMultisampling;
            }
        }
        else
        {
noMultisampling:
            multisampleTarget.configure();
        }
    }

    void resize(Size const &newSize)
    {
        if(size != newSize)
        {
            size = newSize;
            reconfigure();
        }
    }

    void drawSwap()
    {
        if(isMultisampled())
        {
            target.updateFromProxy();
        }
        bufSwap.draw();
    }

    void swapBuffers(Canvas &canvas, gl::SwapBufferMode swapMode)
    {
        GLTarget defaultTarget;

        GLState::push()
                .setTarget(defaultTarget)
                .setViewport(Rectangleui::fromSize(size))
                .apply();

        switch(swapMode)
        {
        case gl::SwapMonoBuffer:
            if(GLInfo::extensions().EXT_framebuffer_blit)
            {
                if(isMultisampled())
                {
                    multisampleTarget.blit(defaultTarget); // resolve multisampling to system backbuffer
                }
                else
                {
                    target.blit(defaultTarget);  // copy to system backbuffer
                }
            }
            else
            {
                // Fallback: draw the back buffer texture to the main framebuffer.
                bufSwap.draw();
            }
            canvas.QGLWidget::swapBuffers();
            break;

        case gl::SwapStereoLeftBuffer:
            glDrawBuffer(GL_BACK_LEFT);
            drawSwap();
            glDrawBuffer(GL_BACK);
            break;

        case gl::SwapStereoRightBuffer:
            glDrawBuffer(GL_BACK_RIGHT);
            drawSwap();
            glDrawBuffer(GL_BACK);
            break;

        case gl::SwapStereoBuffers:
            canvas.QGLWidget::swapBuffers();
            break;
        }

        GLState::pop().apply();
    }
};

GLFramebuffer::GLFramebuffer(Image::Format const &colorFormat, Size const &initialSize, int sampleCount)
    : d(new Instance(this))
{
    d->colorFormat = colorFormat;
    d->size        = initialSize;
    d->_samples    = sampleCount;
}

void GLFramebuffer::glInit()
{
    if(isReady()) return;

    LOG_AS("GLFramebuffer");

    // Check for some integral OpenGL functionality.
    if(!GLInfo::extensions().ARB_framebuffer_object)
    {
        LOG_GL_WARNING("Required GL_ARB_framebuffer_object is missing!");
    }
    if(!GLInfo::extensions().EXT_packed_depth_stencil)
    {
        LOG_GL_WARNING("GL_EXT_packed_depth_stencil is missing, some features may be unavailable");
    }

    d->alloc();
    setState(Ready);

    d->reconfigure();
}

void GLFramebuffer::glDeinit()
{
    setState(NotReady);
    d->release();
}

void GLFramebuffer::setSampleCount(int sampleCount)
{
    if(!GLInfo::isFramebufferMultisamplingSupported())
    {
        sampleCount = 1;
    }

    if(d->_samples != sampleCount)
    {
        LOG_AS("GLFramebuffer");

        d->_samples = sampleCount;
        d->reconfigure();
    }
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

void GLFramebuffer::swapBuffers(Canvas &canvas, gl::SwapBufferMode swapMode)
{
    d->swapBuffers(canvas, swapMode);
}

bool GLFramebuffer::setDefaultMultisampling(int sampleCount)
{   
    LOG_AS("GLFramebuffer");

    int const newCount = max(1, sampleCount);
    if(defaultSampleCount.samples != newCount)
    {
        defaultSampleCount.set(newCount);
        return true;
    }
    return false;
}

int GLFramebuffer::defaultMultisampling()
{
    return defaultSampleCount.samples;
}

} // namespace de
