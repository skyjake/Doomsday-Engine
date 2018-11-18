/** @file vrwindowtransform.cpp  Window content transformation for virtual reality.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/VRWindowTransform"
#include "de/VRConfig"
#include "de/BaseGuiApp"
#include "de/BaseWindow"
#include "de/GuiWidget"

#include <de/Drawable>
#include <de/GLInfo>
#include <de/GLTextureFramebuffer>

namespace de {

DENG2_PIMPL(VRWindowTransform)
{
    VRConfig &vrCfg;

    GLTextureFramebuffer unwarpedFB;

    // Row-interleaved drawing:
    GLTextureFramebuffer rowInterRightFB;
    Drawable             rowInterDrawable;
    GLUniform            rowInterUniformTex { "uTex", GLUniform::Sampler2D };
    bool                 rowInterNeedRelease = true;

    Impl(Public *i)
        : Base(i)
        , vrCfg(DENG2_BASE_GUI_APP->vr())
    {}

    ~Impl()
    {
        vrCfg.oculusRift().deinit();
        rowInterRightFB.glDeinit();
    }

    GLFramebuffer &target() const
    {
        return self().window().framebuffer();
    }

    int width() const
    {
        return self().window().pixelWidth();
    }

    int height() const
    {
        return self().window().pixelHeight();
    }

    float displayModeDependentUIScalingFactor() const
    {
#if defined (DENG_MOBILE)
        return 1.0f;
#else
        if (GuiWidget::pointsToPixels(1) == 1) return 1.0f; // Not enough pixels for good-quality scaling.

        // Since the UI style doesn't yet support scaling at runtime based on
        // display resolution (or any other factor).
        return 1.f / Rangef(.5f, 1.0f)
                         .clamp((width()) /
                                GuiWidget::pointsToPixels(640.f));
#endif
    }

    void drawContent() const
    {
        LIBGUI_ASSERT_GL_OK();
        self().window().drawWindowContent();
        LIBGUI_ASSERT_GL_OK();
    }

    /**
     * Draws the entire UI in two halves, one for the left eye and one for the right. The
     * Oculus Rift optical distortion effect is applied using a shader.
     *
     * @todo unwarpedTarget and unwarpedTexture should be cleared/deleted when Oculus
     * Rift mode is disabled (or whenever they are not needed).
     */
    void vrDrawOculusRift()
    {
        OculusRift &ovr = vrCfg.oculusRift();

        vrCfg.enableFrustumShift(false);

        // Use a little bit of multisampling to smooth out the magnified jagged edges.
        // Note: Independent of the window FSAA setting because this is beneficial even
        // when FSAA is disabled.
        unwarpedFB.setSampleCount(1); //vrCfg.riftFramebufferSampleCount());

        // Set render target to offscreen temporarily.
        GLState::push()
                .setTarget(unwarpedFB)
                .setViewport(Rectangleui::fromSize(unwarpedFB.size()));
        unwarpedFB.unsetActiveRect(true);

        GLTextureFramebuffer::Size const fbSize = unwarpedFB.size();

        // Left eye view on left side of screen.
        for (int eyeIdx = 0; eyeIdx < 2; ++eyeIdx)
        {
            ovr.setCurrentEye(eyeIdx);
            if (ovr.currentEye() == OculusRift::LeftEye)
            {
                // Left eye on the left side of the screen.
                unwarpedFB.setActiveRect(Rectangleui(0, 0, fbSize.x/2, fbSize.y), true);
            }
            else
            {
                // Right eye on the right side of screen.
                unwarpedFB.setActiveRect(Rectangleui(fbSize.x/2, 0, fbSize.x/2, fbSize.y), true);
            }
            drawContent();
        }

        unwarpedFB.unsetActiveRect(true);
        GLState::pop();

        vrCfg.enableFrustumShift(); // restore default
    }

    /**
     * Initialize drawable for row-interleaved stereo.
     */
    void vrInitRowInterleaved()
    {
        if (rowInterDrawable.isReady())
        {
            return;
        }

        typedef GLBufferT<Vertex2Tex> VBuf;
        VBuf *buf = new VBuf;
        rowInterDrawable.addBuffer(buf);
        rowInterDrawable.program().build(// Vertex shader:
                                          Block("attribute highp vec4 aVertex; "
                                                "attribute highp vec2 aUV; "
                                                "varying highp vec2 vUV; "
                                                "void main(void) {"
                                                "gl_Position = aVertex; "
                                                "vUV = aUV; }"),
                                          // Fragment shader:
                                          Block("uniform sampler2D uTex; "
                                                "varying highp vec2 vUV; "
                                                "void main(void) { "
                                                "if(int(mod(gl_FragCoord.y - 1023.5, 2.0)) != 1) { discard; }\n"
                                                "gl_FragColor = texture2D(uTex, vUV); }"))
                << rowInterUniformTex;
        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(-1, -1, 2, 2), Rectanglef(0, 0, 1, 1)),
                         gl::Static);
    }

    void draw()
    {
        // Release the row-interleaved FB if not being used.
        rowInterNeedRelease = true;

        switch (vrCfg.mode())
        {
        // A) Single view type stereo 3D modes here:
        case VRConfig::Mono:
            // Non-stereoscopic frame.
            drawContent();
            break;

        case VRConfig::LeftOnly:
            // Left eye view
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            drawContent();
            break;

        case VRConfig::RightOnly:
            // Right eye view
            vrCfg.setCurrentEye(VRConfig::RightEye);
            drawContent();
            break;

        // B) Split-screen type stereo 3D modes here:
        case VRConfig::TopBottom: // Left goes on top
            // Left eye view on top of screen.
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            target().setActiveRect(Rectangleui(0, 0, width(), height()/2), true);
            drawContent();
            // Right eye view on bottom of screen.
            vrCfg.setCurrentEye(VRConfig::RightEye);
            target().setActiveRect(Rectangleui(0, height()/2, width(), height()/2), true);
            drawContent();
            break;

        case VRConfig::SideBySide: // Squished aspect
            // Left eye view on left side of screen.
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            target().setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
            drawContent();
            // Right eye view on right side of screen.
            vrCfg.setCurrentEye(VRConfig::RightEye);
            target().setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
            drawContent();
            break;

        case VRConfig::Parallel: // Normal aspect
            // Left eye view on left side of screen.
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            target().setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
            drawContent();
            // Right eye view on right side of screen.
            vrCfg.setCurrentEye(VRConfig::RightEye);
            target().setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
            drawContent();
            break;

        case VRConfig::CrossEye: // Normal aspect
            // Right eye view on left side of screen.
            vrCfg.setCurrentEye(VRConfig::RightEye);
            target().setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
            drawContent();
            // Left eye view on right side of screen.
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            target().setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
            drawContent();
            break;

        case VRConfig::OculusRift:
            vrDrawOculusRift();
            break;

        // Overlaid type stereo 3D modes below:
        case VRConfig::GreenMagenta:
            // Left eye view
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            GLState::push().setColorMask(gl::WriteGreen | gl::WriteAlpha); // Left eye view green
            drawContent();
            // Right eye view
            vrCfg.setCurrentEye(VRConfig::RightEye);
            GLState::current().setColorMask(gl::WriteRed | gl::WriteBlue | gl::WriteAlpha); // Right eye view magenta
            drawContent();
            GLState::pop();
            break;

        case VRConfig::RedCyan:
            // Left eye view
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            GLState::push().setColorMask(gl::WriteRed | gl::WriteAlpha); // Left eye view red
            drawContent();
            // Right eye view
            vrCfg.setCurrentEye(VRConfig::RightEye);
            GLState::current().setColorMask(gl::WriteGreen | gl::WriteBlue | gl::WriteAlpha); // Right eye view cyan
            drawContent();
            GLState::pop();
            break;

        case VRConfig::QuadBuffered:
            if (self().window().format().stereo())
            {
                /// @todo Fix me!

                // Left eye view
                vrCfg.setCurrentEye(VRConfig::LeftEye);
                drawContent();
                //canvas().framebuffer().swapBuffers(canvas(), gl::SwapStereoLeftBuffer);

                // Right eye view
                vrCfg.setCurrentEye(VRConfig::RightEye);
                drawContent();
                //canvas().framebuffer().swapBuffers(canvas(), gl::SwapStereoRightBuffer);
            }
            else
            {
                // Normal non-stereoscopic frame.
                drawContent();
            }
            break;

        case VRConfig::RowInterleaved: {
#if !defined (DENG_MOBILE)
            // Use absolute screen position of window to determine whether the
            // first scan line is odd or even.
            QPoint ulCorner(0, 0);
            ulCorner = self().window().mapToGlobal(ulCorner); // widget to screen coordinates
            bool const rowParityIsEven = ((int(ulCorner.y()) % 2) == 0);

            // Draw left eye view directly to the screen
            vrCfg.setCurrentEye(rowParityIsEven? VRConfig::LeftEye : VRConfig::RightEye);
            drawContent();

            // Draw right eye view to FBO
            rowInterNeedRelease = false;
            rowInterRightFB.glInit();
            rowInterRightFB.resize(GLFramebuffer::Size(width(), height()));
            rowInterRightFB.colorTexture().setFilter(gl::Linear, gl::Linear, gl::MipNone);
            rowInterRightFB.colorTexture().glApplyParameters();
            GLState::push()
                    .setTarget(rowInterRightFB)
                    .setViewport(Rectangleui::fromSize(rowInterRightFB.size()));
            vrCfg.setCurrentEye(rowParityIsEven ? VRConfig::RightEye : VRConfig::LeftEye);
            drawContent();
            GLState::pop();

            // Draw right eye view to the screen from FBO color texture
            vrInitRowInterleaved();
            rowInterUniformTex = rowInterRightFB.colorTexture();
            rowInterDrawable.draw();
#endif
            break;
          }

        case VRConfig::ColumnInterleaved: /// @todo implement column interleaved stereo 3D after row intleaved is working correctly...
        case VRConfig::Checkerboard: /// @todo implement checker stereo 3D after row intleaved is working correctly ...
        default:
            // Non-stereoscopic frame.
            drawContent();
            break;
        }

        if (rowInterNeedRelease)
        {
            // release unused FBOs
            rowInterRightFB.glDeinit();
        }

        // Restore default VR dynamic parameters
        target().unsetActiveRect(true);
        vrCfg.setCurrentEye(VRConfig::NeitherEye);

        LIBGUI_ASSERT_GL_OK();
    }
};

VRWindowTransform::VRWindowTransform(BaseWindow &window)
    : WindowTransform(window), d(new Impl(this))
{}

void VRWindowTransform::glInit()
{
    //d->init();
}

void VRWindowTransform::glDeinit()
{
    //d->deinit();
}

Vector2ui VRWindowTransform::logicalRootSize(Vector2ui const &physicalWindowSize) const
{
    GLWindow::Size size = physicalWindowSize;

    switch (d->vrCfg.mode())
    {
    // Left-right screen split modes
    case VRConfig::CrossEye:
    case VRConfig::Parallel:
        // Adjust effective UI size for stereoscopic rendering.
        size.y *= 2;
        size *= .75f; // Make it a bit bigger.
        break;

    case VRConfig::OculusRift:
        // Adjust effective UI size for stereoscopic rendering.
        size.x = size.y * d->vrCfg.oculusRift().aspect();
        //size.y *= d->vrCfg.oculusRift().aspect();
        size *= GuiWidget::pointsToPixels(1) * .75f;
        break;

    // Allow UI to squish in top/bottom and SBS mode: 3D hardware will unsquish them
    case VRConfig::TopBottom:
    case VRConfig::SideBySide:
    default:
        break;
    }

    size *= d->displayModeDependentUIScalingFactor();

    return size;
}

Vector2f VRWindowTransform::windowToLogicalCoords(Vector2i const &winPos) const
{
    // We need to map the real window coordinates to logical root view
    // coordinates according to the used transformation.

    Vector2f pos = winPos;

    Vector2f const size = window().pixelSize();
    Vector2f viewSize = window().windowContentSize();

    switch (d->vrCfg.mode())
    {
    // Left-right screen split modes
    case VRConfig::SideBySide:
    case VRConfig::CrossEye:
    case VRConfig::Parallel:
    case VRConfig::OculusRift:
        // Make it possible to access both frames.
        if (pos.x >= size.x/2)
        {
            pos.x -= size.x/2;
        }
        pos.x *= 2;
        break;

    // Top-bottom screen split modes
    case VRConfig::TopBottom:
        // Make it possible to access both frames.
        if (pos.y >= size.y/2)
        {
            pos.y -= size.y/2;
        }
        pos.y *= 2;
        break;

    default:
        // Not transformed.
        break;
    }

    // Scale to logical size.
    pos = pos / size * viewSize;

    return pos;
}

Vector2f VRWindowTransform::logicalToWindowCoords(Vector2i const &logicalPos) const
{
    Vector2f pos = logicalPos;

    Vector2f const size = window().pixelSize();
    Vector2f viewSize = window().windowContentSize();

    // Scale to pixel size.
    pos = pos / viewSize * size;

    return pos;
}

void VRWindowTransform::drawTransformed()
{
    d->draw();
}

GLTextureFramebuffer &VRWindowTransform::unwarpedFramebuffer()
{
    return d->unwarpedFB;
}

} // namespace de
