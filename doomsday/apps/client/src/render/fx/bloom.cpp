/** @file bloom.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/fx/bloom.h"
#include "world/clientworld.h"
#include "clientapp.h"

#include <doomsday/console/var.h>
#include <de/Drawable>
#include <de/GLTextureFramebuffer>
#include <de/WindowTransform>

using namespace de;

namespace fx {

static int   bloomEnabled    = true;
static float bloomIntensity  = .65f;
static float bloomThreshold  = .35f;
static float bloomDispersion = 1;
static int   bloomComplexity = 1;

DE_PIMPL(Bloom)
{
    typedef GLBufferT<Vertex2Tex> VBuf;

    Drawable bloom;
    GLTextureFramebuffer workFB;
    GLUniform uMvpMatrix;
    GLUniform uTex;
    GLUniform uColor;
    GLUniform uBlurStep;
    GLUniform uWindow;
    GLUniform uThreshold;
    GLUniform uIntensity;

    Impl(Public *i)
        : Base(i)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uTex      ("uTex",       GLUniform::Sampler2D)
        , uColor    ("uColor",     GLUniform::Vec4)
        , uBlurStep ("uBlurStep",  GLUniform::Vec2)
        , uWindow   ("uWindow",    GLUniform::Vec4)
        , uThreshold("uThreshold", GLUniform::Float)
        , uIntensity("uIntensity", GLUniform::Float)
    {}

    void glInit()
    {
        // Geometry for drawing with: a single quad.
        VBuf *buf = new VBuf;
        buf->setVertices(gfx::TriangleStrip,
                         VBuf::Builder().makeQuad(
                             Rectanglef(0, 0, 1, 1),
                             Rectanglef(0, 0, 1, 1)),
                         gfx::Static);
        bloom.addBuffer(buf);

        // The work buffer does not need alpha because the result will be additively
        // blended back to the framebuffer.
        workFB.setColorFormat(Image::RGB_888);
        workFB.setSampleCount(1);
        workFB.glInit();

        ClientApp::shaders().build(bloom.program(), "fx.bloom.horizontal")
                << uMvpMatrix << uTex << uBlurStep << uWindow << uThreshold << uIntensity;

        bloom.addProgram("vert");
        ClientApp::shaders().build(bloom.program("vert"), "fx.bloom.vertical")
                << uMvpMatrix << uTex << uBlurStep << uWindow;

        uMvpMatrix = Mat4f::ortho(0, 1, 0, 1);
    }

    void glDeinit()
    {
        bloom.clear();
        workFB.glDeinit();
    }

    /**
     * Takes the current rendered frame buffer contents and applies bloom on it.
     */
    void draw()
    {
        GLFramebuffer &target = GLState::current().target();
        GLTexture *colorTex = target.attachedTexture(GLFramebuffer::Color0);

        //qDebug() << "bloom with" << colorTex;

        // Must have access to the color texture containing the frame buffer contents.
        if (!colorTex) return;

        // Determine the dimensions of the viewport and the target.
        //Rectanglef const rectf(0, 0, 1, 1); //= GLState::current().normalizedViewport();
        const Vec2ui targetSize = colorTex->size(); // (rectf.size() * target.rectInUse().size()).toVec2ui();

        // Quarter resolution is used for better efficiency (without significant loss
        // of quality).
        Vec2ui blurSize = (targetSize / 4).max(Vec2ui(1, 1));

        // Update the size of the work buffer if needed. Also ensure linear filtering
        // is used for better-quality blurring.
        workFB.resize(blurSize);
        workFB.colorTexture().setFilter(gfx::Linear, gfx::Linear, gfx::MipNone);

        GLState::push()
                .setDepthWrite(false) // don't mess with depth information
                .setDepthTest(false);

        switch (bloomComplexity)
        {
        case 1:
            // Two passes result in a better glow effect: combining multiple Gaussian curves
            // ensures that the middle peak is higher/sharper.
            drawBloomPass(*colorTex, .5f, .75f);
            drawBloomPass(*colorTex, 1.f, 1.f);
            break;

        default:
            // Single-pass for HW with slow fill rate.
            drawBloomPass(*colorTex, 1.f, 1.75f);
            break;
        }

        GLState::pop();
    }

    /**
     * Draws a bloom pass that takes the contents of the framebuffer, applies blurring
     * and thresholding, and blends the result additively back to the framebuffer.
     *
     * @param rectf        Normalized viewport rectangle within the target.
     * @param targetSize   Size of the actual area in pixels (affected by target
     *                     active rectangle and viewport).
     * @param colorTarget  Texture containing the frame buffer colors.
     * @param bloomSize    Size factor for the effect: at most 1.0; smaller values
     *                     cause more blurring/less quality as the work resolution
     *                     reduces.
     * @param weight       Weight factor for intensity.
     * @param targetOp     Blending factor (should be gfx::One unless debugging).
     */
    void drawBloomPass(//const Rectanglef &rectf, //const Vec2ui &/*targetSize*/,
                       GLTexture &colorTarget, float bloomSize, float weight,
                       gfx::Blend targetOp = gfx::One)
    {
        uThreshold = bloomThreshold * (1 + bloomSize) / 2.f;
        uIntensity = bloomIntensity * weight;

        // Initialize the work buffer for this pass.
        workFB.clear(GLFramebuffer::Color0);

        // Divert rendering to the work area (full or partial area used).
        //GLFramebuffer &target = GLState::current().target();
        const Vec2ui workSize = workFB.size() * bloomSize;
        GLState::push()
                .setTarget(workFB)
                .setViewport(Rectangleui::fromSize(workSize));

        // Normalized active rectangle of the target.
        /*Vec4f const active(target.activeRectScale(),
                              target.activeRectNormalizedOffset());*/

        /*
         * Draw step #1: thresholding and horizontal blur.
         */
        uTex = colorTarget;

        // Window in the color buffer: area occupied by the viewport. Top needs to
        // be flipped because the shader uses the bottom left corner as UV origin.
        // Also need to apply the active rectangle as it affects where the viewport
        // ends up inside the frame buffer.
        uWindow = Vec4f(0, 0, 1, 1); /*Vec4f(rectf.left() * active.x        + active.z,
                           1 - (rectf.bottom() * active.y + active.w),
                           rectf.width()  * active.x,
                           rectf.height() * active.y);*/

        // Spread out or contract the texture sampling of the Gaussian blur kernel.
        // If dispersion is too large, the quality of the blur will suffer.
        uBlurStep = Vec2f(bloomDispersion / workFB.size().x,
                          bloomDispersion / workFB.size().y);

        bloom.setProgram(bloom.program()); // horizontal shader
        bloom.draw();

        GLState::pop();

        workFB.resolveSamples();

        /*
         * Draw step #2: vertical blur and blending back to the real framebuffer.
         */
        GLState::push()
                .setBlend(true)
                .setBlendFunc(gfx::One, targetOp);

        // Use the work buffer's texture as the source.
        uTex    = workFB.colorTexture();
        uWindow = Vec4f(0, 1 - bloomSize, bloomSize, bloomSize);

        bloom.setProgram("vert"); // vertical shader
        bloom.draw();

        GLState::pop();
    }
};

Bloom::Bloom(int console)
    : ConsoleEffect(console)
    , d(new Impl(this))
{}

void Bloom::glInit()
{
    d->glInit();
    ConsoleEffect::glInit();
}

void Bloom::glDeinit()
{
    ConsoleEffect::glDeinit();
    d->glDeinit();
}

void Bloom::draw()
{
    if (!ClientApp::world().hasMap())
    {
        return;
    }

    if (!bloomEnabled || bloomIntensity <= 0)
    {
        return;
    }

    d->draw();
}

void Bloom::consoleRegister()
{
    C_VAR_INT  ("rend-bloom",            &bloomEnabled,    0, 0, 1);
    C_VAR_FLOAT("rend-bloom-threshold",  &bloomThreshold,  0, 0, 1);
    C_VAR_FLOAT("rend-bloom-intensity",  &bloomIntensity,  0, 0, 10);
    C_VAR_FLOAT("rend-bloom-dispersion", &bloomDispersion, 0, 0, 3.5f);
    C_VAR_INT  ("rend-bloom-complexity", &bloomComplexity, 0, 0, 1);
}

bool Bloom::isEnabled() // static
{
    return bloomEnabled;
}

float Bloom::intensity()
{
    return bloomIntensity;
}

} // namespace fx
