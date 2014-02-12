/** @file vrwindowtransform.cpp  Window content transformation for virtual reality.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
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

#include "de/VRWindowTransform"
#include "de/VRConfig"
#include "de/BaseGuiApp"
#include "de/BaseWindow"
//#include "de_platform.h"
//#include "con_main.h"
//#include "render/vr.h"

#include <de/Drawable>
#include <de/GLFramebuffer>

namespace de {

DENG2_PIMPL(VRWindowTransform)
{
    VRConfig &vrCfg;
    Drawable oculusRift;
    GLUniform uOculusRiftFB;
    GLUniform uOculusDistortionScale;
    GLUniform uOculusScreenSize;
    GLUniform uOculusLensSeparation;
    GLUniform uOculusHmdWarpParam;
    GLUniform uOculusChromAbParam;

    typedef GLBufferT<Vertex3Tex> OculusRiftVBuf;
    GLFramebuffer unwarpedFB;

    Instance(Public *i)
        : Base(i)
        , vrCfg(DENG2_BASE_GUI_APP->vr())
        , uOculusRiftFB         ("texture",                GLUniform::Sampler2D)
        , uOculusDistortionScale("distortionScale",        GLUniform::Float)
        , uOculusScreenSize     ("screenSize",             GLUniform::Vec2)
        , uOculusLensSeparation ("lensSeparationDistance", GLUniform::Float)
        , uOculusHmdWarpParam   ("hmdWarpParam",           GLUniform::Vec4)
        , uOculusChromAbParam   ("chromAbParam",           GLUniform::Vec4)
    {}

    void init()
    {
        /// @todo Only do this when Oculus Rift mode is enabled. Free the allocated
        /// resources when non-Rift mode in use.

        OculusRiftVBuf *buf = new OculusRiftVBuf;
        oculusRift.addBuffer(buf);

        // Set up a simple static quad.
        OculusRiftVBuf::Type const verts[4] = {
            { Vector3f(-1,  1, 0.5f), Vector2f(0, 1), },
            { Vector3f( 1,  1, 0.5f), Vector2f(1, 1), },
            { Vector3f(-1, -1, 0.5f), Vector2f(0, 0), },
            { Vector3f( 1, -1, 0.5f), Vector2f(1, 0), }
        };
        buf->setVertices(gl::TriangleStrip, verts, 4, gl::Static);

        DENG2_BASE_GUI_APP->shaders()
                .build(oculusRift.program(), "vr.oculusrift.barrel")
                    << uOculusRiftFB
                    << uOculusDistortionScale
                    << uOculusScreenSize
                    << uOculusLensSeparation
                    << uOculusHmdWarpParam
                    << uOculusChromAbParam;

        unwarpedFB.glInit();
        uOculusRiftFB = unwarpedFB.colorTexture();
    }

    void deinit()
    {
        oculusRift.clear();
        unwarpedFB.glDeinit();
    }

    Canvas &canvas() const
    {
        return self.window().windowCanvas();
    }

    GLTarget &target() const
    {
        return canvas().renderTarget();
    }

    int width() const
    {
        return canvas().width();
    }

    int height() const
    {
        return canvas().height();
    }

    void drawContent() const
    {
        LIBGUI_ASSERT_GL_OK();
        self.window().drawWindowContent();
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
        vrCfg.enableFrustumShift(false);

        /// @todo shrunken hud
        // Allocate offscreen buffers - larger than Oculus Rift size, to get adequate resolution at center after warp
        // For some reason, 1.5X looks best, even though objects are ~2.3X unwarped size at center.
        float unwarpFactor = 1.5f;
        Canvas::Size textureSize = Canvas::Size(1280, 800) * unwarpFactor;
        // Canvas::Size textureSize(2560, 1600); // 2 * 1280x800 // Undesirable relative softness at very center of image
        // Canvas::Size textureSize(3200, 2000); // 2.5 * 1280x800 // Softness here too
        unwarpedFB.resize(textureSize);

        // Use a little bit of multisampling to smooth out the magnified jagged edges.
        // Note: Independent of the vid-fsaa setting because this is beneficial even when
        // vid-fsaa is disabled.
        unwarpedFB.setSampleCount(vrCfg.riftFramebufferSampleCount());
        unwarpedFB.colorTexture().setFilter(gl::Linear, gl::Linear, gl::MipNone);

        // Set render target to offscreen temporarily.
        GLState::push()
                .setTarget(unwarpedFB.target())
                .setViewport(Rectangleui::fromSize(unwarpedFB.size()))
                .apply();
        unwarpedFB.target().unsetActiveRect(true);
        unwarpedFB.target().clear(GLTarget::ColorDepth);

        // Left eye view on left side of screen.
        vrCfg.setCurrentEye(VRConfig::LeftEye);
        unwarpedFB.target().setActiveRect(Rectangleui(0, 0, textureSize.x/2, textureSize.y), true);
        drawContent();

        // Right eye view on right side of screen.
        vrCfg.setCurrentEye(VRConfig::RightEye);
        unwarpedFB.target().setActiveRect(Rectangleui(textureSize.x/2, 0, textureSize.x/2, textureSize.y), true);
        drawContent();

        unwarpedFB.target().unsetActiveRect(true);

        GLState::pop().apply();

        // Necessary until the legacy code uses GLState, too:
        glEnable(GL_TEXTURE_2D);

        target().clear(GLTarget::Color);
        GLState::push()
                .setDepthTest(false);

        // Copy contents of offscreen buffer to normal screen.
        uOculusDistortionScale = vrCfg.oculusRift().distortionScale();
        uOculusScreenSize      = vrCfg.oculusRift().screenSize();
        uOculusLensSeparation  = vrCfg.oculusRift().lensSeparationDistance();
        uOculusHmdWarpParam    = vrCfg.oculusRift().hmdWarpParam();
        uOculusChromAbParam    = vrCfg.oculusRift().chromAbParam();
        //
        oculusRift.draw();

        glBindTexture(GL_TEXTURE_2D, 0);
        glDepthMask(GL_TRUE);

        GLState::pop().apply();

        vrCfg.enableFrustumShift(); // restore default
    }

    void draw()
    {
        // Allow Oculus Rift to use the latest head tracking position for the upcoming
        // draw operations.
        vrCfg.oculusRift().allowUpdate();

        switch(vrCfg.mode())
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
            GLState::push().setColorMask(gl::WriteGreen | gl::WriteAlpha).apply(); // Left eye view green
            drawContent();
            // Right eye view
            vrCfg.setCurrentEye(VRConfig::RightEye);
            GLState::current().setColorMask(gl::WriteRed | gl::WriteBlue | gl::WriteAlpha).apply(); // Right eye view magenta
            drawContent();
            GLState::pop().apply();
            break;

        case VRConfig::RedCyan:
            // Left eye view
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            GLState::push().setColorMask(gl::WriteRed | gl::WriteAlpha).apply(); // Left eye view red
            drawContent();
            // Right eye view
            vrCfg.setCurrentEye(VRConfig::RightEye);
            GLState::current().setColorMask(gl::WriteGreen | gl::WriteBlue | gl::WriteAlpha).apply(); // Right eye view cyan
            drawContent();
            GLState::pop().apply();
            break;

        case VRConfig::QuadBuffered:
            if(canvas().format().stereo())
            {
                // Left eye view
                vrCfg.setCurrentEye(VRConfig::LeftEye);
                drawContent();
                canvas().framebuffer().swapBuffers(canvas(), gl::SwapStereoLeftBuffer);

                // Right eye view
                vrCfg.setCurrentEye(VRConfig::RightEye);
                drawContent();
                canvas().framebuffer().swapBuffers(canvas(), gl::SwapStereoRightBuffer);
            }
            else
            {
                // Normal non-stereoscopic frame.
                drawContent();
            }
            break;

        case VRConfig::RowInterleaved:
        {
            // Use absolute screen position of window to determine whether the
            // first scan line is odd or even.
            QPoint ulCorner(0, 0);
            ulCorner = canvas().mapToGlobal(ulCorner); // widget to screen coordinates
            bool rowParityIsEven = ((ulCorner.x() % 2) == 0);
            DENG2_UNUSED(rowParityIsEven);
            /// @todo - use row parity in shader or stencil, to actually interleave rows.
            // Left eye view
            vrCfg.setCurrentEye(VRConfig::LeftEye);
            drawContent();
            // Right eye view
            vrCfg.setCurrentEye(VRConfig::RightEye);
            drawContent();
            break;
        }

        case VRConfig::ColumnInterleaved: /// @todo implement column interleaved stereo 3D after row intleaved is working correctly...
        case VRConfig::Checkerboard: /// @todo implement checker stereo 3D after row intleaved is working correctly ...
        default:
            // Non-stereoscopic frame.
            drawContent();
            break;
        }

        // Restore default VR dynamic parameters
        target().unsetActiveRect(true);
        vrCfg.setCurrentEye(VRConfig::NeitherEye);

        LIBGUI_ASSERT_GL_OK();
    }
};

VRWindowTransform::VRWindowTransform(BaseWindow &window)
    : WindowTransform(window), d(new Instance(this))
{}

void VRWindowTransform::glInit()
{
    d->init();
}

void VRWindowTransform::glDeinit()
{
    d->deinit();
}

Vector2ui VRWindowTransform::logicalRootSize(Vector2ui const &physicalCanvasSize) const
{
    Canvas::Size size = physicalCanvasSize;

    switch(d->vrCfg.mode())
    {
    // Left-right screen split modes
    case VRConfig::CrossEye:
    case VRConfig::Parallel:
        // Adjust effective UI size for stereoscopic rendering.
        size.y *= 2;
        size *= .75f; // Make it a bit bigger.
        break;

    case VRConfig::OculusRift:
        /// @todo - taskbar needs to elevate above bottom of screen in Rift mode
        // Adjust effective UI size for stereoscopic rendering.
        size.x = size.y * d->vrCfg.oculusRift().aspect();
        size *= 1.0f; // Use a large font in taskbar
        break;

    // Allow UI to squish in top/bottom and SBS mode: 3D hardware will unsquish them
    case VRConfig::TopBottom:
    case VRConfig::SideBySide:
    default:
        break;
    }

    return size;
}

Vector2f VRWindowTransform::windowToLogicalCoords(Vector2i const &winPos) const
{
    // We need to map the real window coordinates to logical root view
    // coordinates according to the used transformation.

    Vector2f pos = winPos;

    Vector2f const size = window().windowCanvas().size();
    Vector2f const viewSize = window().windowContentSize();

    switch(d->vrCfg.mode())
    {
    // Left-right screen split modes
    case VRConfig::SideBySide:
    case VRConfig::CrossEye:
    case VRConfig::Parallel:
    case VRConfig::OculusRift:
        // Make it possible to access both frames.
        if(pos.x >= size.x/2)
        {
            pos.x -= size.x/2;
        }
        pos.x *= 2;

        // Scale to logical size.
        pos = pos / size * viewSize;
        break;

    // Top-bottom screen split modes
    case VRConfig::TopBottom:
        // Make it possible to access both frames.
        if(pos.y >= size.y/2)
        {
            pos.y -= size.y/2;
        }
        pos.y *= 2;

        // Scale to logical size.
        pos = pos / size * viewSize;
        break;

    default:
        // Not transformed.
        break;
    }

    return pos;
}

void VRWindowTransform::drawTransformed()
{
    d->draw();
}

} // namespace de
