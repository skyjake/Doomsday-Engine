#include "skybox.h"
#include "../../src/gloomapp.h"

#include <de/Drawable>

using namespace de;

namespace gloom {

DENG2_PIMPL_NOREF(SkyBox)
{
    typedef GLBufferT<Vertex3Tex2BoundsRgba> VBuf;

    AtlasTexture *atlas{nullptr};
    Id            skyTex;
    Drawable      skyBox;
    GLUniform     uMvpMatrix{"uMvpMatrix", GLUniform::Mat4};
    GLUniform     uTex      {"uTex",       GLUniform::Sampler2D};
    float         scale = 1.f;
};

SkyBox::SkyBox() : d(new Impl)
{}

void SkyBox::setAtlas(AtlasTexture &atlas)
{
    d->atlas = &atlas;
    d->uTex  = atlas;
}

void SkyBox::setSize(float scale)
{
    d->scale = scale;
}

void SkyBox::glInit()
{
    using VBuf = Impl::VBuf;

    d->skyTex = d->atlas->alloc(GloomApp::images().image("sky.day"));

    // Make a sky box.
    VBuf *buf = new VBuf;
    VBuf::Builder verts;
    VBuf::Type v;

    v.texBounds   = d->atlas->imageRectf(d->skyTex).xywh();
    v.texCoord[1] = Vector2f(512, 512);
    v.rgba        = Vector4f(1, 1, 1, 1);

    const float gap = 4.f / (6*512);

    // Sides.
    v.pos = Vector3f(-1, -1, -1); v.texCoord[0] = Vector2f(gap,             1 - gap); verts << v;
    v.pos = Vector3f(-1,  1, -1); v.texCoord[0] = Vector2f(gap,             gap); verts << v;
    v.pos = Vector3f(-1, -1,  1); v.texCoord[0] = Vector2f(1.f/6.f,         1 - gap); verts << v;
    v.pos = Vector3f(-1,  1,  1); v.texCoord[0] = Vector2f(1.f/6.f,         gap); verts << v;
    v.pos = Vector3f( 1, -1,  1); v.texCoord[0] = Vector2f(2.f/6.f,         1 - gap); verts << v;
    v.pos = Vector3f( 1,  1,  1); v.texCoord[0] = Vector2f(2.f/6.f,         gap); verts << v;
    v.pos = Vector3f( 1, -1, -1); v.texCoord[0] = Vector2f(3.f/6.f,         1 - gap); verts << v;
    v.pos = Vector3f( 1,  1, -1); v.texCoord[0] = Vector2f(3.f/6.f,         gap); verts << v;
    v.pos = Vector3f(-1, -1, -1); v.texCoord[0] = Vector2f(4.f/6.f - gap,   1 - gap); verts << v;
    v.pos = Vector3f(-1,  1, -1); v.texCoord[0] = Vector2f(4.f/6.f - gap,   gap); verts << v << v;

    // Top cap.
    v.pos = Vector3f( 1,  1, -1); v.texCoord[0] = Vector2f(6.f/6.f - gap, gap); verts << v << v;
    v.pos = Vector3f(-1,  1, -1); v.texCoord[0] = Vector2f(5.f/6.f + gap, gap); verts << v;
    v.pos = Vector3f( 1,  1,  1); v.texCoord[0] = Vector2f(6.f/6.f - gap, 1 - gap); verts << v;
    v.pos = Vector3f(-1,  1,  1); v.texCoord[0] = Vector2f(5.f/6.f + gap, 1 - gap); verts << v << v;

    // Bottom cap.
    v.pos = Vector3f( 1, -1, -1); v.texCoord[0] = Vector2f(5.f/6.f - gap, gap); verts << v << v;
    v.pos = Vector3f(-1, -1, -1); v.texCoord[0] = Vector2f(4.f/6.f + gap, gap); verts << v;
    v.pos = Vector3f( 1, -1,  1); v.texCoord[0] = Vector2f(5.f/6.f - gap, 1 - gap); verts << v;
    v.pos = Vector3f(-1, -1,  1); v.texCoord[0] = Vector2f(4.f/6.f + gap, 1 - gap); verts << v;

    buf->setVertices(gl::TriangleStrip, verts, gl::Static);
    d->skyBox.addBuffer(buf);

    GloomApp::shaders().build(d->skyBox.program(), "indirect.textured.color")
            << d->uMvpMatrix
            << d->uTex;
}

void SkyBox::glDeinit()
{
    d->skyBox.clear();
}

void SkyBox::render(Matrix4f const &mvpMatrix)
{
    GLState::push().setDepthWrite(false);

    DENG2_ASSERT(d->skyBox.program().isReady());
    d->uMvpMatrix = mvpMatrix * Matrix4f::scale(d->scale);
    d->skyBox.draw();

    GLState::pop();
}

} // namespace gloom
